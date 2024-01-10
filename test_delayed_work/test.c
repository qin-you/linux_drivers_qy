#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Delayed Work Example");

// 定义 delayed_work 结构体
struct delayed_work my_delayed_work;

// 定义工作回调函数
void my_work_callback(struct work_struct *work) {
    pr_info("Delayed work executed after delay!\n");
}

static int __init my_init(void) {
    pr_info("Initializing Delayed Work Example\n");

    // 初始化 delayed_work 结构体
    INIT_DELAYED_WORK(&my_delayed_work, my_work_callback);

    // 延迟执行工作，单位是 jiffies，HZ 为内核的时钟频率
    schedule_delayed_work(&my_delayed_work, msecs_to_jiffies(2*HZ));

    return 0;
}

static void __exit my_exit(void) {
    pr_info("Exiting Delayed Work Example\n");

    // 取消延迟工作
    cancel_delayed_work_sync(&my_delayed_work);
}

module_init(my_init);
module_exit(my_exit);

