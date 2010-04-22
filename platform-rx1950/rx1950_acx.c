/*
 * WLAN (TI TNETW1100B) support in the rx1950.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Copyright (c) 2008 Denis Grigoriev <dgreenday at gmail.com>
 * Copyright (c) 2010 Vasily Khoruzhick <anarsoul at gmail.com>
 */


#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <mach/regs-gpio.h>

#define WLAN_BASE	0x20000000

#define WLAN_POWER_PIN	S3C2410_GPA(11)
#define WLAN_RESET_PIN	S3C2410_GPA(14)
#define WLAN_LED_PIN_1	S3C2410_GPC(8)
#define WLAN_LED_PIN_2	S3C2410_GPC(9)
#define WLAN_CHIP_SELECT S3C2410_GPA(15)

static int
rx1950_wlan_start(void)
{
	printk(KERN_INFO "rx1950_wlan_start\n");
	s3c2410_gpio_cfgpin(S3C2410_GPH(10), S3C2410_GPH10_CLKOUT1);
	s3c2410_gpio_cfgpin(WLAN_LED_PIN_1, S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_setpin(WLAN_LED_PIN_1, 1);
	s3c2410_gpio_cfgpin(WLAN_LED_PIN_2, S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_setpin(WLAN_LED_PIN_2, 1);

	s3c2410_gpio_cfgpin(WLAN_CHIP_SELECT, S3C2410_GPA15_nGCS4);

	s3c2410_gpio_cfgpin(WLAN_RESET_PIN, S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_cfgpin(WLAN_POWER_PIN, S3C2410_GPIO_OUTPUT);

	s3c2410_gpio_setpin(WLAN_RESET_PIN, 0);
	mdelay(200);
	s3c2410_gpio_setpin(WLAN_RESET_PIN, 1);

	s3c2410_gpio_setpin(WLAN_POWER_PIN, 1);

	return 0;
}

static int
rx1950_wlan_stop(void)
{
	printk(KERN_INFO "rx1950_wlan_stop\n");
	s3c2410_gpio_setpin(WLAN_POWER_PIN, 0);
	s3c2410_gpio_setpin(WLAN_RESET_PIN, 0);

	s3c2410_gpio_cfgpin(WLAN_CHIP_SELECT, S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_setpin(WLAN_CHIP_SELECT, 1);
	s3c2410_gpio_setpin(WLAN_LED_PIN_1, 0);
	s3c2410_gpio_setpin(WLAN_LED_PIN_2, 0);

	s3c2410_gpio_cfgpin(S3C2410_GPH(10), S3C2410_GPIO_OUTPUT);
	s3c2410_gpio_setpin(S3C2410_GPH(10), 0);

	return 0;
}

static void
rx1950_wlan_e_release(struct device *dev)
{
}

static struct resource acx_resources[] = {
	[0] = {
		.start	= WLAN_BASE,
		.end	= WLAN_BASE + 0x20,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_EINT16,
		.end	= IRQ_EINT16,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device acx_device = {
	.name	= "acx-mem",
	.dev	= {
		.release = rx1950_wlan_e_release
	},
	.num_resources	= ARRAY_SIZE(acx_resources),
	.resource	= acx_resources,
};

static int __init
rx1950_wlan_init(void)
{
	int res;
	printk(KERN_INFO "rx1950_wlan_init: acx-mem platform_device_register\n");
	rx1950_wlan_start();
	res = platform_device_register(&acx_device);
	if (acx_device.dev.driver == NULL) {
		printk(KERN_ERR "%s: acx-mem driver is not loaded\n", __func__);
		platform_device_unregister(&acx_device);
		rx1950_wlan_stop();
		return -EINVAL;
	}

	if (res != 0) {
		printk(KERN_ERR "%s: acx-mem platform_device_register: failed\n",
			__func__);
		rx1950_wlan_stop();
	}

	return res;
}

static void __exit
rx1950_wlan_exit(void)
{
	platform_device_unregister(&acx_device);
	rx1950_wlan_stop();
}

module_init(rx1950_wlan_init);
module_exit(rx1950_wlan_exit);

MODULE_AUTHOR("Vasily Khoruzhick <anarsoul at gmail.com>");
MODULE_DESCRIPTION("WLAN driver for iPAQ rx1950");
MODULE_LICENSE("GPL");
