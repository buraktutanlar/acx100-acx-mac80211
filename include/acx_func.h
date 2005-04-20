/* include/acx100_helper.h
 *
 * --------------------------------------------------------------------
 *
 * Copyright (C) 2003  ACX100 Open Source Project
 *
 *   The contents of this file are subject to the Mozilla Public
 *   License Version 1.1 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.mozilla.org/MPL/
 *
 *   Software distributed under the License is distributed on an "AS
 *   IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *   implied. See the License for the specific language governing
 *   rights and limitations under the License.
 *
 *   Alternatively, the contents of this file may be used under the
 *   terms of the GNU Public License version 2 (the "GPL"), in which
 *   case the provisions of the GPL are applicable instead of the
 *   above.  If you wish to allow the use of your version of this file
 *   only under the terms of the GPL and not to allow others to use
 *   your version of this file under the MPL, indicate your decision
 *   by deleting the provisions above and replace them with the notice
 *   and other provisions required by the GPL.  If you do not delete
 *   the provisions above, a recipient may use your version of this
 *   file under either the MPL or the GPL.
 *
 * --------------------------------------------------------------------
 *
 * Inquiries regarding the ACX100 Open Source Project can be
 * made directly to:
 *
 * acx100-users@lists.sf.net
 * http://acx100.sf.net
 *
 * --------------------------------------------------------------------
 */

/*============================================================================*
 * Debug / log functionality                                                  *
 *============================================================================*/

/* NOTE: If we still want basic logging of driver info if ACX_DEBUG is not
 * defined, we should provide an acxlog variant that is never turned off. We
 * should make a acx_msg(), and rename acxlog() to acx_debug() to make the
 * difference very clear.
 */

#if ACX_DEBUG

#define acxlog(chan, args...) \
	do { \
		if (debug & (chan)) \
			printk(KERN_WARNING args); \
	} while (0)

void log_fn_enter(const char *funcname);
void log_fn_exit(const char *funcname);
void log_fn_exit_v(const char *funcname, int v);

#define FN_ENTER \
	do { \
		if (unlikely(debug & L_FUNC)) { \
			log_fn_enter(__func__); \
		} \
	} while (0)

#define FN_EXIT1(v) \
	do { \
		if (unlikely(debug & L_FUNC)) { \
			log_fn_exit_v(__func__, v); \
		} \
	} while (0)
#define FN_EXIT0() \
	do { \
		if (unlikely(debug & L_FUNC)) { \
			log_fn_exit(__func__); \
		} \
	} while (0)

#else

#define acxlog(chan, args...)
#define FN_ENTER
#define FN_EXIT1(v)
#define FN_EXIT0()

#endif /* ACX_DEBUG */

/*============================================================================*
 * Locking and synchronization functions                                      *
 *============================================================================*/

/* These functions *must* be inline or they will break horribly on SPARC, due
 * to its weird semantics for save/restore flags. extern inline should prevent
 * the kernel from linking or module from loading if they are not inlined. */

#ifdef BROKEN_LOCKING
extern inline int acx_lock(wlandevice_t *priv, unsigned long *flags)
{
	local_irq_save(*flags);
	if (!spin_trylock(&priv->lock)) {
		printk("ARGH! Lock already taken in %s\n", __func__);
		local_irq_restore(*flags);
		return -EFAULT;
	} else {
		printk("Lock given out in %s\n", __func__);
	}
	if (priv->hw_unavailable) {
		printk(KERN_WARNING
		       "acx_lock() called with hw_unavailable (dev=%p)\n",
		       priv->netdev);
		spin_unlock_irqrestore(&priv->lock, *flags);
		return -EBUSY;
	}
	return OK;
}

extern inline void acx_unlock(wlandevice_t *priv, unsigned long *flags)
{
	/* printk(KERN_WARNING "unlock\n"); */
	spin_unlock_irqrestore(&priv->lock, *flags);
	/* printk(KERN_WARNING "/unlock\n"); */
}

#else /* BROKEN_LOCKING */

extern inline int acx_lock(wlandevice_t *priv, unsigned long *flags)
{
	/* do nothing and be quiet */
	/*@-noeffect@*/
	(void)*priv;
	(void)*flags;
	/*@=noeffect@*/
	return OK;
}

extern inline void acx_unlock(wlandevice_t *priv, unsigned long *flags)
{
	/* do nothing and be quiet */
	/*@-noeffect@*/
	(void)*priv;
	(void)*flags;
	/*@=noeffect@*/
}
#endif /* BROKEN_LOCKING */
static inline void acx_stop_queue(netdevice_t *dev, const char *msg)
{
	netif_stop_queue(dev);
	if (msg)
		acxlog(L_BUFT, "tx: stop queue %s\n", msg);
}

static inline int acx_queue_stopped(netdevice_t *dev)
{
	return netif_queue_stopped(dev);
}

static inline void acx_start_queue(netdevice_t *dev, const char *msg)
{
	netif_start_queue(dev);
	if (msg)
		acxlog(L_BUFT, "tx: start queue %s\n", msg);
}

static inline void acx_wake_queue(netdevice_t *dev, const char *msg)
{
	netif_wake_queue(dev);
	if (msg)
		acxlog(L_BUFT, "tx: wake queue %s\n", msg);
}

static inline void acx_carrier_off(netdevice_t *dev, const char *msg)
{
	netif_carrier_off(dev);
	if (msg)
		acxlog(L_BUFT, "tx: carrier off %s\n", msg);
}

static inline void acx_carrier_on(netdevice_t *dev, const char *msg)
{
	netif_carrier_on(dev);
	if (msg)
		acxlog(L_BUFT, "tx: carrier on %s\n", msg);
}



void acx_schedule(long timeout);
int acx_reset_dev(netdevice_t *dev);
void acx_cmd_join_bssid(wlandevice_t *priv, const u8 *bssid);
int acx_init_mac(netdevice_t *dev, u16 init);
void acx_set_reg_domain(wlandevice_t *priv, unsigned char reg_dom_id);
void acx_set_timer(wlandevice_t *priv, u32 time);
void acx_update_capabilities(wlandevice_t *priv);
u16 acx_read_eeprom_offset(wlandevice_t *priv, u32 addr,
					u8 *charbuf);
u16 acx_read_eeprom_area(wlandevice_t *priv);
u16 acx_write_eeprom_offset(wlandevice_t *priv, u32 addr,
					u32 len, const u8 *charbuf);
u16 acx_read_phy_reg(wlandevice_t *priv, u32 reg, u8 *charbuf);
u16 acx_write_phy_reg(wlandevice_t *priv, u32 reg, u8 value);
void acx_start(wlandevice_t *priv);
void acx_reset_mac(wlandevice_t *priv);
#ifdef USE_FW_LOADER_26
firmware_image_t *acx_read_fw(struct device *dev, const char *file, u32 *size);
#else
firmware_image_t *acx_read_fw(const char *file, u32 *size);
#endif
void acx100_set_wepkey(wlandevice_t *priv);
void acx111_set_wepkey(wlandevice_t *priv);
int acx100_init_wep(wlandevice_t *priv);
void acx_initialize_rx_config(wlandevice_t *priv);
void acx_update_card_settings(wlandevice_t *priv, int init, int get_all, int set_all);
void acx_init_task_scheduler(wlandevice_t *priv);
void acx_flush_task_scheduler(void);
void acx_schedule_after_interrupt_task(wlandevice_t *priv, unsigned int set_flag);
void acx_cmd_start_scan(wlandevice_t *priv);
int acx_upload_radio(wlandevice_t *priv);
void acx_read_configoption(wlandevice_t *priv);
u16 acx_proc_register_entries(const struct net_device *dev);
u16 acx_proc_unregister_entries(const struct net_device *dev);
void acx_update_dot11_ratevector(wlandevice_t *priv);

int acx_recalib_radio(wlandevice_t *priv);
int acx111_get_feature_config(wlandevice_t *priv, u32 *feature_options, u32 *data_flow_options);
int acx111_set_feature_config(wlandevice_t *priv, u32 feature_options, u32 data_flow_options, int mode /* 0 == remove, 1 == add, 2 == set */);
static inline int acx111_feature_off(wlandevice_t *priv, u32 f, u32 d)
{
	return acx111_set_feature_config(priv, f, d, 0);
}
static inline int acx111_feature_on(wlandevice_t *priv, u32 f, u32 d)
{
	return acx111_set_feature_config(priv, f, d, 1);
}
static inline int acx111_feature_set(wlandevice_t *priv, u32 f, u32 d)
{
	return acx111_set_feature_config(priv, f, d, 2);
}



/* acx100_ioctl.c */
int acx_ioctl_old(netdevice_t *dev, struct ifreq *ifr, int cmd);

void acx_sta_list_init(wlandevice_t *priv);
client_t *acx_sta_list_get(wlandevice_t *priv, const u8 *address);
client_t *acx_sta_list_add(wlandevice_t *priv, const u8 *address);
client_t *acx_sta_list_get_or_add(wlandevice_t *priv, const u8 *address);
void acx_sta_list_del(wlandevice_t *priv, client_t *clt);

void acx_set_status(wlandevice_t *priv, u16 status);
int acx_rx_ieee802_11_frame(wlandevice_t *priv, rxhostdesc_t *desc);
u32 acx_transmit_disassoc(client_t *arg_0, wlandevice_t *priv);
void acx_timer(unsigned long a);
void acx_complete_dot11_scan(wlandevice_t *priv);

#define COUNT_STATE_STR	6

static inline const char *acx_get_status_name(u16 status)
{
	extern const char * const g_wlan_state_str[COUNT_STATE_STR];
	return g_wlan_state_str[
		(status < COUNT_STATE_STR) ?
			status : COUNT_STATE_STR-1
		];
}

static inline p80211_hdr_t *acx_get_p80211_hdr(wlandevice_t *priv, const rxbuffer_t *rxbuf)
{
	if (!(priv->rx_config_1 & RX_CFG1_INCLUDE_PHY_HDR))
		return (p80211_hdr_t *)&rxbuf->hdr_a3;

	/* take into account phy header in front of packet */
	if(CHIPTYPE_ACX111 == priv->chip_type)
		return (p80211_hdr_t*)((u8*)&rxbuf->hdr_a3 + 8);
		
	return (p80211_hdr_t*)((u8*)&rxbuf->hdr_a3 + 4);
}



/*============================================================================*
 * Function Declarations                                                      *
 *============================================================================*/

int acx_ether_to_txdesc(struct wlandevice *priv,
			struct txdescriptor *txdesc, const struct sk_buff *skb);
struct sk_buff *acx_rxdesc_to_ether(struct wlandevice *priv,
			const struct rxhostdescriptor *rxdesc);
void acx_rxdesc_to_txdesc(const struct rxhostdescriptor *rxhostdesc,
			struct txdescriptor *txdesc);


#if (WLAN_HOSTIF!=WLAN_USB) /* must be used for non-USB only */
#define INLINE_IO static inline /* undefine for out-of-line */
#include <acx_inline.h>
#endif /* (WLAN_HOSTIF!=WLAN_USB) */

#define ACX_CMD_TIMEOUT_DEFAULT	5000

void acx_get_info_state(wlandevice_t *priv);
void acx_get_cmd_state(wlandevice_t *priv);
void acx_write_cmd_type_or_status(wlandevice_t *priv, u32 val, unsigned int is_status);
int acx_issue_cmd(wlandevice_t *priv, unsigned int cmd, /*@null@*/ void *pcmdparam,
		     unsigned int paramlen, u32 timeout);
int acx_configure(wlandevice_t *priv, void *pdr, short type);
int acx_configure_length(wlandevice_t *priv, void *pdr, short type,
			    short length);
int acx_interrogate(wlandevice_t *priv, void *pdr, short type);
void acx_power_led(wlandevice_t *priv, u8 enable);

void acx_select_io_register_set(wlandevice_t *priv, u16 chip_type);

static inline void acx_log_mac_address(int level, const u8 *mac, const char *tail)
{
	if (debug & level) {
		printk(MACSTR"%s", MAC(mac), tail);
	}
}



/* former idma.h */
int acx100_create_dma_regions(wlandevice_t *priv);
int acx111_create_dma_regions(wlandevice_t *priv);
int acx_delete_dma_regions(wlandevice_t *priv);
void acx_dma_tx_data(wlandevice_t *wlandev, struct txdescriptor *txdesc);
void acx_clean_tx_desc(wlandevice_t *priv);
void acx_clean_tx_desc_emergency(wlandevice_t *priv);
u8 acx_signal_to_winlevel(u8 rawlevel);
void acx_process_rx_desc(wlandevice_t *priv);
struct txdescriptor *acx_get_tx_desc(wlandevice_t *priv);
