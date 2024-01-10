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

/*******************************************************************************
 * 原子整数（atomic_t）实现对led设备的进程互斥访问：用原子整数起信号量作用。
********************************************************************************/

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
        int major;                      /* 定义该结构体全局变量以后 major会赋0*/
        int minor;
        struct device_node *pnode;
        int gpio_num;                   /*led使用的GPIO编号*/
        atomic_t lock;                  /*原子变量*/
};

static struct gpioled_dev gpioled;      /*静态全局变量 里面的成员变量都会赋0初值*/

static int led_open(struct inode *inode, struct file *filp)
{
        if (! atomic_dec_and_test(&gpioled.lock)) {
                atomic_inc(&gpioled.lock);
                printk("too more processes use the device\r\n");
                return -EBUSY;
        }
        filp->private_data = &gpioled;
        return 0;
}

/*应用层没有读led设备的操作，因此驱动这里可以不写任何逻辑*/
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
        return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
        int ret = 0;
        u8 val = 0;
        char kernel_buf[1];
        ret = copy_from_user(kernel_buf, buf, cnt);
        if (ret < 0) {
                printk("kernel write failed\r\n");
                return -EFAULT;
        }
        val = kernel_buf[0];
        if (val == LEDON)
                gpio_set_value(gpioled.gpio_num, 0);
        else if (val == LEDOFF)
                gpio_set_value(gpioled.gpio_num, 1);
        
        return 0;
}

/* close设备，原子变量增加1*/
static int led_release(struct inode *inode, struct file *filp)
{
        atomic_inc(&gpioled.lock);
        return 0;
}

struct file_operations gpioled_fops = {
        .owner = THIS_MODULE,
        .open = led_open,
        .read = led_read,
        .write = led_write,
        .release = led_release,
};

static int __init led_init(void)
{
        int ret = 0;
        /* 0. 初始化原子变量*/
        atomic_set(&gpioled.lock, 1);

        /* 1. 从设备树获取信息 */
        /* 1.1 获取节点 */
        gpioled.pnode = of_find_node_by_path("/gpioled");
        if (gpioled.pnode == NULL) {
                printk("cannot find device tree node\r\n");
                return -EINVAL;
        } else {
                printk("device tree node has been found\r\n");
        }
        /* 1.2 获取gpio编号*/
        gpioled.gpio_num = of_get_named_gpio(gpioled.pnode, "led-gpio", 0);
        if (gpioled.gpio_num < 0) {
                printk("get gpio number failed\r\n");
                return -EINVAL;
        } else {
                printk("gpio number is %d\r\n", gpioled.gpio_num);
        }

        ret = gpio_request(gpioled.gpio_num, "gpioled");
        if (ret != 0) {
                printk("gpio request error\r\n");
                return -EINVAL;
        }

        /* 2. 设置寄存器 与初始化原子变量 */
        /* 2.1 设置GDIR DR默认值 */
        gpio_direction_output(gpioled.gpio_num, 1);

        /* 3. 注册设备驱动 */
        /* 3.1 (申请)注册设备号 */
        printk("3\r\n");
        if (gpioled.major) {
                gpioled.devid = MKDEV(gpioled.major, gpioled.minor);
                ret = register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
        } else {
                ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
                gpioled.major = MAJOR(gpioled.devid);
                gpioled.minor = MINOR(gpioled.devid);   
        }
        /* 3.2 注册设备驱动 */
        gpioled.cdev.owner = THIS_MODULE;
        cdev_init(&gpioled.cdev, &gpioled_fops);
        cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
        
        /* 4. 创建设备节点 */
        /* 4.1 创建class*/
        printk("4\r\n");
        gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
        if (IS_ERR(gpioled.class))
                return PTR_ERR(gpioled.class);
        /* 4.2 创建设备节点 */
        printk("4.2\r\n");
        gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
        if (IS_ERR(gpioled.device))
                return PTR_ERR(gpioled.device);
        printk("init end \r\n");

        return 0;
}

static void __exit led_exit(void)
{

        /* 0. free gpio*/
        gpio_free(gpioled.gpio_num);
        /*1. 注销设备驱动*/
        cdev_del(&gpioled.cdev);
        unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);

        /*2. 注销设备节点*/
        device_destroy(gpioled.class, gpioled.devid);
        class_destroy(gpioled.class);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");