#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x4cf819e6, "module_layout" },
	{ 0xa4c6afdf, "kset_unregister" },
	{ 0x4599779, "kset_create_and_add" },
	{ 0x9e03848f, "kernel_kobj" },
	{ 0x3d099b2a, "kobject_uevent" },
	{ 0x4a165127, "kobject_put" },
	{ 0x50759163, "kobject_init_and_add" },
	{ 0x4f00afd3, "kmem_cache_alloc_trace" },
	{ 0x696f0c3, "kmalloc_caches" },
	{ 0x37a0cba, "kfree" },
	{ 0x3c3ff9fd, "sprintf" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x8c8569cb, "kstrtoint" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "FD4E1CF75F01E1CA67DDCEB");
