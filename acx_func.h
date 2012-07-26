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
#ifndef _ACX_FUNC_H_
#define _ACX_FUNC_H_

#include <linux/version.h>

/* CONFIG_ACX_MAC80211_VERSION allows to specify the version of the used
 * wireless mac80211 api, in case it is different of the used kernel.
 * OpenWRT e.g. uses a version of compat-wireless, which is ahead of
 * the used kernel.
 */
//
/* CONFIG_ACX_MAC80211_VERSION can be defined on the make command line by
 * passing EXTRA_CFLAGS="-DCONFIG_ACX_MAC80211_VERSION=\"KERNEL_VERSION(2,6,34)\""
 */

#ifndef CONFIG_ACX_MAC80211_VERSION
	#define CONFIG_ACX_MAC80211_VERSION LINUX_VERSION_CODE
#endif

#if CONFIG_ACX_MAC80211_VERSION >= KERNEL_VERSION(3, 2, 0)
#include <net/iw_handler.h>
#endif

/*
 * BOM Config
 * ==================================================
 */
#define CMD_TIMEOUT_MS(n)	(n)
#define ACX_CMD_TIMEOUT_DEFAULT	CMD_TIMEOUT_MS(50)

/* Define ACX_GIT_VERSION with "undef" value, if undefined for some reason */
#ifndef ACX_GIT_VERSION
        #define ACX_GIT_VERSION "unknown"
#endif

/*
 * BOM Common
 * ==================================================
 */

/* BOM Locking (Common)
 * -----
 */

/*
 * Locking is done mainly using the adev->sem.
 *
 * The locking rule is: All external entry paths are protected by the
 * sem.
 *
 * The adev->spinlock is still kept for the irq top-half, although
 * even there it wouldn't be really required. It's just to not get
 * interrupted during irq handling itself. For this we don't need the
 * acx_lock macros anymore.
 */

/* These functions *must* be inline or they will break horribly on
 * SPARC, due to its weird semantics for save/restore flags */

#define acx_sem_lock(adev)	mutex_lock(&(adev)->mutex)
#define acx_sem_unlock(adev)	mutex_unlock(&(adev)->mutex)

/*
 * BOM Logging (Common)
 *
 * - Avoid SHOUTING needlessly. Avoid excessive verbosity.
 *   Gradually remove messages which are old debugging aids.
 *
 * - Use printk() for messages which are to be always logged.
 *   Supply either 'acx:' or '<devname>:' prefix so that user
 *   can figure out who's speaking among other kernel chatter.
 *   acx: is for general issues (e.g. "acx: no firmware image!")
 *   while <devname>: is related to a particular device
 *   (think about multi-card setup). Double check that message
 *   is not confusing to the average user.
 *
 * - use printk KERN_xxx level only if message is not a WARNING
 *   but is INFO, ERR etc.
 *
 * - Use printk_ratelimited() for messages which may flood
 *   (e.g. "rx DUP pkt!").
 *
 * - Use log() for messages which may be omitted (and they
 *   _will_ be omitted in non-debug builds). Note that
 *   message levels may be disabled at compile-time selectively,
 *   thus select them wisely. Example: L_DEBUG is the lowest
 *   (most likely to be compiled out) -> use for less important stuff.
 *
 * - Do not print important stuff with log(), or else people
 *   will never build non-debug driver.
 *
 * Style:
 * hex: capital letters, zero filled (e.g. 0x02AC)
 * str: dont start from capitals, no trailing periods ("tx: queue is  * - stopped")
 */

/* Debug build */
#if ACX_DEBUG

#define log(chan, args...) \
	do { \
		if (acx_debug & (chan)) \
			pr_notice(args);	\
	} while (0)

/* Log with prefix "acx: __func__.
   No - suppress this, its handled now by pr_fmt() etc.
   preserve the api just in case..
 */
#define logf0	log
#define logf1	log

/* None-Debug build
 * OW 20100405: An none-debug build is currently probably broken
 */
#else

#define log(chan, args...)
/* Standard way of log flood prevention */
#define printk_ratelimited(args...) \
do { \
	if (printk_ratelimit()) \
		printk(args); \
} while (0)

#endif 
/* --- */

#define TODO()		\
        do {		\
                printk(KERN_INFO "TODO: Incomplete code in %s() at %s:%d\n",	\
			__FUNCTION__, __FILE__, __LINE__);			\
        } while (0)

#define FIXME()		\
        do {		\
                printk(KERN_INFO "FIXME: Possibly broken code in %s() at %s:%d\n", \
			__FUNCTION__, __FILE__, __LINE__);			\
        } while (0)

/* BOM Proc, Debug (Common)
 * -----
 */

#if defined CONFIG_PROC_FS && defined ACX_WANT_PROC_FILES_ANYWAY
#  define PROC_ENTRIES
#endif

DECL_OR_STUB(PROC_ENTRIES,
	int acx_proc_register_entries(struct ieee80211_hw *ieee),
	{ return 0; })
DECL_OR_STUB(PROC_ENTRIES,
	int acx_proc_unregister_entries(struct ieee80211_hw *ieee),
	{ return 0; })

/*
 * BOM Helpers (Common)
 *
 */

void acx_mwait(int ms);

/*
 * MAC address helpers
 */
static inline void MAC_COPY(u8 *mac, const u8 *src)
{
	memcpy(mac, src, ETH_ALEN);
}

static inline void MAC_FILL(u8 *mac, u8 val)
{
	memset(mac, val, ETH_ALEN);
}

static inline void MAC_BCAST(u8 *mac)
{
	((u16*)mac)[2] = *(u32*)mac = -1;
}

static inline void MAC_ZERO(u8 *mac)
{
	((u16*)mac)[2] = *(u32*)mac = 0;
}

static inline int mac_is_equal(const u8 *a, const u8 *b)
{
	/* can't beat this */
	return memcmp(a, b, ETH_ALEN) == 0;
}

static inline int mac_is_bcast(const u8 *mac)
{
	/* AND together 4 first bytes with sign-extended 2 last bytes
	 * Only bcast address gives 0xffffffff. +1 gives 0 */
	return ( *(s32*)mac & ((s16*)mac)[2] ) + 1 == 0;
}

static inline int mac_is_zero(const u8 *mac)
{
	return ( *(u32*)mac | ((u16*)mac)[2] ) == 0;
}

static inline int mac_is_directed(const u8 *mac)
{
	return (mac[0] & 1)==0;
}

static inline int mac_is_mcast(const u8 *mac)
{
	return (mac[0] & 1) && !mac_is_bcast(mac);
}

#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#define MACSTR_SIZE 3*6 /* 2 chars plus ':' per byte, trailing ':' is \0 */
#define MAC(bytevector) \
	((unsigned char *)bytevector)[0], \
	((unsigned char *)bytevector)[1], \
	((unsigned char *)bytevector)[2], \
	((unsigned char *)bytevector)[3], \
	((unsigned char *)bytevector)[4], \
	((unsigned char *)bytevector)[5]


/* Random helpers
 * ---
 */
#define TO_STRING(x)	#x
#define STRING(x)	TO_STRING(x)

#define CLEAR_BIT(val, mask) ((val) &= ~(mask))
#define SET_BIT(val, mask) ((val) |= (mask))
#define CHECK_BIT(val, mask) ((val) & (mask))

/* undefined if v==0 */
static inline unsigned int lowest_bit(u16 v)
{
	unsigned int n = 0;
	while (!(v & 0xf)) { v>>=4; n+=4; }
	while (!(v & 1)) { v>>=1; n++; }
	return n;
}

/* undefined if v==0 */
static inline unsigned int highest_bit(u16 v)
{
	unsigned int n = 0;
	while (v>0xf) { v>>=4; n+=4; }
	while (v>1) { v>>=1; n++; }
	return n;
}

/* undefined if v==0 */
static inline int has_only_one_bit(u16 v)
{
	return ((v-1) ^ v) >= v;
}


static inline int is_hidden_essid(char *essid)
{
	return (('\0' == essid[0]) ||
		((' ' == essid[0]) && ('\0' == essid[1])));
}

/* More random helpers
 * ---
 */
static inline struct ieee80211_hdr* acx_get_wlan_hdr(acx_device_t *adev,
						const rxbuffer_t *rxbuf)
{
	return (struct ieee80211_hdr *)((u8 *)&rxbuf->hdr_a3 + adev->phy_header_len);
}


/*
 * Mem prototypes
 * ==================================================
 */

int acxmem_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd,
		void *buffer, unsigned buflen, unsigned cmd_timeout,
		const char* cmdstr);

#endif /* _ACX_FUNC_H_ */
