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
#include "acx_commands.h"

/*
 * BOM Common prototypes
 * ==================================================
 */

// BOM Locking (Common)
// -----

// BOM Logging (Common)
// -----

/*
 * LOGGING
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
 * str: dont start from capitals, no trailing periods ("tx: queue is stopped")
 */

// Debug build
#if ACX_DEBUG

void log_fn_enter(const char *funcname);
void log_fn_exit(const char *funcname);
void log_fn_exit_v(const char *funcname, int v);

char* acx_print_mac(char *buf, const u8 *mac);
void acx_print_mac2(const char *head, const u8 *mac, const char *tail);

#define FN_ENTER \
	do { \
		if (unlikely(acx_debug & L_FUNC)) { \
			log_fn_enter(__func__); \
		} \
	} while (0)

#define FN_EXIT1(v) \
	do { \
		if (unlikely(acx_debug & L_FUNC)) { \
			log_fn_exit_v(__func__, v); \
		} \
	} while (0)
#define FN_EXIT0 \
	do { \
		if (unlikely(acx_debug & L_FUNC)) { \
			log_fn_exit(__func__); \
		} \
	} while (0)

#define log(chan, args...) \
	do { \
		if (acx_debug & (chan)) \
			printk(args); \
	} while (0)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
#define printk_ratelimited(args...) printk(args)
#endif

// Log with prefix "acx: __func__ 
#define logf0(chan, msg) \
		log(chan, "acx: %s: " msg, __func__);
#define logf1(chan, msg, args...) \
		log(chan, "acx: %s: " msg, __func__, args);

// None-Debug build
// OW 20100405: An none-debug build is currently probably broken
#else

#define FN_ENTER do {} while(0)
#define FN_EXIT1(v) do {} while(0)
#define FN_EXIT0 do {} while(0)

#define log(chan, args...)
/* Standard way of log flood prevention */
#define printk_ratelimited(args...) \
do { \
	if (printk_ratelimit()) \
		printk(args); \
} while (0)

#endif 
//---

void acxlog_mac(int level, const char *head, const u8 *mac, const char *tail);


// BOM Data Access (Common)
// -----

// BOM Firmware, EEPROM, Phy (Common)
// -----

// BOM Control Path (CMD handling, init, reset) (Common)
// -----

// BOM CMDs (Common:Control Path)
// -----

// BOM Configure (Common:Control Path)
// -----

// BOM Template (Common:Control Path)
// -----

// BOM Recalibration (Common:Control Path)
// -----

// BOM Other (Common:Control Path)
// -----

// BOM Proc, Debug (Common)
// -----

// BOM Rx Path (Common)
// -----

// BOM Tx Path (Common)
// -----

// BOM Crypto (Common)
// -----

// BOM Irq Handling, Timer (Common)
// -----

// BOM Mac80211 Ops (Common)
// -----

// BOM Helpers (Common)
// -----

// MAC address helpers
static inline void
MAC_COPY(u8 *mac, const u8 *src)
{
	memcpy(mac, src, ETH_ALEN);
}

static inline void
MAC_FILL(u8 *mac, u8 val)
{
	memset(mac, val, ETH_ALEN);
}

static inline void
MAC_BCAST(u8 *mac)
{
	((u16*)mac)[2] = *(u32*)mac = -1;
}

static inline void
MAC_ZERO(u8 *mac)
{
	((u16*)mac)[2] = *(u32*)mac = 0;
}

static inline int
mac_is_equal(const u8 *a, const u8 *b)
{
	/* can't beat this */
	return memcmp(a, b, ETH_ALEN) == 0;
}

static inline int
mac_is_bcast(const u8 *mac)
{
	/* AND together 4 first bytes with sign-extended 2 last bytes
	** Only bcast address gives 0xffffffff. +1 gives 0 */
	return ( *(s32*)mac & ((s16*)mac)[2] ) + 1 == 0;
}

static inline int
mac_is_zero(const u8 *mac)
{
	return ( *(u32*)mac | ((u16*)mac)[2] ) == 0;
}

static inline int
mac_is_directed(const u8 *mac)
{
	return (mac[0] & 1)==0;
}

static inline int
mac_is_mcast(const u8 *mac)
{
	return (mac[0] & 1) && !mac_is_bcast(mac);
}

#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC(bytevector) \
	((unsigned char *)bytevector)[0], \
	((unsigned char *)bytevector)[1], \
	((unsigned char *)bytevector)[2], \
	((unsigned char *)bytevector)[3], \
	((unsigned char *)bytevector)[4], \
	((unsigned char *)bytevector)[5]


// Random helpers
#define TO_STRING(x)	#x
#define STRING(x)	TO_STRING(x)

#define CLEAR_BIT(val, mask) ((val) &= ~(mask))
#define SET_BIT(val, mask) ((val) |= (mask))
#define CHECK_BIT(val, mask) ((val) & (mask))

/* undefined if v==0 */
static inline unsigned int
lowest_bit(u16 v)
{
	unsigned int n = 0;
	while (!(v & 0xf)) { v>>=4; n+=4; }
	while (!(v & 1)) { v>>=1; n++; }
	return n;
}

/* undefined if v==0 */
static inline unsigned int
highest_bit(u16 v)
{
	unsigned int n = 0;
	while (v>0xf) { v>>=4; n+=4; }
	while (v>1) { v>>=1; n++; }
	return n;
}

/* undefined if v==0 */
static inline int
has_only_one_bit(u16 v)
{
	return ((v-1) ^ v) >= v;
}


static inline int
is_hidden_essid(char *essid)
{
	return (('\0' == essid[0]) ||
		((' ' == essid[0]) && ('\0' == essid[1])));
}

// BOM Driver, Module (Common)
// -----




/*
* LOCKING
* We have adev->sem and adev->spinlock.
*
* We employ following naming convention in order to get locking right:
*
* acx_e_xxxx - external entry points called from process context.
*	It is okay to sleep. adev->sem is to be taken on entry.
* acx_i_xxxx - external entry points possibly called from atomic context.
*	Sleeping is not allowed (and thus down(sem) is not legal!)
* acx_s_xxxx - potentially sleeping functions. Do not ever call under lock!
* acx_l_xxxx - functions which expect lock to be already taken.
* rest       - non-sleeping functions which do not require locking
*		but may be run under lock
*
* A small number of local helpers do not have acx_[eisl]_ prefix.
* They are always close to caller and are to be reviewed locally.
*
* Theory of operation:
*
* All process-context entry points (_e_ functions) take sem
* immediately. IRQ handler and other 'atomic-context' entry points
* (_i_ functions) take lock immediately on entry, but dont take sem
* because that might sleep.
*
* Thus *all* code is either protected by sem or lock, or both.
*
* Code which must not run concurrently with IRQ takes lock.
* Such code is marked with _l_.
*
* This results in the following rules of thumb useful in code review:
*
* + If a function calls _s_ fn, it must be an _s_ itself.
* + You can call _l_ fn only (a) from another _l_ fn
*   or (b) from _s_, _e_ or _i_ fn by taking lock, calling _l_,
*   and dropping lock.
* + All IRQ code runs under lock.
* + Any _s_ fn is running under sem.
* + Code under sem can race only with IRQ code.
* + Code under sem+lock cannot race with anything.
* 
* OW TODO: Explain semantics of the acx_lock and sem
*/

/* These functions *must* be inline or they will break horribly on SPARC, due
 * to its weird semantics for save/restore flags */

#if defined(PARANOID_LOCKING) /* Lock debugging */

void acx_lock_debug(acx_device_t *adev, const char* where);
void acx_unlock_debug(acx_device_t *adev, const char* where);
void acx_down_debug(acx_device_t *adev, const char* where);
void acx_up_debug(acx_device_t *adev, const char* where);
void acx_lock_unhold(void);
void acx_sem_unhold(void);

static inline void
acx_lock_helper(acx_device_t *adev, unsigned long *fp, const char* where)
{
	acx_lock_debug(adev, where);
	spin_lock_irqsave(&adev->spinlock, *fp);
}
static inline void
acx_unlock_helper(acx_device_t *adev, unsigned long *fp, const char* where)
{
	acx_unlock_debug(adev, where);
	spin_unlock_irqrestore(&adev->spinlock, *fp);
}
#define acx_lock(adev, flags)	acx_lock_helper(adev, &(flags), __FILE__ ":" STRING(__LINE__))
#define acx_unlock(adev, flags)	acx_unlock_helper(adev, &(flags), __FILE__ ":" STRING(__LINE__))
#define acx_sem_lock(adev)	mutex_lock(&(adev)->mutex)
#define acx_sem_unlock(adev)	mutex_unlock(&(adev)->mutex)

#elif defined(DO_LOCKING)

#define acx_lock(adev, flags)	spin_lock_irqsave(&adev->spinlock, flags)
#define acx_unlock(adev, flags)	spin_unlock_irqrestore(&adev->spinlock, flags)
#define acx_sem_lock(adev)	mutex_lock(&(adev)->mutex)
#define acx_sem_unlock(adev)	mutex_unlock(&(adev)->mutex)
#define acx_lock_unhold()	((void)0)
#define acx_sem_unhold()	((void)0)

#else /* no locking! :( */

#define acx_lock(adev, flags)	((void)0)
#define acx_unlock(adev, flags)	((void)0)
#define acx_sem_lock(adev)	((void)0)
#define acx_sem_unlock(adev)	((void)0)
#define acx_lock_unhold()	((void)0)
#define acx_sem_unhold()	((void)0)

#endif


/***********************************************************************
*/

/* Can race with rx path (which is not protected by sem):
** rx -> process_[re]assocresp() -> set_status(ASSOCIATED) -> wake_queue()
** Can race with tx_complete IRQ:
** IRQ -> acxpci_l_clean_txdesc -> acx_wake_queue
** Review carefully all callsites */
static inline void
acx_stop_queue(struct ieee80211_hw *hw, const char *msg)
{
	FN_ENTER;
	ieee80211_stop_queues(hw);
	if (msg)
		log(L_BUFT, "acx: tx: stop queue %s\n", msg);
	FN_EXIT0;
}

static inline int
acx_queue_stopped(struct ieee80211_hw *ieee)
{
	return ieee80211_queue_stopped(ieee, 0);
}

/*
static inline void
acx_start_queue(struct ieee80211_hw *hw, const char *msg)
{
	ieee80211_start_queues(hw);
	if (msg)
		log(L_BUFT, "acx: tx: start queue %s\n", msg);
}
*/
static inline void
acx_wake_queue(struct ieee80211_hw *hw, const char *msg)
{
	FN_ENTER;
	ieee80211_wake_queues(hw);
	if (msg)
		log(L_BUFT, "acx: tx: wake queue %s\n", msg);
	FN_EXIT0;
}
/*
static inline void
acx_carrier_off(struct net_device *ndev, const char *msg)
{
	netif_carrier_off(ndev);
	if (msg)
		log(L_BUFT, "acx: tx: carrier off %s\n", msg);
}

static inline void
acx_carrier_on(struct net_device *ndev, const char *msg)
{
	netif_carrier_on(ndev);
	if (msg)
		log(L_BUFT, "acx: tx: carrier on %s\n", msg);
}

*/


/***********************************************************************
** Communication with firmware
*/
#define CMD_TIMEOUT_MS(n)	(n)
#define ACX_CMD_TIMEOUT_DEFAULT	CMD_TIMEOUT_MS(50)

// OW TODO Review for cleanup. Is special _debug #defs for logging required ?
// We can just log all in case of errors.
#if ACX_DEBUG

/* We want to log cmd names */
int acxpci_s_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr);
int acxusb_s_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr);
int acxmem_s_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr);
static inline int
acx_s_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr)
{
	if (IS_PCI(adev))
		return acxpci_s_issue_cmd_timeo_debug(adev, cmd, param, len, timeout, cmdstr);
	if (IS_USB(adev))
		return acxusb_s_issue_cmd_timeo_debug(adev, cmd, param, len, timeout, cmdstr);
	if (IS_MEM(adev))
		return acxmem_s_issue_cmd_timeo_debug(adev, cmd, param, len, timeout, cmdstr);

	log(L_ANY, "acx: %s: Unsupported dev_type=%i\n",  __func__, (adev)->dev_type);
	return (NOT_OK);
}
#define acx_s_issue_cmd(adev,cmd,param,len) \
	acx_s_issue_cmd_timeo_debug(adev,cmd,param,len,ACX_CMD_TIMEOUT_DEFAULT,#cmd)
#define acx_s_issue_cmd_timeo(adev,cmd,param,len,timeo) \
	acx_s_issue_cmd_timeo_debug(adev,cmd,param,len,timeo,#cmd)
int acx_s_configure_debug(acx_device_t *adev, void *pdr, int type, const char* str);
#define acx_s_configure(adev,pdr,type) \
	acx_s_configure_debug(adev,pdr,type,#type)
int acx_s_interrogate_debug(acx_device_t *adev, void *pdr, int type, const char* str);
#define acx_s_interrogate(adev,pdr,type) \
	acx_s_interrogate_debug(adev,pdr,type,#type)

#else

int acxpci_s_issue_cmd_timeo(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout);
int acxusb_s_issue_cmd_timeo(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout);
int acxmem_s_issue_cmd_timeo(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout);
static inline int
acx_s_issue_cmd_timeo(acx_device_t *adev, unsigned cmd,	void *param, unsigned len, unsigned timeout)
{
	if (IS_PCI(adev))
		return acxpci_s_issue_cmd_timeo(adev, cmd, param, len, timeout);
	if (IS_USB(adev))
		return acxusb_s_issue_cmd_timeo(adev, cmd, param, len, timeout);
	if (IS_MEM(adev))
		return acxmem_s_issue_cmd_timeo(adev, cmd, param, len, timeout);

	log(L_ANY, "acx: %s: Unsupported dev_type=%i\n",  __func__, (adev)->dev_type);
	return (NOT_OK);
}
static inline int
acx_s_issue_cmd(acx_device_t *adev, unsigned cmd, void *param, unsigned len)
{
	if (IS_PCI(adev))
		return acxpci_s_issue_cmd_timeo(adev, cmd, param, len, ACX_CMD_TIMEOUT_DEFAULT);
	if (IS_USB(adev))
		return acxusb_s_issue_cmd_timeo(adev, cmd, param, len, ACX_CMD_TIMEOUT_DEFAULT);
	if (IS_MEM(adev))
		return acxmem_s_issue_cmd_timeo(adev, cmd, param, len, ACX_CMD_TIMEOUT_DEFAULT);

	log(L_ANY, "acx: %s: Unsupported dev_type=%i\n",  __func__, (adev)->dev_type);
	return (NOT_OK);
}
int acx_s_configure(acx_device_t *adev, void *pdr, int type);
int acx_s_interrogate(acx_device_t *adev, void *pdr, int type);

#endif

void acx_s_cmd_start_scan(acx_device_t *adev);


/***********************************************************************
** Ioctls
*/
/*int
acx111pci_ioctl_info(
	struct net_device *ndev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra);
int
acx100pci_ioctl_set_phy_amp_bias(
	struct net_device *ndev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra);
*/

/***********************************************************************
** /proc
*/
#ifdef CONFIG_PROC_FS
int acx_proc_register_entries(struct ieee80211_hw *ieee, int num);
int acx_proc_unregister_entries(struct ieee80211_hw *ieee, int num);
#else
static inline int
acx_proc_register_entries(const struct ieee80211_hw *ieee) { return OK; }
static inline int
acx_proc_unregister_entries(const struct ieee80211_hw *ieee) { return OK; }
#endif


/***********************************************************************
*/
firmware_image_t *acx_s_read_fw(struct device *dev, const char *file, u32 *size);
int acxpci_s_upload_radio(acx_device_t *adev);
int acxmem_s_upload_radio(acx_device_t *adev);


/***********************************************************************
** Unsorted yet :)
*/
int acxpci_s_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
int acxusb_s_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
int acxmem_s_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
static inline int
acx_s_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf)
{
	if (IS_PCI(adev))
		return acxpci_s_read_phy_reg(adev, reg, charbuf);
	if (IS_USB(adev))
		return acxusb_s_read_phy_reg(adev, reg, charbuf);
	if (IS_MEM(adev))
		return acxmem_s_read_phy_reg(adev, reg, charbuf);

	log(L_ANY, "acx: %s: Unsupported dev_type=%i\n",  __func__, (adev)->dev_type);
	return (NOT_OK);
}

int acxpci_s_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);
int acxusb_s_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);
int acxmem_s_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);
static inline int
acx_s_write_phy_reg(acx_device_t *adev, u32 reg, u8 value)
{
	if (IS_PCI(adev))
		return acxpci_s_write_phy_reg(adev, reg, value);
	if (IS_USB(adev))
		return acxusb_s_write_phy_reg(adev, reg, value);
	if (IS_MEM(adev))
		return acxmem_s_write_phy_reg(adev, reg, value);

	log(L_ANY, "acx: %s: Unsupported dev_type=%i\n",  __func__, (adev)->dev_type);
	return (NOT_OK);
}

tx_t* acxpci_l_alloc_tx(acx_device_t *adev);
tx_t* acxusb_l_alloc_tx(acx_device_t *adev);

// OW TODO Included skb->len to check required blocks upfront in acx_l_alloc_tx
// This should perhaps also go into pci and usb ?!
tx_t* acxmem_l_alloc_tx(acx_device_t *adev, unsigned int len);
static inline tx_t*
acx_l_alloc_tx(acx_device_t *adev, unsigned int len)
{
	if (IS_PCI(adev))
		return acxpci_l_alloc_tx(adev);
	if (IS_USB(adev))
		return acxusb_l_alloc_tx(adev);
	if (IS_MEM(adev))
		return acxmem_l_alloc_tx(adev, len);

	log(L_ANY, "acx: %s: Unsupported dev_type=%i\n",  __func__, (adev)->dev_type);
	return (NULL);
}

void acxusb_l_dealloc_tx(tx_t *tx_opaque);
void acxmem_l_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque);
static inline void
acx_l_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque)
{
	if (IS_USB(adev))
		acxusb_l_dealloc_tx(tx_opaque);
	if (IS_MEM(adev))
		acxmem_l_dealloc_tx (adev, tx_opaque);

	log(L_ANY, "acx: %s: Unsupported dev_type=%i\n",  __func__, (adev)->dev_type);
	return;
}

void* acxpci_l_get_txbuf(acx_device_t *adev, tx_t *tx_opaque);
void* acxusb_l_get_txbuf(acx_device_t *adev, tx_t *tx_opaque);
void* acxmem_l_get_txbuf(acx_device_t *adev, tx_t *tx_opaque);
static inline void *
acx_l_get_txbuf(acx_device_t *adev, tx_t *tx_opaque)
{
	if (IS_PCI(adev))
		return acxpci_l_get_txbuf(adev, tx_opaque);
	if (IS_USB(adev))
		return acxusb_l_get_txbuf(adev, tx_opaque);
	if (IS_MEM(adev))
		return acxmem_l_get_txbuf(adev, tx_opaque);

	log(L_ANY, "acx: %s: Unsupported dev_type=%i\n",  __func__, (adev)->dev_type);
	return (NULL);
}

void acxpci_l_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
		struct ieee80211_tx_info *ieeectl, struct sk_buff *skb);
void acxusb_l_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
		struct ieee80211_tx_info *ieeectl, struct sk_buff *skb);
void acxmem_l_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
                        struct ieee80211_tx_info *ieeectl, struct sk_buff *skb);
static inline void
acx_l_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
                        struct ieee80211_tx_info *ieeectl, struct sk_buff *skb)
{
	if (IS_PCI(adev))
		return acxpci_l_tx_data(adev, tx_opaque, len, ieeectl, skb);
	if (IS_USB(adev))
		return acxusb_l_tx_data(adev, tx_opaque, len, ieeectl, skb);
	if (IS_MEM(adev))
		return acxmem_l_tx_data(adev, tx_opaque, len, ieeectl, skb);

	log(L_ANY, "acx: %s: Unsupported dev_type=%i\n",  __func__, (adev)->dev_type);
	return;
}

static inline struct ieee80211_hdr *
acx_get_wlan_hdr(acx_device_t *adev, const rxbuffer_t *rxbuf)
{
	return (struct ieee80211_hdr *)((u8 *)&rxbuf->hdr_a3 + adev->phy_header_len);
}


void acx_s_mwait(int ms);
int acx_s_init_mac(acx_device_t *adev);
void acx_set_reg_domain(acx_device_t *adev, unsigned char reg_dom_id);
void acx_update_capabilities(acx_device_t *adev);
void acx_s_start(acx_device_t *adev);

void acx_s_update_card_settings(acx_device_t *adev);
void acx_s_parse_configoption(acx_device_t *adev, const acx111_ie_configoption_t *pcfg);

void acx_init_task_scheduler(acx_device_t *adev);
void acx_schedule_task(acx_device_t *adev, unsigned int set_flag);

int acx_e_ioctl_old(struct net_device *ndev, struct ifreq *ifr, int cmd);

client_t *acx_l_sta_list_get(acx_device_t *adev, const u8 *address);
void acx_l_sta_list_del(acx_device_t *adev, client_t *clt);

void acx_i_timer(unsigned long a);

struct sk_buff *acx_rxbuf_to_ether(acx_device_t *adev, rxbuffer_t *rxbuf);
int acx_ether_to_txbuf(acx_device_t *adev, void *txbuf, const struct sk_buff *skb);

u8 acx_signal_determine_quality(u8 signal, u8 noise);

void acx_l_process_rxbuf(acx_device_t *adev, rxbuffer_t *rxbuf);
void acx_l_handle_txrate_auto(acx_device_t *adev, struct client *txc,
			u16 intended_rate, u8 rate100, u16 rate111, u8 error,
			int pkts_to_ignore);

void acx_dump_bytes(const void *, int);

void acx_s_set_defaults(acx_device_t *adev);

#if !ACX_DEBUG
static inline const char *acx_get_packet_type_string(u16 fc) { return ""; }
#else
const char *acx_get_packet_type_string(u16 fc);
#endif
const char *acx_cmd_status_str(unsigned int state);

/*** mac80211 functions ***/
int acx_setup_modes(acx_device_t *adev);
void acx_free_modes(acx_device_t *adev);
int acx_i_op_tx(struct ieee80211_hw *ieee,	struct sk_buff *skb);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34)
int acx_e_op_add_interface(struct ieee80211_hw* ieee,
		struct ieee80211_if_init_conf *conf);
void acx_e_op_remove_interface(struct ieee80211_hw* ieee,
		struct ieee80211_if_init_conf *conf);
#else
int acx_e_op_add_interface(struct ieee80211_hw* ieee,
		struct ieee80211_vif *vif);
void acx_e_op_remove_interface(struct ieee80211_hw* ieee,
		struct ieee80211_vif *vif);
#endif

int acx_net_reset(struct ieee80211_hw *ieee);
int acx_e_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
		struct ieee80211_vif *vif, struct ieee80211_sta *sta,
		struct ieee80211_key_conf *key);
int acx_e_op_config(struct ieee80211_hw *hw, u32 changed);
void acx_e_op_bss_info_changed(struct ieee80211_hw *hw,
		struct ieee80211_vif *vif, struct ieee80211_bss_conf *info, u32 changed);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 34)
int acx_e_op_get_tx_stats(struct ieee80211_hw* ieee, struct ieee80211_tx_queue_stats *stats);
#endif
int acx_e_conf_tx(struct ieee80211_hw* ieee, u16 queue,
		const struct ieee80211_tx_queue_params *params);
//int acx_passive_scan(struct net_device *net_dev, int state, struct ieee80211_scan_conf *conf);
//static void acx_netdev_init(struct net_device *ndev);

int acxpci_s_reset_dev(acx_device_t *adev);
int acxmem_s_reset_dev(acx_device_t *adev);

void acx_i_op_configure_filter(struct ieee80211_hw *hw,
		unsigned int changed_flags, unsigned int *total_flags, u64 multicast);

/*** End mac80211 Functions **/

void great_inquisitor(acx_device_t *adev);

void acx_s_get_firmware_version(acx_device_t *adev);
void acx_display_hardware_details(acx_device_t *adev);

int acx_e_change_mtu(struct ieee80211_hw *hw, int mtu);
int acx_e_op_get_stats(struct ieee80211_hw *hw, struct ieee80211_low_level_stats *stats);
struct iw_statistics* acx_e_get_wireless_stats(struct ieee80211_hw *hw);

void acxpci_interrupt_tasklet(struct work_struct *work);
void acxusb_interrupt_tasklet(struct work_struct *work);
void acxmem_i_interrupt_tasklet(struct work_struct *work);

// void acx_interrupt_tasklet(acx_device_t *adev);
// OW TODO void acx_e_after_interrupt_task(struct work_struct* work);
void acx_e_after_interrupt_task(acx_device_t *adev);





/*
 * BOM PCI prototypes
 * ==================================================
 */

// Locking (PCI)

// Logging (PCI)

// Data Access (PCI)

// Firmware, EEPROM, Phy (PCI)

// Control Path (CMD handling, init, reset) (PCI)

// CMDs (PCI:Control Path)

// Configure (PCI:Control Path)

// Template (PCI:Control Path)

// Recalibration (PCI:Control Path)

// Other (PCI:Control Path)

// Proc, Debug (PCI)

// Rx Path (PCI)

// Tx Path (PCI)

// Crypto (PCI)

// Irq Handling, Timer (PCI)

// Mac80211 Ops (PCI)

// Helpers (PCI)

// Driver, Module (PCI)


void acxpci_l_power_led(acx_device_t *adev, int enable);
int acxpci_read_eeprom_byte(acx_device_t *adev, u32 addr, u8 *charbuf);
unsigned int acxpci_l_clean_txdesc(acx_device_t *adev);
void acxpci_l_clean_txdesc_emergency(acx_device_t *adev);
int acxpci_s_create_hostdesc_queues(acx_device_t *adev);
void acxpci_create_desc_queues(acx_device_t *adev, u32 tx_queue_start, u32 rx_queue_start);
void acxpci_free_desc_queues(acx_device_t *adev);
int acxpci_s_proc_diag_output(struct seq_file *file, acx_device_t *adev);
int acxpci_proc_eeprom_output(char *p, acx_device_t *adev);
void acxpci_set_interrupt_mask(acx_device_t *adev);
int acx100pci_s_set_tx_level(acx_device_t *adev, u8 level_dbm);

int __init acxpci_e_init_module(void);
void __exit acxpci_e_cleanup_module(void);


/*
 * BOM USB prototypes
 * ==================================================
 */

// Locking (USB)

// Logging (USB)

// Data Access (USB)

// Firmware, EEPROM, Phy (USB)

// Control Path (CMD handling, init, reset) (USB)

// CMDs (USB:Control Path)

// Configure (USB:Control Path)

// Template (USB:Control Path)

// Recalibration (USB:Control Path)

// Other (USB:Control Path)

// Proc, Debug (USB)

// Rx Path (USB)

// Tx Path (USB)

// Crypto (USB)

// Irq Handling, Timer (USB)

// Mac80211 Ops (USB)

// Helpers (USB)

// Driver, Module (USB)



int __init acxusb_e_init_module(void);
void __exit acxusb_e_cleanup_module(void);

/*
 * BOM Mem prototypes
 * ==================================================
 */

// Locking (Mem)

// Logging (Mem)

// Data Access (Mem)

// Firmware, EEPROM, Phy (Mem)

// Control Path (CMD handling, init, reset) (Mem)

// CMDs (Mem:Control Path)

// Configure (Mem:Control Path)

// Template (Mem:Control Path)

// Recalibration (Mem:Control Path)

// Other (Mem:Control Path)

// Proc, Debug (Mem)

// Rx Path (Mem)

// Tx Path (Mem)

// Crypto (Mem)

// Irq Handling, Timer (Mem)

// Mac80211 Ops (Mem)

// Helpers (Mem)

// Driver, Module (Mem)


void acxmem_l_power_led(acx_device_t *adev, int enable);
int acxmem_read_eeprom_byte(acx_device_t *adev, u32 addr, u8 *charbuf);
unsigned int acxmem_l_clean_txdesc(acx_device_t *adev);
void acxmem_l_clean_txdesc_emergency(acx_device_t *adev);
int acxmem_s_create_hostdesc_queues(acx_device_t *adev);
void acxmem_create_desc_queues(acx_device_t *adev, u32 tx_queue_start, u32 rx_queue_start);
void acxmem_free_desc_queues(acx_device_t *adev);
int acxmem_s_proc_diag_output(struct seq_file *file, acx_device_t *adev);
int acxmem_proc_eeprom_output(char *p, acx_device_t *adev);
void acxmem_set_interrupt_mask(acx_device_t *adev);
int acx100mem_s_set_tx_level(acx_device_t *adev, u8 level_dbm);

int __init acxmem_e_init_module(void);
void __exit acxmem_e_cleanup_module(void);

#endif /* _ACX_FUNC_H_ */
