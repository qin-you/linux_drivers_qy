#include <linux/types.h>
#include<linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/**********************************************************************
 * platform驱动LED 基于设备树获取设备信息 不需要设备模块device.c/device.ko
 * 主线是初始化和注册platform_driver结构体，注册在入口函数完成 
***********************************************************************/

#define LEDDEV_CNT      1
#define LEDDEV_NAME     "dtsplatled"
#define LEDOFF          0
#define LEDON           1

struct leddev_dev {
        dev_t devid;
        struct cdev cdev;
        struct class *class;
        struct device *device;
        int major;

        struct device_node *node;
        int     led0;                   //gpio号
};

struct leddev_dev leddev;

void led0_switch(u8 sta)
{
        if (sta == LEDON) {
                gpio_set_value(leddev.led0, 0);
        } else if (sta == LEDOFF) {
                gpio_set_value(leddev.led0, 1);
        }
}

static int led_open(struct inode *inode, struct file *filp)
{
        filp->private_data = &leddev;
        return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
        int retvalue;
        unsigned char databuf[1];
        unsigned char ledstat;
        retvalue = copy_from_user(databuf, buf, cnt);
        if (retvalue < 0) {
                return -EFAULT;
        }

        ledstat = databuf[0];
        if (ledstat == LEDON)
                led0_switch(LEDON);
        else if (ledstat == LEDOFF)
                led0_switch(LEDOFF);
        
        return 0;
}

struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = led_open,
        .write = led_write,
};

static int led_probe(struct platform_device *dev)
{

        printk("led driver and device have matched\r\n");

        /* 1. 获取设备信息 这里通过设备树获取 */
        leddev.node = of_find_node_by_path("/gpioled");
        if (leddev.node == NULL) {
                printk("gpioled node not find\r\n");
                return -EINVAL;
        }
        leddev.led0 = of_get_named_gpio(leddev.node, "led-gpio", 0);
        if (leddev.led0 < 0) {
                printk("cannot get led gpio number\r\n");
                return -EINVAL;
        }
        gpio_request(leddev.led0, "led0");

        /* 2. 初始化寄存器 */
        gpio_direction_output(leddev.led0, 1);



        /* 3. 注册字符设备驱动 */
        if (leddev.major) {
                leddev.devid = MKDEV(leddev.major, 0);
                register_chrdev_region(leddev.devid, LEDDEV_CNT, LEDDEV_NAME);
        } else {
                alloc_chrdev_region(&leddev.devid, 0, LEDDEV_CNT, LEDDEV_NAME);
                leddev.major = MAJOR(leddev.devid);
        }

        leddev.cdev.owner = THIS_MODULE;
        cdev_init(&leddev.cdev, &fops);
        cdev_add(&leddev.cdev, leddev.devid, LEDDEV_CNT);

        leddev.class = class_create(THIS_MODULE, LEDDEV_NAME);
        if (IS_ERR(leddev.class))
                return PTR_ERR(leddev.class);
        leddev.device = device_create(leddev.class, NULL, leddev.devid, NULL, LEDDEV_NAME);
        if (IS_ERR(leddev.device))
                return PTR_ERR(leddev.device);
        
        return 0;
}

static int led_remove(struct platform_device *dev)
{
        printk("locate: led_remove in leddriver.ko\r\n");
        // iounmap(IMX6U_CCM_CCGR1);
        gpio_set_value(leddev.led0, 1);
        gpio_free(leddev.led0);
        cdev_del(&leddev.cdev);
        unregister_chrdev_region(leddev.devid, LEDDEV_CNT);
        device_destroy(leddev.class, leddev.devid);
        class_destroy(leddev.class);
        return 0;
}

static const struct of_device_id led_match_table[] = {
        {.compatible = "atkalpha-gpioled"},
        {},
};


static struct platform_driver led_driver = {
        .driver = {
                .name = "led",
                .of_match_table = led_match_table,
        },
        .probe = led_probe,
        .remove = led_remove,
};

static int __init leddriver_init(void)
{
        return platform_driver_register(&led_driver);
}

static void __exit leddriver_exit(void)
{
        platform_driver_unregister(&led_driver);
}

module_init(leddriver_init);
module_exit(leddriver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");