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
#ifndef __ACX_ACX100_HELPER_H
#define __ACX_ACX100_HELPER_H

/*============================================================================*
 * Debug / log functionality                                                  *
 *============================================================================*/

/* NOTE: If we still want basic logging of driver info if ACX_DEBUG is not
 * defined, we should provide an acxlog variant that is never turned off. We
 * should make a acx_msg(), and rename acxlog() to acx_debug() to make the
 * difference very clear.
 */

#ifdef ACX_DEBUG

extern int acx_debug_func_indent;

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

#define FN_EXIT(p, v) \
	do { \
		if (unlikely(debug & L_FUNC)) { \
			if (p) { \
				log_fn_exit_v(__func__, v); \
			} else { \
				log_fn_exit(__func__); \
			} \
		} \
	} while (0)

#else

#define acxlog(chan, args...)
#define FN_ENTER
#define FN_EXIT(p, v)

#endif /* ACX_DEBUG */


#define ISTATUS_0_STARTED	0
#define ISTATUS_1_SCANNING	1
#define ISTATUS_2_WAIT_AUTH	2
#define ISTATUS_3_AUTHENTICATED	3
#define ISTATUS_4_ASSOCIATED	4
/* TODO: ISTATUS_4_ASSOCIATED_IBSS, ISTATUS_4_ASSOCIATED_AP */



typedef struct acx100_ie_queueconfig {
	u16	type ACX_PACKED;
	u16	len ACX_PACKED;
	u32	AreaSize ACX_PACKED;
	u32	RxQueueStart ACX_PACKED;
	u8	QueueOptions ACX_PACKED; /* queue options, val0xd */
	u8	NumTxQueues ACX_PACKED;	 /* # tx queues, val0xe */
	u8	NumRxDesc ACX_PACKED;	 /* for USB only */
	u8	padf2 ACX_PACKED;	 /* # rx buffers */
	u32	QueueEnd ACX_PACKED;
	u32	HostQueueEnd ACX_PACKED; /* QueueEnd2 */
	u32	TxQueueStart ACX_PACKED;
	u8	TxQueuePri ACX_PACKED;
	u8	NumTxDesc ACX_PACKED;
	u16	pad ACX_PACKED;
} acx100_ie_queueconfig_t;

typedef struct acx111_ie_queueconfig {
	u16 type ACX_PACKED;
	u16 len ACX_PACKED;
	u32 tx_memory_block_address ACX_PACKED;
	u32 rx_memory_block_address ACX_PACKED;
	u32 rx1_queue_address ACX_PACKED;
	u32 reserved1 ACX_PACKED;
	u32 tx1_queue_address ACX_PACKED;
	u8  tx1_attributes ACX_PACKED;
	u16 reserved2 ACX_PACKED;
	u8  reserved3 ACX_PACKED;
} acx111_ie_queueconfig_t;

typedef struct acx100_ie_memconfigoption {
	u16	type ACX_PACKED;
	u16	len ACX_PACKED;
	u32	DMA_config ACX_PACKED;
	u32  pRxHostDesc ACX_PACKED; /* val0x8 */
	u32	rx_mem ACX_PACKED;
	u32	tx_mem ACX_PACKED;
	u16	RxBlockNum ACX_PACKED;
	u16	TxBlockNum ACX_PACKED;
} acx100_ie_memconfigoption_t;

typedef struct acx111_ie_memoryconfig {
	u16 type ACX_PACKED;
	u16 len ACX_PACKED;
	u16 no_of_stations ACX_PACKED;
	u16 memory_block_size ACX_PACKED;
	u8 tx_rx_memory_block_allocation ACX_PACKED;
	u8 count_rx_queues ACX_PACKED;
	u8 count_tx_queues ACX_PACKED;
	u8 options ACX_PACKED;
	u8 fragmentation ACX_PACKED;
	u16 reserved1 ACX_PACKED;
	u8 reserved2 ACX_PACKED;

	/* start of rx1 block */
	u8 rx_queue1_count_descs ACX_PACKED;
	u8 rx_queue1_reserved1 ACX_PACKED;
	u8 rx_queue1_reserved2 ACX_PACKED; /* must be set to 7 */
	u8 rx_queue1_reserved3 ACX_PACKED; /* must be set to 0 */
	u32 rx_queue1_host_rx_start ACX_PACKED;
	/* end of rx1 block */

	/* start of tx1 block */
	u8 tx_queue1_count_descs ACX_PACKED;
	u8 tx_queue1_reserved1 ACX_PACKED;
	u8 tx_queue1_reserved2 ACX_PACKED;
	u8 tx_queue1_attributes ACX_PACKED;
	/* end of tx1 block */
} acx111_ie_memoryconfig_t;

typedef struct acx_ie_memmap {
	u16	type ACX_PACKED;
	u16	len ACX_PACKED;
	u32	CodeStart ACX_PACKED;
	u32	CodeEnd ACX_PACKED;
	u32	WEPCacheStart ACX_PACKED;
	u32	WEPCacheEnd ACX_PACKED;
	u32	PacketTemplateStart ACX_PACKED;
	u32	PacketTemplateEnd ACX_PACKED;
	u32	QueueStart ACX_PACKED;
	u32	QueueEnd ACX_PACKED;
	u32	PoolStart ACX_PACKED;
	u32	PoolEnd ACX_PACKED;
} acx_ie_memmap_t;

typedef struct ACX111FeatureConfig {
	u16 type ACX_PACKED;
	u16 len ACX_PACKED;
	u32 feature_options ACX_PACKED;
	u32 data_flow_options ACX_PACKED;
} ACX111FeatureConfig_t;

typedef struct ACX111TxLevel {
	u16 type ACX_PACKED;
	u16 len ACX_PACKED;
	u8 level ACX_PACKED;
} ACX111TxLevel_t;

#if MAYBE_BOGUS
typedef struct wep {
	u16 vala ACX_PACKED;

	u8 wep_key[MAX_KEYLEN] ACX_PACKED;
	char key_name[0x16] ACX_PACKED;
} wep_t;
#endif

typedef struct associd {
	u16 vala ACX_PACKED;
} associd_t;

#define PS_CFG_ENABLE		0x80
#define PS_CFG_PENDING		0x40 /* status flag when entering PS */
#define PS_CFG_WAKEUP_MODE_MASK	0x07
#define PS_CFG_WAKEUP_BY_HOST	0x03
#define PS_CFG_WAKEUP_EACH_ITVL	0x02
#define PS_CFG_WAKEUP_ON_DTIM	0x01
#define PS_CFG_WAKEUP_ALL_BEAC	0x00

#define PS_OPT_ENA_ENHANCED_PS	0x04 /* Enhanced PS mode: sleep until Rx Beacon w/ the STA's AID bit set in the TIM; newer firmwares only(?) */
#define PS_OPT_STILL_RCV_BCASTS	0x01

typedef struct acx100_ie_powermgmt {
	u32	type ACX_PACKED;
	u32	len ACX_PACKED;
	u8 wakeup_cfg ACX_PACKED;
	u8 listen_interval ACX_PACKED; /* for EACH_ITVL: wake up every "beacon units" interval */
	u8 options ACX_PACKED;
	u8 hangover_period ACX_PACKED; /* remaining wake time after Tx MPDU w/ PS bit, in values of 1/1024 seconds */
	u16 enhanced_ps_transition_time ACX_PACKED; /* rem. wake time for Enh. PS */
} acx100_ie_powermgmt_t;

typedef struct acx111_ie_powermgmt {
	u32	type ACX_PACKED;
	u32	len ACX_PACKED;
	u8	wakeup_cfg ACX_PACKED;
	u8	listen_interval ACX_PACKED; /* for EACH_ITVL: wake up every "beacon units" interval */
	u8	options ACX_PACKED;
	u8	hangover_period ACX_PACKED; /* remaining wake time after Tx MPDU w/ PS bit, in values of 1/1024 seconds */
	u32	beaconRxTime ACX_PACKED;
	u32 enhanced_ps_transition_time ACX_PACKED; /* rem. wake time for Enh. PS */
} acx111_ie_powermgmt_t;

typedef struct sub3info {
	u8 size ACX_PACKED;
	u8 buf[0x4] ACX_PACKED;
} sub3info_t;

typedef struct subsubinfo {
	u8 size ACX_PACKED;
	struct sub3info buf ACX_PACKED;
} subsubinfo_t;

typedef struct subinfo {
	u16 size ACX_PACKED;
	char buf[0x8] ACX_PACKED;
	struct subsubinfo buf2 ACX_PACKED;
	char buf3[0x8] ACX_PACKED;
} subinfo_t;

typedef struct beaconinfo {
	u16 size ACX_PACKED;
	u16 status ACX_PACKED;
	u8 addr1[0x6] ACX_PACKED;
	u8 addr2[0x6] ACX_PACKED;
	u8 addr3[0x6] ACX_PACKED;
	struct subinfo inf ACX_PACKED;
	u8 buf[0x1c] ACX_PACKED;
} beaconinfo_t;

typedef struct beacon {
	u16 size ACX_PACKED;
	struct beaconinfo inf ACX_PACKED;
/*  u16 vala ACX_PACKED; */
} beacon_t;

#define ACX_SCAN_ACTIVE		0
#define ACX_SCAN_PASSIVE	1
#define ACX_SCAN_BACKGROUND	2

typedef struct acx100_scan {
	u16 count ACX_PACKED; /* number of scans to do, 0xffff == continuous */
	u16 start_chan ACX_PACKED;
	u16 flags ACX_PACKED; /* channel list mask; 0x8000 == all channels? */
	u8 max_rate ACX_PACKED; /* max. probe rate */
	u8 options ACX_PACKED; /* scan mode: 0 == active, 1 == passive, 2 == background */
	u16 chan_duration ACX_PACKED;
	u16 max_probe_delay ACX_PACKED;
} acx100_scan_t;			/* length 0xc */

typedef struct acx111_scan {
	u16 count ACX_PACKED; /* number of scans to do */
	u8 channel_list_select ACX_PACKED;
	u16 reserved1 ACX_PACKED;
	u8 reserved2 ACX_PACKED;
	u8 rate ACX_PACKED;
	u8 options ACX_PACKED;
	u16 chan_duration ACX_PACKED;
	u16 max_probe_delay ACX_PACKED;
	u8 modulation ACX_PACKED;
	u8 channel_list[26] ACX_PACKED;
} acx111_scan_t;

typedef struct acx111_cmd_radiocalib {
	u32 methods ACX_PACKED; /* 0x80000000 == automatic calibration by firmware, according to interval; Bits 0..3: select calibration methods to go through: calib based on DC, AfeDC, Tx mismatch, Tx equilization */
	u32 interval ACX_PACKED;
} acx111_cmd_radiocalib_t;

typedef struct acx_tim {
	u16 size ACX_PACKED;
	u8 buf[0x100] ACX_PACKED;
} acx_tim_t;

typedef struct acx_proberesp {
	u16 size ACX_PACKED;
	char buf[0x54] ACX_PACKED;
} acx_proberesp_t;

typedef struct acx_probereq {
	u16 size ACX_PACKED;
	char buf[0x44] ACX_PACKED;
} acx_probereq_t;

/* as opposed to acx100, acx111 dtim interval is AFTER rates_basic111.
 * NOTE: took me about an hour to get !@#$%^& packing right --> struct packing is eeeeevil... */
typedef struct acx_joinbss {
	u8 bssid[ETH_ALEN] ACX_PACKED;
	u16 beacon_interval ACX_PACKED;
	union {
		struct {
			u8 dtim_interval ACX_PACKED;
			u8 rates_basic ACX_PACKED;
			u8 rates_supported ACX_PACKED;
		} acx100 ACX_PACKED;
		struct {
			u16 rates_basic ACX_PACKED;
			u8 dtim_interval ACX_PACKED;
		} acx111 ACX_PACKED;
	} u ACX_PACKED;
	u8 genfrm_txrate ACX_PACKED;	/* generated frame (beacon, probe resp, RTS, PS poll) tx rate */
	u8 genfrm_mod_pre ACX_PACKED;	/* generated frame modulation/preamble:
				** bit7: PBCC, bit6: OFDM (else CCK/DQPSK/DBPSK)
				** bit5: short pre */
	u8 macmode ACX_PACKED;		/* BSS Type, must be one of ACX_MODE_xxx */
	u8 channel ACX_PACKED;
	u8 essid_len ACX_PACKED;
	char essid[IW_ESSID_MAX_SIZE] ACX_PACKED;	
} acx_joinbss_t;

#define JOINBSS_RATES_1		0x01
#define JOINBSS_RATES_2		0x02
#define JOINBSS_RATES_5		0x04
#define JOINBSS_RATES_11	0x08
#define JOINBSS_RATES_22	0x10

/* Looks like missing bits are used to indicate 11g rates!
** (it follows from the fact that constants below match 1:1 to RATE111_nn)
** This was actually seen! Look at that Assoc Request,
** it _does_ contain 11g rates in basic set:
01:30:20.070772 Beacon (xxx) [1.0* 2.0* 5.5* 11.0* 6.0* 9.0* 12.0* 18.0* 24.0* 36.0* 48.0* 54.0* Mbit] ESS CH: 1
01:30:20.074425 Authentication (Open System)-1: Succesful
01:30:20.076539 Authentication (Open System)-2:
01:30:20.076620 Acknowledgment
01:30:20.088546 Assoc Request (xxx) [1.0* 2.0* 5.5* 6.0* 9.0* 11.0* 12.0* 18.0* 24.0* 36.0* 48.0* 54.0* Mbit]
01:30:20.122413 Assoc Response AID(1) :: Succesful
01:30:20.122679 Acknowledgment
01:30:20.173204 Beacon (xxx) [1.0* 2.0* 5.5* 11.0* 6.0* 9.0* 12.0* 18.0* 24.0* 36.0* 48.0* 54.0* Mbit] ESS CH: 1
*/
#define JOINBSS_RATES_BASIC111_1	0x0001
#define JOINBSS_RATES_BASIC111_2	0x0002
#define JOINBSS_RATES_BASIC111_5	0x0004
#define JOINBSS_RATES_BASIC111_11	0x0020
#define JOINBSS_RATES_BASIC111_22	0x0100

/*
 * I am temporarily redefining this because the above struct is somewhat wrong.
 *
 * NOTE: this is a generic struct for all sorts of different packet types.
 * As such I guess that someone got confused about structure layout.
 * In other words: it always has an "official" packet header (A4?)
 * and then a custom packet body.
 * The current form is probably a bit sub-optimal. However let's keep it
 * for now and then redefine it later. Or maybe change it now to have a
 * better struct layout for all sorts of management packets?
 */
typedef struct acxp80211_hdr {
	p80211_hdr_a3_t a4 ACX_PACKED;
	u8 timestamp[8] ACX_PACKED;	/* this contains the Logical Link Control data
				   the first three bytes are the dsap, ssap and control
				   respectively. the following 3 bytes are either ieee_oui
				   or an RFC. See p80211conv.c/h for more. */
	u16 beacon_interval ACX_PACKED;
	u16 caps ACX_PACKED;
	u8 info[0x30] ACX_PACKED;	/* 0x24 */
	/*
	 * info[1] is essid_len
	 * &info[2] is essid (max. 32 chars)
	 * FIXME: huh, then sizeof(info) == 0x18 cannot be correct! Adjusting
	 * to 0x30, which extends to end of this struct
	 */
/*  p80211_hdr_a4_t b4 ACX_PACKED; */
} acxp80211_hdr_t;		/* size: 0x54 */

/* struct used for Beacon and for Probe Response frames */
typedef struct acxp80211_beacon_prb_resp {
	p80211_hdr_a3_t hdr ACX_PACKED;     /* 24 Bytes */
	u8 timestamp[8] ACX_PACKED;      /* 8 Bytes */
	u16 beacon_interval ACX_PACKED;  /* 2 Bytes */
	u16 caps ACX_PACKED;             /* 2 Bytes */
	u8 info[48] ACX_PACKED;          /* 48 Bytes */
#if INFO_CONSISTS_OF
	u8 ssid[1+1+IW_ESSID_MAX_SIZE] ACX_PACKED; /* 34 Bytes */
	u8 supp_rates[1+1+8] ACX_PACKED; /* 10 Bytes */
	u8 ds_parms[1+1+1] ACX_PACKED; /* 3 Bytes */
	u8 filler ACX_PACKED;		/* 1 Byte alignment filler */
#endif
} acxp80211_beacon_prb_resp_t;

typedef struct acxp80211_packet {
	u16 size ACX_PACKED; /* packet len indicator for firmware */
	struct acxp80211_hdr hdr ACX_PACKED; /* actual packet */
} acxp80211_packet_t;		/* size: 0x56 */

typedef struct mem_read_write {
    u16 addr ACX_PACKED;
    u16 type ACX_PACKED; /* 0x0 int. RAM / 0xffff MAC reg. / 0x81 PHY RAM / 0x82 PHY reg. */
    u32 len ACX_PACKED;
    u32 data ACX_PACKED;
} mem_read_write_t;

typedef struct acxp80211_beacon_prb_resp_template {
	u16 size ACX_PACKED; /* packet len indicator for firmware */
	struct acxp80211_beacon_prb_resp pkt ACX_PACKED;
} acxp80211_beacon_prb_resp_template_t;

typedef struct acxp80211_nullframe {
	u16 size ACX_PACKED;
	struct p80211_hdr_a3 hdr ACX_PACKED;
} acxp80211_nullframe_t;

typedef struct {
    u32 chksum ACX_PACKED;
    u32 size ACX_PACKED;
    u8 data[1] ACX_PACKED; /* the byte array of the actual firmware... */
} firmware_image_t;

typedef struct {
    u32 offset ACX_PACKED;
    u32 len ACX_PACKED;
} radioinit_t;

typedef struct acx100_ie_wep_options {
    u16	type ACX_PACKED;
    u16	len ACX_PACKED;
    u16	NumKeys ACX_PACKED;	/* max # of keys */
    u8	WEPOption ACX_PACKED;	/* 0 == decrypt default key only, 1 == override decrypt */
    u8	Pad ACX_PACKED;		/* used only for acx111 */
} acx100_ie_wep_options_t;

typedef struct ie_dot11WEPDefaultKey {
    u16	type ACX_PACKED;
    u16	len ACX_PACKED;
    u8	action ACX_PACKED;
    u8	keySize ACX_PACKED;
    u8	defaultKeyNum ACX_PACKED;
    u8	key[29] ACX_PACKED;	/* check this! was Key[19]. */
} ie_dot11WEPDefaultKey_t;

typedef struct acx111WEPDefaultKey {
    u8	MacAddr[ETH_ALEN] ACX_PACKED;
    u8	action ACX_PACKED;
    u16	reserved ACX_PACKED;
    u8	keySize ACX_PACKED;
    u8	type ACX_PACKED;
    u8	index ACX_PACKED;
    u8	defaultKeyNum ACX_PACKED;
    u8	counter[6] ACX_PACKED;
    u8	key[29] ACX_PACKED;	/* check this! was Key[19]. */
} acx111WEPDefaultKey_t;

typedef struct ie_dot11WEPDefaultKeyID {
    u16	type ACX_PACKED;
    u16	len ACX_PACKED;
    u8	KeyID ACX_PACKED;
} ie_dot11WEPDefaultKeyID_t;

typedef struct acx100_wep_mgmt {
    u8	MacAddr[ETH_ALEN] ACX_PACKED;
    u16	Action ACX_PACKED;
    u16	KeySize ACX_PACKED;
    u8	Key[29] ACX_PACKED; /* 29*8 == 232bits == WEP256 */
} acx100_wep_mgmt_t;

/* a helper struct for quick implementation of commands */
typedef struct GenericPacket {
	u8 bytes[32] ACX_PACKED;
} GenericPacket_t;

typedef struct  defaultkey {
	u8 num ACX_PACKED;
} defaultkey_t;

typedef struct acx_ie_generic {
	u16 type ACX_PACKED;
	u16 len ACX_PACKED;
	union data {
		struct GenericPacket gp ACX_PACKED;
		/* struct wep wp ACX_PACKED; */
		struct associd asid ACX_PACKED;
		struct defaultkey dkey ACX_PACKED;
	} m ACX_PACKED;
} acx_ie_generic_t;

/* Config Option structs */

typedef struct co_antennas {
    u8	type ACX_PACKED;
    u8	len ACX_PACKED;
    u8	list[2] ACX_PACKED;
} co_antennas_t;

typedef struct co_powerlevels {
    u8	type ACX_PACKED;
    u8	len ACX_PACKED;
    u16	list[8] ACX_PACKED;
} co_powerlevels_t;

typedef struct co_datarates {
    u8	type ACX_PACKED;
    u8	len ACX_PACKED;
    u8	list[8] ACX_PACKED;
} co_datarates_t;

typedef struct co_domains {
    u8	type ACX_PACKED;
    u8	len ACX_PACKED;
    u8	list[6] ACX_PACKED;
} co_domains_t;

typedef struct co_product_id {
    u8	type ACX_PACKED;
    u8	len ACX_PACKED;
    u8	list[128] ACX_PACKED;
} co_product_id_t;

typedef struct co_manuf_id {
    u8	type ACX_PACKED;
    u8	len ACX_PACKED;
    u8	list[128] ACX_PACKED;
} co_manuf_t;

typedef struct co_fixed {
    u8	type ACX_PACKED;
    u8	len ACX_PACKED;
    char	NVSv[8] ACX_PACKED;
    u8	MAC[6] ACX_PACKED;
    u16	probe_delay ACX_PACKED;
    u32	eof_memory ACX_PACKED;
    u8	dot11CCAModes ACX_PACKED;
    u8	dot11Diversity ACX_PACKED;
    u8	dot11ShortPreambleOption ACX_PACKED;
    u8	dot11PBCCOption ACX_PACKED;
    u8	dot11ChannelAgility ACX_PACKED;
    u8	dot11PhyType ACX_PACKED;
/*    u8	dot11TempType ACX_PACKED;
    u8	num_var ACX_PACKED;           seems to be erased     */
} co_fixed_t;


typedef struct acx_configoption {
    co_fixed_t			configoption_fixed ACX_PACKED;
    co_antennas_t		antennas ACX_PACKED;
    co_powerlevels_t		power_levels ACX_PACKED;
    co_datarates_t		data_rates ACX_PACKED;
    co_domains_t		domains ACX_PACKED;
    co_product_id_t		product_id ACX_PACKED;
    co_manuf_t			manufacturer ACX_PACKED;
} acx_configoption_t;



void acx_stop_queue(netdevice_t *dev, const char *msg);
int acx_queue_stopped(netdevice_t *dev);
void acx_start_queue(netdevice_t *dev, const char *msg);
void acx_wake_queue(netdevice_t *dev, const char *msg);
void acx_carrier_off(netdevice_t *dev, const char *msg);
void acx_carrier_on(netdevice_t *dev, const char *msg);
void acx_schedule(long timeout);
int acx_reset_dev(netdevice_t *dev);
void acx_join_bssid(wlandevice_t *priv);
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
/*@null@*/ firmware_image_t *acx_read_fw( const char *file, u32 *size);
int acx_upload_fw(wlandevice_t *priv);
int acx_write_fw(wlandevice_t *priv, const firmware_image_t *apfw_image, u32 offset);
int acx_validate_fw(wlandevice_t *priv, const firmware_image_t *apfw_mage, u32 offset);
void acx100_set_wepkey(wlandevice_t *priv);
void acx111_set_wepkey(wlandevice_t *priv);
int acx_init_wep(wlandevice_t *priv);
void acx_initialize_rx_config(wlandevice_t *priv, unsigned int setting);
void acx_update_card_settings(wlandevice_t *priv, int init, int get_all, int set_all);
void acx_init_task_scheduler(wlandevice_t *priv);
void acx_flush_task_scheduler(void);
void acx_schedule_after_interrupt_task(wlandevice_t *priv, unsigned int set_flag);
void acx100_scan_chan(wlandevice_t *priv);
void acx100_scan_chan_p(wlandevice_t *priv, acx100_scan_t *s);
void acx111_scan_chan_p(wlandevice_t *priv, struct acx111_scan *s);
void acx111_scan_chan(wlandevice_t *priv);
int acx_upload_radio(wlandevice_t *priv);
void acx_read_configoption(wlandevice_t *priv);
u16 acx_proc_register_entries(const struct net_device *dev);
u16 acx_proc_unregister_entries(const struct net_device *dev);
void acx_update_dot11_ratevector(wlandevice_t *priv);
void acx_update_peerinfo(wlandevice_t *priv, struct peer *peer, struct bss_info *bsspeer);

int acx111_recalib_radio(wlandevice_t *priv);
int acx100_recalib_radio(wlandevice_t *priv);
int acx111_get_feature_config(wlandevice_t *priv, struct ACX111FeatureConfig *config);
int acx111_set_feature_config(wlandevice_t *priv, struct ACX111FeatureConfig *config);

/* acx100_ioctl.c */
int acx_ioctl_old(netdevice_t *dev, struct ifreq *ifr, int cmd);

#endif /* __ACX_ACX100_HELPER_H */
