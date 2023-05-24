/*
 * @Author: You Qin
 * @Date: 2023-03-20 09:25:14
 * @LastEditTime: 2023-03-21 08:58:34
 * @FilePath: /Linux_Drivers/led/led.c
 * @Description: arm开发板led灯驱动
 * ctrl+h+e增加header，ctrl+h+f增加光标所在function的注释
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define LED_MAJOR 200
#define LED_NAME "led"

#define LEDOFF 0
#define LEDON 1

#define CCM_CCGR1_BASE (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE (0X020E02F4)
#define GPIO1_DR_BASE (0X0209C000)
#define GPIO1_GDIR_BASE (0X0209C004)

static void __iomem * IMX6U_CCM_CCGR1;
static void __iomem * SW_MUX_GPIO1_IO03;
static void __iomem * SW_PAD_GPIO1_IO03;
static void __iomem * GPIO1_DR;
static void __iomem * GPIO1_GDIR;

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
    int ret = 0;
    u32 val = 0;

    // 初始化寄存器地址映射
    IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);       // 手动映射物理地址到虚拟地址
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);

    // 使能gpio1时钟
    val = readl(IMX6U_CCM_CCGR1);                // 需要时才使能时钟，省电（降低功耗）
    val &= ~(3 << 26);                     // 清楚以前的设置
    val |= (3 << 26);               // 设置新值
    writel(val, IMX6U_CCM_CCGR1);

    // 配置GPIO1_io03复用功能，将其复用为GPIO1_io03，最后设置IO属性
    writel(5, SW_MUX_GPIO1_IO03);

    // 寄存器SW_PAD_GPIO1_IO03设置IO属性
    writel(0x10B0, SW_PAD_GPIO1_IO03);

    // 设置GPIO1_IO03为输出功能
    val = readl(GPIO1_GDIR);
    val &= ~(1 << 3);
    val |= (1 << 3);
    writel(val, GPIO1_GDIR);

    // 默认关闭led
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);

    // 注册字符设备驱动
    ret = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    if (ret < 0) {
        printk("led driver register failed!\r\n");
        return -EIO;
    }

    return 0;
}

static void __exit led_exit(void)
{
    int val = 0;
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);

    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    unregister_chrdev(LED_MAJOR, LED_NAME);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");