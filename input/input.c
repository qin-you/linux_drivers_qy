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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/input.h>

/********************************************
 * input子系统实验 驱动按键 含中断 定时器消抖逻辑
*********************************************/

#define KEYINPUT_CNT		1
#define KEYINPUT_NAME		"keyinput"
#define KEY0VALUE		0x01
#define INVAKEY			0xFF
#define KEY_NUM			1

struct irq_keydesc {
	int gpio;
	int irqnum;
	unsigned char value;				// 按键值 如键盘0键的0
	char name[10];
	irqreturn_t (*handler)(int, void*);
};

struct keyinput_dev{
	dev_t 			devid;
	struct cdev		cdev;
	struct class		*class;
	struct device		*device;
	struct device_node	*node;
	struct timer_list	timer;
	struct irq_keydesc	keyirq[KEY_NUM];
	unsigned char 		curkeynum;		// 当前按键号
	struct input_dev	*inputdev;
};

static struct keyinput_dev	keyinputdev;

static irqreturn_t key0_handler(int irq, void *dev_id)
{
	printk("here: interrupt\r\n");
	struct keyinput_dev	*dev = (struct keyinput_dev *)dev_id;
	dev->curkeynum = 0;
	dev->timer.data = (volatile long)dev_id;
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
	return IRQ_RETVAL(IRQ_HANDLED);
}

static void timer_function(unsigned long arg)
{
	printk("here:timer fun\r\n");
	unsigned char	value;
	unsigned char 	num;
	struct irq_keydesc	*keydesc;
	struct keyinput_dev	*dev = (struct keyinput_dev *)arg;

	num = dev->curkeynum;
	keydesc = &dev->keyirq[num];
	value = gpio_get_value(keydesc->gpio);

	if (value == 0) {
		// input_event(dev->inputdev, EV_KEY, num, 1);
		input_report_key(dev->inputdev, keydesc->value, 1);	//这里用1表按下
		input_sync(dev->inputdev);
	} else {
		// input_event(dev->inputdev, EV_KEY, num, 0);
		input_report_key(dev->inputdev, keydesc->value, 0);
		input_sync(dev->inputdev);
	}
}

static int keyio_init(void)
{
	unsigned char i = 0;
	int ret = 0;

	keyinputdev.node = of_find_node_by_path("/key");
	if (keyinputdev.node == NULL) {
                printk("get key device node failed\r\n");
                return -EINVAL;
        }

	// 获取gpio号 
	for (i = 0; i < KEY_NUM; i++) {
		keyinputdev.keyirq[i].gpio = of_get_named_gpio(keyinputdev.node, "key-gpio", i);
		if (keyinputdev.keyirq[i].gpio < 0) {
                        printk("cannot get gpio number\r\n");
                        return -EINVAL;
                }
	}

	// 申请gpio资源（gpio_request) 初始化引脚 获取中断号
	for (i = 0; i < KEY_NUM; i++) {
		memset(keyinputdev.keyirq[i].name, 0, sizeof(keyinputdev.keyirq[i].name));
		sprintf(keyinputdev.keyirq[i].name, "KEY%d", i);
		gpio_request(keyinputdev.keyirq[i].gpio, keyinputdev.keyirq[i].name);
		gpio_direction_input(keyinputdev.keyirq[i].gpio);
		keyinputdev.keyirq[i].irqnum = gpio_to_irq(keyinputdev.keyirq[i].gpio);
	}

	keyinputdev.keyirq[0].handler = key0_handler;	// 只有一个按键
	keyinputdev.keyirq[0].value = KEY_0;		// 给按键赋值 相当于键盘0

	// 注册中断
	for (i = 0; i < KEY_NUM; i++) {
		ret = request_irq(keyinputdev.keyirq[i].irqnum, keyinputdev.keyirq[i].handler, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, keyinputdev.keyirq[i].name, &keyinputdev);
		if (ret < 0) {
                        printk("irq %d request failed\r\n",keyinputdev.keyirq[i].irqnum);
                        return -EFAULT;
                }
	}

	// 创建定时器
	init_timer(&keyinputdev.timer);
	keyinputdev.timer.function = timer_function;

	// 申请 input_dev
	keyinputdev.inputdev = input_allocate_device();
	keyinputdev.inputdev->name = KEYINPUT_NAME;
	// 初始化input_dev
	keyinputdev.inputdev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
	input_set_capability(keyinputdev.inputdev, EV_KEY, KEY_0);
	// 注册输入设备
	ret = input_register_device(keyinputdev.inputdev);
	if (ret) {
		printk("register input device failed\r\n");
		return ret;
	}

	printk("init success\r\n");
	return 0;
}

static int __init keyinput_init(void)
{
	return keyio_init();
}

static void __exit keyinput_exit(void)
{
	unsigned int i = 0;
	del_timer_sync(&keyinputdev.timer);

	for (i = 0; i < KEY_NUM; i++) {
		free_irq(keyinputdev.keyirq[i].irqnum, &keyinputdev);
	}

	for (i = 0; i < KEY_NUM; i++) {
		gpio_free(keyinputdev.keyirq[i].gpio);
	}

	input_unregister_device(keyinputdev.inputdev);
	input_free_device(keyinputdev.inputdev);
}

module_init(keyinput_init);
module_exit(keyinput_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("You Qin");