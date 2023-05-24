/*
 * @Author: You Qin
 * @Date: 2023-03-20 09:25:14
 * @LastEditTime: 2023-03-21 11:23:47
 * @FilePath: /Linux_Drivers/newcharled/led.c
 * @Description: arm开发板led灯驱动
 * ctrl+h+e增加header，ctrl+h+f增加光标所在function的注释
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define NEWCHRLED_CNT 1                 // 设备号个数
#define NEWCHRLED_NAME "newchrled"

#define LEDOFF 0
#define LEDON 1

// 寄存器物理地址
#define CCM_CCGR1_BASE (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE (0X020E02F4)
#define GPIO1_DR_BASE (0X0209C000)
#define GPIO1_GDIR_BASE (0X0209C004)

// 声明变量用来保存寄存器虚拟地址
static void __iomem * IMX6U_CCM_CCGR1;
static void __iomem * SW_MUX_GPIO1_IO03;
static void __iomem * SW_PAD_GPIO1_IO03;
static void __iomem * GPIO1_DR;
static void __iomem * GPIO1_GDIR;

// 自定义结构体用来表示led设备，包含设备号、cdev、类、设备等信息
struct newchrled_dev {
    dev_t devid;                    // 设备号
    struct cdev cdev;               // cdev
    struct class *class;             // 类
    struct device *device;          // 设备
    int major;                      // 主设备号
    int minor;                      // 次设备号
};

struct newchrled_dev newchrled;     // 定义一个led的设备

void led_switch(u8 sta)
{
    u32 val = 0;
    val = readl(GPIO1_DR);
    if (sta == LEDON) {
        val &= ~(1 << 3);
        writel(val, GPIO1_DR);
    } else if(sta == LEDOFF) {
        val |= (1 << 3);
        writel(val, GPIO1_DR);
    }
}

static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &newchrled;        // 设置设备文件的私有数据
    return 0;
}

static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int ret;
    unsigned char databuf[1];
    unsigned char ledstat;

    ret = copy_from_user(databuf, buf, cnt);        //从用户空间拷贝数据到内核空间 这里buf不是值传递，还是用户空间地址 注意这一步！
    if (ret < 0) {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledstat = databuf[0];                           //获取状态值

    if (ledstat == LEDON) {
        led_switch(LEDON);
    } else if (ledstat == LEDOFF) {
        led_switch(LEDOFF);
    }

    return 0;
}

static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    // .release = led_release
};

static int __init led_init(void)
{
    u32 val = 0;

    // 1.初始化寄存器地址映射
    IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);       // 手动映射物理地址到虚拟地址
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);

    // 2.设置寄存器 （时钟 复用 电气 gpio）
    // 2.1 时钟使能
    val = readl(IMX6U_CCM_CCGR1);                // 需要时才使能时钟，省电（降低功耗）
    val &= ~(3 << 26);                     // 清楚以前的设置
    val |= (3 << 26);               // 设置新值
    writel(val, IMX6U_CCM_CCGR1);

    // 2.2 配置GPIO1_io03复用功能，将其复用为GPIO1_io03，最后设置IO属性
    writel(5, SW_MUX_GPIO1_IO03);

    // 2.3 寄存器SW_PAD_GPIO1_IO03设置IO电气属性
    writel(0x10B0, SW_PAD_GPIO1_IO03);

    // 2.4 设置GPIO1_IO03为输出功能 GDIR
    val = readl(GPIO1_GDIR);
    val &= ~(1 << 3);
    val |= (1 << 3);
    writel(val, GPIO1_GDIR);

    // 2.5. 默认关闭led DR
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);

    // 3.注册字符设备驱动
    // 3.1获取设备号
    if (newchrled.major) {
        newchrled.devid = MKDEV(newchrled.major, 0);
        register_chrdev_region(newchrled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
    } else {
        alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME);
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }
    // 3.2 填充cdev结构体
    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev, &led_fops);
    // 3.4 使用cdev注册设备驱动（cdev加到内核）
    cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_CNT);

    // 4.创建设备节点 /dev/xxx
    newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if (IS_ERR(newchrled.class)) {
        return PTR_ERR(newchrled.class);
    }

    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, NEWCHRLED_NAME);
    if (IS_ERR(newchrled.device)) {
        return PTR_ERR(newchrled.device);
    }

    return 0;
}


static void __exit led_exit(void)
{
    // 0. 关led
    int val = 0;
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);

    // 1. 取消虚拟地址映射
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    //2. 注销字符设备
    cdev_del(&newchrled.cdev);
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);
    
    //3. 删除设备节点
    device_destroy(newchrled.class, newchrled.devid);
    class_destroy(newchrled.class);
}

// 注册驱动出入口函数
module_init(led_init);
module_exit(led_exit);

// 设置证书和作者
MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");