#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("pci:v0000104Cd00008400sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v0000104Cd00008401sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v0000104Cd00009066sv*sd*bc*sc*i*");
