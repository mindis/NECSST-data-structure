#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xf257752c, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x3e54b244, __VMLINUX_SYMBOL_STR(btree_lookup) },
	{ 0x4d7a03b5, __VMLINUX_SYMBOL_STR(btree_insert) },
	{ 0xa3a04602, __VMLINUX_SYMBOL_STR(btree_geo64) },
	{ 0xcad91b90, __VMLINUX_SYMBOL_STR(btree_init) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "0AB053AB5FF72FA6E35AEC3");
