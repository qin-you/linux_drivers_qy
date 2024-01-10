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
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/ide.h>
#include <linux/poll.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/*****************************************************************************
 * 非阻塞方式访问IO设备 （驱动中不阻塞）
******************************************************************************/

#define IRQ_CNT         1
#define IMX6UIRQ_NAME   "noblockio"
#define KEY0VALUE       0x01
#define INVAKEY         0xFF
#define KEY_NUM         1               //按键数量 本开发板只有一个按键

// #define IF_WAIT_EVENT                  // 表示用wait_event的等待方式 注释掉就是用被动休眠的方式
/* 中断IO描述结构体 */
struct irq_keydesc {
        int     gpio;           //gpio号
        int     irqnum;         //中断号
        unsigned char value;    //按键对应键值
        char    name[10];       //按键名字
        irqreturn_t (*handler)(int, void*);     //中断服务函数 
};

/* 设备结构体 */
struct irq_dev {
        dev_t                   devid;
        struct cdev             cdev;
        struct class            *class;
        struct device           *device;
        int                     major;
        int                     minor;
        struct device_node      *node;
        atomic_t                keyvalue;       //有效的按键键值
        atomic_t                releasekey;     //标记是否完成一次按键
        struct timer_list       timer;          //定时器
        struct irq_keydesc      irqkeydesc[KEY_NUM];    //按键描述数组 有的板子有多个按键
        unsigned char           curkeynum;              //当前按键号

        wait_queue_head_t       r_wait;         // 读等待队列头
};

struct irq_dev mydev;

/* ISR：用于启动定时器（上升下降沿都启动） 读取引脚电平在定时器处理函数中执行 */
static irqreturn_t key0_handler(int irq, void *pdev)    
{
        struct irq_dev *dev = (struct irq_dev *)pdev;

        dev->curkeynum = 0;
        dev->timer.data = (volatile long)pdev;
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
        return IRQ_HANDLED;
}

/* 定时器处理函数 */
static void timer_function(unsigned long arg)
{
        unsigned char value;
        unsigned char num;
        struct irq_keydesc *keydesc;
        struct irq_dev *dev = (struct irq_dev *)arg;

        num = dev->curkeynum;
        keydesc = &dev->irqkeydesc[num];

        value = gpio_get_value(keydesc->gpio);  //读引脚电平
        if (value == 0) {                       // 按下
                atomic_set(&dev->keyvalue, keydesc->value);
        } else {                                // 按键松开 一定是通过上升沿中断过来的
                atomic_set(&dev->keyvalue, 0x80 | keydesc->value);              // 0x80作用类似校验码不一定有必要性，releasekey=1总是和value的7号位为1绑定的
                atomic_set(&dev->releasekey, 1);                        //标记松开按键
        }

        /* 唤醒进程*/
        if (atomic_read(&dev->releasekey))
                 wake_up_interruptible(&dev->r_wait);

}

static int keyio_init(void)
{
        unsigned char i = 0;
        int ret = 0;

        /* 从设备树获取硬件信息 节点 gpio号 中断号 */
        mydev.node = of_find_node_by_path("/key");
        if (mydev.node == NULL) {
                printk("get key device node failed\r\n");
                return -EINVAL;
        }
        for (i=0; i < KEY_NUM; i++) {
                mydev.irqkeydesc[i].gpio = of_get_named_gpio(mydev.node, "key-gpio", i);
                if (mydev.irqkeydesc[i].gpio < 0) {
                        printk("cannot get gpio number\r\n");
                        return -EINVAL;
                }
                mydev.irqkeydesc[i].irqnum = irq_of_parse_and_map(mydev.node, i);
#if 0
                mydev.irqkeydesc[i].irqnum = gpio_to_irq(mydev.irqkeydesc[i].gpio);
#endif

        /* 初始化等待队列头 */
        init_waitqueue_head(&mydev.r_wait);
        }

        /* 设置寄存器*/
        for (i = 0; i < KEY_NUM; i++) {
                memset(mydev.irqkeydesc[i].name, 0, sizeof(mydev.irqkeydesc[i].name));
                sprintf(mydev.irqkeydesc[i].name, "KEY%d", i);
                gpio_request(mydev.irqkeydesc[i].gpio, mydev.irqkeydesc[i].name);
                gpio_direction_input(mydev.irqkeydesc[i].gpio);

                printk("key%d:gpio=%d, irqnum=%d\r\n", i, mydev.irqkeydesc[i].gpio, mydev.irqkeydesc[i].irqnum);
        }

        /* 注册中断 */
        mydev.irqkeydesc[0].value = KEY0VALUE;
        mydev.irqkeydesc[0].handler = key0_handler;             //实际只有一个按键，只需要定义一个ISR

        for (i = 0; i < KEY_NUM; i++) {
                ret = request_irq(mydev.irqkeydesc[i].irqnum,
                                mydev.irqkeydesc[i].handler,
                                IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                                mydev.irqkeydesc->name, &mydev);
                if (ret < 0) {
                        printk("irq %d request failed\r\n",mydev.irqkeydesc[i].irqnum);
                        return -EFAULT;
                }
        }

        /* 初始化定时器 */
        init_timer(&mydev.timer);
        mydev.timer.function = timer_function;
        return 0;
}

static int myirq_open(struct inode *inode, struct file *filp)
{
        filp->private_data = &mydev;
        return 0;
}

static ssize_t myirq_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
        int ret = 0;
        unsigned char keyvalue = 0;
        unsigned char releasekey = 0;
        struct irq_dev *dev = (struct irq_dev *)filp->private_data;

        if (filp->f_flags & O_NONBLOCK) {
                if (atomic_read(&dev->releasekey) == 0)
                        return -EAGAIN;
        } else {
                return -EIO;    // 简化逻辑
        }
        
        keyvalue = atomic_read(&dev->keyvalue);
        releasekey = atomic_read(&dev->releasekey);

        if (releasekey) {               //按键已弹起
                if (keyvalue & 0x80) {                  // 按键已弹起
                        keyvalue &= ~0x80;
                        ret = copy_to_user(buf, &keyvalue, sizeof(keyvalue));
                } else {                                // 数据错误 
                        goto data_error;
                }

                atomic_set(&dev->releasekey, 0);
        } else {                                        // 还按着
                goto still_pressed;
        }

        return 0;

data_error:
        return -EINVAL;
still_pressed:
        return -EFAULT;
}


unsigned int myirq_poll(struct file *filp, struct poll_table_struct *wait)
{
        unsigned int mask = 0;
        struct irq_dev *dev = (struct irq_dev *)filp->private_data;

        poll_wait(filp, &dev->r_wait, wait);

        if (atomic_read(&dev->releasekey)) {
                mask = POLLIN | POLLRDNORM;
        }
        return mask;
}

static struct file_operations myirq_fops = {
        .owner = THIS_MODULE,
        .open = myirq_open,
        .read = myirq_read,
        .poll = myirq_poll,
};

static int __init myirq_init(void)
{
        int ret = 0;
        
        /* 注册设备驱动 */
        if (mydev.major) {
                mydev.devid = MKDEV(mydev.major, mydev.minor);
                register_chrdev_region(mydev.devid, IRQ_CNT, IMX6UIRQ_NAME);
        } else {
                alloc_chrdev_region(&mydev.devid, 0, IRQ_CNT, IMX6UIRQ_NAME);
                mydev.major = MAJOR(mydev.devid);
                mydev.minor = MINOR(mydev.devid);
        }
        mydev.cdev.owner = THIS_MODULE;
        cdev_init(&mydev.cdev, &myirq_fops);
        cdev_add(&mydev.cdev, mydev.devid, IRQ_CNT);

        /* 创建设备节点 */
        mydev.class = class_create(THIS_MODULE, IMX6UIRQ_NAME);
        if (IS_ERR(mydev.class))
                return PTR_ERR(mydev.class);
        mydev.device = device_create(mydev.class, NULL, mydev.devid, NULL, IMX6UIRQ_NAME);
        if (IS_ERR(mydev.device))
                return PTR_ERR(mydev.device);
        
        /* 初始化按键 中断 定时器*/
        ret = keyio_init();
        if (ret < 0) {
                printk("init failed\r\n");
                return ret;
        }
        atomic_set(&mydev.keyvalue, INVAKEY);
        atomic_set(&mydev.releasekey, 0);
        return 0;
}

static void __exit myirq_exit(void)
{
        unsigned char i = 0;

        /*释放gpio*/
        for (i = 0; i < KEY_NUM; i++) {
                gpio_free(mydev.irqkeydesc[i].gpio);
        }

        /*注销设备驱动*/
        cdev_del(&mydev.cdev);
        unregister_chrdev_region(mydev.devid, IRQ_CNT);

        /*注销设备节点*/
        device_destroy(mydev.class, mydev.devid);
        class_destroy(mydev.class);

        /*删除定时器 */
        del_timer_sync(&mydev.timer);

        /*释放中断*/
        for (i = 0; i < KEY_NUM; i++) {
                free_irq(mydev.irqkeydesc[i].irqnum, &mydev);
        }
}

module_init(myirq_init);
module_exit(myirq_exit);

MODULE_AUTHOR("You Qin");
MODULE_LICENSE("GPL");
