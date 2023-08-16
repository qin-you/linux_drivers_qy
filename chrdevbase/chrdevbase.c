/*
 * @Author: You Qin
 * @Date: 2023-03-18 09:40:37
 * @LastEditTime: 2023-03-20 14:30:31
 * @FilePath: /Linux_Drivers/chrdevbase/chrdevbase.c
 * @Description: 
 */

#include <linux/module.h>
#include "linux/kernel.h"
#include "linux/init.h"
#include "linux/fs.h"
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <cdev.h>
#include <device.h>


#define CHRDEVBASE_NAME "chrdevbase"    // 设备名

static char readbuf[100];
static char writebuf[100];
static char kerneldata[] = "kernel data";


/**
 * @description: 打开设备
 * @param {inode} *inode 传递给驱动的inode
 * @param {file} *file 设备文件，file结构体有个叫做private_data的成员变量，一般在open的时候将private_data指向设备结构体
 * @return {*} 0 成功;其他 失败
 */
static int chrdevbase_open(struct inode *inode, struct file *file)
{
    dump_stack();
    printk(KERN_INFO "chrdevbase_open\r\n");
    return 0;
}

/**
 * @description: 从设备读取数据 注意在驱动里说读写的主体是用户程序，而不是内核，即是用户在读和写。驱动的动作是用户下达的
 * @param {file} *filp
 * @param {char __user} *buf
 * @param {size_t} count
 * @param {loff_t} *fpos
 * @return {*} 0 成功;其他 失败
 */
static ssize_t chrdevbase_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)
{
    int ret = 0;
    dump_stack();
    memcpy(readbuf, kerneldata, sizeof(kerneldata));
    ret = copy_to_user(buf, readbuf, count);
    if (ret != 0) {
        printk(KERN_INFO "chrdevbase read failed\r\n");
        return -1;
    }
    printk(KERN_INFO "%s has been read\n", readbuf);
    printk(KERN_INFO "chrdevbase read success\r\n");
    return 0;
}


/**
 * @description: 
 * @param {file} *filp
 * @param {char __user} *buf
 * @param {size_t} count
 * @param {loff_t} *fpos
 * @return {*}
 */
static ssize_t chrdevbase_write(struct file *filp, const char __user *buf, size_t count, loff_t *fpos)
{
    int ret = 0;
    dump_stack();
    ret = copy_from_user(writebuf, buf, count);
    if (ret != 0) {
        printk(KERN_INFO "chrdevbase write failed\r\n");
        return -1;
    }

    printk(KERN_INFO "chrdevbase write success\r\n");
    return 0;
}


/**
 * @description: 关闭/释放设备
 * @param {inode} *inode 
 * @param {file} *filp_close 要关闭的设备文件(文件描述符)
 * @return {*} 0 成功;其他 失败
 */
static int chrdevbase_release(struct inode *inode, struct file *filp_close)
{
    dump_stack();
    printk(KERN_INFO "chrdevbase_release");
    return 0;
}

/*
设备操作函数结构体
*/
static struct file_operations fops = {
    .owner = THIS_MODULE,        // THIS_MODULE是一个宏，指向当前模块的struct module结构体，每个模块在内核看来都是一个struct module结构体
    .open = chrdevbase_open,
    .read = chrdevbase_read,
    .write = chrdevbase_write,
    .release = chrdevbase_release
};


struct class *class;
struct device *device;
dev_t devid;
struct cdev cdev;

/**
 * @description: 模块加载函数
 * @pipeline: 1.申请设备号(注册字符设备驱动)
 * @return 0 成功;其他 失败
 */
static int __init chrdev_init(void)
{
    dump_stack();

    alloc_chrdev_region(&devid, 0, 1, CHRDEVBASE_NAME);
    
    cdev.owner = THIS_MODULE;
    cdev_init(&cdev, &fops);
    cdev_add(&cdev, devid, 1);

    class = class_create(THIS_MODULE, CHRDEVBASE_NAME);
    
    device = device_create(class, NULL, devid, NULL, CHRDEVBASE_NAME);

    printk(KERN_INFO "chrdev_init\r\n");
    return 0;
}


/**
 * @description: 模块卸载/出口函数
 * @pipeline: 1.注销设备号(注销字符设备驱动)
 * @return 没有返回值
 */
static void __exit chrdev_exit(void)
{
    unregister_chrdev_region(devid, 1);
    printk(KERN_INFO "chrdev_exit\r\n");
}

// 模块入口 出口
module_init(chrdev_init);
module_exit(chrdev_exit);

// 模块许可证
MODULE_AUTHOR("<insert your name here>");
MODULE_DESCRIPTION("LLKD book:ch4/helloworld_lkm: hello, world, our first LKM");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("0.1");
