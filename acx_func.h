#ifndef _ACX_FUNC_H_
#define _ACX_FUNC_H_

/** (legal) claimer in README
** Copyright (C) 2003  ACX100 Open Source Project
*/

#include <linux/version.h>
#include "acx_debug.h"
#include "acx_log.h"
/*
 * FIXME: this file is needed only so that the lock debugging functions have an
 * acx_device_t structure to play with :(
 */
#include "acx_struct.h"

void acx_print_mac(const char *head, const u8 *mac, const char *tail);

/* Optimized out to nothing in non-debug build */

/***********************************************************************
** MAC address helpers
*/
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


/***********************************************************************
** Random helpers
*/
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

/***********************************************************************
** LOCKING
** We have adev->sem and adev->spinlock.
**
** We employ following naming convention in order to get locking right:
**
** acx_e_xxxx - external entry points called from process context.
**	It is okay to sleep. adev->sem is to be taken on entry.
** acx_i_xxxx - external entry points possibly called from atomic context.
**	Sleeping is not allowed (and thus down(sem) is not legal!)
** acx_s_xxxx - potentially sleeping functions. Do not ever call under lock!
** acx_l_xxxx - functions which expect lock to be already taken.
** rest       - non-sleeping functions which do not require locking
**		but may be run under lock
**
** A small number of local helpers do not have acx_[eisl]_ prefix.
** They are always close to caller and are to be reviewed locally.
**
** Theory of operation:
**
** All process-context entry points (_e_ functions) take sem
** immediately. IRQ handler and other 'atomic-context' entry points
** (_i_ functions) take lock immediately on entry, but dont take sem
** because that might sleep.
**
** Thus *all* code is either protected by sem or lock, or both.
**
** Code which must not run concurrently with IRQ takes lock.
** Such code is marked with _l_.
**
** This results in the following rules of thumb useful in code review:
**
** + If a function calls _s_ fn, it must be an _s_ itself.
** + You can call _l_ fn only (a) from another _l_ fn
**   or (b) from _s_, _e_ or _i_ fn by taking lock, calling _l_,
**   and dropping lock.
** + All IRQ code runs under lock.
** + Any _s_ fn is running under sem.
** + Code under sem can race only with IRQ code.
** + Code under sem+lock cannot race with anything.
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
{/*
	if (netif_queue_stopped(ndev))
		return;
*/
	ieee80211_stop_queues(hw);
	if (msg)
		acx_log(LOG_DEBUG, L_BUFT, "tx: stop queue %s\n", msg);
}

/*static inline int
acx_queue_stopped(struct ieee80211_hw *ieee)
{
	return netif_queue_stopped(ieee);
}


static inline void
acx_start_queue(struct ieee80211_hw *hw, const char *msg)
{
	ieee80211_start_queues(hw);
	if (msg)
		log(L_BUFT, "tx: start queue %s\n", msg);
}
*/
static inline void
acx_wake_queue(struct ieee80211_hw *hw, const char *msg)
{
	ieee80211_wake_queues(hw);
	if (msg)
		acx_log(LOG_DEBUG, L_BUFT, "tx: wake queue %s\n", msg);
}
/*
static inline void
acx_carrier_off(struct net_device *ndev, const char *msg)
{
	netif_carrier_off(ndev);
	if (msg)
		log(L_BUFT, "tx: carrier off %s\n", msg);
}

static inline void
acx_carrier_on(struct net_device *ndev, const char *msg)
{
	netif_carrier_on(ndev);
	if (msg)
		log(L_BUFT, "tx: carrier on %s\n", msg);
}

*/


/***********************************************************************
** Communication with firmware
*/
#define CMD_TIMEOUT_MS(n)	(n)
#define ACX_CMD_TIMEOUT_DEFAULT	CMD_TIMEOUT_MS(50)

#if ACX_DEBUG

/* We want to log cmd names */
int acxpci_s_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr);
int acxusb_s_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr);
static inline int
acx_s_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr)
{
	if (IS_PCI(adev))
		return acxpci_s_issue_cmd_timeo_debug(adev, cmd, param, len, timeout, cmdstr);
	return acxusb_s_issue_cmd_timeo_debug(adev, cmd, param, len, timeout, cmdstr);
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
static inline int
acx_s_issue_cmd_timeo(acx_device_t *adev, unsigned cmd,	void *param, unsigned len, unsigned timeout)
{
	if (IS_PCI(adev))
		return acxpci_s_issue_cmd_timeo(adev, cmd, param, len, timeout);
	return acxusb_s_issue_cmd_timeo(adev, cmd, param, len, timeout);
}
static inline int
acx_s_issue_cmd(acx_device_t *adev, unsigned cmd, void *param, unsigned len)
{
	if (IS_PCI(adev))
		return acxpci_s_issue_cmd_timeo(adev, cmd, param, len, ACX_CMD_TIMEOUT_DEFAULT);
	return acxusb_s_issue_cmd_timeo(adev, cmd, param, len, ACX_CMD_TIMEOUT_DEFAULT);
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
int acx_proc_register_entries(struct ieee80211_hw *ieee);
int acx_proc_unregister_entries(struct ieee80211_hw *ieee);
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


/***********************************************************************
** Unsorted yet :)
*/
int acxpci_s_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
int acxusb_s_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
static inline int
acx_s_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf)
{
	if (IS_PCI(adev))
		return acxpci_s_read_phy_reg(adev, reg, charbuf);
	return acxusb_s_read_phy_reg(adev, reg, charbuf);
}

int acxpci_s_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);
int acxusb_s_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);
static inline int
acx_s_write_phy_reg(acx_device_t *adev, u32 reg, u8 value)
{
	if (IS_PCI(adev))
		return acxpci_s_write_phy_reg(adev, reg, value);
	return acxusb_s_write_phy_reg(adev, reg, value);
}

tx_t* acxpci_l_alloc_tx(acx_device_t *adev);
tx_t* acxusb_l_alloc_tx(acx_device_t *adev);
static inline tx_t*
acx_l_alloc_tx(acx_device_t *adev)
{
	if (IS_PCI(adev))
		return acxpci_l_alloc_tx(adev);
	return acxusb_l_alloc_tx(adev);
}

void acxusb_l_dealloc_tx(tx_t *tx_opaque);
static inline void
acx_l_dealloc_tx(acx_device_t *adev, tx_t *tx_opaque)
{
	if (IS_USB(adev))
		acxusb_l_dealloc_tx(tx_opaque);
}

void* acxpci_l_get_txbuf(acx_device_t *adev, tx_t *tx_opaque);
void* acxusb_l_get_txbuf(acx_device_t *adev, tx_t *tx_opaque);
static inline void*
acx_l_get_txbuf(acx_device_t *adev, tx_t *tx_opaque)
{
	if (IS_PCI(adev))
		return acxpci_l_get_txbuf(adev, tx_opaque);
	return acxusb_l_get_txbuf(adev, tx_opaque);
}

void acxpci_l_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
                        struct ieee80211_tx_control *ieeectl, struct sk_buff *skb);
void acxusb_l_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len, struct ieee80211_tx_control *ieeectl,
			struct sk_buff *skb);
static inline void
acx_l_tx_data(acx_device_t *adev, tx_t *tx_opaque, int len,
                        struct ieee80211_tx_control *ieeectl, struct sk_buff *skb)
{
	if (IS_PCI(adev))
		acxpci_l_tx_data(adev, tx_opaque, len, ieeectl,skb);
	else
		acxusb_l_tx_data(adev, tx_opaque, len, ieeectl,skb);
}

static inline struct ieee80211_hdr *
acx_get_wlan_hdr(acx_device_t *adev, const rxbuffer_t *rxbuf)
{
	return (struct ieee80211_hdr *)((u8 *)&rxbuf->hdr_a3 + adev->phy_header_len);
}
void acxpci_l_power_led(acx_device_t *adev, int enable);
int acxpci_read_eeprom_byte(acx_device_t *adev, u32 addr, u8 *charbuf);
unsigned int acxpci_l_clean_txdesc(acx_device_t *adev);
void acxpci_l_clean_txdesc_emergency(acx_device_t *adev);
int acxpci_s_create_hostdesc_queues(acx_device_t *adev);
void acxpci_create_desc_queues(acx_device_t *adev, u32 tx_queue_start, u32 rx_queue_start);
void acxpci_free_desc_queues(acx_device_t *adev);
char* acxpci_s_proc_diag_output(char *p, acx_device_t *adev);
int acxpci_proc_eeprom_output(char *p, acx_device_t *adev);
void acxpci_set_interrupt_mask(acx_device_t *adev);
int acx100pci_s_set_tx_level(acx_device_t *adev, u8 level_dbm);

void acx_s_mwait(int ms);
int acx_s_init_mac(acx_device_t *adev);
void acx_set_reg_domain(acx_device_t *adev, unsigned char reg_dom_id);
void acx_update_capabilities(acx_device_t *adev);
void acx_s_start(acx_device_t *adev);

void acx_s_update_card_settings(acx_device_t *adev);
void acx_s_parse_configoption(acx_device_t *adev, const acx111_ie_configoption_t *pcfg);
void acx_l_update_ratevector(acx_device_t *adev);

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

u8 acx_rate111to100(u16);

void acx_s_set_defaults(acx_device_t *adev);

#if !ACX_DEBUG
static inline const char* acx_get_packet_type_string(u16 fc) { return ""; }
#else
const char* acx_get_packet_type_string(u16 fc);
#endif
const char* acx_cmd_status_str(unsigned int state);

/*** Devicescape functions ***/
int acx_setup_modes(acx_device_t *adev);
void acx_free_modes(acx_device_t *adev);
int acx_i_start_xmit(struct ieee80211_hw* ieee, 
			struct sk_buff *skb, 
			struct ieee80211_tx_control *ctl);
int acx_add_interface(struct ieee80211_hw* ieee,
		struct ieee80211_if_init_conf *conf);
void acx_remove_interface(struct ieee80211_hw* ieee,
		struct ieee80211_if_init_conf *conf);
int acx_net_reset(struct ieee80211_hw* ieee);
int acx_net_set_key(struct ieee80211_hw *hw, 
		enum set_key_cmd cmd,
		const u8 *local_addr, const u8 *addr,
		struct ieee80211_key_conf *key);

extern int acx_config_interface(struct ieee80211_hw* ieee,
				struct ieee80211_vif *vif,
				struct ieee80211_if_conf *conf);

int acx_net_config(struct ieee80211_hw* ieee, struct ieee80211_conf *conf);
int acx_net_get_tx_stats(struct ieee80211_hw* ieee, struct ieee80211_tx_queue_stats *stats);
int acx_net_conf_tx(struct ieee80211_hw* ieee, int queue,
		const struct ieee80211_tx_queue_params *params);
//int acx_passive_scan(struct net_device *net_dev, int state, struct ieee80211_scan_conf *conf);
//static void acx_netdev_init(struct net_device *ndev);
int acxpci_s_reset_dev(acx_device_t *adev);
void acx_e_after_interrupt_task(struct work_struct* work);
void acx_i_set_multicast_list(struct ieee80211_hw *hw,
                            unsigned int changed_flags,
                            unsigned int *total_flags,
                            int mc_count, struct dev_addr_list *mc_list);

/*** End DeviceScape Functions **/

void great_inquisitor(acx_device_t *adev);

void acx_s_get_firmware_version(acx_device_t *adev);
void acx_display_hardware_details(acx_device_t *adev);

int acx_e_change_mtu(struct ieee80211_hw *hw, int mtu);
int acx_e_get_stats(struct ieee80211_hw *hw, struct ieee80211_low_level_stats *stats);
struct iw_statistics* acx_e_get_wireless_stats(struct ieee80211_hw *hw);
void acx_interrupt_tasklet(struct work_struct *work);

int __init acxpci_e_init_module(void);
int __init acxusb_e_init_module(void);
void __exit acxpci_e_cleanup_module(void);
void __exit acxusb_e_cleanup_module(void);

#endif /* _ACX_FUNC_H_ */
