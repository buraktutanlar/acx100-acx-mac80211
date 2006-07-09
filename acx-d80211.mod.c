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

static const struct modversion_info ____versions[]
__attribute_used__
__attribute__((section("__versions"))) = {
	{ 0xcb706872, "struct_module" },
	{ 0xb7b17b7d, "ieee80211_rx_irqsafe" },
	{ 0x1a1a4f09, "__request_region" },
	{ 0x12da5bb2, "__kmalloc" },
	{ 0xf9a482f9, "msleep" },
	{ 0xd6ee688f, "vmalloc" },
	{ 0x75b38522, "del_timer" },
	{ 0x9efed5af, "iomem_resource" },
	{ 0xb13d73c9, "malloc_sizes" },
	{ 0xedd2e10b, "pci_disable_device" },
	{ 0x3cebca6e, "netif_carrier_on" },
	{ 0xf6a5a6c8, "schedule_work" },
	{ 0xdd6fbe21, "netif_carrier_off" },
	{ 0x16808853, "remove_proc_entry" },
	{ 0x3ef1186a, "ieee80211_unregister_hw" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0x2fd1d81c, "vfree" },
	{ 0x1d26aa98, "sprintf" },
	{ 0x93a4cc66, "usb_unlink_urb" },
	{ 0x7d11c268, "jiffies" },
	{ 0x49ef338b, "ieee80211_get_hdrlen" },
	{ 0x12f237eb, "__kzalloc" },
	{ 0x7e4207b, "ieee80211_tx_status" },
	{ 0xfe6e3495, "pci_set_master" },
	{ 0x7374c8ef, "ieee80211_dev_hw_data" },
	{ 0x143a5c20, "pci_restore_state" },
	{ 0x2d51cd05, "usb_deregister" },
	{ 0x1b7d4074, "printk" },
	{ 0x5f0fbd28, "ethtool_op_get_link" },
	{ 0xc7475cc2, "free_netdev" },
	{ 0xf07d7f3c, "dma_free_coherent" },
	{ 0x8e16ede8, "wireless_send_event" },
	{ 0x9de928e2, "usb_control_msg" },
	{ 0x20187c7, "mod_timer" },
	{ 0x52f11078, "ieee80211_netif_oper" },
	{ 0x9eac042a, "__ioremap" },
	{ 0x33fe2c7a, "dma_alloc_coherent" },
	{ 0x23d9f85, "ieee80211_update_hw" },
	{ 0x62c46a05, "skb_over_panic" },
	{ 0x3cc610c4, "netif_device_attach" },
	{ 0xb907d860, "usb_submit_urb" },
	{ 0x8523fd53, "kmem_cache_alloc" },
	{ 0x15670e2d, "_mmx_memcpy" },
	{ 0xad00cd2b, "netif_device_detach" },
	{ 0x8cd1e798, "__alloc_skb" },
	{ 0x72216fa9, "param_get_uint" },
	{ 0xd859ff27, "usb_bulk_msg" },
	{ 0x26e96637, "request_irq" },
	{ 0x6b2dc060, "dump_stack" },
	{ 0xd9fd3bbd, "create_proc_entry" },
	{ 0xd49501d4, "__release_region" },
	{ 0xe3b58cbb, "pci_unregister_driver" },
	{ 0xfde9fe64, "init_timer" },
	{ 0xdceffaa3, "pci_set_power_state" },
	{ 0xc818ff24, "kmem_cache_zalloc" },
	{ 0x15c1b673, "ieee80211_register_hw" },
	{ 0x37a0cba, "kfree" },
	{ 0x801678, "flush_scheduled_work" },
	{ 0xbeefc49, "ieee80211_alloc_hw" },
	{ 0x8abac70a, "param_set_uint" },
	{ 0xedc03953, "iounmap" },
	{ 0x2975e69c, "__pci_register_driver" },
	{ 0xba49676e, "usb_register_driver" },
	{ 0x719372b8, "request_firmware" },
	{ 0x69783f0b, "ieee80211_free_hw" },
	{ 0x60a4461c, "__up_wakeup" },
	{ 0x2d85651b, "unregister_netdev" },
	{ 0x25da070, "snprintf" },
	{ 0x11ac8ac9, "__netif_schedule" },
	{ 0x96b27088, "__down_failed" },
	{ 0xf7da9f2a, "pci_enable_device" },
	{ 0xd800e35c, "usb_free_urb" },
	{ 0x33934162, "release_firmware" },
	{ 0x475ab34, "usb_alloc_urb" },
	{ 0xf20dabd8, "free_irq" },
	{ 0xb02be17a, "pci_save_state" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=80211,usbcore,firmware_class";

MODULE_ALIAS("usb:v2001p3B00d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v2001p3B01d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v07B8pB21Ad*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v057Cp5601d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v057Cp6201d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0CDEp0017d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("usb:v0451p60C5d*dc*dsc*dp*ic*isc*ip*");
MODULE_ALIAS("pci:v0000104Cd00008400sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v0000104Cd00008401sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v0000104Cd00009066sv*sd*bc*sc*i*");
