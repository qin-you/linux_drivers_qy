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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/*******************************
 * 采用pinctrl和gpio子系统驱动led
********************************/

#define GPIOLED_CNT             1       /*设备号个数*/
#define GPIOLED_NAME            "gpioled"       /*名字*/
#define LEDOFF                  0
#define LEDON                   1

/*gpioled设备结构体*/
struct gpioled_dev {
        dev_t devid;
        struct cdev cdev;
        struct class *class;
        struct device *device;
        int major;
        int minor;
        struct device_node *pnode;
        int gpio_num;                   /*led使用的GPIO编号*/
};

static struct gpioled_dev gpioled;

static int led_open(struct inode *inode, struct file *filp)
{
        filp->private_data = &gpioled;
        return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
        return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
        int retvalue;
        unsigned char databuf[1];
        unsigned char ledstat;
        struct gpioled_dev *dev = filp->private_data;

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

static int led_release(struct inode *inode, struct file *filp)
{
        return 0;
}

/*注意这种定义&初始化方式 注意逗号 =号*/
static struct file_operations gpioled_fops = {
        .owner = THIS_MODULE,
        .open = led_open,
        .read = led_read,
        .write = led_write,
        .release = led_release,
};

static int __init led_init(void)
{
        int ret = 0;

        /*1.获取设备树信息*/
        /*1.1 获取设备树上节点*/
        gpioled.pnode = of_find_node_by_path("/gpioled");
        if (gpioled.pnode == NULL) {
                printk("gpioled node can't be found\r\n");
                return -EINVAL;
        } else {
                printk("gpioled node has been found\r\n");
        }
        /*1.2 获取设备树种的gpio属性 得到led使用的gpio编号 */
        gpioled.gpio_num = of_get_named_gpio(gpioled.pnode, "led-gpio", 0);
        if (gpioled.gpio_num < 0) {
                printk("get gpio number failed\r\n");
                return -EINVAL;
        }
        printk("led's gpio number = %d\r\n", gpioled.gpio_num);
        ret = gpio_request(gpioled.gpio_num, "gpioled");
        if (ret != 0) {
                printk("gpio request error\r\n");
                return -EINVAL;
        }

        /*2.设置寄存器 （时钟，复用，电气已经被gpio子系统自动设置 剩DR GDIR）*/
        /*2.1 设置GPIO1——IO03 方向为输出，默认高电平关灯*/
        ret = gpio_direction_output(gpioled.gpio_num, 1);
        if (ret < 0)
                printk("can't set gpio\r\n");

        /*3. 注册设备驱动*/
        /*3.1 申请设备号*/
        if (gpioled.major) {
                gpioled.devid = MKDEV(gpioled.major, gpioled.minor);
                ret = register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
        } else {
                ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
        }
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.minor);
        /*3.2 初始化cdev结构体*/
        gpioled.cdev.owner = THIS_MODULE;
        cdev_init(&gpioled.cdev, &gpioled_fops);
        /*3.3 注册设备驱动*/
        cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);

        /*4.创建设备节点*/
        gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
        if (IS_ERR(gpioled.class))
                return PTR_ERR(gpioled.class);
        gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
        if (IS_ERR(gpioled.device))
                return PTR_ERR(gpioled.device);

        return 0;
}

static void __exit led_exit(void)
{
        /* 0. free gpio*/
        gpio_free(gpioled.gpio_num);
        /* 1. 注销设备驱动 */
        cdev_del(&gpioled.cdev);
        unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);

        /* 2. 注销设备节点 */
        device_destroy(gpioled.class, gpioled.devid);
        class_destroy(gpioled.class);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");