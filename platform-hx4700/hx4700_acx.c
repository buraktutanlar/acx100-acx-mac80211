/*
 * WLAN (TI TNETW1100B) support in the hx470x.
 *
 * Copyright (c) 2006 SDG Systems, LLC
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * 28-March-2006          Todd Blumer <todd@sdgsystems.com>
 */


#include <linux/kernel.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 0)
#include <linux/module.h>
#endif

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/leds.h>

#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <mach/hx4700.h>
#include <mach/pxa27x.h>

#define WLAN_OFFSET	0x1000000
#define WLAN_BASE	(PXA_CS5_PHYS+WLAN_OFFSET)

static int
hx4700_wlan_start( void )
{

	gpio_set_value(GPIO83_HX4700_WLAN_nRESET, 0);
	mdelay(5);
	gpio_set_value(EGPIO0_VCC_3V3_EN, 1);
	mdelay(100);
	gpio_set_value(EGPIO7_VCC_3V3_WL_EN, 1);
	mdelay(150);
	gpio_set_value(EGPIO1_WL_VREG_EN, 1);
	gpio_set_value(EGPIO2_VCC_2V1_WL_EN, 1);
	gpio_set_value(EGPIO6_WL1V8_EN, 1);
	mdelay(10);
	gpio_set_value(GPIO83_HX4700_WLAN_nRESET, 1);
	mdelay(50);
	//OW led_trigger_event_shared(hx4700_radio_trig, LED_FULL);

	printk("hx4700_acx: %s: done\n", __func__);
	return 0;
}

static int
hx4700_wlan_stop( void )
{
	gpio_set_value(EGPIO0_VCC_3V3_EN, 0);
	gpio_set_value(EGPIO1_WL_VREG_EN, 0);
	gpio_set_value(EGPIO7_VCC_3V3_WL_EN, 0);
	gpio_set_value(EGPIO2_VCC_2V1_WL_EN, 0);
	gpio_set_value(EGPIO6_WL1V8_EN, 0);
	gpio_set_value(GPIO83_HX4700_WLAN_nRESET, 0);
	// OW led_trigger_event_shared(hx4700_radio_trig, LED_OFF);

	printk("hx4700_acx: %s: done\n", __func__);
	return 0;
}

static void hx4700_wlan_e_release(struct device *dev) {
}

static struct resource acx_resources[] = {
	[0] = {
		.start	= WLAN_BASE,
		.end	= WLAN_BASE + 0x20,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gpio_to_irq(GPIO14_HX4700_nWLAN_IRQ),
		.end	= gpio_to_irq(GPIO14_HX4700_nWLAN_IRQ),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device acx_device = {
	.name	= "acx-mem",
	.dev	= {
		.release = &hx4700_wlan_e_release
	},
	.num_resources	= ARRAY_SIZE( acx_resources ),
	.resource	= acx_resources,
};

static int __init
hx4700_wlan_init( void )
{
	int res;

	hx4700_wlan_start();

	printk("hx4700_acx: %s: platform_device_register ... \n", __func__);
	res=platform_device_register( &acx_device );
	printk("hx4700_acx: %s: platform_device_register: done\n", __func__);
	// Check if the (or another) driver was found, aka if probe succeeded
	if (acx_device.dev.driver == NULL) {
		printk( "hx4700_acx: %s: acx-mem platform_device_register: failed\n", __func__);
		platform_device_unregister( &acx_device );
		hx4700_wlan_stop();
		return(-EINVAL);
	}

	return res;
}


static void __exit
hx4700_wlan_exit( void )
{
	printk( "hx4700_acx: %s: platform_device_unregister ... \n", __func__ );
	platform_device_unregister( &acx_device );
	printk( "hx4700_acx: %s: platform_device_unregister: done\n", __func__ );

	hx4700_wlan_stop();
}


module_init( hx4700_wlan_init );
module_exit( hx4700_wlan_exit );

MODULE_AUTHOR( "Todd Blumer <todd@sdgsystems.com>" );
MODULE_DESCRIPTION( "WLAN driver for iPAQ hx4700" );
MODULE_LICENSE( "GPL" );
