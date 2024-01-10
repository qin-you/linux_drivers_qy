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
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "icm20608reg.h"

/************************************************
 * spi驱动六轴传感器icm20608 分两条主线：
 * 总线通信设施搭建（初始化注册spi_driver结构体） 和
 * 常规字符设备驱动编写（读写 申请设备号 注册驱动）
**************************************************/

#define ICM20608_CNT	1
#define ICM20608_NAME	"icm20608"

struct icm20608_dev {
	dev_t devid;				
	struct cdev cdev;			
	struct class *class;		
	struct device *device;		
	struct device_node	*nd;
	int major;
	void *private_data;			/* 私有数据 放spi_device的指针 从probe形参里得到值 读写都需要这个 这里大可以换个名字 和filp->private_data重名纯属巧合*/
	signed int gyro_x_adc;		/* 陀螺仪X轴原始值 	 */
	signed int gyro_y_adc;		/* 陀螺仪Y轴原始值		*/
	signed int gyro_z_adc;		/* 陀螺仪Z轴原始值 		*/
	signed int accel_x_adc;		/* 加速度计X轴原始值 	*/
	signed int accel_y_adc;		/* 加速度计Y轴原始值	*/
	signed int accel_z_adc;		/* 加速度计Z轴原始值 	*/
	signed int temp_adc;		/* 温度原始值 			*/
};

static struct icm20608_dev icm20608dev;


static int icm20608_read_regs(struct icm20608_dev *dev, u8 reg, void *buf, int len)
{

	int ret = -1;
	unsigned char txdata[1];
	unsigned char * rxdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->private_data;
    
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}

	rxdata = kzalloc(sizeof(char) * len, GFP_KERNEL);	/* 申请内存 */
	if(!rxdata) {
		goto out1;
	}

	/* 一共发送len+1个字节的数据，第一个字节为寄存器首地址，一共要读取len个字节长度的数据，*/
	txdata[0] = reg | 0x80;		/* 写数据的时候首寄存器地址bit8要置1 */		

	t->tx_buf = txdata;			/* 要发送的数据 */
    	t->rx_buf = rxdata;			/* 要读取的数据 */

	t->len = len+1;				/* t->len=发送的长度+读取的长度 */

	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */

	ret = spi_sync(spi, &m);	/* 同步发送 */
	if(ret) {
		goto out2;
	}
	
    	memcpy(buf , rxdata+1, len);  /* 只需要读取的数据 */

out2:
	kfree(rxdata);					/* 释放内存 */
out1:	
	kfree(t);						/* 释放内存 */
	
	return ret;
}


static s32 icm20608_write_regs(struct icm20608_dev *dev, u8 reg, u8 *buf, u8 len)
{
	int ret = -1;
	unsigned char *txdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->private_data;

	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}

	txdata = kzalloc(sizeof(char)+len, GFP_KERNEL);
	if(!txdata) {
		goto out1;
	}

	/* 一共发送len+1个字节的数据，第一个字节为寄存器首地址，len为要写入的寄存器的集合，*/
	*txdata = reg & ~0x80;	/* 写数据的时候首寄存器地址bit8要清零 */
    	memcpy(txdata+1, buf, len);	/* 把len个寄存器拷贝到txdata里，等待发送 */
	t->tx_buf = txdata;			/* 要发送的数据 */
	t->len = len+1;				/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
    if(ret) {
        goto out2;
    }

out2:
	kfree(txdata);				/* 释放内存 */
out1:
	kfree(t);					/* 释放内存 */
	return ret;
}


static unsigned char icm20608_read_onereg(struct icm20608_dev *dev, u8 reg)
{
	u8 data = 0;
	icm20608_read_regs(dev, reg, &data, 1);
	return data;
}


static void icm20608_write_onereg(struct icm20608_dev *dev, u8 reg, u8 value)
{
	u8 buf = value;
	icm20608_write_regs(dev, reg, &buf, 1);
}


void icm20608_readdata(struct icm20608_dev *dev)
{
	unsigned char data[14] = { 0 };
	icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);

	dev->accel_x_adc = (signed short)((data[0] << 8) | data[1]); 
	dev->accel_y_adc = (signed short)((data[2] << 8) | data[3]); 
	dev->accel_z_adc = (signed short)((data[4] << 8) | data[5]); 
	dev->temp_adc    = (signed short)((data[6] << 8) | data[7]); 
	dev->gyro_x_adc  = (signed short)((data[8] << 8) | data[9]); 
	dev->gyro_y_adc  = (signed short)((data[10] << 8) | data[11]);
	dev->gyro_z_adc  = (signed short)((data[12] << 8) | data[13]);
}


static int icm20608_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &icm20608dev; /* 设置私有数据 */
	return 0;
}


static ssize_t icm20608_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	signed int data[7];
	long err = 0;
	struct icm20608_dev *dev = (struct icm20608_dev *)filp->private_data;

	icm20608_readdata(dev);
	data[0] = dev->gyro_x_adc;
	data[1] = dev->gyro_y_adc;
	data[2] = dev->gyro_z_adc;
	data[3] = dev->accel_x_adc;
	data[4] = dev->accel_y_adc;
	data[5] = dev->accel_z_adc;
	data[6] = dev->temp_adc;
	err = copy_to_user(buf, data, sizeof(data));
	return 0;
}


static const struct file_operations icm20608_ops = {
	.owner = THIS_MODULE,
	.open = icm20608_open,
	.read = icm20608_read,
};


void icm20608_reginit(void)
{
	u8 value = 0;
	
	icm20608_write_onereg(&icm20608dev, ICM20_PWR_MGMT_1, 0x80);
	mdelay(50);
	icm20608_write_onereg(&icm20608dev, ICM20_PWR_MGMT_1, 0x01);
	mdelay(50);

	value = icm20608_read_onereg(&icm20608dev, ICM20_WHO_AM_I);
	printk("ICM20608 ID = %#X\r\n", value);	

	icm20608_write_onereg(&icm20608dev, ICM20_SMPLRT_DIV, 0x00); 	/* 输出速率是内部采样率					*/
	icm20608_write_onereg(&icm20608dev, ICM20_GYRO_CONFIG, 0x18); 	/* 陀螺仪±2000dps量程 				*/
	icm20608_write_onereg(&icm20608dev, ICM20_ACCEL_CONFIG, 0x18); 	/* 加速度计±16G量程 					*/
	icm20608_write_onereg(&icm20608dev, ICM20_CONFIG, 0x04); 		/* 陀螺仪低通滤波BW=20Hz 				*/
	icm20608_write_onereg(&icm20608dev, ICM20_ACCEL_CONFIG2, 0x04); /* 加速度计低通滤波BW=21.2Hz 			*/
	icm20608_write_onereg(&icm20608dev, ICM20_PWR_MGMT_2, 0x00); 	/* 打开加速度计和陀螺仪所有轴 				*/
	icm20608_write_onereg(&icm20608dev, ICM20_LP_MODE_CFG, 0x00); 	/* 关闭低功耗 						*/
	icm20608_write_onereg(&icm20608dev, ICM20_FIFO_EN, 0x00);		/* 关闭FIFO						*/
}


static int icm20608_probe(struct spi_device *spi)
{
	/* 1、申请设备号 */
	if (icm20608dev.major) {
		icm20608dev.devid = MKDEV(icm20608dev.major, 0);
		register_chrdev_region(icm20608dev.devid, ICM20608_CNT, ICM20608_NAME);
	} else {
		alloc_chrdev_region(&icm20608dev.devid, 0, ICM20608_CNT, ICM20608_NAME);
		icm20608dev.major = MAJOR(icm20608dev.devid);
	}

	/* 2、注册驱动 */
	cdev_init(&icm20608dev.cdev, &icm20608_ops);
	cdev_add(&icm20608dev.cdev, icm20608dev.devid, ICM20608_CNT);

	/* 3、创建设备 */
	icm20608dev.class = class_create(THIS_MODULE, ICM20608_NAME);
	if (IS_ERR(icm20608dev.class)) {
		return PTR_ERR(icm20608dev.class);
	}
	icm20608dev.device = device_create(icm20608dev.class, NULL, icm20608dev.devid, NULL, ICM20608_NAME);
	if (IS_ERR(icm20608dev.device)) {
		return PTR_ERR(icm20608dev.device);
	}

	/*初始化spi_device */
	spi->mode = SPI_MODE_0;	/*MODE0，CPOL=0，CPHA=0*/
	spi_setup(spi);
	icm20608dev.private_data = spi;

	/* 初始化ICM20608内部寄存器 必须放在icm20608dev.private_data赋值之后 */
	icm20608_reginit();		
	return 0;
}


static int icm20608_remove(struct spi_device *spi)
{
	cdev_del(&icm20608dev.cdev);
	unregister_chrdev_region(icm20608dev.devid, ICM20608_CNT);
	device_destroy(icm20608dev.class, icm20608dev.devid);
	class_destroy(icm20608dev.class);
	return 0;
}

/* 传统匹配方式ID列表 */
static const struct spi_device_id icm20608_id[] = {
	{"alientek,icm20608", 0},  
	{}
};

/* 设备树匹配列表 */
static const struct of_device_id icm20608_of_match[] = {
	{ .compatible = "alientek,icm20608" },
	{ /* Sentinel */ }
};

/* SPI驱动结构体 */	
static struct spi_driver icm20608_driver = {
	.probe = icm20608_probe,
	.remove = icm20608_remove,
	.driver = {
			.owner = THIS_MODULE,
		   	.name = "icm20608",
		   	.of_match_table = icm20608_of_match, 
		   },
	.id_table = icm20608_id,
};
		   

static int __init icm20608_init(void)
{
	return spi_register_driver(&icm20608_driver);
}


static void __exit icm20608_exit(void)
{
	spi_unregister_driver(&icm20608_driver);
}

module_init(icm20608_init);
module_exit(icm20608_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");



