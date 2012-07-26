/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008
 * The ACX100 Open Source Project <acx100-devel@lists.sourceforge.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "acx_debug.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/pm.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/ethtool.h>
#include <linux/pci.h>
#include <linux/nl80211.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>

#include <net/mac80211.h>

#include "acx.h"
#include "usb.h"
#include "merge.h"
#include "mem.h"
#include "pci.h"
#include "cmd.h"
#include "ie.h"
#include "utils.h"
#include "cardsetting.h"
#include "main.h"
#include "debug.h"

/* Firmware, EEPROM, Phy */
MODULE_FIRMWARE("tiacx111");
MODULE_FIRMWARE("tiacx111c16");
MODULE_FIRMWARE("tiacx111r16");

/* Driver, Module */
static int __init acx_init_module(void);
static void __exit acx_cleanup_module(void);

/*
 * Helpers
 * ==================================================
 */

/*
 * Basically a mdelay/msleep with logging
 */
void acx_mwait(int ms)
{
	msleep(ms);
}

#if CMD_DISCOVERY
void great_inquisitor(acx_device_t * adev)
{
	static struct {
		u16 type;
		u16 len;
		/* 0x200 was too large here: */
		u8 data[0x100 - 4];
	} ACX_PACKED ie;
	u16 type;



	/* 0..0x20, 0x1000..0x1020 */
	for (type = 0; type <= 0x1020; type++) {
		if (type == 0x21)
			type = 0x1000;
		ie.type = cpu_to_le16(type);
		ie.len = cpu_to_le16(sizeof(ie) - 4);
		acx_issue_cmd(adev, ACX1xx_CMD_INTERROGATE, &ie, sizeof(ie));
	}

}
#endif


/*
 * BOM Driver, Module
 * ==================================================
 */

static int __init acx_init_module(void)
{
	int r1, r2, r3;

	acx_struct_size_check();

	/* ACX_GIT_VERSION can be an empty string, if something went
	   wrong before on Makefile/shell level. We trap this here
	   ... since trapping empty macro strings in cpp seems not
	   possible (didn't find how ) !? */
	pr_info("acx-mac80211, version: %s (git: %s)\n",
		ACX_RELEASE,
		strlen(ACX_GIT_VERSION) ? ACX_GIT_VERSION : "unknown");

	pr_info("this driver is still EXPERIMENTAL\n"
	       "acx: please read the README file and/or "
	       "go to http://acx100.sourceforge.net/wiki for "
	       "further information\n");

	r1 = r2 = r3 = -EINVAL;

	r1 = acxpci_init_module();
	r2 = acxusb_init_module();
	r3 = acxmem_init_module();

	if (r3 && r2 && r1) {		/* all three failed! */
		pr_info("r1_pci=%i, r2_usb=%i, r3_mem=%i\n", r1, r2, r3);
		return -EINVAL;
	}

	acx_proc_init();
	acx_debugfs_init();

	/* return success if at least one succeeded */
	return 0;
}

static void __exit acx_cleanup_module(void)
{
	/* TODO Check, that interface isn't still up */
	acx_debugfs_exit();
	acx_proc_exit();

	acxpci_cleanup_module();
	acxusb_cleanup_module();
	acxmem_cleanup_module();
}

/*
 * BOM Module
 * ==================================================
 */

module_init(acx_init_module);
module_exit(acx_cleanup_module);

#if ACX_DEBUG

/* will add __read_mostly later */
unsigned int acx_debug = ACX_DEFAULT_MSG;
/* parameter is 'debug', corresponding var is acx_debug */
module_param_named(debug, acx_debug, uint, 0644);
MODULE_PARM_DESC(debug, "Debug level mask (see L_xxx constants)");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
/* implement /sys/module/acx_mac80211/parameters/debugflags */

static const char *flag_names[] = {
	"L_LOCK", "L_INIT", "L_IRQ", "L_ASSOC", "L_FUNC", "L_XFER",
	"L_DATA", "L_DEBUG", "L_IOCTL", "L_CTL", "L_BUFR", "L_XFER_BEACON",
	"L_BUFT", "L_USBRXTX", "L_BUF",
};
/* should check enum debug_flags, but no suitable val is set */
BUILD_BUG_DECL(flag_names, ARRAY_SIZE(flag_names) != 15);

static int acx_debug_flag_get(char *buf, const struct kernel_param *kp)
{
	int i, len;
	char *p = buf; // 1 page preallocated (I think - it didnt crash !!)

	for (i = 0; i < ARRAY_SIZE(flag_names); i++) {
		if (acx_debug & 1 << i)
			len = sprintf(p, "bit %d:%s = 1\n", i, flag_names[i]);
		else
			len = sprintf(p, "bit %d:%s = 0\n", i, flag_names[i]);
		pr_info("%s", p);
		p += len;
	}
	return p - buf;
}

static int acx_debug_flag_set(const char *val, const struct kernel_param *kp)
{
	int i;
	char sign, *e, *p = (char*) val; /* cast away const */

	if ((e = strchr(p, '\n')))
		*e = '\0';	/* trim trailing newline */

	pr_info("parsing flags: %s\n", p);
	for (; p; p = e) {
		sign = *p++;
		switch (sign) {
		case '+':
		case '-':
			break;
		default:
			return -EINVAL;
		}
		if ((e = strchr(p, ',')))
			*e++ = '\0';	/* end 1st term, ready next */

		for (i = 0; i < ARRAY_SIZE(flag_names); i++) {

			if (!strcmp(p, flag_names[i])) {
				if (sign == '+')
					acx_debug |= (1 << i);
				else
					acx_debug &= ~(1 << i);
				break;
			}
		}
		if (i == ARRAY_SIZE(flag_names)) {
			pr_err("no match on val: %s\n", p);
			return -EINVAL;
		}
	}
	return 0;
}

static struct kernel_param_ops acx_debug_flag_ops = {
        .get = acx_debug_flag_get,
        .set = acx_debug_flag_set,
};
module_param_cb(debugflags, &acx_debug_flag_ops, "str", 0644);
MODULE_PARM_DESC(debugflags, "read/set flag names: +L_CTL,-L_BUFT etc");

#endif	/* implement /sys/module/acx_mac80211/parameters/debugflags */
#endif	/* ACX_DEBUG */

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif
/* USB had this: MODULE_AUTHOR("Martin Wawro <martin.wawro AT uni-dortmund.de>"); */
MODULE_AUTHOR("ACX100 Open Source Driver development team");
MODULE_DESCRIPTION
    ("Driver for TI ACX1xx based wireless cards (CardBus/PCI/USB)");

MODULE_VERSION(ACX_RELEASE);
