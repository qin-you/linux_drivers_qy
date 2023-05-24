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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/*********************************
 * 利用设备树提供硬件信息的led灯驱动
*********************************/

#define DTSLED_CNT      1               /*设备号个数*/
#define DTSLED_NAME     "dtsled"        /*名字*/
#define LEDOFF          0               /*关灯*/
#define LEDON           1               /*开灯*/

/*声明 映射后的寄存器虚拟地址指针 记住这5个寄存器*/
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/*自定义 dtsled设备结构体*/
struct dtsled_dev {
        dev_t devid;            /*设备号*/
        struct cdev cdev;       /*cdev*/
        struct class *class;    /*类 创建文件系统中的设备节点需要*/
        struct device *device;  /*设备*/
        int major;              /*主设备号*/
        int minor;              /*次设备号*/
        struct device_node *node; /*设备节点*/
};

struct dtsled_dev dtsled;       /*创建一个自定义的dtsled设备*/

static void led_switch(u8 sta)
{
        u32 val = 0;
        if (sta == LEDON) {
                val = readl(GPIO1_DR);
                val &= ~(1 << 3);
                writel(val, GPIO1_DR);
        } else if (sta == LEDOFF) {
                val = readl(GPIO1_DR);
                val |= (1 << 3);
                writel(val, GPIO1_DR);
        }
}

static int led_open(struct inode *inode, struct file *filp)
{
        filp->private_data = &dtsled;   /*设置私有数据*/
        return 0;
}


/**
 * filp:内核创建的关于已打开文件的file结构体指针 file在内核空间
 * buf：返回给用户空间的数据缓冲区
 * cnt：要读取的数据长度
 * offt：相对于文件首地址的偏移
 * 读取失败返回负数，否则返回读到的字节数
 * 注：读写的主语是应用程序
*/
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
        return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
        int retvalue;
        unsigned char databuf[1];
        unsigned char ledstat;

        retvalue = copy_from_user(databuf, buf, cnt);
        if (retvalue < 0) {
                printk(KERN_INFO "kernel write failed\r\n");
                return -EIO;
        }

        ledstat = databuf[0];

        if (ledstat == LEDON)
                led_switch(LEDON);
        else if (ledstat == LEDOFF)
                led_switch(LEDOFF);

        return 0;
}

/**
 * 注：release对应应用层close设备文件，这里不需要释放设备号，在驱动exit完成。
*/
static int led_release(struct inode *inode, struct file *filp)
{
        return 0;
}

/*设备操作结构体*/
static struct file_operations dtsled_fops = {
        .owner = THIS_MODULE,
        .open = led_open,
        .read = led_read,
        .write = led_write,
        .release = led_release,
};

/**
 * 驱动入口函数
*/
static int __init led_init(void)
{
        u32 val = 0;
        int ret;
        u32 regdata[14];
        const char *str;
        struct property *proper;

        /* 1. 获取设备树的属性数据 */
        /* 1.1 获取设备节点：alphaled */
        dtsled.node = of_find_node_by_path("/alphaled");
        if (dtsled.node == NULL) {
                printk("alphaled node cannot be found\r\n");
                return -EINVAL;
        } else {
                printk("alphaled node has been found\r\n");
        }

        /* 1.2 获取compatible属性值 */
        proper = of_find_property(dtsled.node, "compatible", NULL);
        if (proper == NULL)
                printk("compatible property cannot found\r\n");
        else
                printk("compatible = %s\r\n", (char*)proper->value);
        
        /* 1.3 获取status属性内容 */
        ret = of_property_read_string(dtsled.node, "status", &str);
        if (ret < 0)
                printk("status read failed\r\n");
        else
                printk("status = %s\r\n", str);
        
        /* 1.4 获取reg属性内容 */
        ret = of_property_read_u32_array(dtsled.node, "reg", regdata, 10);
        if (ret < 0) {
                printk("reg read failed\r\n");
        } else {
                u8 i = 0;
                printk("reg data: \r\n");
                for (i = 0; i < 10; i++)
                        printk("%#X ", regdata[i]);
                printk("\r\n");
        }

        /* 2 配置时钟和引脚 */
        /* 2.1 寄存器地址映射到虚拟空间 */
#if 0
        IMX6U_CCM_CCGR1 = ioremap(regdata[0], regdata[1]);
        SW_MUX_GPIO1_IO03 = ioremap(regdata[2], regdata[3]);
        SW_PAD_GPIO1_IO03 = ioremap(regdata[4], regdata[5]);
        GPIO1_DR = ioremap(regdata[6], regdata[7]);
        GPIO1_GDIR = ioremap(regdata[8], regdata[9]);
#else
        IMX6U_CCM_CCGR1 = of_iomap(dtsled.node, 0);
        SW_MUX_GPIO1_IO03 = of_iomap(dtsled.node, 1);
        SW_PAD_GPIO1_IO03 = of_iomap(dtsled.node, 2);
        GPIO1_DR = of_iomap(dtsled.node, 3);
        GPIO1_GDIR = of_iomap(dtsled.node, 4);
#endif

        /* 2.2 使能gpio时钟 */
        val = readl(IMX6U_CCM_CCGR1);
        val &= ~(3 << 26);
        val |= (3 << 26);
        writel(val, IMX6U_CCM_CCGR1);

        /* 2.3 设置复用与电气属性*/
        writel(5, SW_MUX_GPIO1_IO03);
        writel(0x10B0, SW_PAD_GPIO1_IO03);
        
        /* 2.4 设置GPIO1_IO03引脚为输出方向 */
        val = readl(GPIO1_GDIR);
        val &= ~(1 << 3);
        val |= (1 << 3);
        writel(val, GPIO1_GDIR);

        /* 2.5 设置GPIO1_DR 默认关闭led */
        val = readl(GPIO1_DR);
        val |= (1 << 3);
        writel(val, GPIO1_DR);

        /* 3. 注册字符设备驱动 */
        /* 3.1 创建设备号 */
        if (dtsled.major) {
                dtsled.devid = MKDEV(dtsled.major, 0);
                register_chrdev_region(dtsled.devid, DTSLED_CNT, DTSLED_NAME);
        } else {
                alloc_chrdev_region(&dtsled.devid, 0, DTSLED_CNT, DTSLED_NAME);
                dtsled.major = MAJOR(dtsled.devid);
                dtsled.minor = MINOR(dtsled.devid);
        }
        printk("dtsled major=%d,minor=%d\r\n", dtsled.major, dtsled.minor);

        /* 3.2 初始化cdev */
        dtsled.cdev.owner = THIS_MODULE;
        cdev_init(&dtsled.cdev, &dtsled_fops);

        /* 3.3 添加cdev到内核（注册设备驱动） */
        cdev_add(&dtsled.cdev, dtsled.devid, DTSLED_CNT);

        /* 4. 创建设备节点 */
        /* 4.1 创建类 */
        dtsled.class = class_create(THIS_MODULE, DTSLED_NAME);
        if (IS_ERR(dtsled.class))
                return PTR_ERR(dtsled.class);
        
        /*4.2 创建设备 */
        dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, DTSLED_NAME);
        if (IS_ERR(dtsled.device))
                return PTR_ERR(dtsled.device);
        return 0;
}

static void __exit led_exit(void)
{
        /* 1. 取消映射 */
        iounmap(IMX6U_CCM_CCGR1);
        iounmap(SW_MUX_GPIO1_IO03);
        iounmap(SW_PAD_GPIO1_IO03);
        iounmap(GPIO1_DR);
        iounmap(GPIO1_GDIR);

        /* 2. 注销字符设备驱动 */
        cdev_del(&dtsled.cdev);
        unregister_chrdev_region(dtsled.devid, DTSLED_CNT);
        device_destroy(dtsled.class, dtsled.devid);
        class_destroy(dtsled.class);
}

module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("You qin");

