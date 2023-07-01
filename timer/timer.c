#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/******************************
 * 定时器控制led闪烁实验
 * 用spinlock保护全局变量timerdev.timeperiod，防止多进程调用造成冲突
*******************************/

#define TIMER_CNT               1
#define TIMER_NAME              "timer"
#define CLOSED_CMD              (_IO(0xEF, 0x1))        // 关闭定时器
#define OPEN_CMD                (_IO(0xEF, 0x2))        // 打开定时器
#define SETPERIOD_CMD           (_IO(0xEF, 0x3))        // 设置定时器周期命令

#define LEDON                   1
#define LEDOFF                  0

struct timer_dev {
        dev_t                   devid;
        struct cdev             cdev;
        struct class            *class;
        struct device           *device;
        int                     major;
        int                     minor;
        struct device_node      *nd;
        int                     led_gpio;
        int                     timeperiod;     // 定时周期 单位ms
        struct timer_list       timer;          // 定时器
        spinlock_t              lock;
};

struct timer_dev timerdev;

static int led_init(void)
{
        int ret = 0;
        /* 1. 从设备树获取信息*/
        timerdev.nd = of_find_node_by_path("/gpioled");
        if (timerdev.nd == NULL) {
                printk("get device node failed\r\n");
                return -EINVAL;
        }
        timerdev.led_gpio = of_get_named_gpio(timerdev.nd, "led-gpio", 0);
        if (timerdev.led_gpio < 0 ) {
                printk("get gpio number failed\r\n");
                return -EINVAL;
        }
        printk("gpio number is %d\r\n", timerdev.led_gpio);
        /* 2. 设置寄存器*/
        ret = gpio_request(timerdev.led_gpio, "timer");
        if (ret != 0) {
                printk("gpio request error ret=%d\r\n", ret);
                return ret;
        }
        ret = gpio_direction_output(timerdev.led_gpio, 1);
        if (ret < 0) {
                printk("cannot set gpio\r\n");
                return ret;
        }

        return 0;
}

static int timer_open(struct inode *inode, struct file *filp)
{
        filp->private_data = &timerdev;

        timerdev.timeperiod = 1000;
        return 0;
}

static long timer_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        int timerperiod;
        unsigned long flags;

        switch(cmd) {
        case CLOSED_CMD:
                del_timer_sync(&timerdev.timer);
                break;
        case OPEN_CMD:
                spin_lock_irqsave(&timerdev.lock, flags);
                timerperiod = timerdev.timeperiod;
                spin_unlock_irqrestore(&timerdev.lock, flags);
                mod_timer(&timerdev.timer, jiffies + msecs_to_jiffies(timerperiod));
                break;
        case SETPERIOD_CMD:
                spin_lock_irqsave(&timerdev.lock, flags);
                timerdev.timeperiod = arg;
                spin_unlock_irqrestore(&timerdev.lock, flags);
                mod_timer(&timerdev.timer, jiffies + msecs_to_jiffies(arg));
                break;
        default:
                break;
        }

        return 0;
}

static struct file_operations timer_fops = {
        .owner = THIS_MODULE,
        .open = timer_open,
        .unlocked_ioctl = timer_unlocked_ioctl,
};

static void timer_function(unsigned long arg)
{
        static int sta = 1;
        int timerperiod;
        unsigned long flags;

        sta = !sta;     // 闪烁
        gpio_set_value(timerdev.led_gpio, sta);

        /* 循环计时 */
        spin_lock_irqsave(&timerdev.lock, flags);
        timerperiod = timerdev.timeperiod;
        spin_unlock_irqrestore(&timerdev.lock, flags);
        mod_timer(&timerdev.timer, jiffies + msecs_to_jiffies(timerdev.timeperiod));
}

static int __init timer_init(void)
{
        int ret = 0;
        /* 1. 初始化自旋锁 */
        spin_lock_init(&timerdev.lock);

        /* 2. 初始化led*/
        ret = led_init();
        if (ret < 0) {
                printk("init led failed\r\n");
                return ret;
        }

        /* 3. 注册设备驱动 */
        if (timerdev.major) {
                timerdev.devid = MKDEV(timerdev.major, timerdev.minor);
                register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
        } else {
                alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);
                timerdev.major = MAJOR(timerdev.devid);
                timerdev.minor = MINOR(timerdev.devid);
        }

        timerdev.cdev.owner = THIS_MODULE;
        cdev_init(&timerdev.cdev, &timer_fops);
        cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);

        /* 4. 创建设备节点 */
        timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
        if (IS_ERR(timerdev.class))
                return PTR_ERR(timerdev.class);
        timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
        if (IS_ERR(timerdev.device))
                return PTR_ERR(timerdev.device);
        
        /* 5. 初始化定时器 */
        init_timer(&timerdev.timer);
        timerdev.timer.function = timer_function;
        timerdev.timer.data = (unsigned long) &timerdev;

        return 0;
}

static void __exit timer_exit(void)
{
        /*1.关灯然后释放gpio引脚*/
        gpio_set_value(timerdev.led_gpio, 1);
        gpio_free(timerdev.led_gpio);
        /*2.删除定时器*/
        del_timer_sync(&timerdev.timer);
        /*3.注销字符设备驱动*/
        cdev_del(&timerdev.cdev);
        unregister_chrdev_region(timerdev.devid, TIMER_CNT);
        /*4.注销设备节点*/
        device_destroy(timerdev.class, timerdev.devid);
        class_destroy(timerdev.class);
}

module_init(timer_init);
module_exit(timer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");


































