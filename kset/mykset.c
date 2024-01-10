#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/stat.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("add kset");

struct kset     kset_p;
struct kset     kset_c;

static int kset_filter(struct kset *kset, struct kobject *kobj)
{
        printk("kset_filter: %s\n", kobj->name);
        return 1;       // 1: allow report uevent
}

static const char *kset_name(struct kset *kset, struct kobject *kobj)
{
        static char buf[20];    // static cause this will be return value
        printk("kset_name: %s\n", kobj->name);
        sprintf(buf, "%s", "kset_name");
        return buf;
}

static int kset_uevent(struct kset *kset, struct kobject *kobj, struct kobj_uevent_env *env)
{
        int i = 0;
        printk("kset_uevent:%s\n", kobj->name);

        while (i < env->envp_idx) {
                printk("env info%d: %s\n", i, env->envp[i]);
                i++;
        }
        
        return 0;
}


struct kset_uevent_ops uevent_ops = {
        .filter = kset_filter,
        .name   = kset_name,
        .uevent = kset_uevent,
};


static int __init kset_test_init(void)
{
        printk("kset test init\n");
        kobject_set_name(&kset_p.kobj, "kset_p");       // kset directory name
        kset_p.uevent_ops = &uevent_ops;
        kset_register(&kset_p);                         // register kset_p

        kobject_set_name(&kset_c.kobj, "kset_c");
        kset_c.kobj.kset = &kset_p;                     // set logic relationship
        kset_register(&kset_c);

        return 0;

}

static void __exit kset_test_exit(void)
{
        printk("kset test exit\n");
        kset_unregister(&kset_p);
        kset_unregister(&kset_c);
}

module_init(kset_test_init);
module_exit(kset_test_exit);







