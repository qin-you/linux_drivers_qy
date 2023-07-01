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
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/i2c.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include "ap3216creg.h"

/**********************************************************************
 * IIC驱动ap3216c芯片 一个传感器 不使用misc
 * 主线是初始化和注册i2c_driver结构体，注册在入口函数完成。难点在做组件而不是搭组件 
***********************************************************************/

#define AP3216C_CNT      1
#define AP3216C_NAME     "ap3216c"

struct ap3216c_dev {
        dev_t devid;
        struct cdev cdev;
        struct class *class;
        struct device *device;
        int major;
        struct device_node *nd;

        void *private_data;     // 自定义设备结构体的私有数据 放client地址 client从probe里获取
        unsigned short ir, als, ps;     // 三个光传感数据
};

struct ap3216c_dev ap3216cdev;

/*params: reg寄存器地址 val传出参数 len要读取的连续寄存器个数 一个寄存器地址是一个字节*/
static int _ap3216c_read_regs(struct ap3216c_dev *dev, u8 reg, u8 *buf, u8 len)
{
        int ret;
        struct i2c_msg msg[2];
        struct i2c_client *client = (struct i2c_client *)dev->private_data;

        msg[0].addr = client->addr;     // ap3216c地址
        msg[0].flags = 0;               // 标记写 内核视角
        msg[0].buf = &reg;
        msg[0].len = 1;

        msg[1].addr = client->addr;     // ap3216c地址
        msg[1].flags = I2C_M_RD;               // 标记读
        msg[1].buf = buf;
        msg[1].len = len;
        ret = i2c_transfer(client->adapter, msg, 2);
        if (ret == 2) {
                ret = 0;
        } else {
                printk("i2c rd failed=%d reg=%06x len=%d\r\n", ret, reg, len);
                ret = -EREMOTEIO;
        }
        return ret;
}

static int ap3216c_open(struct inode *inode, struct file *filp)
{
        filp->private_data = &ap3216cdev;
        return 0;
}

static ssize_t ap3216c_read(struct file *filp, char __user *buf, size_t cnt, loff_t *loff)      // buf类型固定就是char 字节 cnt是字节数
{
        int ret = 0;
        unsigned short res[3];
        struct ap3216c_dev *dev = filp->private_data;
        unsigned char data[6] = {0};

        /* 获取寄存器原始读数*/
        ret = _ap3216c_read_regs(dev, AP3216C_IRDATALOW, data, 6);

        /* 将高低位寄存器有效数据拼接 参照ap3216c手册*/
        if (data[0] & (1<<7))   // IR_OF位为1 数据无效
                dev->ir = 0;    
        else 
                dev->ir = ((unsigned short)data[1] << 2) | (data[0] & 0x03);

        dev->als = data[2] | ((unsigned short)data[3] << 8);

        if (data[4]&(1<<6))
                dev->ps = 0;
        else
                dev->ps = (((unsigned short)data[5] & 0b00111111) << 4) | (data[4] & 0x0f);

        /* 得到最终的三个传感器的值 */
        res[0] = dev->ir;
        res[1] = dev->als;
        res[2] = dev->ps;

        /* 拷贝到用户空间 */
        ret = copy_to_user(buf, res, sizeof(res));
        return 0;
}

struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = ap3216c_open,
        .read = ap3216c_read,
};

static int _ap3216c_write_regs(struct ap3216c_dev *dev, unsigned char reg, u8 *buf, u8 len)
{
        struct i2c_msg msg;
        struct i2c_client *client = dev->private_data;
        u8 msg_buf[256];                                        //容器 把寄存器地址拼在发送数据之前 i2c写的规则
        // 拼接
        msg_buf[0] = reg;
        memcpy(&msg_buf[1], buf, len);
        msg.addr = client->addr;
        msg.flags = 0;
        msg.buf = msg_buf;
        msg.len = len + 1;

        return i2c_transfer(client->adapter, &msg, 1);
}

static int _ap3216c_write_reg(struct ap3216c_dev *dev, unsigned char reg, u8 val)
{
        return _ap3216c_write_regs(dev, reg, &val, 1);
}

static int ap3216c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        printk("ap3216cdev driver and device have matched\r\n");
 
        /* client的值应该是从iic核心或总线驱动传下来的 这句话要放在前面 初始化ap3216c需要client的值 */
        ap3216cdev.private_data = client;               

        /* 初始化ap3216c */
        _ap3216c_write_reg(&ap3216cdev, AP3216C_SYSTEMCONG, 0x04);       // 复位
        mdelay(50);     /* 该芯片复位最少10 ms */
        _ap3216c_write_reg(&ap3216cdev, AP3216C_SYSTEMCONG, 0x03);       // 开启

        /* 注册字符设备驱动 */
        if (ap3216cdev.major) {
                ap3216cdev.devid = MKDEV(ap3216cdev.major, 0);
                register_chrdev_region(ap3216cdev.devid, AP3216C_CNT, AP3216C_NAME);
        } else {
                alloc_chrdev_region(&ap3216cdev.devid, 0, AP3216C_CNT, AP3216C_NAME);
                ap3216cdev.major = MAJOR(ap3216cdev.devid);
        }

        ap3216cdev.cdev.owner = THIS_MODULE;
        cdev_init(&ap3216cdev.cdev, &fops);
        cdev_add(&ap3216cdev.cdev, ap3216cdev.devid, AP3216C_CNT);

        ap3216cdev.class = class_create(THIS_MODULE, AP3216C_NAME);
        if (IS_ERR(ap3216cdev.class))
                return PTR_ERR(ap3216cdev.class);
        ap3216cdev.device = device_create(ap3216cdev.class, NULL, ap3216cdev.devid, NULL, AP3216C_NAME);
        if (IS_ERR(ap3216cdev.device))
                return PTR_ERR(ap3216cdev.device);
        
        return 0;
}

static int ap3216c_remove(struct i2c_client *client)
{
        cdev_del(&ap3216cdev.cdev);
        unregister_chrdev_region(ap3216cdev.devid, AP3216C_CNT);
        device_destroy(ap3216cdev.class, ap3216cdev.devid);
        class_destroy(ap3216cdev.class);
        return 0;
}

static const struct i2c_device_id ap3216c_id[] = {
        {"alientek,ap3216c", 0},
        {},
};
// MODULE_DEVICE_TABLE(of, ap3216c_id);

static const struct of_device_id ap3216c_match_table[] = {
        {.compatible = "alientek,ap3216c"},
        {},
};
// MODULE_DEVICE_TABLE(of, ap3216c_match_table);

static struct i2c_driver ap3216c_driver = {
        .driver = {
                .owner = THIS_MODULE,
                .name = "ap3216c",
                .of_match_table = ap3216c_match_table,
        },
        .probe = ap3216c_probe,
        .remove = ap3216c_remove,
        .id_table = ap3216c_id,
};


static int __init ap3216cdriver_init(void)
{
        return i2c_add_driver(&ap3216c_driver);
}

static void __exit ap3216cdriver_exit(void)
{
        i2c_del_driver(&ap3216c_driver);
}

module_init(ap3216cdriver_init);
module_exit(ap3216cdriver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");