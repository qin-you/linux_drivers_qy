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

/*******************
 * 按键驱动 
********************/

#define KEY_CNT         1
#define KEY_NAME        "key"

// 按下并弹起就返回0xF0，没按下返回0x00
#define KEY0VALUE       0xF0
#define INVAKEY         0x00

struct key_dev {
        dev_t           devid;
        struct cdev     cdev;
        struct class    *class;
        struct device   *device;
        int             major;
        int             minor;
        struct device_node      *nd;
        int             gpio_num;
        atomic_t        keyvalue;
};

static struct key_dev   keydev;

static int keyio_init(void)
{
        int ret = 0;
        keydev.nd = of_find_node_by_path("/key");
        if (keydev.nd == NULL) {
                printk("cannot find device node\r\n");
                return -EINVAL;
        }
        keydev.gpio_num = of_get_named_gpio(keydev.nd, "key-gpio", 0);
        if (keydev.gpio_num < 0) {
                printk("get gpio number wrong\r\n");
                return -EINVAL;
        }
        printk("key0's gpio number is %d\r\n", keydev.gpio_num);

        ret = gpio_request(keydev.gpio_num, "key0");
        if (ret != 0) {
                printk("gpio_request error\r\n");
                return -EINVAL;
        }
        
        return 0;
}

static int key_open(struct inode *inode, struct file *filp)
{
        filp->private_data = &keydev;
        return 0;
}

static ssize_t key_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
        int ret = 0;
        unsigned char value;

        if (gpio_get_value(keydev.gpio_num) == 0) {
                while (gpio_get_value(keydev.gpio_num) == 0);
                // 这里原子用了全局变量存按键值，所以需要原子变量，如果这里用局部变量保存则不需要原子操作，因为内核栈不共享
                atomic_set(&keydev.keyvalue, KEY0VALUE);
        } else {
                atomic_set(&keydev.keyvalue, INVAKEY);
        }

        value = atomic_read(&keydev.keyvalue);
        ret = copy_to_user(buf, &value, sizeof(value));

        return ret;
}

static struct file_operations key_fops = {
        .owner = THIS_MODULE,
        .open = key_open,
        .read = key_read,
};

static int __init mykey_init(void)
{
        int ret = 0;
        /* 1. 获取设备树信息 节点、gpio号*/
        ret = keyio_init();
        if (ret < 0) {
                printk("get device tree information failed\r\n");
                return ret;
        }
        /* 2. 初始化寄存器 (可放到1.里作为完整的key初始化)*/
        gpio_direction_input(keydev.gpio_num);

        /* 3. 注册设备驱动 */
        if (keydev.major) {
                keydev.devid = MKDEV(keydev.major, keydev.minor);
                register_chrdev_region(keydev.devid, KEY_CNT, KEY_NAME);
        } else {
                alloc_chrdev_region(&keydev.devid, 0, KEY_CNT, KEY_NAME);
                keydev.major = MAJOR(keydev.devid);
                keydev.minor = MINOR(keydev.devid);
        }

        keydev.cdev.owner = THIS_MODULE;
        cdev_init(&keydev.cdev, &key_fops);
        cdev_add(&keydev.cdev, keydev.devid, KEY_CNT);

        /* 4. 注册设备节点 */
        keydev.class = class_create(THIS_MODULE, KEY_NAME);
        if (IS_ERR(keydev.class))
                return PTR_ERR(keydev.class);
        keydev.device = device_create(keydev.class, NULL, keydev.devid, NULL, KEY_NAME);
        if (IS_ERR(keydev.device))
                return PTR_ERR(keydev.device);

        return 0;
}

static void __exit mykey_exit(void)
{
        /* 1. 释放gpio */
        gpio_free(keydev.gpio_num);
        
        /* 2. 注销设备驱动*/
        cdev_del(&keydev.cdev);
        unregister_chrdev_region(keydev.devid, KEY_CNT);

        /* 3. 注销设备节点*/
        device_destroy(keydev.class, keydev.devid);
        class_destroy(keydev.class);
}

module_init(mykey_init);
module_exit(mykey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");