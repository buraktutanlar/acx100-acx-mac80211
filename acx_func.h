/***********************************************************************
** Copyright (C) 2003  ACX100 Open Source Project
**
** The contents of this file are subject to the Mozilla Public
** License Version 1.1 (the "License"); you may not use this file
** except in compliance with the License. You may obtain a copy of
** the License at http://www.mozilla.org/MPL/
**
** Software distributed under the License is distributed on an "AS
** IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
** implied. See the License for the specific language governing
** rights and limitations under the License.
**
** Alternatively, the contents of this file may be used under the
** terms of the GNU Public License version 2 (the "GPL"), in which
** case the provisions of the GPL are applicable instead of the
** above.  If you wish to allow the use of your version of this file
** only under the terms of the GPL and not to allow others to use
** your version of this file under the MPL, indicate your decision
** by deleting the provisions above and replace them with the notice
** and other provisions required by the GPL.  If you do not delete
** the provisions above, a recipient may use your version of this
** file under either the MPL or the GPL.
** ---------------------------------------------------------------------
** Inquiries regarding the ACX100 Open Source Project can be
** made directly to:
**
** acx100-users@lists.sf.net
** http://acx100.sf.net
** ---------------------------------------------------------------------
*/


/***********************************************************************
** LOGGING
**
** - Avoid SHOUTING needlessly. Avoid excessive verbosity.
**   Gradually remove messages which are old debugging aids.
**
** - Use printk() for messages which are to be always logged.
**   Supply either 'acx:' or '<devname>:' prefix so that user
**   can figure out who's speaking among other kernel chatter.
**   acx: is for general issues (e.g. "acx: no firmware image!")
**   while <devname>: is related to a particular device
**   (think about multi-card setup). Double check that message
**   is not confusing to the average user.
**
** - use printk KERN_xxx level only if message is not a WARNING
**   but is INFO, ERR etc.
**
** - Use printk_ratelimited() for messages which may flood
**   (e.g. "rx DUP pkt!").
**
** - Use acxlog() for messages which may be omitted (and they
**   _will_ be omitted in non-debug builds). Note that
**   message levels may be disabled at compile-time selectively,
**   thus select them wisely. Example: L_DEBUG is the lowest
**   (most likely to be compiled out) -> use for less important stuff.
**
** - Do not print important stuff with acxlog(), or else people
**   will never build non-debug driver.
**
** Style:
** hex: capital letters, zero filled (e.g. 0x02AC)
** str: dont start from capitals, no trailing periods ("tx: queue is stopped")
*/
#if ACX_DEBUG > 1

void log_fn_enter(const char *funcname);
void log_fn_exit(const char *funcname);
void log_fn_exit_v(const char *funcname, int v);

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

#else

#define FN_ENTER
#define FN_EXIT1(v)
#define FN_EXIT0

#endif /* ACX_DEBUG > 1 */


#if ACX_DEBUG

#define acxlog(chan, args...) \
	do { \
		if (acx_debug & (chan)) \
			printk(args); \
	} while (0)
#define printk_ratelimited(args...) printk(args)

#else /* Non-debug build: */

#define acxlog(chan, args...)
/* Standard way of log flood prevention */
#define printk_ratelimited(args...) \
do { \
	if (printk_ratelimit()) \
		printk(args); \
} while (0)

#endif /* ACX_DEBUG */

void acx_print_mac(const char *head, const u8 *mac, const char *tail);

/* Optimized out to nothing in non-debug build */
static inline void
acxlog_mac(int level, const char *head, const u8 *mac, const char *tail)
{
	if (acx_debug & level) {
		acx_print_mac(head, mac, tail);
	}
}


/***********************************************************************
** MAC address helpers
*/
static inline void
MAC_COPY(u8 *mac, const u8 *src)
{
	*(u32*)mac = *(u32*)src;
	((u16*)mac)[2] = ((u16*)src)[2];
	/* kernel's memcpy will do the same: memcpy(dst, src, ETH_ALEN); */
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
	/* AND together 4 first bytes with sign-entended 2 last bytes
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


/***********************************************************************
** Random helpers
*/
#define TO_STRING(x)	#x
#define STRING(x)	TO_STRING(x)

#define CLEAR_BIT(val, mask) ((val) &= ~(mask))
#define SET_BIT(val, mask) ((val) |= (mask))

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


/***********************************************************************
** LOCKING
** We have priv->sem and priv->lock.
**
** We employ following naming convention in order to get locking right:
**
** acx_e_xxxx - external entry points called from process context.
**	It is okay to sleep. priv->sem is to be taken on entry.
** acx_i_xxxx - external entry points possibly called from atomic context.
**	Sleeping is not allowed (and thus down(sem) is not legal!)
** acx_s_xxxx - potentially sleeping functions. Do not ever call under lock!
** acx_l_xxxx - functions which expect lock to be already taken.
** rest       - non-sleeping functions which do not require locking
**		but may be run inder lock
**
** A small number of local helpers do not have acx_[eisl]_ prefix.
** They are always close to caller and are to be revieved locally.
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

void acx_lock_debug(wlandevice_t *priv, const char* where);
void acx_unlock_debug(wlandevice_t *priv, const char* where);
void acx_down_debug(wlandevice_t *priv, const char* where);
void acx_up_debug(wlandevice_t *priv, const char* where);
void acx_lock_unhold(void);
void acx_sem_unhold(void);

static inline void
acx_lock_helper(wlandevice_t *priv, unsigned long *fp, const char* where)
{
	acx_lock_debug(priv, where);
	spin_lock_irqsave(&priv->lock, *fp);
}
static inline void
acx_unlock_helper(wlandevice_t *priv, unsigned long *fp, const char* where)
{
	acx_unlock_debug(priv, where);
	spin_unlock_irqrestore(&priv->lock, *fp);
}
static inline void
acx_down_helper(wlandevice_t *priv, const char* where)
{
	acx_down_debug(priv, where);
}
static inline void
acx_up_helper(wlandevice_t *priv, const char* where)
{
	acx_up_debug(priv, where);
}
#define acx_lock(priv, flags)	acx_lock_helper(priv, &(flags), __FILE__ ":" STRING(__LINE__))
#define acx_unlock(priv, flags)	acx_unlock_helper(priv, &(flags), __FILE__ ":" STRING(__LINE__))
#define acx_sem_lock(priv)	acx_down_helper(priv, __FILE__ ":" STRING(__LINE__))
#define acx_sem_unlock(priv)	acx_up_helper(priv, __FILE__ ":" STRING(__LINE__))

#elif defined(DO_LOCKING)

#define acx_lock(priv, flags)	spin_lock_irqsave(&priv->lock, flags)
#define acx_unlock(priv, flags)	spin_unlock_irqrestore(&priv->lock, flags)
#define acx_sem_lock(priv)	down(&priv->sem)
#define acx_sem_unlock(priv)	up(&priv->sem)
#define acx_lock_unhold()	((void)0)
#define acx_sem_unhold()	((void)0)

#else /* no locking! :( */

#define acx_lock(priv, flags)	((void)0)
#define acx_unlock(priv, flags)	((void)0)
#define acx_sem_lock(priv)	((void)0)
#define acx_sem_unlock(priv)	((void)0)
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
acx_stop_queue(netdevice_t *dev, const char *msg)
{
	if (netif_queue_stopped(dev))
		return;

	netif_stop_queue(dev);
	if (msg)
		acxlog(L_BUFT, "tx: stop queue %s\n", msg);
}

static inline int
acx_queue_stopped(netdevice_t *dev)
{
	return netif_queue_stopped(dev);
}

static inline void
acx_start_queue(netdevice_t *dev, const char *msg)
{
	netif_start_queue(dev);
	if (msg)
		acxlog(L_BUFT, "tx: start queue %s\n", msg);
}

static inline void
acx_wake_queue(netdevice_t *dev, const char *msg)
{
	netif_wake_queue(dev);
	if (msg)
		acxlog(L_BUFT, "tx: wake queue %s\n", msg);
}

static inline void
acx_carrier_off(netdevice_t *dev, const char *msg)
{
	netif_carrier_off(dev);
	if (msg)
		acxlog(L_BUFT, "tx: carrier off %s\n", msg);
}

static inline void
acx_carrier_on(netdevice_t *dev, const char *msg)
{
	netif_carrier_on(dev);
	if (msg)
		acxlog(L_BUFT, "tx: carrier on %s\n", msg);
}

/* This function does not need locking UNLESS you call it
** as acx_set_status(ACX_STATUS_4_ASSOCIATED), bacause this can
** wake queue. This can race with stop_queue elsewhere. */
void acx_set_status(wlandevice_t *priv, u16 status);


/***********************************************************************
** Communication with firmware
*/
#define CMD_TIMEOUT_MS(n)	(n)
#define ACX_CMD_TIMEOUT_DEFAULT	CMD_TIMEOUT_MS(50)

#if ACX_DEBUG

/* We want to log cmd names */
int acxpci_s_issue_cmd_timeo_debug(wlandevice_t *priv, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr);
int acxusb_s_issue_cmd_timeo_debug(wlandevice_t *priv, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr);
static inline int
acx_s_issue_cmd_timeo_debug(wlandevice_t *priv, unsigned cmd, void *param, unsigned len, unsigned timeout, const char* cmdstr)
{
	if (IS_PCI(priv))
		return acxpci_s_issue_cmd_timeo_debug(priv, cmd, param, len, timeout, cmdstr);
	return acxusb_s_issue_cmd_timeo_debug(priv, cmd, param, len, timeout, cmdstr);
}
#define acx_s_issue_cmd(priv,cmd,param,len) \
	acx_s_issue_cmd_timeo_debug(priv,cmd,param,len,ACX_CMD_TIMEOUT_DEFAULT,#cmd)
#define acx_s_issue_cmd_timeo(priv,cmd,param,len,timeo) \
	acx_s_issue_cmd_timeo_debug(priv,cmd,param,len,timeo,#cmd)
int acx_s_configure_debug(wlandevice_t *priv, void *pdr, int type, const char* str);
#define acx_s_configure(priv,pdr,type) \
	acx_s_configure_debug(priv,pdr,type,#type)
int acx_s_interrogate_debug(wlandevice_t *priv, void *pdr, int type, const char* str);
#define acx_s_interrogate(priv,pdr,type) \
	acx_s_interrogate_debug(priv,pdr,type,#type)

#else

int acxpci_s_issue_cmd_timeo(wlandevice_t *priv, unsigned cmd, void *param, unsigned len, unsigned timeout);
int acxusb_s_issue_cmd_timeo(wlandevice_t *priv, unsigned cmd, void *param, unsigned len, unsigned timeout);
static inline int
acx_s_issue_cmd_timeo(wlandevice_t *priv, unsigned cmd,	void *param, unsigned len, unsigned timeout)
{
	if (IS_PCI(priv))
		return acxpci_s_issue_cmd_timeo(priv, cmd, param, len, timeout);
	return acxusb_s_issue_cmd_timeo(priv, cmd, param, len, timeout);
}
static inline int
acx_s_issue_cmd(wlandevice_t *priv, unsigned cmd, void *param, unsigned len)
{
	if (IS_PCI(priv))
		return acxpci_s_issue_cmd_timeo(priv, cmd, param, len, ACX_CMD_TIMEOUT_DEFAULT);
	return acxusb_s_issue_cmd_timeo(priv, cmd, param, len, ACX_CMD_TIMEOUT_DEFAULT);
}
int acx_s_configure(wlandevice_t *priv, void *pdr, int type);
int acx_s_interrogate(wlandevice_t *priv, void *pdr, int type);

#endif

void acx_s_cmd_start_scan(wlandevice_t *priv);


/***********************************************************************
** Ioctls
*/
int
acx111pci_ioctl_info(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra);
int
acx100pci_ioctl_set_phy_amp_bias(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra);


/***********************************************************************
** /proc
*/
#ifdef CONFIG_PROC_FS
int acx_proc_register_entries(const struct net_device *dev);
int acx_proc_unregister_entries(const struct net_device *dev);
#else
static inline int
acx_proc_register_entries(const struct net_device *dev) { return OK; }
static inline int
acx_proc_unregister_entries(const struct net_device *dev) { return OK; }
#endif


/***********************************************************************
** Unsorted yet :)
*/
int acxpci_s_read_phy_reg(wlandevice_t *priv, u32 reg, u8 *charbuf);
int acxusb_s_read_phy_reg(wlandevice_t *priv, u32 reg, u8 *charbuf);
static inline int
acx_s_read_phy_reg(wlandevice_t *priv, u32 reg, u8 *charbuf)
{
	if (IS_PCI(priv))
		return acxpci_s_read_phy_reg(priv, reg, charbuf);
	return acxusb_s_read_phy_reg(priv, reg, charbuf);
}

int acxpci_s_write_phy_reg(wlandevice_t *priv, u32 reg, u8 value);
int acxusb_s_write_phy_reg(wlandevice_t *priv, u32 reg, u8 value);
static inline int
acx_s_write_phy_reg(wlandevice_t *priv, u32 reg, u8 value)
{
	if (IS_PCI(priv))
		return acxpci_s_write_phy_reg(priv, reg, value);
	return acxusb_s_write_phy_reg(priv, reg, value);
}

void acx_s_msleep(int ms);
int acx_s_init_mac(netdevice_t *dev);
void acx_set_reg_domain(wlandevice_t *priv, unsigned char reg_dom_id);
void acx_set_timer(wlandevice_t *priv, int timeout_us);
void acx_update_capabilities(wlandevice_t *priv);
int acxpci_read_eeprom_byte(wlandevice_t *priv, u32 addr, u8 *charbuf);
void acx_s_start(wlandevice_t *priv);

#if USE_FW_LOADER_26
firmware_image_t *acx_s_read_fw(struct device *dev, const char *file, u32 *size);
#else
firmware_image_t *acx_s_read_fw(const char *file, u32 *size);
#define acx_s_read_fw(dev, file, size) acx_s_read_fw(file, size)
#endif
int acxpci_s_upload_radio(wlandevice_t *priv);

void acx_s_initialize_rx_config(wlandevice_t *priv);
void acx_s_update_card_settings(wlandevice_t *priv, int get_all, int set_all);
void acx_read_configoption(wlandevice_t *priv);
void acx_l_update_ratevector(wlandevice_t *priv);

void acx_init_task_scheduler(wlandevice_t *priv);
void acx_schedule_task(wlandevice_t *priv, unsigned int set_flag);

int acx_e_ioctl_old(netdevice_t *dev, struct ifreq *ifr, int cmd);

client_t *acx_l_sta_list_get(wlandevice_t *priv, const u8 *address);
void acx_l_sta_list_del(wlandevice_t *priv, client_t *clt);

int acx_l_transmit_disassoc(wlandevice_t *priv, client_t *clt);
void acx_i_timer(unsigned long a);
int acx_s_complete_scan(wlandevice_t *priv);

static inline wlan_hdr_t*
acx_get_wlan_hdr(wlandevice_t *priv, const rxbuffer_t *rxbuf)
{
	if (!(priv->rx_config_1 & RX_CFG1_INCLUDE_PHY_HDR))
		return (wlan_hdr_t*)&rxbuf->hdr_a3;

	/* take into account phy header in front of packet */
	if (IS_ACX111(priv))
		return (wlan_hdr_t*)((u8*)&rxbuf->hdr_a3 + 8);

	return (wlan_hdr_t*)((u8*)&rxbuf->hdr_a3 + 4);
}

struct sk_buff *acx_rxbuf_to_ether(struct wlandevice *priv, rxbuffer_t *rxbuf);
int acx_ether_to_txbuf(wlandevice_t *priv, void *txbuf, const struct sk_buff *skb);

void acxpci_l_power_led(wlandevice_t *priv, int enable);

unsigned int acxpci_l_clean_txdesc(wlandevice_t *priv);
void acxpci_l_clean_txdesc_emergency(wlandevice_t *priv);

u8 acx_signal_determine_quality(u8 signal, u8 noise);

void acx_l_process_rxbuf(wlandevice_t *priv, rxbuffer_t *rxbuf);
void acx_l_handle_txrate_auto(wlandevice_t *priv, struct client *txc,
				u8 rate100, u16 rate111, u8 error,
				int pkts_to_ignore);

tx_t* acxpci_l_alloc_tx(wlandevice_t *priv);
tx_t* acxusb_l_alloc_tx(wlandevice_t *priv);
static inline tx_t*
acx_l_alloc_tx(wlandevice_t *priv)
{
	if (IS_PCI(priv))
		return acxpci_l_alloc_tx(priv);
	return acxusb_l_alloc_tx(priv);
}

void* acxpci_l_get_txbuf(wlandevice_t *priv, tx_t *tx_opaque);
void* acxusb_l_get_txbuf(wlandevice_t *priv, tx_t *tx_opaque);
static inline void*
acx_l_get_txbuf(wlandevice_t *priv, tx_t *tx_opaque)
{
	if (IS_PCI(priv))
		return acxpci_l_get_txbuf(priv, tx_opaque);
	return acxusb_l_get_txbuf(priv, tx_opaque);
}

void acxpci_l_tx_data(wlandevice_t *priv, tx_t *tx_opaque, int len);
void acxusb_l_tx_data(wlandevice_t *priv, tx_t *tx_opaque, int len);
static inline void
acx_l_tx_data(wlandevice_t *priv, tx_t *tx_opaque, int len)
{
	if (IS_PCI(priv))
		acxpci_l_tx_data(priv, tx_opaque, len);
	else
		acxusb_l_tx_data(priv, tx_opaque, len);
}

void acx_dump_bytes(const void *, int);
void acx_log_bad_eid(wlan_hdr_t* hdr, int len, wlan_ie_t* ie_ptr);

u8 acx_rate111to100(u16);

int acx_s_set_defaults(wlandevice_t *priv);
void acxpci_init_mboxes(wlandevice_t *priv);


#if !ACX_DEBUG
static inline const char* acx_get_packet_type_string(u16 fc) { return ""; }
#else
const char* acx_get_packet_type_string(u16 fc);
#endif
const char* acx_cmd_status_str(unsigned int state);

int acx_i_start_xmit(struct sk_buff *skb, netdevice_t *dev);
void acxpci_free_desc_queues(wlandevice_t *priv);

int acxpci_s_create_hostdesc_queues(wlandevice_t *priv);
void acxpci_create_desc_queues(wlandevice_t *priv, u32 tx_queue_start, u32 rx_queue_start);

int acx100_s_init_wep(wlandevice_t *priv);
int acx100_s_init_packet_templates(wlandevice_t *priv);
int acx111_s_init_packet_templates(wlandevice_t *priv);

void great_inquisitor(wlandevice_t *priv);

char* acxpci_s_proc_diag_output(char *p, wlandevice_t *priv);
int acxpci_proc_eeprom_output(char *p, wlandevice_t *priv);
void acxpci_set_interrupt_mask(wlandevice_t *priv);
int acx100pci_s_set_tx_level(wlandevice_t *priv, u8 level_dbm);

int acx_e_change_mtu(struct net_device *dev, int mtu);
struct net_device_stats* acx_e_get_stats(netdevice_t *dev);
struct iw_statistics* acx_e_get_wireless_stats(netdevice_t *dev);

int __init acxpci_e_init_module(void);
int __init acxusb_e_init_module(void);
void __exit acxpci_e_cleanup_module(void);
void __exit acxusb_e_cleanup_module(void);
