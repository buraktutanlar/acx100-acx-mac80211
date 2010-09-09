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

/*
 * BOM Config
 * ==================================================
 */
#define CMD_TIMEOUT_MS(n)	(n)
#define ACX_CMD_TIMEOUT_DEFAULT	CMD_TIMEOUT_MS(50)

// CONFIG_ACX_MAC80211_VERSION allows to specify the version of the used
// wireless mac80211 api, in case it is different of the used kernel.
// OpenWRT e.g. uses a version of compat-wireless, which is ahead of
// the used kernel.
//
// CONFIG_ACX_MAC80211_VERSION can be defined on the make command line by
// passing EXTRA_CFLAGS="-DCONFIG_ACX_MAC80211_VERSION=\"KERNEL_VERSION(2,6,34)\""

#ifndef CONFIG_ACX_MAC80211_VERSION
	#define CONFIG_ACX_MAC80211_VERSION LINUX_VERSION_CODE
#endif

// Define ACX_GIT_VERSION with "undef" value, if undefined for some reason
#ifndef ACX_GIT_VERSION
        #define ACX_GIT_VERSION "unknown"
#endif

/*
 * BOM Common
 * ==================================================
 */

// BOM Locking (Common)
// -----

/*
 * Locking is done mainly using the adev->sem.
 *
 * The locking rule is: All external entry paths are protected by the sem.
 *
 * The adev->spinlock is still kept for the irq top-half, although even there it
 * wouldn't be really required. It's just to not get interrupted during irq
 * handling itself. For this we don't need the acx_lock macros anymore.
 * 
 */

/* These functions *must* be inline or they will break horribly on SPARC, due
 * to its weird semantics for save/restore flags */

#define acx_sem_lock(adev)		mutex_lock(&(adev)->mutex)
#define acx_sem_unlock(adev)	mutex_unlock(&(adev)->mutex)

#ifdef OW_20100613_OBSELETE_ACXLOCK_REMOVE
#if defined(PARANOID_LOCKING) /* Lock debugging */

void acx_lock_debug(acx_device_t *adev, const char* where);
void acx_unlock_debug(acx_device_t *adev, const char* where);
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
#ifdef OBSELETE_OW20100613
#define acx_lock(adev, flags)	acx_lock_helper(adev, &(flags), __FILE__ ":" STRING(__LINE__))
#define acx_unlock(adev, flags)	acx_unlock_helper(adev, &(flags), __FILE__ ":" STRING(__LINE__))
#endif

#elif defined(DO_LOCKING)

//#define acx_lock(adev, flags)	spin_lock_irqsave(&adev->spinlock, flags)
//#define acx_unlock(adev, flags)	spin_unlock_irqrestore(&adev->spinlock, flags)
#define acx_lock(adev, flags)	((void)0)
#define acx_unlock(adev, flags)	((void)0)

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
#endif


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

char *acx_print_mac(char *buf, const u8 *mac);
void acx_print_mac2(const char *head, const u8 *mac, const char *tail);
void acxlog_mac(int level, const char *head, const u8 *mac, const char *tail);

void acx_dump_bytes(const void *data, int num);
const char *acx_cmd_status_str(unsigned int state);

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

#define printk_ratelimited(args...) \
do { \
	if (printk_ratelimit()) \
		printk(args); \
} while (0)

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

#define TODO()  \
        do {                                                                            \
                printk(KERN_INFO "TODO: Incomplete code in %s() at %s:%d\n",        \
                       __FUNCTION__, __FILE__, __LINE__);                               \
        } while (0)

#define FIXME()  \
        do {                                                                            \
                printk(KERN_INFO "FIXME: Possibly broken code in %s() at %s:%d\n",  \
                       __FUNCTION__, __FILE__, __LINE__);                               \
        } while (0)

// BOM Data Access (Common)
// -----

// BOM Firmware, EEPROM, Phy (Common)
// -----
void acx_s_get_firmware_version(acx_device_t * adev);
void acx_display_hardware_details(acx_device_t * adev);
firmware_image_t *acx_s_read_fw(struct device *dev, const char *file, u32 * size);
void acx_s_parse_configoption(acx_device_t * adev, const acx111_ie_configoption_t * pcfg);
int acx_s_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
int acx_s_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);

// BOM CMDs (Common:Control Path)
// -----

int acx_s_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr);
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

void acx_s_cmd_join_bssid(acx_device_t *adev, const u8 *bssid);

// BOM Configuration (Common:Control Path)
// -----
void acx_s_set_defaults(acx_device_t * adev);
void acx_s_update_card_settings(acx_device_t *adev);
void acx_s_start(acx_device_t * adev);
int acx_net_reset(struct ieee80211_hw *ieee);
int acx_s_init_mac(acx_device_t * adev);
int acx_setup_modes(acx_device_t *adev);
int acx_selectchannel(acx_device_t *adev, u8 channel, int freq);
// void acx_update_capabilities(acx_device_t *adev);

// BOM Template (Common:Control Path)
// -----

// BOM Recalibration (Common:Control Path)
// -----

// BOM Other (Common:Control Path)
// -----

// BOM Proc, Debug (Common)
// -----
#ifdef CONFIG_PROC_FS
int acx_proc_register_entries(struct ieee80211_hw *ieee, int num);
int acx_proc_unregister_entries(struct ieee80211_hw *ieee, int num);
#else
static inline int
acx_proc_register_entries(const struct ieee80211_hw *ieee) { return OK; }
static inline int
acx_proc_unregister_entries(const struct ieee80211_hw *ieee) { return OK; }
#endif

// BOM Rx Path (Common)
// -----
void acx_l_process_rxbuf(acx_device_t *adev, rxbuffer_t *rxbuf);


// BOM Tx Path (Common)
// -----
int acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
void acx_tx_work(struct work_struct *work);
void acx_tx_queue_go(acx_device_t *adev);
int acx_tx_frame(acx_device_t *adev, struct sk_buff *skb);
void acx_tx_queue_flush(acx_device_t *adev);
void acx_stop_queue(struct ieee80211_hw *hw, const char *msg);
int acx_queue_stopped(struct ieee80211_hw *ieee);
void acx_wake_queue(struct ieee80211_hw *hw, const char *msg);
tx_t* acx_l_alloc_tx(acx_device_t *adev, unsigned int len);
void acxpcimem_handle_tx_error(acx_device_t *adev, u8 error, unsigned int finger, struct ieee80211_tx_info *info);

//void acx_l_handle_txrate_auto(acx_device_t *adev, struct client *txc,
//		u16 intended_rate, u8 rate100, u16 rate111, u8 error,
//		int pkts_to_ignore);


// BOM Crypto (Common)
// -----
int acx_clear_keys(acx_device_t * adev);
int acx_key_write(acx_device_t *adev, u16 index, u8 algorithm, const struct ieee80211_key_conf *key, const u8 *mac_addr);

// BOM Irq Handling, Timer (Common)
// -----
void acx_init_task_scheduler(acx_device_t *adev);
void acx_e_after_interrupt_task(acx_device_t *adev);
void acx_schedule_task(acx_device_t *adev, unsigned int set_flag);
void acx_log_irq(u16 irqtype);
void acx_i_timer(unsigned long address);
void acx_set_timer(acx_device_t * adev, int timeout_us);

// BOM Mac80211 Ops (Common)
// -----

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
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

int acx_e_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
		struct ieee80211_vif *vif, struct ieee80211_sta *sta,
		struct ieee80211_key_conf *key);
int acx_e_op_config(struct ieee80211_hw *hw, u32 changed);
void acx_e_op_bss_info_changed(struct ieee80211_hw *hw,
		struct ieee80211_vif *vif, struct ieee80211_bss_conf *info, u32 changed);

void acx_i_op_configure_filter(struct ieee80211_hw *hw,
		unsigned int changed_flags, unsigned int *total_flags, u64 multicast);
int acx_e_conf_tx(struct ieee80211_hw* ieee, u16 queue,
		const struct ieee80211_tx_queue_params *params);
int acx_e_op_get_stats(struct ieee80211_hw *hw, struct ieee80211_low_level_stats *stats);

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
int acx_e_op_get_tx_stats(struct ieee80211_hw* ieee, struct ieee80211_tx_queue_stats *stats);
#endif

// BOM Helpers (Common)
// -----

void acx_s_mwait(int ms);
u8 acx_signal_determine_quality(u8 signal, u8 noise);
// void great_inquisitor(acx_device_t *adev);

#if !ACX_DEBUG
static inline const char *acx_get_packet_type_string(u16 fc) { return ""; }
#else
const char *acx_get_packet_type_string(u16 fc);
#endif

// MAC address helpers
// ---
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
// ---
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

// More random helpers
// ---
static inline struct ieee80211_hdr*
acx_get_wlan_hdr(acx_device_t *adev, const rxbuffer_t *rxbuf)
{
	return (struct ieee80211_hdr *)((u8 *)&rxbuf->hdr_a3 + adev->phy_header_len);
}

// BOM Driver, Module (Common)
// -----


/*
 * BOM PCI prototypes
 * ==================================================
 */

// Data Access

int acxpci_s_create_hostdesc_queues(acx_device_t * adev);
void acxpci_create_desc_queues(acx_device_t * adev, u32 tx_queue_start, u32 rx_queue_start);
void acxpci_free_desc_queues(acx_device_t * adev);

// Firmware, EEPROM, Phy
int acxpci_s_upload_radio(acx_device_t * adev);
int acxpci_read_eeprom_byte(acx_device_t * adev, u32 addr, u8 * charbuf);
// int acxpci_s_write_eeprom(acx_device_t * adev, u32 addr, u32 len, const u8 * charbuf);
int acxpci_s_read_phy_reg(acx_device_t * adev, u32 reg, u8 * charbuf);
int acxpci_s_write_phy_reg(acx_device_t * adev, u32 reg, u8 value);

// CMDs (Control Path)
int acxpci_s_issue_cmd_timeo_debug(acx_device_t * adev, unsigned cmd, void *buffer, unsigned buflen, unsigned cmd_timeout, const char *cmdstr);

// Init, Configuration (Control Path)
int acxpci_s_reset_dev(acx_device_t * adev);

// Other (Control Path)

// Proc, Debug
int acxpci_s_proc_diag_output(struct seq_file *file, acx_device_t *adev);
int acxpci_proc_eeprom_output(char *buf, acx_device_t * adev);

// Rx Path

// Tx Path
tx_t *acxpci_l_alloc_tx(acx_device_t * adev);
void *acxpci_l_get_txbuf(acx_device_t * adev, tx_t * tx_opaque);
void acxpci_l_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len, struct ieee80211_tx_info *ieeectl, struct sk_buff *skb);
unsigned int acxpci_l_clean_txdesc(acx_device_t * adev);
void acxpci_l_clean_txdesc_emergency(acx_device_t * adev);
int acx100pci_s_set_tx_level(acx_device_t * adev, u8 level_dbm);

// Irq Handling, Timer
void acxpci_irq_work(struct work_struct *work);
void acxpci_set_interrupt_mask(acx_device_t * adev);

// Mac80211 Ops

// Helpers
void acxpci_l_power_led(acx_device_t * adev, int enable);

// Ioctls
int acx111pci_ioctl_info(struct net_device *ndev, struct iw_request_info *info, struct iw_param *vwrq, char *extra);
int acx100pci_ioctl_set_phy_amp_bias(struct net_device *ndev, struct iw_request_info *info, struct iw_param *vwrq, char *extra);

// Driver, Module

int __init acxpci_e_init_module(void);
void __exit acxpci_e_cleanup_module(void);

/*
 * BOM USB prototypes
 * ==================================================
 */

// Logging

// Data Access

// Firmware, EEPROM, Phy
int acxusb_s_read_phy_reg(acx_device_t * adev, u32 reg, u8 * charbuf);
int acxusb_s_write_phy_reg(acx_device_t * adev, u32 reg, u8 value);

// CMDs (Control Path)
int acxusb_s_issue_cmd_timeo_debug(acx_device_t * adev, unsigned cmd, void *buffer, unsigned buflen, unsigned timeout, const char *cmdstr);

// Init, Configure (Control Path)

// Other (Control Path)

// Proc, Debug

// Rx Path

// Tx Path
tx_t *acxusb_l_alloc_tx(acx_device_t *adev);
void acxusb_l_dealloc_tx(tx_t * tx_opaque);
void *acxusb_l_get_txbuf(acx_device_t * adev, tx_t * tx_opaque);
void acxusb_l_tx_data(acx_device_t *adev, tx_t *tx_opaque, int wlanpkt_len, struct ieee80211_tx_info *ieeectl, struct sk_buff *skb);

// Irq Handling, Timer
void acxusb_irq_work(struct work_struct *work);

// Mac80211 Ops

// Helpers

// Driver, Module
int __init acxusb_e_init_module(void);
void __exit acxusb_e_cleanup_module(void);

/*
 * BOM Mem prototypes
 * ==================================================
 */
// Data Access
int acxmem_create_hostdesc_queues(acx_device_t *adev);
void acxmem_create_desc_queues(acx_device_t *adev, u32 tx_queue_start, u32 rx_queue_start);
void acxmem_free_desc_queues(acx_device_t *adev);

// Firmware, EEPROM, Phy
int acxmem_upload_radio(acx_device_t *adev);
int acxmem_read_eeprom_byte(acx_device_t *adev, u32 addr, u8 *charbuf);
#ifdef UNUSED
int acxmem_s_write_eeprom(acx_device_t *adev, u32 addr, u32 len, const u8 *charbuf);
#endif
int acxmem_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
int acxmem_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);

// CMDs (Control Path)
int acxmem_s_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *buffer, unsigned buflen, unsigned cmd_timeout, const char* cmdstr);

// Init, Configure (Control Path)
int acxmem_s_reset_dev(acx_device_t *adev);

// Other (Control Path)

// Proc, Debug
int acxmem_s_proc_diag_output(struct seq_file *file, acx_device_t *adev);
int acxmem_proc_eeprom_output(char *buf, acx_device_t *adev);

// Rx Path

// Tx Path
tx_t *acxmem_l_alloc_tx(acx_device_t *adev, unsigned int len);
void acxmem_l_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque);

void *acxmem_l_get_txbuf(acx_device_t *adev, tx_t *tx_opaque);
void acxmem_init_acx_txbuf2(acx_device_t *adev);

void acxmem_l_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len, struct ieee80211_tx_info *ieeectl, struct sk_buff *skb);
unsigned int acxmem_l_clean_txdesc(acx_device_t *adev);
void acxmem_l_clean_txdesc_emergency(acx_device_t *adev);

void acxmem_update_queue_indicator(acx_device_t *adev, int txqueue);
int acx100mem_s_set_tx_level(acx_device_t *adev, u8 level_dbm);

// Irq Handling, Timer
void acxmem_irq_work(struct work_struct *work);
void acxmem_set_interrupt_mask(acx_device_t *adev);

// Helpers
void acxmem_l_power_led(acx_device_t *adev, int enable);

// Ioctls
//int acx111pci_ioctl_info(struct ieee80211_hw *hw, struct iw_request_info *info, struct iw_param *vwrq, char *extra);
//int acx100mem_ioctl_set_phy_amp_bias(struct ieee80211_hw *hw, struct iw_request_info *info, struct iw_param *vwrq, char *extra);

int __init acxmem_e_init_module(void);
void __exit acxmem_e_cleanup_module(void);

#endif /* _ACX_FUNC_H_ */
