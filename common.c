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

//-void acx_get_firmware_version(acx_device_t *adev);
//-void acx_display_hardware_details(acx_device_t *adev);
//-firmware_image_t *acx_read_fw(struct device *dev, const char *file, u32 * size);
//-void acx_parse_configoption(acx_device_t *adev, const acx111_ie_configoption_t * pcfg);
int acx_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
int acx_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);

/* Rx Path */
//-void acx_process_rxbuf(acx_device_t *adev, rxbuffer_t * rxbuf);
static void acx_rx(acx_device_t *adev, rxbuffer_t *rxbuf);

/* Tx Path */
#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 39)
int acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
#else
void acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
#endif

//-void acx_tx_work(struct work_struct *work);
void acx_tx_queue_go(acx_device_t *adev);
int acx_tx_frame(acx_device_t *adev, struct sk_buff *skb);
//-void acx_tx_queue_flush(acx_device_t *adev);
//-void acx_stop_queue(struct ieee80211_hw *hw, const char *msg);
//-int acx_queue_stopped(struct ieee80211_hw *ieee);
//-void acx_wake_queue(struct ieee80211_hw *hw, const char *msg);
tx_t *acx_alloc_tx(acx_device_t *adev, unsigned int len, int q);
static void acx_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque);
static void *acx_get_txbuf(acx_device_t *adev, tx_t *tx_opaque, int q);
static void acx_tx_data(acx_device_t *adev, tx_t *tx_opaque,
			int len, struct ieee80211_tx_info *ieeectl,
			struct sk_buff *skb, int q);
void acxpcimem_handle_tx_error(acx_device_t *adev, u8 error,
			unsigned int finger,
			struct ieee80211_tx_info *info);
//-u16 acx111_tx_build_rateset(acx_device_t *adev, txdesc_t *txdesc,
//-			struct ieee80211_tx_info *info);
//-void acx111_tx_build_txstatus(acx_device_t *adev,
//-			struct ieee80211_tx_info *txstatus, u16 r111,
//-			u8 ack_failures);
u16 acx_rate111_hwvalue_to_bitrate(u16 hw_value);
int acx_rate111_hwvalue_to_rateindex(u16 hw_value);

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

/* Irq Handling, Timer */
//-void acx_init_task_scheduler(acx_device_t *adev);
//-void acx_after_interrupt_task(acx_device_t *adev);
//-void acx_schedule_task(acx_device_t *adev, unsigned int set_flag);
//-void acx_log_irq(u16 irqtype);
//-void acx_timer(unsigned long address);
void acx_set_timer(acx_device_t *adev, int timeout_us);

/* Mac80211 Ops */
//-int acx_op_config(struct ieee80211_hw *hw, u32 changed);
/* void acx_op_bss_info_changed(struct ieee80211_hw *hw,
			struct ieee80211_vif *vif,
			struct ieee80211_bss_conf *info, u32 changed);

int acx_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
		struct ieee80211_vif *vif, struct ieee80211_sta *sta,
		struct ieee80211_key_conf *key);
*/
/* void acx_op_configure_filter(struct ieee80211_hw *hw,
			unsigned int changed_flags,
			unsigned int *total_flags, u64 multicast);
*/

/*
#if CONFIG_ACX_MAC80211_VERSION >= KERNEL_VERSION(3, 2, 0)
int acx_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		u16 queue, const struct ieee80211_tx_queue_params *params);
#else
int acx_conf_tx(struct ieee80211_hw *hw, u16 queue,
		const struct ieee80211_tx_queue_params *params);
#endif
*/

/* int acx_op_get_stats(struct ieee80211_hw *hw, struct ieee80211_low_level_stats *stats);

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
int acx_e_op_get_tx_stats(struct ieee80211_hw *hw,
			struct ieee80211_tx_queue_stats *stats);
#endif


int acx_op_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
		bool set);
*/
static int acx_do_job_update_tim(acx_device_t *adev);

/* Helpers */
//-void acx_mwait(int ms);
static u8 acx_signal_to_winlevel(u8 rawlevel);
static u8 acx_signal_to_winlevel(u8 rawlevel);
u8 acx_signal_determine_quality(u8 signal, u8 noise);
const char *acx_get_packet_type_string(u16 fc);
/* void great_inquisitor(acx_device_t * adev); */

/* Driver, Module */
static int __init acx_init_module(void);
static void __exit acx_cleanup_module(void);


/*
 * BOM Defines, static vars, etc.
 * ==================================================
 */

/* minutes to wait until next radio recalibration: */
#define RECALIB_PAUSE	5

/* BOM Rate and channel definition
 * ---
 */

/* We define rates without short-preamble support fo now */

static struct ieee80211_rate acx100_rates[] = {
	{ .bitrate = 10, .hw_value = RATE100_1, },
	{ .bitrate = 20, .hw_value = RATE100_2, },
	{ .bitrate = 55, .hw_value = RATE100_5, },
	{ .bitrate = 110, .hw_value = RATE100_11, },
	{ .bitrate = 220, .hw_value = RATE100_22, },
};

static struct ieee80211_rate acx111_rates[] = {
	{ .bitrate = 10, .hw_value = RATE111_1, },
	{ .bitrate = 20, .hw_value = RATE111_2, },
	{ .bitrate = 55, .hw_value = RATE111_5, },
	{ .bitrate = 60, .hw_value = RATE111_6, },
	{ .bitrate = 90, .hw_value = RATE111_9, },
	{ .bitrate = 110, .hw_value = RATE111_11, },
	{ .bitrate = 120, .hw_value = RATE111_12, },
	{ .bitrate = 180, .hw_value = RATE111_18, },
	{ .bitrate = 240, .hw_value = RATE111_24, },
	{ .bitrate = 360, .hw_value = RATE111_36, },
	{ .bitrate = 480, .hw_value = RATE111_48, },
	{ .bitrate = 540, .hw_value = RATE111_54, },
};

static struct ieee80211_channel channels[] = {
	{ .center_freq = 2412, .hw_value = 1, },
	{ .center_freq = 2417, .hw_value = 2, },
	{ .center_freq = 2422, .hw_value = 3, },
	{ .center_freq = 2427, .hw_value = 4, },
	{ .center_freq = 2432, .hw_value = 5, },
	{ .center_freq = 2437, .hw_value = 6, },
	{ .center_freq = 2442, .hw_value = 7, },
	{ .center_freq = 2447, .hw_value = 8, },
	{ .center_freq = 2452, .hw_value = 9, },
	{ .center_freq = 2457, .hw_value = 10, },
	{ .center_freq = 2462, .hw_value = 11, },
	{ .center_freq = 2467, .hw_value = 12, },
	{ .center_freq = 2472, .hw_value = 13, },
	{ .center_freq = 2484, .hw_value = 14, },
};

static struct ieee80211_supported_band acx100_band_2GHz = {
	.channels	= channels,
	.n_channels	= ARRAY_SIZE(channels),
	.bitrates	= acx100_rates,
	.n_bitrates	= ARRAY_SIZE(acx100_rates),
};

static struct ieee80211_supported_band acx111_band_2GHz = {
	.channels	= channels,
	.n_channels	= ARRAY_SIZE(channels),
	.bitrates	= acx111_rates,
	.n_bitrates	= ARRAY_SIZE(acx111_rates),
};

const u8 bitpos2genframe_txrate[] = {
	[0] = 10,		/*  1 Mbit/s */
	[1] = 20,		/*  2 Mbit/s */
	[2] = 55,		/*  5*5 Mbit/s */
	[3] = 0x0B,		/*  6 Mbit/s */
	[4] = 0x0F,		/*  9 Mbit/s */
	[5] = 110,		/* 11 Mbit/s */
	[6] = 0x0A,		/* 12 Mbit/s */
	[7] = 0x0E,		/* 18 Mbit/s */
	[8] = 220,		/* 22 Mbit/s */
	[9] = 0x09,		/* 24 Mbit/s */
	[10] = 0x0D,		/* 36 Mbit/s */
	[11] = 0x08,		/* 48 Mbit/s */
	[12] = 0x0C,		/* 54 Mbit/s */
	[13] = 10,		/*  1 Mbit/s, should never happen */
	[14] = 10,		/*  1 Mbit/s, should never happen */
	[15] = 10,		/*  1 Mbit/s, should never happen */
};

const u8 acx_bitpos2ratebyte[] = {
	DOT11RATEBYTE_1,
	DOT11RATEBYTE_2,
	DOT11RATEBYTE_5_5,
	DOT11RATEBYTE_6_G,
	DOT11RATEBYTE_9_G,
	DOT11RATEBYTE_11,
	DOT11RATEBYTE_12_G,
	DOT11RATEBYTE_18_G,
	DOT11RATEBYTE_22,
	DOT11RATEBYTE_24_G,
	DOT11RATEBYTE_36_G,
	DOT11RATEBYTE_48_G,
	DOT11RATEBYTE_54_G,
};

const u8 acx_bitpos2rate100[] = {
	[0] = RATE100_1,
	[1] = RATE100_2,
	[2] = RATE100_5,
	[3] = RATE100_2,	/* should not happen */
	[4] = RATE100_2,	/* should not happen */
	[5] = RATE100_11,
	[6] = RATE100_2,	/* should not happen */
	[7] = RATE100_2,	/* should not happen */
	[8] = RATE100_22,
	[9] = RATE100_2,	/* should not happen */
	[10] = RATE100_2,	/* should not happen */
	[11] = RATE100_2,	/* should not happen */
	[12] = RATE100_2,	/* should not happen */
	[13] = RATE100_2,	/* should not happen */
	[14] = RATE100_2,	/* should not happen */
	[15] = RATE100_2,	/* should not happen */
};
BUILD_BUG_DECL(Rates, ARRAY_SIZE(acx_bitpos2rate100)
		   != ARRAY_SIZE(bitpos2genframe_txrate));

/*
 * maps acx111 tx descr rate field to acx100 one
 */
/*
static u8 acx_rate111to100(u16 r)
{
	return acx_bitpos2rate100[highest_bit(r)];
}
*/


int acx_rate111_hwvalue_to_rateindex(u16 hw_value)
{
	int i, r=-1;

	for (i = 0; i < ARRAY_SIZE(acx111_rates); i++) {
		if (acx111_rates[i].hw_value == hw_value) {
			r = i;
			break;
		}
	}
	return (r);
}

u16 acx_rate111_hwvalue_to_bitrate(u16 hw_value)
{
	int i;
	u16 bitrate = -1;

	i = acx_rate111_hwvalue_to_rateindex(hw_value);
	if (i != -1)
		bitrate = acx111_rates[i].bitrate;

	return (bitrate);
}


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
 * BOM Logging
 * ==================================================
 */

/* ----- */
#if ACX_DEBUG > 1

static int acx_debug_func_indent;
#define ACX_DEBUG_FUNC_INDENT_INCREMENT 2
static const char acx_debug_spaces[] = "          " "          ";	/* Nx10 spaces */

void log_fn_enter(const char *funcname)
{
	int indent;
	TIMESTAMP(d);

	indent = acx_debug_func_indent;
	if (indent >= sizeof(acx_debug_spaces))
		indent = sizeof(acx_debug_spaces) - 1;

	pr_info("%08ld %s==> %s\n", d % 100000000,
		acx_debug_spaces + (sizeof(acx_debug_spaces) - 1) - indent,
		funcname);

	acx_debug_func_indent += ACX_DEBUG_FUNC_INDENT_INCREMENT;
}

void log_fn_exit(const char *funcname)
{
	int indent;
	TIMESTAMP(d);

	/* OW Handle underflow */
	if (acx_debug_func_indent >= ACX_DEBUG_FUNC_INDENT_INCREMENT)
		acx_debug_func_indent -= ACX_DEBUG_FUNC_INDENT_INCREMENT;
	else
		acx_debug_func_indent = 0;

	indent = acx_debug_func_indent;
	if (indent >= sizeof(acx_debug_spaces))
		indent = sizeof(acx_debug_spaces) - 1;

	pr_info(" %08ld %s<== %s\n", d % 100000000,
		acx_debug_spaces + (sizeof(acx_debug_spaces) - 1) - indent,
		funcname);
}

void log_fn_exitv(const char *funcname, int v)
{
	int indent;
	TIMESTAMP(d);

	acx_debug_func_indent -= ACX_DEBUG_FUNC_INDENT_INCREMENT;

	indent = acx_debug_func_indent;
	if (indent >= sizeof(acx_debug_spaces))
		indent = sizeof(acx_debug_spaces) - 1;

	pr_info("%08ld %s<== %s: %08X\n", d % 100000000,
		acx_debug_spaces + (sizeof(acx_debug_spaces) - 1) - indent,
		funcname, v);
}
#endif /* ACX_DEBUG > 1 */

const char *acx_cmd_status_str(unsigned int state)
{
	static const char *const cmd_error_strings[] = {
		"Idle",
		"Success",
		"Unknown Command",
		"Invalid Information Element",
		"Channel rejected",
		"Channel invalid in current regulatory domain",
		"MAC invalid",
		"Command rejected (read-only information element)",
		"Command rejected",
		"Already asleep",
		"TX in progress",
		"Already awake",
		"Write only",
		"RX in progress",
		"Invalid parameter",
		"Scan in progress",
		"Failed"
	};
	return state < ARRAY_SIZE(cmd_error_strings) ?
	    cmd_error_strings[state] : "?";
}

/*
 * BOM Firmware, EEPROM, Phy
 * ==================================================
 */

void acx_get_firmware_version(acx_device_t * adev)
{
	fw_ver_t fw;
	u8 hexarr[4] = { 0, 0, 0, 0 };
	int hexidx = 0, val = 0;
	const char *num;
	char c;

	FN_ENTER;

	memset(fw.fw_id, 'E', FW_ID_SIZE);
	acx_interrogate(adev, &fw, ACX1xx_IE_FWREV);
	memcpy(adev->firmware_version, fw.fw_id, FW_ID_SIZE);
	adev->firmware_version[FW_ID_SIZE] = '\0';

	log(L_INIT, "Firmware: firmware_version='%s' hw_id=%08X\n",
	    adev->firmware_version, fw.hw_id);

	if (strncmp(fw.fw_id, "Rev ", 4) != 0) {
		pr_info("strange firmware version string "
			"'%s', please report\n", adev->firmware_version);
		adev->firmware_numver = 0x01090407;	/* assume 1.9.4.7 */
	} else {
		num = &fw.fw_id[4];
		while (1) {
			c = *num++;
			if ((c == '.') || (c == '\0')) {
				hexarr[hexidx++] = val;
				if ((hexidx > 3) || (c == '\0')) /* end? */
					break;
				val = 0;
				continue;
			}
			if ((c >= '0') && (c <= '9'))
				c -= '0';
			else
				c = c - 'a' + (char)10;
			val = val * 16 + c;
		}

		adev->firmware_numver = (u32) ((hexarr[0] << 24) |
					       (hexarr[1] << 16)
					       | (hexarr[2] << 8) | hexarr[3]);
		log(L_DEBUG, "firmware_numver 0x%08X\n",
			adev->firmware_numver);
	}
	if (IS_ACX111(adev)) {
		if (adev->firmware_numver == 0x00010011) {
			/* This one does not survive floodpinging */
			pr_info("firmware '%s' is known to be buggy, "
				"please upgrade\n", adev->firmware_version);
		}
	}

	adev->firmware_id = le32_to_cpu(fw.hw_id);

	/* we're able to find out more detailed chip names now */
	switch (adev->firmware_id & 0xffff0000) {
	case 0x01010000:
	case 0x01020000:
		adev->chip_name = "TNETW1100A";
		break;
	case 0x01030000:
		adev->chip_name = "TNETW1100B";
		break;
	case 0x03000000:
	case 0x03010000:
		adev->chip_name = "TNETW1130";
		break;
	case 0x04030000:	/* 0x04030101 is TNETW1450 */
		adev->chip_name = "TNETW1450";
		break;
	default:
		pr_info("unknown chip ID 0x%08X, "
			"please report\n", adev->firmware_id);
		break;
	}
	FN_EXIT0;
}

/*
 * acx_display_hardware_details
 *
 * Displays hw/fw version, radio type etc...
 */
void acx_display_hardware_details(acx_device_t *adev)
{
	const char *radio_str, *form_str;

	FN_ENTER;

	switch (adev->radio_type) {
	case RADIO_0D_MAXIM_MAX2820:
		/* DWL-650+ B1: MAXIM MAX2820 EGM 236 A7NOCH */
		/* USB DWL-120+ flip-antenna version:
		   MAXIM MAX2820 EGM 243 A7NO10
		   (large G logo) W22 B003A P01
		   (reference: W22-P01-B003A) */
		radio_str = "Maxim (MAX2820)";
		break;
	case RADIO_11_RFMD:
		radio_str = "RFMD";
		break;
	case RADIO_15_RALINK:
		radio_str = "Ralink";
		break;
	case RADIO_16_RADIA_RC2422:
		/* WL311v2 indicates that it's a Radia,
                   semi-recognizable label: RC2422(?) */
		radio_str = "Radia (RC2422?)";
		break;
	case RADIO_17_UNKNOWN:
		/* TI seems to have a radio which is
		 * additionally 802.11a capable, too */
		radio_str = "802.11a/b/g radio?! Please report";
		break;
	case RADIO_19_UNKNOWN:
		radio_str = "A radio used by Safecom cards?! Please report";
		break;
	case RADIO_1B_TI_TNETW3422:
		/* ex-Radia (consumed by TI), i.e. likely a RC2422 successor */
		radio_str = "TI (TNETW3422)";
		break;
	default:
		radio_str = "UNKNOWN, please report radio type name!";
		break;
	}

	switch (adev->form_factor) {
	case 0x00:
		form_str = "unspecified";
		break;
	case 0x01:
		form_str = "(mini-)PCI / CardBus";
		break;
	case 0x02:
		form_str = "USB";
		break;
	case 0x03:
		form_str = "Compact Flash";
		break;
	default:
		form_str = "UNKNOWN, please report";
		break;
	}

	pr_info("chipset %s, radio type 0x%02X (%s), "
	       "form factor 0x%02X (%s), EEPROM version 0x%02X, "
	       "uploaded firmware '%s'\n",
	       adev->chip_name, adev->radio_type, radio_str,
	       adev->form_factor, form_str, adev->eeprom_version,
	       adev->firmware_version);

	FN_EXIT0;
}

/*
 * acx_s_read_fw
 *
 * Loads a firmware image
 * Returns:
 *  0:						unable to load file
 *  pointer to firmware:	success
 */
firmware_image_t *acx_read_fw(struct device *dev, const char *file,
				u32 * size)
{
	firmware_image_t *res;
	const struct firmware *fw_entry;

	res = NULL;
	log(L_INIT, "requesting firmware image '%s'\n", file);
	if (!request_firmware(&fw_entry, file, dev)) {
		*size = 8;
		if (fw_entry->size >= 8)
			*size = 8 + le32_to_cpu(*(u32 *) (fw_entry->data + 4));
		if (fw_entry->size != *size) {
			pr_info("firmware size does not match "
				"firmware header: %d != %d, "
				"aborting fw upload\n",
				(int)fw_entry->size, (int)*size);
			goto release_ret;
		}
		res = vmalloc(*size);
		if (!res) {
			pr_info("no memory for firmware "
			       "(%u bytes)\n", *size);
			goto release_ret;
		}
		memcpy(res, fw_entry->data, fw_entry->size);
	      release_ret:
		release_firmware(fw_entry);
		return res;
	}
	pr_info("firmware image '%s' was not provided. "
	       "Check your hotplug scripts\n", file);

	/* checksum will be verified in write_fw, so don't bother here */
	return res;
}

/*
 * Common function to parse ALL configoption struct formats
 * (ACX100 and ACX111; FIXME: how to make it work with ACX100 USB!?!?).
 *
 * FIXME: logging should be removed here and added to a /proc file instead
 */
void acx_parse_configoption(acx_device_t *adev,
			 const acx111_ie_configoption_t *pcfg)
{
	const u8 *pEle;
	struct eeprom_cfg *acfg = &adev->cfgopt;
	int i;
	int is_acx111 = IS_ACX111(adev);

	if (acx_debug & L_DEBUG) {
		pr_info("configoption struct content:\n");
		acx_dump_bytes(pcfg, sizeof(*pcfg));
	}

	if ((is_acx111 && (adev->eeprom_version == 5))
	    || (!is_acx111 && (adev->eeprom_version == 4))
	    || (!is_acx111 && (adev->eeprom_version == 5))) {
		/* these versions are known to be supported */
	} else {
		pr_info("unknown chip and EEPROM version combination (%s, v%d), "
		       "don't know how to parse config options yet. "
		       "Please report\n", is_acx111 ? "ACX111" : "ACX100",
		       adev->eeprom_version);
		return;
	}

	/* first custom-parse the first part which has chip-specific layout */

	pEle = (const u8 *)pcfg;

	pEle += 4;		/* skip (type,len) header */

	memcpy(acfg->NVSv, pEle, sizeof(acfg->NVSv));
	pEle += sizeof(acfg->NVSv);

	pr_info("NVSv: ");
	for (i = 0; i < sizeof(acfg->NVSv); i++) {
		printk("%02X ", acfg->NVSv[i]);
	}
	printk("\n");

	if (is_acx111) {
		acfg->NVS_vendor_offs = le16_to_cpu(*(u16 *) pEle);
		pEle += sizeof(acfg->NVS_vendor_offs);

		acfg->probe_delay = 200;	/* good default value? */
		pEle += 2;	/* FIXME: unknown, value 0x0001 */
	} else {
		memcpy(acfg->MAC, pEle, sizeof(acfg->MAC));
		pEle += sizeof(acfg->MAC);

		acfg->probe_delay = le16_to_cpu(*(u16 *) pEle);
		pEle += sizeof(acfg->probe_delay);
		if ((acfg->probe_delay < 100)
		    || (acfg->probe_delay > 500)) {
			pr_info("strange probe_delay value %d, "
			       "tweaking to 200\n", acfg->probe_delay);
			acfg->probe_delay = 200;
		}
	}

	acfg->eof_memory = le32_to_cpu(*(u32 *) pEle);
	pEle += sizeof(acfg->eof_memory);

	pr_info("NVS_vendor_offs:%04X probe_delay:%d eof_memory:%d\n",
	       acfg->NVS_vendor_offs,
	       acfg->probe_delay, acfg->eof_memory);

	acfg->dot11CCAModes = *pEle++;
	acfg->dot11Diversity = *pEle++;
	acfg->dot11ShortPreambleOption = *pEle++;
	acfg->dot11PBCCOption = *pEle++;
	acfg->dot11ChannelAgility = *pEle++;
	acfg->dot11PhyType = *pEle++;
	acfg->dot11TempType = *pEle++;
	pr_info("CCAModes:%02X Diversity:%02X ShortPreOpt:%02X "
	       "PBCC:%02X ChanAgil:%02X PHY:%02X Temp:%02X\n",
	       acfg->dot11CCAModes,
	       acfg->dot11Diversity,
	       acfg->dot11ShortPreambleOption,
	       acfg->dot11PBCCOption,
	       acfg->dot11ChannelAgility,
	       acfg->dot11PhyType, acfg->dot11TempType);

	/* then use common parsing for next part which has common layout */

	pEle++;			/* skip table_count (6) */

	if (IS_MEM(adev) && IS_ACX100(adev)) {
		/*
		 * For iPaq hx4700 Generic Slave F/W 1.10.7.K.  I'm
		 * not sure if these 4 extra bytes are before the
		 * dot11 things above or after, so I'm just going to
		 * guess after.  If someone sees these aren't
		 * reasonable numbers, please fix this.
		 * The area from which the dot11 values above are read
		 * contains: 04 01 01 01 00 05 01 06 00 02 01 02 the 8
		 * dot11 reads above take care of 8 of them, but which
		 * 8...
		 */
		pEle += 4;
	}

	acfg->antennas.type = pEle[0];
	acfg->antennas.len = pEle[1];
	pr_info("AntennaID:%02X Len:%02X Data:",
	       acfg->antennas.type, acfg->antennas.len);
	for (i = 0; i < pEle[1]; i++) {
		acfg->antennas.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += pEle[1] + 2;
	acfg->power_levels.type = pEle[0];
	acfg->power_levels.len = pEle[1];
	pr_info("PowerLevelID:%02X Len:%02X Data:",
	       acfg->power_levels.type, acfg->power_levels.len);
	for (i = 0; i < pEle[1]; i++) {
		acfg->power_levels.list[i] =
		    le16_to_cpu(*(u16 *) & pEle[i * 2 + 2]);
		printk("%04X ", acfg->power_levels.list[i]);
	}
	printk("\n");

	pEle += pEle[1] * 2 + 2;
	acfg->data_rates.type = pEle[0];
	acfg->data_rates.len = pEle[1];
	pr_info("DataRatesID:%02X Len:%02X Data:",
	       acfg->data_rates.type, acfg->data_rates.len);
	for (i = 0; i < pEle[1]; i++) {
		acfg->data_rates.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += pEle[1] + 2;
	acfg->domains.type = pEle[0];
	acfg->domains.len = pEle[1];

	if (IS_MEM(adev) && IS_ACX100(adev)) {
		/*
		 * For iPaq hx4700 Generic Slave F/W 1.10.7.K.
		 * There's an extra byte between this structure and
		 * the next that is not accounted for with this
		 * structure's length.  It's most likely a bug in the
		 * firmware, but we can fix it here by bumping the
		 * length of this field by 1.
		 */
		acfg->domains.len++;
	}

	pr_info("DomainID:%02X Len:%02X Data:",
	       acfg->domains.type, acfg->domains.len);
	for (i = 0; i < acfg->domains.len; i++) {
		acfg->domains.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += acfg->domains.len + 2;
	acfg->product_id.type = pEle[0];
	acfg->product_id.len = pEle[1];
	for (i = 0; i < pEle[1]; i++)
		acfg->product_id.list[i] = pEle[i + 2];

	pr_info("ProductID:%02X Len:%02X Data:%.*s\n",
	       acfg->product_id.type, acfg->product_id.len,
	       acfg->product_id.len,
	       (char *)acfg->product_id.list);

	pEle += pEle[1] + 2;
	acfg->manufacturer.type = pEle[0];
	acfg->manufacturer.len = pEle[1];
	for (i = 0; i < pEle[1]; i++)
		acfg->manufacturer.list[i] = pEle[i + 2];

	pr_info("ManufacturerID:%02X Len:%02X Data:%.*s\n",
	       acfg->manufacturer.type, acfg->manufacturer.len,
	       acfg->manufacturer.len,
	       (char *)acfg->manufacturer.list);
	/*
	pr_info("EEPROM part:\n");
	for (i = 0; i < 58; i++) {
		printk("%02X =======>  0x%02X\n",
			i, (u8 *)acfg->NVSv[i-2]);
	}
	*/
}

int acx_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf)
{
	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_read_phy_reg(adev, reg, charbuf);
	if (IS_USB(adev))
		return acxusb_read_phy_reg(adev, reg, charbuf);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);

	return (NOT_OK);
}

int acx_write_phy_reg(acx_device_t *adev, u32 reg, u8 value)
{
	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_write_phy_reg(adev, reg, value);
	if (IS_USB(adev))
		return acxusb_write_phy_reg(adev, reg, value);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);

	return (NOT_OK);
}

/*
 * BOM CMDs (Control Path)
 * ==================================================
 */



void acx_start(acx_device_t *adev)
{
	FN_ENTER;

	log(L_INIT, "Updating initial settings\n");

	acx1xx_update_station_id(adev);

	acx1xx_update_rate_fallback(adev);
	acx1xx_update_tx_level(adev);
	acx1xx_update_antenna(adev);

	acx1xx_update_ed_threshold(adev);
	acx1xx_update_cca(adev);

	acx1xx_update_tx(adev);
	acx1xx_update_rx(adev);

	acx1xx_update_retry(adev);
	acx1xx_update_msdu_lifetime(adev);
	acx_update_reg_domain(adev);

	acx_update_mode(adev);

	/* For the acx100, we leave the firmware sensitivity and it
	   doesn't support auto recalib, so don't set it */
	if (IS_ACX111(adev)) {
		acx_update_sensitivity(adev);
		acx111_set_recalib_auto(adev, 1);
	}

	FN_EXIT0;
}


int acx_net_reset(struct ieee80211_hw *ieee)
{
	acx_device_t *adev = ieee2adev(ieee);
	FN_ENTER;
	if (IS_PCI(adev) || IS_MEM(adev))
		acx_reset_dev(adev);
	else
		TODO();

	FN_EXIT0;
	return 0;
}


int acx_setup_modes(acx_device_t *adev)
{
	int i;

	FN_ENTER;

	for (i=0; i<ARRAY_SIZE(channels); i++)
		channels[i].max_power = TX_CFG_MAX_DBM_POWER;

	if (IS_ACX100(adev)) {
		adev->ieee->wiphy->bands[IEEE80211_BAND_2GHZ] =
			&acx100_band_2GHz;
	} else {
		if (IS_ACX111(adev))
			adev->ieee->wiphy->bands[IEEE80211_BAND_2GHZ] =
				&acx111_band_2GHz;
		else {
			logf0(L_ANY, "Error: Unknown device");
			return -1;
		}
	}
	FN_EXIT0;
	return 0;
}



static int acx_recalib_radio(acx_device_t *adev)
{
	if (IS_ACX100(adev)) {
		logf0(L_INIT, "acx100: Doing radio re-calibration.\n");
		/* On ACX100, we need to recalibrate the radio
		 * by issuing a GETSET_TX|GETSET_RX */

		/* (OK == acx_s_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX,
		   NULL, 0)) && (OK == acx_s_issue_cmd(adev,
		   ACX1xx_CMD_DISABLE_RX, NULL, 0)) && */
		if ((acx_issue_cmd(adev, ACX1xx_CMD_ENABLE_TX, &adev->channel,
						1) == OK)
			&& (acx_issue_cmd(adev, ACX1xx_CMD_ENABLE_RX,
						&adev->channel, 1) == OK))
			return OK;

		return NOT_OK;
	} else {
		logf0(L_INIT, "acx111: Enabling auto radio re-calibration.\n");
		return(acx111_set_recalib_auto(adev, 1));
	}

}

static void acx_after_interrupt_recalib(acx_device_t *adev)
{
	int res;

	/* this helps with ACX100 at least; hopefully ACX111 also does
	 * a recalibration here */

	/* clear flag beforehand, since we want to make sure it's
	 * cleared; then only set it again on specific
	 * circumstances */
	CLEAR_BIT(adev->after_interrupt_jobs, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);

	/* better wait a bit between recalibrations to prevent
	 * overheating due to torturing the card into working too long
	 * despite high temperature (just a safety measure) */
	if (adev->recalib_time_last_success
	    && time_before(jiffies, adev->recalib_time_last_success
			   + RECALIB_PAUSE * 60 * HZ)) {
		if (adev->recalib_msg_ratelimit <= 4) {
			logf1(L_ANY, "%s: less than " STRING(RECALIB_PAUSE)
			       " minutes since last radio recalibration, "
			       "not recalibrating (maybe the card is too hot?)\n",
			       wiphy_name(adev->ieee->wiphy));
			adev->recalib_msg_ratelimit++;
			if (adev->recalib_msg_ratelimit == 5)
				logf0(L_ANY, "disabling the above message until next recalib\n");
		}
		return;
	}

	adev->recalib_msg_ratelimit = 0;

	/* note that commands sometimes fail (card busy), so only
	 * clear flag if we were fully successful */
	res = acx_recalib_radio(adev);
	if (res == OK) {
		pr_info("%s: successfully recalibrated radio\n",
		       wiphy_name(adev->ieee->wiphy));
		adev->recalib_time_last_success = jiffies;
		adev->recalib_failure_count = 0;
	} else {
		/* failed: resubmit, but only limited amount of times
		 * within some time range to prevent endless loop */

		adev->recalib_time_last_success = 0;	/* we failed */

		/* if some time passed between last attempts, then
		 * reset failure retry counter to be able to do next
		 * recalib attempt */
		if (time_after
		    (jiffies, adev->recalib_time_last_attempt + 5 * HZ))
			adev->recalib_failure_count = 0;

		if (adev->recalib_failure_count < 5) {
			/* increment inside only, for speedup of
			 * outside path */
			adev->recalib_failure_count++;
			adev->recalib_time_last_attempt = jiffies;
			acx_schedule_task(adev,
					  ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
		}
	}
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

/* TODO Verify these functions: translation rxbuffer.phy_plcp_signal to rate_idx */
#if 0

/** Rate values **/
#define ACX_CCK_RATE_1MB            0
#define ACX_CCK_RATE_2MB            1
#define ACX_CCK_RATE_5MB            2
#define ACX_CCK_RATE_11MB           3
#define ACX_OFDM_RATE_6MB           4
#define ACX_OFDM_RATE_9MB           5
#define ACX_OFDM_RATE_12MB          6
#define ACX_OFDM_RATE_18MB          7
#define ACX_OFDM_RATE_24MB          8
#define ACX_OFDM_RATE_36MB          9
#define ACX_OFDM_RATE_48MB          10
#define ACX_OFDM_RATE_54MB          11

static u8 acx_plcp_get_bitrate_cck(u8 plcp)
{
        switch (plcp) {
        case 0x0A:
                return ACX_CCK_RATE_1MB;
        case 0x14:
                return ACX_CCK_RATE_2MB;
        case 0x37:
                return ACX_CCK_RATE_5MB;
        case 0x6E:
                return ACX_CCK_RATE_11MB;
        }
        return 0;
}

/* Extract the bitrate out of an OFDM PLCP header. */
static u8 acx_plcp_get_bitrate_ofdm(u8 plcp)
{
        switch (plcp & 0xF) {
        case 0xB:
                return ACX_OFDM_RATE_6MB;
        case 0xF:
                return ACX_OFDM_RATE_9MB;
        case 0xA:
                return ACX_OFDM_RATE_12MB;
        case 0xE:
                return ACX_OFDM_RATE_18MB;
        case 0x9:
                return ACX_OFDM_RATE_24MB;
        case 0xD:
                return ACX_OFDM_RATE_36MB;
        case 0x8:
                return ACX_OFDM_RATE_48MB;
        case 0xC:
                return ACX_OFDM_RATE_54MB;
        }
        return 0;
}
#endif


/* ##################################################
 * Proc, Debug:
 *
 * File read/write handlers for both procfs, debugfs.  Procfs is
 * deprecated for new files, so proc-files are disabled by default;
 * ACX_WANT_PROC_FILES_ANYWAY enables them.  Debugfs is enabled, it
 * can be disabled by ACX_NO_DEBUG_FILES.
 */

#if (defined CONFIG_PROC_FS  &&  defined ACX_WANT_PROC_FILES_ANYWAY) \
 || (defined CONFIG_DEBUG_FS && !defined ACX_NO_DEBUG_FILES)

static int acx_proc_show_diag(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	ssize_t len = 0, partlen;
	u32 temp1, temp2;
	u8 *st, *st_end;
#ifdef __BIG_ENDIAN
	u8 *st2;
#endif
	fw_stats_t *fw_stats;
	char *part_str = NULL;
	fw_stats_tx_t *tx = NULL;
	fw_stats_rx_t *rx = NULL;
	fw_stats_dma_t *dma = NULL;
	fw_stats_irq_t *irq = NULL;
	fw_stats_wep_t *wep = NULL;
	fw_stats_pwr_t *pwr = NULL;
	fw_stats_mic_t *mic = NULL;
	fw_stats_aes_t *aes = NULL;
	fw_stats_event_t *evt = NULL;

	FN_ENTER;
	acx_sem_lock(adev);

	if (IS_PCI(adev))
		acxpci_proc_diag_output(file, adev);
	else if (IS_MEM(adev))
		acxmem_proc_diag_output(file, adev);

	seq_printf(file,
		     "\n"
		     "** network status **\n"
		     "dev_state_mask 0x%04X\n"
		     "mode %u, channel %u, "
		     "reg_dom_id 0x%02X, reg_dom_chanmask 0x%04X, ",
		     adev->dev_state_mask,
		     adev->mode, adev->channel,
		     adev->reg_dom_id, adev->reg_dom_chanmask);

	seq_printf(file,
		     "ESSID \"%s\", essid_active %d, essid_len %d, "
		     "essid_for_assoc \"%s\", nick \"%s\"\n"
		     "WEP ena %d, restricted %d, idx %d\n",
		     adev->essid, adev->essid_active, (int)adev->essid_len,
		     adev->essid_for_assoc, adev->nick,
		     adev->wep_enabled, adev->wep_restricted,
		     adev->wep_current_index);
	seq_printf(file, "dev_addr  " MACSTR "\n", MAC(adev->dev_addr));
	seq_printf(file, "bssid     " MACSTR "\n", MAC(adev->bssid));
	seq_printf(file, "ap_filter " MACSTR "\n", MAC(adev->ap));

	seq_printf(file, "tx_queue len: %d\n", skb_queue_len(&adev->tx_queue));

	seq_printf(file, "\n" "** PHY status **\n"
		"tx_enabled %d, tx_level_dbm %d, tx_level_val %d,\n "
		/* "tx_level_auto %d\n" */
		"sensitivity %d, antenna[0,1] 0x%02X 0x%02X, ed_threshold %d, cca %d, preamble_mode %d\n"
		"rate_basic 0x%04X, rate_oper 0x%04X\n"
		"rts_threshold %d, frag_threshold %d, short_retry %d, long_retry %d\n"
		"msdu_lifetime %d, listen_interval %d, beacon_interval %d\n",
		adev->tx_enabled, adev->tx_level_dbm, adev->tx_level_val,
		/* adev->tx_level_auto, */
		adev->sensitivity, adev->antenna[0], adev->antenna[1],
		adev->ed_threshold,
		adev->cca, adev->preamble_mode, adev->rate_basic,
		adev->rate_oper, adev->rts_threshold,
		adev->frag_threshold, adev->short_retry, adev->long_retry,
		adev->msdu_lifetime, adev->listen_interval,
		adev->beacon_interval);

	seq_printf(file,
		"\n"
		"** Firmware **\n"
		"NOTE: version dependent statistics layout, "
		"please report if you suspect wrong parsing!\n"
		"\n" "version \"%s\"\n", adev->firmware_version);

	fw_stats = kzalloc(sizeof(*fw_stats), GFP_KERNEL);
	if (!fw_stats) {
		FN_EXIT1(0);
		return 0;
	}
	st = (u8 *) fw_stats;

	part_str = "statistics query command";

	if (OK != acx_interrogate(adev, st, ACX1xx_IE_FIRMWARE_STATISTICS))
		goto fw_stats_end;

	st += sizeof(u16);
	len = *(u16 *) st;

	if (len > sizeof(*fw_stats)) {
		seq_printf(file,
			"firmware version with bigger fw_stats struct detected\n"
			"(%zu vs. %zu), please report\n", len,
			sizeof(fw_stats_t));
		if (len > sizeof(*fw_stats)) {
			seq_printf(file, "struct size exceeded allocation!\n");
			len = sizeof(*fw_stats);
		}
	}
	st += sizeof(u16);
	st_end = st - 2 * sizeof(u16) + len;

#ifdef __BIG_ENDIAN
	/* let's make one bold assumption here:
	 * (hopefully!) *all* statistics fields are u32 only,
	 * thus if we need to make endianness corrections
	 * we can simply do them in one go, in advance */
	st2 = (u8 *) fw_stats;
	for (temp1 = 0; temp1 < len; temp1 += 4, st2 += 4)
		*(u32 *) st2 = le32_to_cpu(*(u32 *) st2);
#endif

	part_str = "Rx/Tx";

	/* directly at end of a struct part? --> no error! */
	if (st == st_end)
		goto fw_stats_end;

	tx = (fw_stats_tx_t *) st;
	st += sizeof(fw_stats_tx_t);
	rx = (fw_stats_rx_t *) st;
	st += sizeof(fw_stats_rx_t);
	partlen = sizeof(fw_stats_tx_t) + sizeof(fw_stats_rx_t);

	if (IS_ACX100(adev)) {
		/* at least ACX100 PCI F/W 1.9.8.b
		 * and ACX100 USB F/W 1.0.7-USB
		 * don't have those two fields... */
		st -= 2 * sizeof(u32);

		/* our parsing doesn't quite match this firmware yet,
		 * log failure */
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = temp2 = 999999999;
	} else {
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = rx->rx_aci_events;
		temp2 = rx->rx_aci_resets;
	}

	seq_printf(file,
		"%s:\n"
		"  tx_desc_overfl %u\n"
		"  rx_OutOfMem %u, rx_hdr_overfl %u, rx_hw_stuck %u\n"
		"  rx_dropped_frame %u, rx_frame_ptr_err %u, rx_xfr_hint_trig %u\n"
		"  rx_aci_events %u, rx_aci_resets %u\n",
		part_str,
		tx->tx_desc_of,
		rx->rx_oom,
		rx->rx_hdr_of,
		rx->rx_hw_stuck,
		rx->rx_dropped_frame,
		rx->rx_frame_ptr_err, rx->rx_xfr_hint_trig, temp1, temp2);

	part_str = "DMA";

	if (st == st_end)
		goto fw_stats_end;

	dma = (fw_stats_dma_t *) st;
	partlen = sizeof(fw_stats_dma_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  rx_dma_req %u, rx_dma_err %u, tx_dma_req %u, tx_dma_err %u\n",
		part_str,
		dma->rx_dma_req,
		dma->rx_dma_err, dma->tx_dma_req, dma->tx_dma_err);

	part_str = "IRQ";

	if (st == st_end)
		goto fw_stats_end;

	irq = (fw_stats_irq_t *) st;
	partlen = sizeof(fw_stats_irq_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  cmd_cplt %u, fiq %u\n"
		"  rx_hdrs %u, rx_cmplt %u, rx_mem_overfl %u, rx_rdys %u\n"
		"  irqs %u, tx_procs %u, decrypt_done %u\n"
		"  dma_0_done %u, dma_1_done %u, tx_exch_complet %u\n"
		"  commands %u, rx_procs %u, hw_pm_mode_changes %u\n"
		"  host_acks %u, pci_pm %u, acm_wakeups %u\n",
		part_str,
		irq->cmd_cplt,
		irq->fiq,
		irq->rx_hdrs,
		irq->rx_cmplt,
		irq->rx_mem_of,
		irq->rx_rdys,
		irq->irqs,
		irq->tx_procs,
		irq->decrypt_done,
		irq->dma_0_done,
		irq->dma_1_done,
		irq->tx_exch_complet,
		irq->commands,
		irq->rx_procs,
		irq->hw_pm_mode_changes,
		irq->host_acks, irq->pci_pm, irq->acm_wakeups);

	part_str = "WEP";

	if (st == st_end)
		goto fw_stats_end;

	wep = (fw_stats_wep_t *) st;
	partlen = sizeof(fw_stats_wep_t);
	st += partlen;

	if (IS_ACX100(adev)) {
		/* at least ACX100 PCI F/W 1.9.8.b
		 * and ACX100 USB F/W 1.0.7-USB
		 * don't have those two fields... */
		st -= 2 * sizeof(u32);
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = temp2 = 999999999;
	} else {
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = wep->wep_pkt_decrypt;
		temp2 = wep->wep_decrypt_irqs;
	}

	seq_printf(file,
		"%s:\n"
		"  wep_key_count %u, wep_default_key_count %u, dot11_def_key_mib %u\n"
		"  wep_key_not_found %u, wep_decrypt_fail %u\n"
		"  wep_pkt_decrypt %u, wep_decrypt_irqs %u\n",
		part_str,
		wep->wep_key_count,
		wep->wep_default_key_count,
		wep->dot11_def_key_mib,
		wep->wep_key_not_found,
		wep->wep_decrypt_fail, temp1, temp2);

	part_str = "power";

	if (st == st_end)
		goto fw_stats_end;

	pwr = (fw_stats_pwr_t *) st;
	partlen = sizeof(fw_stats_pwr_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  tx_start_ctr %u, no_ps_tx_too_short %u\n"
		"  rx_start_ctr %u, no_ps_rx_too_short %u\n"
		"  lppd_started %u\n"
		"  no_lppd_too_noisy %u, no_lppd_too_short %u, no_lppd_matching_frame %u\n",
		part_str,
		pwr->tx_start_ctr,
		pwr->no_ps_tx_too_short,
		pwr->rx_start_ctr,
		pwr->no_ps_rx_too_short,
		pwr->lppd_started,
		pwr->no_lppd_too_noisy,
		pwr->no_lppd_too_short, pwr->no_lppd_matching_frame);

	part_str = "MIC";

	if (st == st_end)
		goto fw_stats_end;

	mic = (fw_stats_mic_t *) st;
	partlen = sizeof(fw_stats_mic_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  mic_rx_pkts %u, mic_calc_fail %u\n",
		part_str, mic->mic_rx_pkts, mic->mic_calc_fail);

	part_str = "AES";

	if (st == st_end)
		goto fw_stats_end;

	aes = (fw_stats_aes_t *) st;
	partlen = sizeof(fw_stats_aes_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  aes_enc_fail %u, aes_dec_fail %u\n"
		"  aes_enc_pkts %u, aes_dec_pkts %u\n"
		"  aes_enc_irq %u, aes_dec_irq %u\n",
		part_str,
		aes->aes_enc_fail,
		aes->aes_dec_fail,
		aes->aes_enc_pkts,
		aes->aes_dec_pkts, aes->aes_enc_irq, aes->aes_dec_irq);

	part_str = "event";

	if (st == st_end)
		goto fw_stats_end;

	evt = (fw_stats_event_t *) st;
	partlen = sizeof(fw_stats_event_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		"%s:\n"
		"  heartbeat %u, calibration %u\n"
		"  rx_mismatch %u, rx_mem_empty %u, rx_pool %u\n"
		"  oom_late %u\n"
		"  phy_tx_err %u, tx_stuck %u\n",
		part_str,
		evt->heartbeat,
		evt->calibration,
		evt->rx_mismatch,
		evt->rx_mem_empty,
		evt->rx_pool,
		evt->oom_late, evt->phy_tx_err, evt->tx_stuck);

	if (st < st_end)
		goto fw_stats_bigger;

	goto fw_stats_end;

	fw_stats_fail:
	st -= partlen;
	seq_printf(file,
		"failed at %s part (size %zu), offset %zu (struct size %zu), "
		"please report\n", part_str, partlen,
		((void *) st - (void *) fw_stats), len);

	fw_stats_bigger:
	for (; st < st_end; st += 4)
		seq_printf(file, "UNKN%3d: %u\n", (int) ((void *) st
				- (void *) fw_stats), *(u32 *) st);

	fw_stats_end:
	kfree(fw_stats);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

/*
 * A write on acx_diag executes different operations for debugging
 */
static ssize_t acx_proc_write_diag(struct file *file, 
				const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	struct proc_dir_entry *pde = PDE(file->f_path.dentry->d_inode);
	acx_device_t *adev = (acx_device_t *) pde->data;

	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned int val;
	size_t size, len;

	FN_ENTER;
	acx_sem_lock(adev);

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = (unsigned int) simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count == size)
		ret = count;
	else
		goto exit_unlock;

	logf1(L_ANY, "acx_diag: 0x%04x\n", val);

	/* Execute operation */
	if (val == ACX_DIAG_OP_RECALIB) {
		logf0(L_ANY, "ACX_DIAG_OP_RECALIB: Scheduling immediate radio recalib\n");
		adev->recalib_time_last_success = jiffies - RECALIB_PAUSE * 60 * HZ;
		acx_schedule_task(adev, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
	} else
	/* Execute operation */
	if (val & ACX_DIAG_OP_PROCESS_TX_RX) {
		logf0(L_ANY, "ACX_DIAG_OP_PROCESS_TX_RX: Scheduling immediate Rx, Tx processing\n");

		if (IS_PCI(adev))
			SET_BIT(adev->irq_reason, HOST_INT_RX_COMPLETE);
		else if (IS_MEM(adev))
			SET_BIT(adev->irq_reason, HOST_INT_RX_DATA);

		SET_BIT(adev->irq_reason, HOST_INT_TX_COMPLETE);
		acx_schedule_task(adev, 0);
	} else
	/* Execute operation */
	if (val & ACX_DIAG_OP_REINIT_TX_BUF) {
		if (IS_MEM(adev)) {
			logf0(L_ANY, "ACX_DIAG_OP_REINIT_TX_BUF\n");
			acxmem_init_acx_txbuf2(adev);
		} else
			logf0(L_ANY, "ACX_DIAG_OP_REINIT_TX_BUF: Only valid for mem device\n");
	}
	/* Unknown */
	else
		logf1(L_ANY, "Unknown command: 0x%04x\n", val);

exit_unlock:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return ret;
}

/*
 * acx_e_read_proc_XXXX
 * Handle our /proc entry
 *
 * Arguments:
 *	standard kernel read_proc interface
 * Returns:
 *	number of bytes written to buf
 * Side effects:
 *	none
 */
static int acx_proc_show_acx(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t*) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	seq_printf(file,
		"acx driver version:\t\t%s (git: %s)\n"
		"Wireless extension version:\t%s\n"
		"chip name:\t\t\t%s (0x%08X)\n"
		"radio type:\t\t\t0x%02X\n"
		"form factor:\t\t\t0x%02X\n"
		     "EEPROM version:\t\t\t0x%02X\n"
		"firmware version:\t\t%s (0x%08X)\n",
		ACX_RELEASE,
		strlen(ACX_GIT_VERSION) ? ACX_GIT_VERSION : "unknown",
		STRING(WIRELESS_EXT),
		adev->chip_name, adev->firmware_id,
		adev->radio_type,
		adev->form_factor,
		adev->eeprom_version,
		adev->firmware_version, adev->firmware_numver);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static int acx_proc_show_eeprom(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	int length;
	char *buf, *p;

	FN_ENTER;
	acx_sem_lock(adev);

	if (IS_PCI(adev) || IS_MEM(adev))
		buf = acx_proc_eeprom_output(&length, adev);
	else
		goto out;

	for (p = buf; p < buf + length; p++)
	     seq_putc(file, *p);

	kfree(buf);
out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static int acx_proc_show_phy(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	int i;
	char *buf, *p;
	/* OW Hopefully enough */
	const int buf_size = 1024*64;

	FN_ENTER;
	acx_sem_lock(adev);

	buf = kmalloc(buf_size, GFP_KERNEL);
	/*
	   if (RADIO_11_RFMD != adev->radio_type) {
	   pr_info("sorry, not yet adapted for radio types "
	   "other than RFMD, please verify "
	   "PHY size etc. first!\n");
	   goto end;
	   }
	 */

	/* The PHY area is only 0x80 bytes long; further pages after that
	 * only have some page number registers with altered value,
	 * all other registers remain the same. */
	p = buf;
	for (i = 0; i < 0x80; i++) {
		acx_read_phy_reg(adev, i, p++);
		seq_putc(file, *p);
	}

	kfree(buf);

	acx_sem_unlock(adev);
	FN_EXIT0;

	return 0;
}

static int acx_proc_show_debug(struct seq_file *file, void *v)
{
	FN_ENTER;
	/* No sem locking required, since debug is global for all devices */

	seq_printf(file, "acx_debug: 0x%04x\n", acx_debug);

	FN_EXIT0;
	return 0;
}

static ssize_t acx_proc_write_debug(struct file *file,
				const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned long val;
	size_t size, len;

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	FN_ENTER;
	/* No sem locking required, since debug is global for all devices */

	if (count == size) {
		ret = count;
		acx_debug = val;
	}

	log(L_ANY, "acx_debug=0x%04x\n", acx_debug);

	FN_EXIT0;
	return ret;
}

static int acx_proc_show_sensitivity(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	acx_get_sensitivity(adev);
	seq_printf(file, "acx_sensitivity: %d\n", adev->sensitivity);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static ssize_t acx_proc_write_sensitivity(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)

{
	acx_device_t *adev = (acx_device_t *)
		PDE(file->f_path.dentry->d_inode)->data;

	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned long val;
	size_t size, len;

	FN_ENTER;
	acx_sem_lock(adev);

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	acx_set_sensitivity(adev, val);
	logf1(L_ANY, "acx_sensitivity=%d\n", adev->sensitivity);

out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return ret;
}

static int acx_proc_show_tx_level(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	acx1xx_get_tx_level(adev);
	seq_printf(file, "tx_level_dbm: %d\n", adev->tx_level_dbm);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static ssize_t acx111_proc_write_tx_level(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	acx_device_t *adev = (acx_device_t *)
		PDE(file->f_path.dentry->d_inode)->data;

	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned long val;
	size_t size, len;

	FN_ENTER;
	acx_sem_lock(adev);

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	logf1(L_ANY, "tx_level_val=%d\n", adev->tx_level_val);
	acx1xx_set_tx_level(adev, val);

out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return ret;
}

static int acx_proc_show_reg_domain(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	acx_get_reg_domain(adev);
	seq_printf(file, "reg_dom_id: 0x%02x\n", adev->reg_dom_id);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static ssize_t acx_proc_write_reg_domain(struct file *file,
					const char __user *ubuf,
					size_t count, loff_t *ppos)
{
	acx_device_t *adev = (acx_device_t *)
		PDE(file->f_path.dentry->d_inode)->data;

	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned long val;
	size_t size, len;

	FN_ENTER;
	acx_sem_lock(adev);

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	acx_set_reg_domain(adev, val);

out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return ret;
}

static int acx_proc_show_antenna(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	acx1xx_get_antenna(adev);
	seq_printf(file, "antenna[0,1]: 0x%02x 0x%02x\n",
		adev->antenna[0], adev->antenna[1]);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static ssize_t acx_proc_write_antenna(struct file *file,
				const char __user *ubuf,
				size_t count, loff_t *ppos)
{
	acx_device_t *adev = (acx_device_t *)
		PDE(file->f_path.dentry->d_inode)->data;

	ssize_t ret = -EINVAL;
	char *after, buf[32];
	unsigned long val;
	u8 val0, val1;
	size_t size, len;

	FN_ENTER;
	acx_sem_lock(adev);

	len = min(count, sizeof(buf) - 1);
	if (unlikely(copy_from_user(buf, ubuf, len)))
		return -EFAULT;

	val = simple_strtoul(buf, &after, 0);
	size = after - buf + 1;

	if (count != size)
		goto out;

	ret = count;

	val0 = (u8) (val & 0xFF);
	val1 = (u8) ((val >> 8) & 0xFF);
	acx1xx_set_antenna(adev, val0, val1);

out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return ret;
}

acx_proc_show_t *const acx_proc_show_funcs[] = {
	acx_proc_show_acx,
	acx_proc_show_diag,
	acx_proc_show_eeprom,
	acx_proc_show_phy,
	acx_proc_show_debug,
	acx_proc_show_sensitivity,
	acx_proc_show_tx_level,
	acx_proc_show_antenna,
	acx_proc_show_reg_domain,
};

acx_proc_write_t *const acx_proc_write_funcs[] = {
	NULL,
	acx_proc_write_diag,
	NULL,
	NULL,
	acx_proc_write_debug,
	acx_proc_write_sensitivity,
	acx111_proc_write_tx_level,
	acx_proc_write_antenna,
	acx_proc_write_reg_domain,
};
BUILD_BUG_DECL(acx_proc_show_funcs__VS__acx_proc_write_funcs,
	ARRAY_SIZE(acx_proc_show_funcs) != ARRAY_SIZE(acx_proc_write_funcs));
	

#if (defined CONFIG_PROC_FS && defined ACX_WANT_PROC_FILES_ANYWAY)
/*
 * procfs has been explicitly enabled
 */
static const char *const proc_files[] = {
	"info", "diag", "eeprom", "phy", "debug",
	"sensitivity", "tx_level", "antenna", "reg_domain",
};
BUILD_BUG_DECL(acx_proc_show_funcs__VS__proc_files,
	ARRAY_SIZE(acx_proc_show_funcs) != ARRAY_SIZE(proc_files));

static struct file_operations acx_e_proc_ops[ARRAY_SIZE(proc_files)];

static int acx_proc_open(struct inode *inode, struct file *file)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
		if (!strcmp(proc_files[i],
			    file->f_path.dentry->d_name.name))
			break;
	}
	/* log(L_ANY, "proc filename=%s\n", proc_files[i]); */

	return single_open(file, acx_proc_show_funcs[i], PDE(inode)->data);
}

static void acx_proc_init(void)
{
	int i;

	/* acx_e_proc_ops init */
	for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
		acx_e_proc_ops[i].owner = THIS_MODULE;
		acx_e_proc_ops[i].open = acx_proc_open;
		acx_e_proc_ops[i].read = seq_read;
		acx_e_proc_ops[i].llseek = seq_lseek;
		acx_e_proc_ops[i].release = single_release;
		acx_e_proc_ops[i].write = acx_proc_write_funcs[i];
	}
}

int acx_proc_register_entries(struct ieee80211_hw *hw)
{
	acx_device_t *adev = ieee2adev(hw);
	char procbuf[80];
	char procbuf2[80];
	int i;
	struct proc_dir_entry *pe;

	FN_ENTER;

	/* Sub-dir for this acx_phy[0-9] instance */

	/* I tried to create a /proc/driver/acx sub-dir in acx_proc_init()
	 * to put the phy[0-9] into, but for some bizarre reason the proc-fs
	 * refuses then to create the phy[0-9] dirs in /proc/driver/acx !?
	 * It only works, if /proc/driver/acx is created here in
	 * acx_proc_register_entries().
	 * ... Anyway, we should swap to sysfs.
	 */
	snprintf(procbuf2, sizeof(procbuf2), "driver/acx_%s",
		wiphy_name(adev->ieee->wiphy));

	proc_mkdir(procbuf2, NULL);

	for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
		snprintf(procbuf, sizeof(procbuf), "%s/%s",
			procbuf2, proc_files[i]);
		log(L_INIT, "creating proc entry /proc/%s\n", procbuf);

		/* Read-only */
		if (acx_proc_write_funcs[i] == NULL)
			pe = proc_create(procbuf, 0444, NULL,
					&acx_e_proc_ops[i]);
		/* Read-Write */
		else
			pe = proc_create(procbuf, 0644, NULL,
					&acx_e_proc_ops[i]);

		if (!pe) {
			pr_info("cannot register proc entry /proc/%s\n",
				procbuf);
			return NOT_OK;
		}
		pe->data = adev;
	}
	FN_EXIT0;
	return OK;
}

int acx_proc_unregister_entries(struct ieee80211_hw *hw)
{
	acx_device_t *adev = ieee2adev(hw);
	char procbuf[80];
	char procbuf2[80];
	int i;

	FN_ENTER;

	/* Subdir for this acx instance */
	snprintf(procbuf2, sizeof(procbuf2), "driver/acx_%s",
		wiphy_name(adev->ieee->wiphy));

	for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
		snprintf(procbuf, sizeof(procbuf), "%s/%s", procbuf2,
			proc_files[i]);
		log(L_INIT, "removing proc entry /proc/%s\n", procbuf);
		remove_proc_entry(procbuf, NULL);
	}
	remove_proc_entry(procbuf2, NULL);

	FN_EXIT0;
	return OK;
}
#else
static inline void acx_proc_init(void) {}
#endif	/* ACX_WANT_PROC_FILES_ANYWAY */
#else
static inline void acx_proc_init(void) {}
#endif	/* (defined CONFIG_PROC_FS  &&  defined ACX_WANT_PROC_FILES_ANYWAY) \
	 || (defined CONFIG_DEBUG_FS && !defined ACX_NO_DEBUG_FILES) */

/* should have a real cleanup func */
static inline void acx_proc_exit(void) {}

/*
 * BOM Rx Path
 * ==================================================
 */

/*
 * acx_l_process_rxbuf
 *
 * NB: used by USB code also
 */
void acx_process_rxbuf(acx_device_t *adev, rxbuffer_t *rxbuf)
{
	struct ieee80211_hdr *hdr;
	u16 fc, buf_len;

	FN_ENTER;

	hdr = acx_get_wlan_hdr(adev, rxbuf);
	fc = le16_to_cpu(hdr->frame_control);
	/* length of frame from control field to first byte of FCS */
	buf_len = RXBUF_BYTES_RCVD(adev, rxbuf);

	/* For debugging */
	if (((IEEE80211_FCTL_STYPE & fc) != IEEE80211_STYPE_BEACON)
		&& (acx_debug & (L_XFER|L_DATA))) {

		printk_ratelimited(
			"acx: rx: %s time:%u len:%u signal:%u,raw=%u"
			"SNR:%u,raw=%u macstat:%02X "
			"phystat:%02X phyrate:%u status:%u\n",
			acx_get_packet_type_string(fc),
			le32_to_cpu(rxbuf->time), buf_len,
			acx_signal_to_winlevel(rxbuf->phy_level),
			rxbuf->phy_level,
			acx_signal_to_winlevel(rxbuf->phy_snr),
			rxbuf->phy_snr, rxbuf->mac_status,
			rxbuf->phy_stat_baseband,
			rxbuf->phy_plcp_signal,
			adev->status);
	}

	if (unlikely(acx_debug & L_DATA)) {
		pr_info("rx: 802.11 buf[%u]: \n", buf_len);
		acx_dump_bytes(hdr, buf_len);
	}

	acx_rx(adev, rxbuf);

	/* Now check Rx quality level, AFTER processing packet.  I
	 * tried to figure out how to map these levels to dBm values,
	 * but for the life of me I really didn't manage to get
	 * it. Either these values are not meant to be expressed in
	 * dBm, or it's some pretty complicated calculation. */

	/* FIXME OW 20100619 Is this still required. Only for adev local use.
	 * Mac80211 signal level is reported in acx_l_rx for each skb.
	 */
	/* TODO: only the RSSI seems to be reported */
	adev->rx_status.signal = acx_signal_to_winlevel(rxbuf->phy_level);

	FN_EXIT0;
}

/*
 * acx_l_rx
 *
 * The end of the Rx path. Pulls data from a rxhostdesc into a socket
 * buffer and feeds it to the network stack via netif_rx().
 */
static void acx_rx(acx_device_t *adev, rxbuffer_t *rxbuf)
{
	struct ieee80211_rx_status *status;

	struct ieee80211_hdr *w_hdr;
	struct sk_buff *skb;
	int buflen;
	int level;

	FN_ENTER;

	if (unlikely(!(adev->dev_state_mask & ACX_STATE_IFACE_UP))) {
		pr_info("asked to receive a packet but the interface is down??\n");
		goto out;
	}

	w_hdr = acx_get_wlan_hdr(adev, rxbuf);
	buflen = RXBUF_BYTES_RCVD(adev, rxbuf);

	/* Allocate our skb */
	skb = dev_alloc_skb(buflen);
	if (!skb) {
		pr_info("skb allocation FAILED\n");
		goto out;
	}

	skb_put(skb, buflen);
	memcpy(skb->data, w_hdr, buflen);

	status = IEEE80211_SKB_RXCB(skb);
	memset(status, 0, sizeof(*status));

	status->mactime = rxbuf->time;

	level = acx_signal_to_winlevel(rxbuf->phy_level);
	/* FIXME cleanup ?: noise = acx_signal_to_winlevel(rxbuf->phy_snr); */

	/* status->signal = acx_signal_determine_quality(level, noise);
	 * TODO OW 20100619 On ACX100 seem to be always zero (seen during hx4700 tests ?!)
	 */
	status->signal = level;

	if(adev->hw_encrypt_enabled)
		status->flag = RX_FLAG_DECRYPTED | RX_FLAG_IV_STRIPPED;

	status->freq = adev->rx_status.freq;
	status->band = adev->rx_status.band;

	status->antenna = 1;

	/* TODO I'm not sure whether this is (currently) really required. In tests
	 * this didn't made a difference. Maybe compare what other drivers do.
	 * TODO Verify translation to rate_idx.
	 */
#if 0
	if (rxbuf->phy_stat_baseband & (1 << 3)) /* Uses OFDM */
		status->rate_idx =
			acx_plcp_get_bitrate_ofdm(rxbuf->phy_plcp_signal);
	else
		status->rate_idx =
			acx_plcp_get_bitrate_cck(rxbuf->phy_plcp_signal);
#endif

	if (IS_PCI(adev)) {
#if CONFIG_ACX_MAC80211_VERSION <= KERNEL_VERSION(2, 6, 32)
		local_bh_disable();
		ieee80211_rx(adev->ieee, skb);
		local_bh_enable();
#else
		ieee80211_rx_ni(adev->ieee, skb);
#endif
	}
	/* Usb Rx is happening in_interupt() */
	else if (IS_USB(adev) || IS_MEM(adev))
		ieee80211_rx_irqsafe(adev->ieee, skb);
	else
		logf0(L_ANY, "ERROR: Undefined device type !?\n");

	adev->stats.rx_packets++;
	adev->stats.rx_bytes += skb->len;
out:
	FN_EXIT0;
}

/*
 * BOM Tx Path
 * ==================================================
 */

/* TODO: consider defining OP_TX_RET_TYPE, OP_TX_RET_VAL in
 * acx_compat, and hiding this #if/else.  OTOH, inclusion doesnt care
 * about old kernels
 */
OP_TX_RET_TYPE acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	acx_device_t *adev = ieee2adev(hw);

	skb_queue_tail(&adev->tx_queue, skb);

	ieee80211_queue_work(adev->ieee, &adev->tx_work);

	if (skb_queue_len(&adev->tx_queue) >= ACX_TX_QUEUE_MAX_LENGTH)
		acx_stop_queue(adev->ieee, NULL);

	return OP_TX_RET_OK;
}

void acx_tx_work(struct work_struct *work)
{
	acx_device_t *adev = container_of(work, struct acx_device, tx_work);

	FN_ENTER;

	acx_sem_lock(adev);

	if (unlikely(!adev))
		goto out;

	if (unlikely(!(adev->dev_state_mask & ACX_STATE_IFACE_UP)))
		goto out;

	if (unlikely(!adev->initialized))
		goto out;

	acx_tx_queue_go(adev);
out:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return;
}

static int acx_is_hw_tx_queue_stop_limit(acx_device_t *adev)
{
	int i;
	for (i=0; i<adev->num_hw_tx_queues; i++)
	{
		if (adev->hw_tx_queue[i].free < TX_STOP_QUEUE)
		{
			logf1(L_BUF, "Tx_free < TX_STOP_QUEUE (queue_id=%d: %u tx desc left):"
				" Stop queue.\n", i, adev->hw_tx_queue[i].free);
			return 1;
		}
	}

	return 0;
}

void acx_tx_queue_go(acx_device_t *adev)
{
	struct sk_buff *skb;
	int ret;

	while ((skb = skb_dequeue(&adev->tx_queue))) {

		ret = acx_tx_frame(adev, skb);

		if (ret == -EBUSY) {
			logf0(L_BUFT, "EBUSY: Stop queue. Requeuing skb.\n");
			acx_stop_queue(adev->ieee, NULL);
			skb_queue_head(&adev->tx_queue, skb);
			goto out;
		} else if (ret < 0) {
			logf0(L_BUF, "Other ERR: (Card was removed ?!):"
				" Stop queue. Dealloc skb.\n");
			acx_stop_queue(adev->ieee, NULL);
			dev_kfree_skb(skb);
			goto out;
		}

		/* Keep a few free descs between head and tail of tx
		 * ring. It is not absolutely needed, just feels
		 * safer */
		if (acx_is_hw_tx_queue_stop_limit(adev))
		{
			acx_stop_queue(adev->ieee, NULL);
			goto out;
		}
	}
out:
	return;
}

int acx_tx_frame(acx_device_t *adev, struct sk_buff *skb)
{
	tx_t *tx;
	void *txbuf;
	struct ieee80211_tx_info *ctl;
	struct ieee80211_hdr *hdr;

	/* Default queue_id for data-frames */
	int queue_id=1;

	ctl = IEEE80211_SKB_CB(skb);
	hdr = (struct ieee80211_hdr*) skb->data;

	/* Sent unencrypted frames (e.g. mgmt- and eapol-frames) on NOENC_QUEUE_ID */
	if (!(hdr->frame_control & IEEE80211_FCTL_PROTECTED))
		queue_id=NOENC_QUEUE_ID;

	/* With hw-encyption disabled, sent all on the NOENC queue.
	 * This is required, if the was previously used using hw-encyption:
	 * once a queue was used, if will not stop encryption, and so the
	 * current solution is to avoid the encrypting queues entirely. */
	if (!adev->hw_encrypt_enabled)
		queue_id=NOENC_QUEUE_ID;

	tx = acx_alloc_tx(adev, skb->len, queue_id);

	if (unlikely(!tx)) {
		logf0(L_BUFT, "No tx available\n");
		return (-EBUSY);
	}

	txbuf = acx_get_txbuf(adev, tx, queue_id);

	if (unlikely(!txbuf)) {
		/* Card was removed */
		logf0(L_BUF, "Txbuf==NULL. (Card was removed ?!):"
			" Stop queue. Dealloc skb.\n");

		/* OW only USB implemented */
		acx_dealloc_tx(adev, tx);
		return (-ENXIO);
	}

	/* FIXME: Is this required for mem ? txbuf is actually not containing to the data
	 * for the device, but actually "addr = acxmem_allocate_acx_txbuf_space in acxmem_tx_data().
	 */
	memcpy(txbuf, skb->data, skb->len);

	acx_tx_data(adev, tx, skb->len, ctl, skb, queue_id);

	adev->stats.tx_packets++;
	adev->stats.tx_bytes += skb->len;

	return 0;
}

void acx_tx_queue_flush(acx_device_t *adev)
{
	struct sk_buff *skb;
	struct ieee80211_tx_info *info;

	while ((skb = skb_dequeue(&adev->tx_queue))) {
		info = IEEE80211_SKB_CB(skb);

		logf1(L_BUF, "Flushing skb 0x%p", skb);

		if (!(info->flags & IEEE80211_TX_CTL_REQ_TX_STATUS))
			continue;

		ieee80211_tx_status(adev->ieee, skb);
	}
}

void acx_stop_queue(struct ieee80211_hw *hw, const char *msg)
{
	FN_ENTER;
	ieee80211_stop_queues(hw);
	if (msg)
		log(L_BUFT, "tx: stop queue %s\n", msg);
	FN_EXIT0;
}

int acx_queue_stopped(struct ieee80211_hw *ieee)
{
	return ieee80211_queue_stopped(ieee, 0);
}

void acx_wake_queue(struct ieee80211_hw *hw, const char *msg)
{
	FN_ENTER;
	ieee80211_wake_queues(hw);
	if (msg)
		log(L_BUFT, "tx: wake queue %s\n", msg);
	FN_EXIT0;
}

/*
 * OW Included skb->len to check required blocks upfront in
 * acx_l_alloc_tx This should perhaps also go into pci and usb ?
 */
tx_t* acx_alloc_tx(acx_device_t *adev, unsigned int len, int q)
{
	if (IS_PCI(adev))
		return acxpci_alloc_tx(adev, q);
	if (IS_USB(adev))
		return acxusb_alloc_tx(adev);
	if (IS_MEM(adev))
		return acxmem_alloc_tx(adev, len);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);
	return (NULL);
}

static void acx_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque)
{
	if (IS_USB(adev))
		acxusb_dealloc_tx(tx_opaque);
	if (IS_MEM(adev))
		acxmem_dealloc_tx (adev, tx_opaque);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);
	return;
}

static void* acx_get_txbuf(acx_device_t *adev, tx_t *tx_opaque, int q)
{
	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_get_txbuf(adev, tx_opaque, q);
	if (IS_USB(adev))
		return acxusb_get_txbuf(adev, tx_opaque);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);
	return (NULL);
}

static void acx_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
	struct ieee80211_tx_info *ieeectl, struct sk_buff *skb, int q)
{
	if (IS_PCI(adev))
		return _acx_tx_data(adev, tx_opaque, len, ieeectl, skb, q);
	if (IS_USB(adev))
		return acxusb_tx_data(adev, tx_opaque, len, ieeectl, skb);
	if (IS_MEM(adev))
		return _acx_tx_data(adev, tx_opaque, len, ieeectl, skb, q);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);

	return;
}

u16 acx111_tx_build_rateset(acx_device_t *adev, txdesc_t *txdesc,
			struct ieee80211_tx_info *info)
{
	int i;

	char tmpstr[256];
	struct ieee80211_rate *tmpbitrate;
	int tmpcount;

	u16 rateset = 0;

	int debug = acx_debug & L_BUFT;

	if (debug)
		sprintf(tmpstr, "rates in info [bitrate,hw_value,count]: ");

	for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
		if (info->control.rates[i].idx < 0)
			break;

		tmpbitrate = &adev->ieee->wiphy->bands[info->band]
			->bitrates[info->control.rates[i].idx];
		tmpcount = info->control.rates[i].count;

		rateset |= tmpbitrate->hw_value;

		if (debug)
			sprintf(tmpstr + strlen(tmpstr), "%i=[%i,0x%04X,%i]%s",
				i, tmpbitrate->bitrate, tmpbitrate->hw_value,
				tmpcount,
				(i < IEEE80211_TX_MAX_RATES - 1)
				? ", " : "");
	}
	if (debug)
		logf1(L_ANY, "%s: rateset=0x%04X\n", tmpstr, rateset);

	return (rateset);
}

void acx111_tx_build_txstatus(acx_device_t *adev,
			struct ieee80211_tx_info *txstatus, u16 r111,
			u8 ack_failures)
{
	u16 rate_hwvalue;
	u16 rate_bitrate;
	int rate_index;
	int j;

	rate_hwvalue = 1 << highest_bit(r111 & RATE111_ALL);
	rate_index = acx_rate111_hwvalue_to_rateindex(rate_hwvalue);

	for (j = 0; j < IEEE80211_TX_MAX_RATES; j++) {
		if (txstatus->status.rates[j].idx == rate_index) {
			txstatus->status.rates[j].count = ack_failures + 1;
			break;
		}
	}

	if ((acx_debug & L_BUFT) && (ack_failures > 0)) {

		rate_bitrate = acx_rate111_hwvalue_to_bitrate(rate_hwvalue);
		logf1(L_ANY,
			"sentrate(bitrate,hw_value)=(%d,0x%04X)"
			" status.rates[%d].count=%d\n",
			rate_bitrate, rate_hwvalue, j,
			(j < IEEE80211_TX_MAX_RATES)
			? txstatus->status.rates[j].count : -1);
	}
}

void acxpcimem_handle_tx_error(acx_device_t *adev, u8 error,
			unsigned int finger,
			struct ieee80211_tx_info *info)
{
	int log_level = L_INIT;

	const char *err = "unknown error";

	/* hmm, should we handle this as a mask
	 * of *several* bits?
	 * For now I think only caring about
	 * individual bits is ok... */
	switch (error) {
	case 0x01:
		err = "no Tx due to error in other fragment";
		/* adev->wstats.discard.fragment++; */
		break;
	case 0x02:
		err = "Tx aborted";
		adev->stats.tx_aborted_errors++;
		break;
	case 0x04:
		err = "Tx desc wrong parameters";
		/* adev->wstats.discard.misc++; */
		break;
	case 0x08:
		err = "WEP key not found";
		/* adev->wstats.discard.misc++; */
		break;
	case 0x10:
		err = "MSDU lifetime timeout? - try changing "
		    "'iwconfig retry lifetime XXX'";
		/* adev->wstats.discard.misc++; */
		break;

	case 0x20:
		err = "excessive Tx retries due to either distance "
		    "too high or unable to Tx or Tx frame error - "
		    "try changing 'iwconfig txpower XXX' or "
		    "'sens'itivity or 'retry'";
		log_level = acx_debug & L_DEBUG;
		/* adev->wstats.discard.retries++; */
		/* Tx error 0x20 also seems to occur on
		 * overheating, so I'm not sure whether we
		 * actually want to do aggressive radio recalibration,
		 * since people maybe won't notice then that their hardware
		 * is slowly getting cooked...
		 * Or is it still a safe long distance from utter
		 * radio non-functionality despite many radio recalibs
		 * to final destructive overheating of the hardware?
		 * In this case we really should do recalib here...
		 * I guess the only way to find out is to do a
		 * potentially fatal self-experiment :-\
		 * Or maybe only recalib in case we're using Tx
		 * rate auto (on errors switching to lower speed
		 * --> less heat?) or 802.11 power save mode?
		 *
		 * ok, just do it. */
		if ((++adev->retry_errors_msg_ratelimit % 4 == 0)) {

			if (adev->retry_errors_msg_ratelimit <= 20) {

				logf1(L_DEBUG, "%s: several excessive Tx "
					"retry errors occurred, attempting "
					"to recalibrate radio. Radio "
					"drift might be caused by increasing "
					"card temperature, please check the "
					"card before it's too late!\n",
					wiphy_name(adev->ieee->wiphy));

				if (adev->retry_errors_msg_ratelimit == 20)
					logf0(L_DEBUG,
						"Disabling above message\n");
			}

			/* On the acx111, we would normally have auto radio-recalibration enabled */
			if (!adev->recalib_auto){
				logf0(L_ANY, "Scheduling radio recalibration after high tx retries.\n");
				acx_schedule_task(adev,
					  ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
			}
		}
		break;
	case 0x40:
		err = "Tx buffer overflow";
		adev->stats.tx_fifo_errors++;
		break;
	case 0x80:
		/* possibly ACPI C-state powersaving related!!!
		 * (DMA timeout due to excessively high wakeup
		 * latency after C-state activation!?)
		 * Disable C-State powersaving and try again,
		 * then PLEASE REPORT, I'm VERY interested in
		 * whether my theory is correct that this is
		 * actually the problem here.
		 * In that case, use new Linux idle wakeup latency
		 * requirements kernel API to prevent this issue. */
		err = "DMA error";
		/*adev->wstats.discard.misc++; */
		break;
	}

	adev->stats.tx_errors++;

	if (adev->stats.tx_errors <= 20)
		log(log_level, "%s: tx error 0x%02X, buf %02u! (%s)\n",
			wiphy_name(adev->ieee->wiphy), error, finger, err);
	else
		log(log_level, "%s: tx error 0x%02X, buf %02u!\n",
			wiphy_name(adev->ieee->wiphy), error, finger);
}

/*
 * OW 20100405 This comment somehow lost it's function (wasn't me though!)
 *
 * acx_l_handle_txrate_auto
 *
 * Theory of operation:
 * client->rate_cap is a bitmask of rates client is capable of.
 * client->rate_cfg is a bitmask of allowed (configured) rates.
 * It is set as a result of iwconfig rate N [auto]
 * or iwpriv set_rates "N,N,N N,N,N" commands.
 * It can be fixed (e.g. 0x0080 == 18Mbit only),
 * auto (0x00ff == 18Mbit or any lower value),
 * and code handles any bitmask (0x1081 == try 54Mbit,18Mbit,1Mbit _only_).
 *
 * client->rate_cur is a value for rate111 field in tx descriptor.  It
 * is always set to txrate_cfg sans zero or more most significant
 * bits. This routine handles selection of new rate_cur value
 * depending on outcome of last tx event.
 *
 * client->rate_100 is a precalculated rate value for acx100 (we can
 * do without it, but will need to calculate it on each tx).
 *
 * You cannot configure mixed usage of 5.5 and/or 11Mbit rate with
 * PBCC and CCK modulation. Either both at CCK or both at PBCC.  In
 * theory you can implement it, but so far it is considered not worth
 * doing.
 *
 * 22Mbit, of course, is PBCC always.
 */

/* maps acx100 tx descr rate field to acx111 one */
/*
static u16 rate100to111(u8 r)
{
	switch (r) {
	case RATE100_1:
		return RATE111_1;
	case RATE100_2:
		return RATE111_2;
	case RATE100_5:
	case (RATE100_5 | RATE100_PBCC511):
		return RATE111_5;
	case RATE100_11:
	case (RATE100_11 | RATE100_PBCC511):
		return RATE111_11;
	case RATE100_22:
		return RATE111_22;
	default:
		pr_info("unexpected acx100 txrate: %u! "
		       "Please report\n", r);
		return RATE111_1;
	}
}
*/



/*
 * BOM Irq Handling, Timer
 * ==================================================
 */
void acx_init_task_scheduler(acx_device_t *adev)
{
	/* configure task scheduler */
#if defined(CONFIG_ACX_MAC80211_PCI)
	if (IS_PCI(adev)) {
		pr_info("device IS_PCI\n");
		INIT_WORK(&adev->irq_work, acx_irq_work);
		return;
	}
#endif
#if defined(CONFIG_ACX_MAC80211_USB)
	if (IS_USB(adev)) {
		pr_info("device IS_USB\n");
		INIT_WORK(&adev->irq_work, acxusb_irq_work);
		return;
	}
#endif
#if defined(CONFIG_ACX_MAC80211_MEM)
	if (IS_MEM(adev)) {
		pr_info("device IS_MEM\n");
		INIT_WORK(&adev->irq_work, acx_irq_work);
		return;
	}
#endif

	logf0(L_ANY, "Unhandled adev device type!\n");
	BUG();

	/* OW TODO Interrupt handling ... */
	/* OW In case of of tasklet ... but workqueues seem to be prefered
	 tasklet_init(&adev->interrupt_tasklet,
	 (void(*)(unsigned long)) acx_interrupt_tasklet,
	 (unsigned long) adev);
	 */

}

void acx_after_interrupt_task(acx_device_t *adev)
{
	FN_ENTER;

	if (!adev->after_interrupt_jobs || !adev->initialized)
		goto end_no_lock;	/* no jobs to do */

	/* we see lotsa tx errors */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_RADIO_RECALIB) {
		logf0(L_DEBUG, "Schedule CMD_RADIO_RECALIB\n");
		acx_after_interrupt_recalib(adev);
	}

	/* 1) we detected that no Scan_Complete IRQ came from fw, or
	 * 2) we found too many STAs */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_STOP_SCAN) {
		log(L_IRQ, "sending a stop scan cmd...\n");

		/* OW Scanning is done by mac80211 */
#if 0
		acx_unlock(adev, flags);
		acx_issue_cmd(adev, ACX1xx_CMD_STOP_SCAN, NULL, 0);
		acx_lock(adev, flags);
		/* HACK: set the IRQ bit, since we won't get a scan
		 * complete IRQ any more on ACX111 (works on ACX100!),
		 * since _we_, not a fw, have stopped the scan */
		SET_BIT(adev->irq_status, HOST_INT_SCAN_COMPLETE);
#endif
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_CMD_STOP_SCAN);
	}

	/* either fw sent Scan_Complete or we detected that no
	 * Scan_Complete IRQ came from fw. Finish scanning, pick join
	 * partner if any */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_COMPLETE_SCAN) {
		/* + scan kills current join status - restore it
		 *   (do we need it for STA?) */
		/* + does it happen only with active scans?
		 *   active and passive scans? ALL scans including
		 *   background one? */
		/* + was not verified that everything is restored
		 *   (but at least we start to emit beacons again) */
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_COMPLETE_SCAN);
	}

	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_RESTART_SCAN) {
		log(L_IRQ, "sending a start_scan cmd...\n");
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_RESTART_SCAN);
	}

	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_UPDATE_TIM) {
		log(L_IRQ, "ACX_AFTER_IRQ_UPDATE_TIM\n");
		acx_do_job_update_tim(adev);
		CLEAR_BIT(adev->after_interrupt_jobs,
			ACX_AFTER_IRQ_UPDATE_TIM);
	}

	/* others */
	if(adev->after_interrupt_jobs)
	{
		pr_info("Jobs still to be run: 0x%02X\n",
			adev->after_interrupt_jobs);
		adev->after_interrupt_jobs = 0;
	}
end_no_lock:
	FN_EXIT0;
}

void acx_log_irq(u16 irqtype)
{
	pr_info("%s: got: ", __func__);

	if (irqtype & HOST_INT_RX_DATA)
		pr_info("Rx_Data,");

	if (irqtype & HOST_INT_TX_COMPLETE)
		pr_info("Tx_Complete,");

	if (irqtype & HOST_INT_TX_XFER)
		pr_info("Tx_Xfer,");

	if (irqtype & HOST_INT_RX_COMPLETE)
		pr_info("Rx_Complete,");

	if (irqtype & HOST_INT_DTIM)
		pr_info("DTIM,");

	if (irqtype & HOST_INT_BEACON)
		pr_info("Beacon,");

	if (irqtype & HOST_INT_TIMER)
		log(L_IRQ, "Timer,");

	if (irqtype & HOST_INT_KEY_NOT_FOUND)
		pr_info("Key_Not_Found,");

	if (irqtype & HOST_INT_IV_ICV_FAILURE)
		pr_info("IV_ICV_Failure (crypto),");

	if (irqtype & HOST_INT_CMD_COMPLETE)
		pr_info("Cmd_Complete,");

	if (irqtype & HOST_INT_INFO)
		pr_info("Info,");

	if (irqtype & HOST_INT_OVERFLOW)
		pr_info("Overflow,");

	if (irqtype & HOST_INT_PROCESS_ERROR)
		pr_info("Process_Error,");

	if (irqtype & HOST_INT_SCAN_COMPLETE)
		pr_info("Scan_Complete,");

	if (irqtype & HOST_INT_FCS_THRESHOLD)
		pr_info("FCS_Threshold,");

	if (irqtype & HOST_INT_UNKNOWN)
		pr_info("Unknown,");

	pr_info(": IRQ(s)\n");
}

/*
 * acx_schedule_task
 *
 * Schedule the call of the after_interrupt method after leaving
 * the interrupt context.
 */
void acx_schedule_task(acx_device_t *adev, unsigned int set_flag)
{
	SET_BIT(adev->after_interrupt_jobs, set_flag);
	ieee80211_queue_work(adev->ieee, &adev->irq_work);
}

/*
* acx_i_timer
*/
void acx_timer(unsigned long address)
{
	/* acx_device_t *adev = (acx_device_t *) address; */

	FN_ENTER;

	FIXME();
	/* We need calibration and stats gather tasks to perform here */

	FN_EXIT0;
}

/*
 * acx_set_timer
 *
 * Sets the 802.11 state management timer's timeout.
 *
 */
void acx_set_timer(acx_device_t *adev, int timeout_us)
{
	FN_ENTER;

	log(L_DEBUG | L_IRQ, "(%u ms)\n", timeout_us / 1000);
	if (!(adev->dev_state_mask & ACX_STATE_IFACE_UP)) {
		pr_info("attempt to set the timer "
		       "when the card interface is not up!\n");
		goto end;
	}

	/* first check if the timer was already initialized, THEN modify it */
	if (adev->mgmt_timer.function) {
		mod_timer(&adev->mgmt_timer,
			  jiffies + (timeout_us * HZ / 1000000));
	}
end:
	FN_EXIT0;
}

/*
 * BOM Mac80211 Ops
 * ==================================================
 */

int acx_op_add_interface(struct ieee80211_hw *ieee, struct ieee80211_VIF *vif)
{
	acx_device_t *adev = ieee2adev(ieee);
	int err = -EOPNOTSUPP;

	u8 *mac_vif;
	char mac[MACSTR_SIZE];

	int vif_type;

	FN_ENTER;
	acx_sem_lock(adev);

	vif_type = vif->type;
	adev->vif_type = vif_type;
	log(L_ANY, "vif_type=%04X\n", vif_type);

	if (vif_type == NL80211_IFTYPE_MONITOR)
		adev->vif_monitor++;
	else if (adev->vif_operating)
		goto out_unlock;

	adev->vif_operating = 1;
	adev->vif = VIF_vif(vif);
	mac_vif = VIF_addr(vif);

	switch (adev->vif_type) {
	case NL80211_IFTYPE_AP:
		log(L_ANY, "NL80211_IFTYPE_AP\n");
		adev->mode = ACX_MODE_3_AP;
		break;

	case NL80211_IFTYPE_ADHOC:
		log(L_ANY, "NL80211_IFTYPE_ADHOC\n");
		adev->mode = ACX_MODE_0_ADHOC;
		break;

	case NL80211_IFTYPE_STATION:
		log(L_ANY, "NL80211_IFTYPE_STATION\n");
		adev->mode = ACX_MODE_2_STA;
		break;

	case NL80211_IFTYPE_MONITOR:
		logf0(L_ANY, "NL80211_IFTYPE_MONITOR\n");
		break;

	case NL80211_IFTYPE_WDS:
		logf0(L_ANY, "NL80211_IFTYPE_WDS: Not implemented\n");
		goto out_unlock;

	default:
		logf1(L_ANY, "Unknown adev->vif_type=%d\n", adev->vif_type);

		goto out_unlock;
		break;
	}

	/* Reconfigure mac-address globally, affecting all vifs */
	if (!mac_is_equal(mac_vif, adev->dev_addr)) {
		memcpy(adev->dev_addr, mac_vif, ETH_ALEN);
		memcpy(adev->bssid, mac_vif, ETH_ALEN);
		acx1xx_set_station_id(adev, mac_vif);
		SET_IEEE80211_PERM_ADDR(adev->ieee, adev->dev_addr);
	}

	acx_update_mode(adev);

	logf0(L_ANY, "Redoing cmd_join_bssid() after add_interface\n");
	acx_cmd_join_bssid(adev, adev->bssid);

	acx_debugfs_add_adev(adev);

	pr_info("Virtual interface added (type: 0x%08X, MAC: %s)\n",
		adev->vif_type,	acx_print_mac(mac, mac_vif));

	err = 0;

out_unlock:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return err;
}

void acx_op_remove_interface(struct ieee80211_hw *hw, struct ieee80211_VIF *vif)
{
	acx_device_t *adev = ieee2adev(hw);

	char mac[MACSTR_SIZE];
	u8 *mac_vif;

	FN_ENTER;
	acx_sem_lock(adev);
	acx_debugfs_remove_adev(adev);

	mac_vif = VIF_addr(vif);

	if (vif->type == NL80211_IFTYPE_MONITOR)
		adev->vif_monitor--;
	else {
		adev->vif_operating = 0;
		adev->vif = NULL;
	}

	acx_set_mode(adev, ACX_MODE_OFF);

	log(L_DEBUG, "vif_operating=%d, vif->type=%d\n",
		adev->vif_operating, vif->type);

	log(L_ANY, "Virtual interface removed: type=%d, MAC=%s\n",
		vif->type, acx_print_mac(mac, mac_vif));

	acx_sem_unlock(adev);
	FN_EXIT0;
}

/* FUNCTION_GREP_RESET
 * The function_grep script can get confused with multiple "{"" opening braces
 * due e.g. due to #ifdefs. This tag reset the parser state of the script.
 */

int acx_op_config(struct ieee80211_hw *hw, u32 changed)
{
	acx_device_t *adev = ieee2adev(hw);
	struct ieee80211_conf *conf = &hw->conf;

	u32 changed_not_done = changed;

	FN_ENTER;
	acx_sem_lock(adev);

	if (!adev->initialized)
		goto end_sem_unlock;

	logf1(L_DEBUG, "changed=%08X\n", changed);

	/* Tx-Power
	 * power_level: requested transmit power (in dBm)
	 */
	if (changed & IEEE80211_CONF_CHANGE_POWER) {
		logf1(L_DEBUG, "IEEE80211_CONF_CHANGE_POWER: %d\n",
			conf->power_level);
		acx1xx_set_tx_level_dbm(adev, conf->power_level);
	}

	/* IEEE80211_CONF_CHANGE_CHANNEL */
	if (changed & IEEE80211_CONF_CHANGE_CHANNEL) {
		changed_not_done &= ~IEEE80211_CONF_CHANGE_CHANNEL;

		logf1(L_DEBUG, "IEEE80211_CONF_CHANGE_CHANNEL, "
			"channel->hw_value=%i\n", conf->channel->hw_value);

		if (conf->channel->hw_value == adev->channel)
			goto change_channel_done;

		acx_selectchannel(adev, conf->channel->hw_value,
				conf->channel->center_freq);
	}
change_channel_done:
	if (changed_not_done)
		logf1(L_DEBUG, "changed_not_done=%08X\n", changed_not_done);

end_sem_unlock:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

void acx_op_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			struct ieee80211_bss_conf *info, u32 changed)
{
	acx_device_t *adev = ieee2adev(hw);

	int err = -ENODEV;

	struct sk_buff *beacon;

	FN_ENTER;
	acx_sem_lock(adev);

	logf1(L_DEBUG, "changed=%04X\n", changed);

	if (!adev->vif_operating)
		goto end_sem_unlock;

	if (changed & BSS_CHANGED_BSSID) {
		MAC_COPY(adev->bssid, info->bssid);

		logf0(L_ANY, "Redoing join following bssid update\n");
		acx_cmd_join_bssid(adev, adev->bssid);
	}

	/* BOM BSS_CHANGED_BEACON */
	if (changed & BSS_CHANGED_BEACON) {

		/* TODO Use ieee80211_beacon_get_tim instead */
		beacon = ieee80211_beacon_get(hw, vif);
		if (!beacon) {
			logf0(L_ANY,
				"Error: BSS_CHANGED_BEACON: skb_tmp==NULL");
			return;
		}

		adev->beacon_interval = info->beacon_int;
		acx_set_beacon(adev, beacon);

		dev_kfree_skb(beacon);
	}
	err = 0;

end_sem_unlock:
	acx_sem_unlock(adev);
	FN_EXIT1(err);
	return;
}

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

static int acx111_set_key_type(acx_device_t *adev, acx111WEPDefaultKey_t *key,
                               struct ieee80211_key_conf *mac80211_key,
                               const u8 *addr)
{
	switch (mac80211_key->cipher) {
#if 0
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		if (is_broadcast_ether_addr(addr))
			key->type = KEY_WEP_DEFAULT;
		else
			key->type = KEY_WEP_ADDR;

		mac80211_key->hw_key_idx = mac80211_key->keyidx;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		if (is_broadcast_ether_addr(addr))
			key->type = KEY_TKIP_MIC_GROUP;
		else
			key->type = KEY_TKIP_MIC_PAIRWISE;

		mac80211_key->hw_key_idx = mac80211_key->keyidx;
		break;
#endif
	case WLAN_CIPHER_SUITE_CCMP:
		if (is_broadcast_ether_addr(addr))
			key->type = KEY_AES_GROUP;
		else
			key->type = KEY_AES_PAIRWISE;

		break;
	default:
		log(L_INIT, "Unknown key cipher 0x%x", mac80211_key->cipher);
		return -EOPNOTSUPP;
	}

	return 0;
}


static int acx111_set_key(acx_device_t *adev, enum set_key_cmd cmd,
                          const u8 *addr, struct ieee80211_key_conf *key)
{
	int ret = -1;
	acx111WEPDefaultKey_t dk;

	memset(&dk, 0, sizeof(dk));

	switch (cmd) {
	case SET_KEY:
		dk.action = cpu_to_le16(KEY_ADD_OR_REPLACE);
		break;
	case DISABLE_KEY:
		dk.action = cpu_to_le16(KEY_REMOVE);
		break;
	default:
		log(L_INIT, "Unsupported key cmd 0x%x", cmd);
		break;
	}

	ret = acx111_set_key_type(adev, &dk, key, addr);
	if (ret < 0) {
		log(L_INIT, "Set KEY type failed");
		return ret;
	}

	memcpy(dk.MacAddr, addr, ETH_ALEN);

	dk.keySize = key->keylen;
	dk.defaultKeyNum = key->keyidx; /* ignored when setting default key */
	dk.index = 0;

	memcpy(dk.key, key->key, dk.keySize);

	ret = acx_issue_cmd(adev, ACX1xx_CMD_WEP_MGMT, &dk, sizeof(dk));

	return ret;
}

int acx_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
                   struct ieee80211_vif *vif, struct ieee80211_sta *sta,
                   struct ieee80211_key_conf *key)
{
	struct acx_device *adev = ieee2adev(hw);
	u8 algorithm;
	int ret=0;

	const u8 *addr;
	static const u8 bcast_addr[ETH_ALEN] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	acx_sem_lock(adev);

	addr = sta ? sta->addr : bcast_addr;

	log(L_DEBUG, "cmd=%d\n", cmd);
	log(L_DEBUG, "addr=" MACSTR, MAC(addr));
	log(L_DEBUG, "key->: cipher=%08x, icv_len=%d, iv_len=%d, hw_key_idx=%d, "
			"flags=%02x, keyidx=%d, keylen=%d\n", key->cipher, key->icv_len,
	        key->iv_len, key->hw_key_idx, key->flags, key->keyidx,
	        key->keylen);
	if (acx_debug & L_DEBUG)
		hexdump("key->: key", key->key, key->keylen);

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        switch (key->alg) {
#else
	switch (key->cipher) {
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_WEP:
                if (key->keylen == 5) {
                    algorithm = ACX_SEC_ALGO_WEP;
                    log(L_INIT, "algorithm=%i: %s\n",
			    algorithm, "ACX_SEC_ALGO_WEP");
                } else {
                    algorithm = ACX_SEC_ALGO_WEP104;
                    log(L_INIT, "algorithm=%i: %s\n", "ACX_SEC_ALGO_WEP104");
                }

		acx_set_hw_encryption_off(adev);
                ret = -EOPNOTSUPP;
                break;
#else
	case WLAN_CIPHER_SUITE_WEP40:
	        algorithm = ACX_SEC_ALGO_WEP;
                log(L_INIT, "algorithm=%i: %s\n", algorithm, "ACX_SEC_ALGO_WEP");

                acx_set_hw_encryption_off(adev);
                ret = -EOPNOTSUPP;
                break;

        case WLAN_CIPHER_SUITE_WEP104:
                algorithm = ACX_SEC_ALGO_WEP104;
                log(L_INIT, "algorithm=%i: %s\n",
			algorithm, "ACX_SEC_ALGO_WEP104");

                acx_set_hw_encryption_off(adev);
                ret = -EOPNOTSUPP;
                break;
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_TKIP:
#else
	case WLAN_CIPHER_SUITE_TKIP:
#endif
	        algorithm = ACX_SEC_ALGO_TKIP;
	        log(L_INIT, "algorithm=%i: %s\n", algorithm, "ACX_SEC_ALGO_TKIP");

	        acx_set_hw_encryption_off(adev);
	        ret = -EOPNOTSUPP;
	        break;

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_CCMP:
#else
	case WLAN_CIPHER_SUITE_CCMP:
#endif
		algorithm = ACX_SEC_ALGO_AES;
		log(L_INIT, "algorithm=%i: %s\n", algorithm, "ACX_SEC_ALGO_AES");

		acx_set_hw_encryption_on(adev);
		acx111_set_key(adev, cmd, addr, key);
		ret = 0;
		break;

	default:
		algorithm = ACX_SEC_ALGO_NONE;

		acx_set_hw_encryption_off(adev);
		ret = 0;
		break;
	}

	acx_sem_unlock(adev);
	return ret;
}

void acx_op_configure_filter(struct ieee80211_hw *hw,
			unsigned int changed_flags,
			unsigned int *total_flags, u64 multicast)
{
	acx_device_t *adev = ieee2adev(hw);

	FN_ENTER;

	acx_sem_lock(adev);

	logf1(L_DEBUG, "1: changed_flags=0x%08x, *total_flags=0x%08x\n",
		changed_flags, *total_flags);

	/* OWI TODO: Set also FIF_PROBE_REQ ? */
	*total_flags &= (FIF_PROMISC_IN_BSS | FIF_ALLMULTI | FIF_FCSFAIL
			| FIF_CONTROL | FIF_OTHER_BSS);

	logf1(L_DEBUG, "2: *total_flags=0x%08x\n", *total_flags);

	acx_sem_unlock(adev);
	FN_EXIT0;
}

#if CONFIG_ACX_MAC80211_VERSION >= KERNEL_VERSION(3, 2, 0)
int acx_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		u16 queue, const struct ieee80211_tx_queue_params *params)
#else
int acx_conf_tx(struct ieee80211_hw *hw, u16 queue,
		const struct ieee80211_tx_queue_params *params)
#endif
{
	acx_device_t *adev = ieee2adev(hw);
	FN_ENTER;
	acx_sem_lock(adev);
    /* TODO */
	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

int acx_op_get_stats(struct ieee80211_hw *hw,
		struct ieee80211_low_level_stats *stats)
{
	acx_device_t *adev = ieee2adev(hw);

	FN_ENTER;
	acx_sem_lock(adev);

	memcpy(stats, &adev->ieee_stats, sizeof(*stats));

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

int acx_op_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta, bool set)
{
	acx_device_t *adev = ieee2adev(hw);

	acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_TIM);

	return 0;
}

static int acx_do_job_update_tim(acx_device_t *adev)
{
	int ret;
	struct sk_buff *beacon;
	u16 tim_offset;
	u16 tim_length;

#if CONFIG_ACX_MAC80211_VERSION > KERNEL_VERSION(2, 6, 32)
	beacon = ieee80211_beacon_get_tim(adev->ieee, adev->vif, &tim_offset,
			&tim_length);
#else
	beacon = ieee80211_beacon_get(adev->ieee, adev->vif);
	if (!beacon)
		goto out;

	tim_offset = acx_beacon_find_tim(beacon) - beacon->data;
out:
#endif
	if (!beacon) {
		logf0(L_ANY, "Error: beacon==NULL");
		return NOT_OK;
	}

	if (IS_ACX111(adev)) {
		ret = acx_set_tim_template(adev, beacon->data + tim_offset,
				beacon->len - tim_offset);
	}

	dev_kfree_skb(beacon);

	return (ret);
}

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
int acx_e_op_get_tx_stats(struct ieee80211_hw *hw,
			 struct ieee80211_tx_queue_stats *stats)
{
	acx_device_t *adev = ieee2adev(hw);
	int err = -ENODEV;

	FN_ENTER;
	acx_sem_lock(adev);

	stats->len = 0;
	stats->limit = TX_CNT;
	stats->count = 0;

	acx_sem_unlock(adev);
	FN_EXIT0;
	return err;
}
#endif

/*
 * BOM Helpers
 * ==================================================
 */

/*
 * Basically a mdelay/msleep with logging
 */
void acx_mwait(int ms)
{
	FN_ENTER;
	msleep(ms);
	FN_EXIT0;
}

/*
 * Calculate level like the feb 2003 windows driver seems to do
 *
 * Note: the FreeBSD and DragonFlyBSD drivers seems to use different
 * so-called correction constants depending on the chip. They will be
 * defined for now, but as it is still unknown whether they are correct
 * or not, only the original value will be used. Something else to take
 * into account is that the OpenBSD driver uses another approach and
 * defines the maximum RSSI value depending on the chip, rather than
 * using a value of 100 for all of them, as it is currently done here.
 */
#define ACX100_RSSI_CORR 8
#define ACX111_RSSI_CORR 5
static u8 acx_signal_to_winlevel(u8 rawlevel)
{
	/* u8 winlevel = (u8) (0.5 + 0.625 * rawlevel); */
	u8 winlevel = (((ACX100_RSSI_CORR / 2) + (rawlevel * 5)) /
			ACX100_RSSI_CORR);

	if (winlevel > 100)
		winlevel = 100;
	return winlevel;
}

u8 acx_signal_determine_quality(u8 signal, u8 noise)
{
	int qual;

	qual = (((signal - 30) * 100 / 70) + (100 - noise * 4)) / 2;

	if (qual > 100)
		return 100;
	if (qual < 0)
		return 0;
	return qual;
}

const char* acx_get_packet_type_string(u16 fc)
{
	static const char * const mgmt_arr[] = {
		"MGMT/AssocReq", "MGMT/AssocResp", "MGMT/ReassocReq",
		"MGMT/ReassocResp", "MGMT/ProbeReq", "MGMT/ProbeResp",
		"MGMT/UNKNOWN", "MGMT/UNKNOWN", "MGMT/Beacon", "MGMT/ATIM",
		"MGMT/Disassoc", "MGMT/Authen", "MGMT/Deauthen",
		"MGMT/UNKNOWN"
	};
	static const char * const ctl_arr[] = {
		"CTL/PSPoll", "CTL/RTS", "CTL/CTS", "CTL/Ack", "CTL/CFEnd",
		"CTL/CFEndCFAck", "CTL/UNKNOWN"
	};
	static const char * const data_arr[] = {
		"DATA/DataOnly", "DATA/Data CFAck", "DATA/Data CFPoll",
		"DATA/Data CFAck/CFPoll", "DATA/Null", "DATA/CFAck",
		"DATA/CFPoll", "DATA/CFAck/CFPoll", "DATA/UNKNOWN"
	};
	const char *str;
	u8 fstype = (IEEE80211_FCTL_STYPE & fc) >> 4;
	u8 ctl;

	switch (IEEE80211_FCTL_FTYPE & fc) {
	case IEEE80211_FTYPE_MGMT:
		str = mgmt_arr[min((size_t)fstype, ARRAY_SIZE(mgmt_arr) - 1)];
		break;
	case IEEE80211_FTYPE_CTL:
		ctl = fstype - 0x0a;
		str = ctl_arr[min((size_t)ctl, ARRAY_SIZE(ctl_arr) - 1)];
		break;
	case IEEE80211_FTYPE_DATA:
		str = data_arr[min((size_t)fstype, ARRAY_SIZE(data_arr) - 1)];
		break;
	default:
		str = "UNKNOWN";
		break;
	}
	return str;
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

	FN_ENTER;

	/* 0..0x20, 0x1000..0x1020 */
	for (type = 0; type <= 0x1020; type++) {
		if (type == 0x21)
			type = 0x1000;
		ie.type = cpu_to_le16(type);
		ie.len = cpu_to_le16(sizeof(ie) - 4);
		acx_issue_cmd(adev, ACX1xx_CMD_INTERROGATE, &ie, sizeof(ie));
	}
	FN_EXIT0;
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
