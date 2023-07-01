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
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/**************************************************
 * 采用pinctrl和gpio子系统驱动led 使用misc驱动控制led
***************************************************/

// #define GPIOLED_CNT             1       /*设备号个数*/
#define MISCLED_NAME            "miscled"       /*名字*/
#define MISCLED                 144             /*找一个没用的misc次设备号如144*/
#define LEDOFF                  0
#define LEDON                   1

/*led设备结构体*/
struct miscled_dev {
        // dev_t devid;                    // misc主设备号规定是10，次设备号自己定义的。所以devid可以自己算 MKDEV(10,MISCLED)
        // struct cdev cdev;
        // struct class *class;
        // struct device *device;
        // int major;
        // int minor;
        struct device_node *pnode;
        int gpio_num;
};

static struct miscled_dev miscled;

static int led_open(struct inode *inode, struct file *filp)
{
        filp->private_data = &miscled;
        return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
        int retvalue;
        unsigned char databuf[1];
        unsigned char ledstat;
        struct miscled_dev *dev = filp->private_data;

        retvalue = copy_from_user(databuf, buf, cnt);
        if (retvalue < 0) {
                printk("kernel write failed\r\n");
                return -EIO;
        }

        ledstat = databuf[0];

        if (ledstat == LEDON)
                gpio_set_value(dev->gpio_num, 0);
        else if (ledstat == LEDOFF)
                gpio_set_value(dev->gpio_num, 1);
        
        return 0;
}


/*注意这种定义&初始化方式 注意逗号 =号*/
static struct file_operations miscled_fops = {
        .owner = THIS_MODULE,
        .open = led_open,
        .write = led_write,
};

/* MISC设备结构体定义 */
static struct miscdevice led_miscdev = {
        .minor = MISCLED,
        .name = MISCLED_NAME,
        .fops = &miscled_fops,
};

static int led_probe(struct platform_device *dev)
{
        int ret = 0;

        /*1.获取设备树信息*/
        /*1.1 获取设备树上节点*/
        miscled.pnode = of_find_node_by_path("/gpioled");
        if (miscled.pnode == NULL) {
                printk("miscled node can't be found\r\n");
                return -EINVAL;
        } else {
                printk("miscled node has been found\r\n");
        }
        /*1.2 获取设备树种的gpio属性 得到led使用的gpio编号 */
        miscled.gpio_num = of_get_named_gpio(miscled.pnode, "led-gpio", 0);
        if (miscled.gpio_num < 0) {
                printk("get gpio number failed\r\n");
                return -EINVAL;
        }
        printk("led's gpio number = %d\r\n", miscled.gpio_num);
        ret = gpio_request(miscled.gpio_num, "miscled");
        if (ret != 0) {
                printk("gpio request error\r\n");
                return -EINVAL;
        }

        /*2.设置寄存器 （时钟，复用，电气已经被gpio子系统自动设置 剩DR GDIR）*/
        /*2.1 设置GPIO1——IO03 方向为输出，默认高电平关灯*/
        ret = gpio_direction_output(miscled.gpio_num, 1);
        if (ret < 0)
                printk("can't set gpio\r\n");

        /*3. 注册设备驱动*/
        // /*3.1 申请设备号*/
        // if (gpioled.major) {
        //         gpioled.devid = MKDEV(gpioled.major, gpioled.minor);
        //         ret = register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
        // } else {
        //         ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
        // }
        // gpioled.major = MAJOR(gpioled.devid);
        // gpioled.minor = MINOR(gpioled.minor);
        // /*3.2 初始化cdev结构体*/
        // gpioled.cdev.owner = THIS_MODULE;
        // cdev_init(&gpioled.cdev, &gpioled_fops);
        // /*3.3 注册设备驱动*/
        // cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);

        // /*4.创建设备节点*/
        // gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
        // if (IS_ERR(gpioled.class))
        //         return PTR_ERR(gpioled.class);
        // gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
        // if (IS_ERR(gpioled.device))
        //         return PTR_ERR(gpioled.device);

        ret = misc_register(&led_miscdev);
        if (ret < 0){
                printk("misc device register failed\r\n");
                return -EFAULT;
        }

        return 0;
} 

static const struct of_device_id led_of_match[] = {
        { .compatible = "atkalpha-gpioled"},
        {},
};

static int led_remove(struct platform_device *dev)
{
        gpio_set_value(miscled.gpio_num, 1);
        gpio_free(miscled.gpio_num);
        // 注销misc驱动 相当于cdev_del  unregister_chrdev_region destroy_device destroy_class
        misc_deregister(&led_miscdev);  
        return 0;
}

static struct platform_driver led_driver = {
        .driver = {
                .name = "miscled",
                .of_match_table = led_of_match,
        },
        .probe = led_probe,
        .remove = led_remove,
};

static int __init miscled_init(void)
{
        return platform_driver_register(&led_driver);
}

static void __exit miscled_exit(void)
{
        platform_driver_unregister(&led_driver);
}

module_init(miscled_init);
module_exit(miscled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");