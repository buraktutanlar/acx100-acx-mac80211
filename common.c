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

/*
 * BOM Config
 * ==================================================
 */

/*
 * BOM Prototypes
 * ... static and also none-static for overview reasons (maybe not best practice ...)
 * ==================================================
 */

/* Locking */
#ifdef OW_20100613_OBSELETE_ACXLOCK_REMOVE
void acx_lock_unhold(void);
void acx_sem_unhold(void);
void acx_lock_debug(acx_device_t *adev, const char *where);
void acx_unlock_debug(acx_device_t *adev, const char *where);
static inline const char *acx_sanitize_str(const char *s);
#endif

/* Logging */
void log_fn_enter(const char *funcname);
void log_fn_exit(const char *funcname);
void log_fn_exitv(const char *funcname, int v);
//-void acx_dump_bytes(const void *data, int num);
//-const char *acx_cmd_status_str(unsigned int state);

/* Firmware, EEPROM, Phy */
MODULE_FIRMWARE("tiacx111");
MODULE_FIRMWARE("tiacx111c16");
MODULE_FIRMWARE("tiacx111r16");

/* Rx Path */

/* Crypto */
#ifdef UNUSED
static void acx100_set_wepkey(acx_device_t * adev);
static void acx111_set_wepkey(acx_device_t * adev);
static void acx_set_wepkey(acx_device_t * adev);
#endif

/* OW, 20100704, Obselete, TBC for cleanup */
#if 0
static void acx_keymac_write(acx_device_t * adev, u16 index, const u32 * addr);
int acx_clear_keys(acx_device_t * adev);
int acx_key_write(acx_device_t *adev, u16 index, u8 algorithm, const struct ieee80211_key_conf *key, const u8 *mac_addr);
#endif

/* Helpers */
//-void acx_mwait(int ms);
/* void great_inquisitor(acx_device_t * adev); */

/* Driver, Module */
static int __init acx_init_module(void);
static void __exit acx_cleanup_module(void);



/*
 * BOM Locking
 * ==================================================
 */
#define DEBUG_TSC 0
#if DEBUG_TSC && defined(CONFIG_X86)
#define TIMESTAMP(d) unsigned long d; rdtscl(d)
#else
#define TIMESTAMP(d) unsigned long d = jiffies
#endif

#ifdef OW_20100613_OBSELETE_ACXLOCK_REMOVE
#ifdef PARANOID_LOCKING
static unsigned max_lock_time;
static unsigned max_sem_time;

/* Obvious or linux kernel specific derived code follows: */

void acx_lock_unhold()
{
	max_lock_time = 0;
}

void acx_sem_unhold()
{
	max_sem_time = 0;
}

static inline const char *acx_sanitize_str(const char *s)
{
	const char *t = strrchr(s, '/');
	if (t)
		return t + 1;
	return s;
}

void acx_lock_debug(acx_device_t * adev, const char *where)
{
	unsigned int count = 100 * 1000 * 1000;
	TIMESTAMP(lock_start);
	where = acx_sanitize_str(where);
	while (--count) {
		if (!spin_is_locked(&adev->spinlock))
			break;
		cpu_relax();
	}
	if (!count) {
		pr_emerg("LOCKUP: already taken at %s!\n",
		       adev->last_lock);
		BUG();
	}
	adev->last_lock = where;
	adev->lock_time = lock_start;
}

void acx_unlock_debug(acx_device_t * adev, const char *where)
{
#ifdef SMP
	if (!spin_is_locked(&adev->spinlock)) {
		where = acx_sanitize_str(where);
		pr_emerg("STRAY UNLOCK at %s!\n", where);
		BUG();
	}
#endif
	if (acx_debug & L_LOCK) {
		TIMESTAMP(diff);
		diff -= adev->lock_time;
		if (diff > max_lock_time) {
			where = acx_sanitize_str(where);
			pr_notice("max lock hold time %ld CPU ticks from %s "
			       "to %s\n", diff, adev->last_lock, where);
			max_lock_time = diff;
		}
	}
}
#endif /* PARANOID_LOCKING */
#endif

/*
 * BOM CMDs (Control Path)
 * ==================================================
 */


int acx_net_reset(struct ieee80211_hw *ieee)
{
	acx_device_t *adev = ieee2adev(ieee);

	if (IS_PCI(adev) || IS_MEM(adev))
		acx_reset_dev(adev);
	else
		TODO();


	return 0;
}


/*
 * BOM Other (Control Path)
 * ==================================================
 */
#if POWER_SAVE_80211
static void acx_s_update_80211_powersave_mode(acx_device_t * adev)
{
	/* merge both structs in a union to be able to have common code */
	union {
		acx111_ie_powersave_t acx111;
		acx100_ie_powersave_t acx100;
	} pm;

	/* change 802.11 power save mode settings */
	log(L_INIT, "updating 802.11 power save mode settings: "
	    "wakeup_cfg 0x%02X, listen interval %u, "
	    "options 0x%02X, hangover period %u, "
	    "enhanced_ps_transition_time %u\n",
	    adev->ps_wakeup_cfg, adev->ps_listen_interval,
	    adev->ps_options, adev->ps_hangover_period,
	    adev->ps_enhanced_transition_time);
	acx_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "Previous PS mode settings: wakeup_cfg 0x%02X, "
	    "listen interval %u, options 0x%02X, "
	    "hangover period %u, "
	    "enhanced_ps_transition_time %u, beacon_rx_time %u\n",
	    pm.acx111.wakeup_cfg,
	    pm.acx111.listen_interval,
	    pm.acx111.options,
	    pm.acx111.hangover_period,
	    IS_ACX111(adev) ?
	    pm.acx111.enhanced_ps_transition_time
	    : pm.acx100.enhanced_ps_transition_time,
	    IS_ACX111(adev) ? pm.acx111.beacon_rx_time : (u32) - 1);
	pm.acx111.wakeup_cfg = adev->ps_wakeup_cfg;
	pm.acx111.listen_interval = adev->ps_listen_interval;
	pm.acx111.options = adev->ps_options;
	pm.acx111.hangover_period = adev->ps_hangover_period;
	if (IS_ACX111(adev)) {
		pm.acx111.beacon_rx_time = cpu_to_le32(adev->ps_beacon_rx_time);
		pm.acx111.enhanced_ps_transition_time =
		    cpu_to_le32(adev->ps_enhanced_transition_time);
	} else {
		pm.acx100.enhanced_ps_transition_time =
		    cpu_to_le16(adev->ps_enhanced_transition_time);
	}
	acx_configure(adev, &pm, ACX1xx_IE_POWER_MGMT);
	acx_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "wakeup_cfg: 0x%02X\n", pm.acx111.wakeup_cfg);
	acx_mwait(40);
	acx_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "wakeup_cfg: 0x%02X\n", pm.acx111.wakeup_cfg);
	log(L_INIT, "power save mode change %s\n",
	    (pm.acx111.
	     wakeup_cfg & PS_CFG_PENDING) ? "FAILED" : "was successful");
	/* FIXME: maybe verify via PS_CFG_PENDING bit here
	 * that power save mode change was successful. */
	/* FIXME: we shouldn't trigger a scan immediately after
	 * fiddling with power save mode (since the firmware is sending
	 * a NULL frame then). */
}
#endif


int acx111_set_default_key(acx_device_t *adev, u8 key_id)
{
	int res;
	ie_dot11WEPDefaultKeyID_t dkey;

#if defined DEBUG_WEP
	struct {
		u16 type;
		u16 len;
		u8 val;
	}ACX_PACKED keyindic;
#endif

	dkey.KeyID = key_id;
	log(L_INIT, "Setting key %u as default\n", dkey.KeyID);
	res = acx_configure(adev, &dkey,
		ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET);

#if defined DEBUG_WEP
	keyindic.val = key_id;
	acx_configure(adev, &keyindic, ACX111_IE_KEY_CHOOSE);
#endif

	adev->default_key = key_id;

	return res;
}

/*
 * BOM Helpers
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
