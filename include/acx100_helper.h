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

#define ISTATUS_0_STARTED	0
#define ISTATUS_1_SCANNING	1
#define ISTATUS_2_WAIT_AUTH	2
#define ISTATUS_3_AUTHENTICATED	3
#define ISTATUS_4_ASSOCIATED	4
/* TODO: ISTATUS_4_ASSOCIATED_IBSS, ISTATUS_4_ASSOCIATED_AP */



typedef struct acx100_ie_queueconfig {
	UINT16	type ACX_PACKED;
	UINT16	len ACX_PACKED;
	UINT32	AreaSize ACX_PACKED;
	UINT32	RxQueueStart ACX_PACKED;
	UINT8	QueueOptions ACX_PACKED; /* queue options, val0xd */
	UINT8	NumTxQueues ACX_PACKED;	 /* # tx queues, val0xe */
	UINT8	NumRxDesc ACX_PACKED;	 /* for USB only */
	UINT8	padf2 ACX_PACKED;	 /* # rx buffers */
	UINT32	QueueEnd ACX_PACKED;
	UINT32	HostQueueEnd ACX_PACKED; /* QueueEnd2 */
	UINT32	TxQueueStart ACX_PACKED;
	UINT8	TxQueuePri ACX_PACKED;
	UINT8	NumTxDesc ACX_PACKED;
	UINT16	pad ACX_PACKED;
} acx100_ie_queueconfig_t;

typedef struct acx111_ie_queueconfig {
	UINT16 type ACX_PACKED;
	UINT16 len ACX_PACKED;
	UINT32 tx_memory_block_address ACX_PACKED;
	UINT32 rx_memory_block_address ACX_PACKED;
	UINT32 rx1_queue_address ACX_PACKED;
	UINT32 reserved1 ACX_PACKED;
	UINT32 tx1_queue_address ACX_PACKED;
	UINT8  tx1_attributes ACX_PACKED;
	UINT16 reserved2 ACX_PACKED;
	UINT8  reserved3 ACX_PACKED;
} acx111_ie_queueconfig_t;

typedef struct acx100_ie_memconfigoption {
	UINT16	type ACX_PACKED;
	UINT16	len ACX_PACKED;
	UINT32	DMA_config ACX_PACKED;
	UINT32  pRxHostDesc ACX_PACKED; /* val0x8 */
	UINT32	rx_mem ACX_PACKED;
	UINT32	tx_mem ACX_PACKED;
	UINT16	RxBlockNum ACX_PACKED;
	UINT16	TxBlockNum ACX_PACKED;
} acx100_ie_memconfigoption_t;

typedef struct acx111_ie_memoryconfig {
	UINT16 type ACX_PACKED;
	UINT16 len ACX_PACKED;
	UINT16 no_of_stations ACX_PACKED;
	UINT16 memory_block_size ACX_PACKED;
	UINT8 tx_rx_memory_block_allocation ACX_PACKED;
	UINT8 count_rx_queues ACX_PACKED;
	UINT8 count_tx_queues ACX_PACKED;
	UINT8 options ACX_PACKED;
	UINT8 fragmentation ACX_PACKED;
	UINT16 reserved1 ACX_PACKED;
	UINT8 reserved2 ACX_PACKED;

	/* start of rx1 block */
	UINT8 rx_queue1_count_descs ACX_PACKED;
	UINT8 rx_queue1_reserved1 ACX_PACKED;
	UINT8 rx_queue1_reserved2 ACX_PACKED; /* must be set to 7 */
	UINT8 rx_queue1_reserved3 ACX_PACKED; /* must be set to 0 */
	UINT32 rx_queue1_host_rx_start ACX_PACKED;
	/* end of rx1 block */

	/* start of tx1 block */
	UINT8 tx_queue1_count_descs ACX_PACKED;
	UINT8 tx_queue1_reserved1 ACX_PACKED;
	UINT8 tx_queue1_reserved2 ACX_PACKED;
	UINT8 tx_queue1_attributes ACX_PACKED;
	/* end of tx1 block */
} acx111_ie_memoryconfig_t;

typedef struct acx_ie_memmap {
	UINT16	type ACX_PACKED;
	UINT16	len ACX_PACKED;
	UINT32	CodeStart ACX_PACKED;
	UINT32	CodeEnd ACX_PACKED;
	UINT32	WEPCacheStart ACX_PACKED;
	UINT32	WEPCacheEnd ACX_PACKED;
	UINT32	PacketTemplateStart ACX_PACKED;
	UINT32	PacketTemplateEnd ACX_PACKED;
	UINT32	QueueStart ACX_PACKED;
	UINT32	QueueEnd ACX_PACKED;
	UINT32	PoolStart ACX_PACKED;
	UINT32	PoolEnd ACX_PACKED;
} acx_ie_memmap_t;

typedef struct ACX111FeatureConfig {
	UINT16 type ACX_PACKED;
	UINT16 len ACX_PACKED;
	UINT32 feature_options ACX_PACKED;
	UINT32 data_flow_options ACX_PACKED;
} ACX111FeatureConfig_t;

typedef struct ACX111TxLevel {
	UINT16 type ACX_PACKED;
	UINT16 len ACX_PACKED;
	UINT8 level ACX_PACKED;
} ACX111TxLevel_t;

#if MAYBE_BOGUS
typedef struct wep {
	UINT16 vala ACX_PACKED;

	UINT8 wep_key[MAX_KEYLEN] ACX_PACKED;
	char key_name[0x16] ACX_PACKED;
} wep_t;
#endif

typedef struct associd {
	UINT16 vala ACX_PACKED;
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
	UINT32	type ACX_PACKED;
	UINT32	len ACX_PACKED;
	UINT8 wakeup_cfg ACX_PACKED;
	UINT8 listen_interval ACX_PACKED; /* for EACH_ITVL: wake up every "beacon units" interval */
	UINT8 options ACX_PACKED;
	UINT8 hangover_period ACX_PACKED; /* remaining wake time after Tx MPDU w/ PS bit, in values of 1/1024 seconds */
	UINT16 enhanced_ps_transition_time ACX_PACKED; /* rem. wake time for Enh. PS */
} acx100_ie_powermgmt_t;

typedef struct acx111_ie_powermgmt {
	UINT32	type ACX_PACKED;
	UINT32	len ACX_PACKED;
	UINT8	wakeup_cfg ACX_PACKED;
	UINT8	listen_interval ACX_PACKED; /* for EACH_ITVL: wake up every "beacon units" interval */
	UINT8	options ACX_PACKED;
	UINT8	hangover_period ACX_PACKED; /* remaining wake time after Tx MPDU w/ PS bit, in values of 1/1024 seconds */
	UINT32	beaconRxTime ACX_PACKED;
	UINT32 enhanced_ps_transition_time ACX_PACKED; /* rem. wake time for Enh. PS */
} acx111_ie_powermgmt_t;

typedef struct sub3info {
	UINT8 size ACX_PACKED;
	UINT8 buf[0x4] ACX_PACKED;
} sub3info_t;

typedef struct subsubinfo {
	UINT8 size ACX_PACKED;
	struct sub3info buf ACX_PACKED;
} subsubinfo_t;

typedef struct subinfo {
	UINT16 size ACX_PACKED;
	char buf[0x8] ACX_PACKED;
	struct subsubinfo buf2 ACX_PACKED;
	char buf3[0x8] ACX_PACKED;
} subinfo_t;

typedef struct beaconinfo {
	UINT16 size ACX_PACKED;
	UINT16 status ACX_PACKED;
	UINT8 addr1[0x6] ACX_PACKED;
	UINT8 addr2[0x6] ACX_PACKED;
	UINT8 addr3[0x6] ACX_PACKED;
	struct subinfo inf ACX_PACKED;
	UINT8 buf[0x1c] ACX_PACKED;
} beaconinfo_t;

typedef struct beacon {
	UINT16 size ACX_PACKED;
	struct beaconinfo inf ACX_PACKED;
/*  UINT16 vala ACX_PACKED; */
} beacon_t;

#define ACX_SCAN_ACTIVE		0
#define ACX_SCAN_PASSIVE	1
#define ACX_SCAN_BACKGROUND	2

typedef struct acx100_scan {
	UINT16 count ACX_PACKED; /* number of scans to do, 0xffff == continuous */
	UINT16 start_chan ACX_PACKED;
	UINT16 flags ACX_PACKED; /* channel list mask; 0x8000 == all channels? */
	UINT8 max_rate ACX_PACKED; /* max. probe rate */
	UINT8 options ACX_PACKED; /* scan mode: 0 == active, 1 == passive, 2 == background */
	UINT16 chan_duration ACX_PACKED;
	UINT16 max_probe_delay ACX_PACKED;
} acx100_scan_t;			/* length 0xc */

typedef struct acx111_scan {
	UINT16 count ACX_PACKED; /* number of scans to do */
	UINT8 channel_list_select ACX_PACKED;
	UINT16 reserved1 ACX_PACKED;
	UINT8 reserved2 ACX_PACKED;
	UINT8 rate ACX_PACKED;
	UINT8 options ACX_PACKED;
	UINT16 chan_duration ACX_PACKED;
	UINT16 max_probe_delay ACX_PACKED;
	UINT8 modulation ACX_PACKED;
	UINT8 channel_list[26] ACX_PACKED;
} acx111_scan_t;


typedef struct acx_tim {
	UINT16 size ACX_PACKED;
	UINT8 buf[0x100] ACX_PACKED;
} acx_tim_t;

typedef struct acx_proberesp {
	UINT16 size ACX_PACKED;
	char buf[0x54] ACX_PACKED;
} acx_proberesp_t;

typedef struct acx_probereq {
	UINT16 size ACX_PACKED;
	char buf[0x44] ACX_PACKED;
} acx_probereq_t;

/* as opposed to acx100, acx111 dtim interval is AFTER rates_basic111.
 * NOTE: took me about an hour to get !@#$%^& packing right --> struct packing is eeeeevil... */
typedef struct acx_joinbss {
	UINT8 bssid[ETH_ALEN] ACX_PACKED;
	UINT16 beacon_interval ACX_PACKED;
	union {
		struct {
			UINT8 dtim_interval ACX_PACKED;
			UINT8 rates_basic ACX_PACKED;
			UINT8 rates_supported ACX_PACKED;
		} acx100 ACX_PACKED;
		struct {
			UINT16 rates_basic ACX_PACKED;
			UINT8 dtim_interval ACX_PACKED;
		} acx111 ACX_PACKED;
	} u ACX_PACKED;
	UINT8 genfrm_txrate ACX_PACKED;	/* generated frame (beacon, probe resp, RTS, PS poll) tx rate */
	UINT8 genfrm_mod_pre ACX_PACKED;	/* generated frame modulation/preamble:
				** bit7: PBCC, bit6: OFDM (else CCK/DQPSK/DBPSK)
				** bit5: short pre */
	UINT8 macmode ACX_PACKED;		/* BSS Type, must be one of ACX_MODE_xxx */
	UINT8 channel ACX_PACKED;
	UINT8 essid_len ACX_PACKED;
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
	UINT8 timestamp[8] ACX_PACKED;	/* this contains the Logical Link Control data
				   the first three bytes are the dsap, ssap and control
				   respectively. the following 3 bytes are either ieee_oui
				   or an RFC. See p80211conv.c/h for more. */
	UINT16 beacon_interval ACX_PACKED;
	UINT16 caps ACX_PACKED;
	UINT8 info[0x30] ACX_PACKED;	/* 0x24 */
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
	UINT8 timestamp[8] ACX_PACKED;      /* 8 Bytes */
	UINT16 beacon_interval ACX_PACKED;  /* 2 Bytes */
	UINT16 caps ACX_PACKED;             /* 2 Bytes */
	UINT8 info[48] ACX_PACKED;          /* 48 Bytes */
#if INFO_CONSISTS_OF
	UINT8 ssid[1+1+IW_ESSID_MAX_SIZE] ACX_PACKED; /* 34 Bytes */
	UINT8 supp_rates[1+1+8] ACX_PACKED; /* 10 Bytes */
	UINT8 ds_parms[1+1+1] ACX_PACKED; /* 3 Bytes */
	UINT8 filler ACX_PACKED;		/* 1 Byte alignment filler */
#endif
} acxp80211_beacon_prb_resp_t;

typedef struct acxp80211_packet {
	UINT16 size ACX_PACKED; /* packet len indicator for firmware */
	struct acxp80211_hdr hdr ACX_PACKED; /* actual packet */
} acxp80211_packet_t;		/* size: 0x56 */

typedef struct mem_read_write {
    UINT16 addr ACX_PACKED;
    UINT16 type ACX_PACKED; /* 0x0 int. RAM / 0xffff MAC reg. / 0x81 PHY RAM / 0x82 PHY reg. */
    UINT32 len ACX_PACKED;
    UINT32 data ACX_PACKED;
} mem_read_write_t;

typedef struct acxp80211_beacon_prb_resp_template {
	UINT16 size ACX_PACKED; /* packet len indicator for firmware */
	struct acxp80211_beacon_prb_resp pkt ACX_PACKED;
} acxp80211_beacon_prb_resp_template_t;

typedef struct acxp80211_nullframe {
	UINT16 size ACX_PACKED;
	struct p80211_hdr_a3 hdr ACX_PACKED;
} acxp80211_nullframe_t;

typedef struct {
    UINT32 chksum ACX_PACKED;
    UINT32 size ACX_PACKED;
    UINT8 data[1] ACX_PACKED; /* the byte array of the actual firmware... */
} firmware_image_t;

typedef struct {
    UINT32 offset ACX_PACKED;
    UINT32 len ACX_PACKED;
} radioinit_t;

typedef struct acx100_ie_wep_options {
    UINT16	type ACX_PACKED;
    UINT16	len ACX_PACKED;
    UINT16	NumKeys ACX_PACKED;	/* max # of keys */
    UINT8	WEPOption ACX_PACKED;	/* 0 == decrypt default key only, 1 == override decrypt */
    UINT8	Pad ACX_PACKED;		/* used only for acx111 */
} acx100_ie_wep_options_t;

typedef struct ie_dot11WEPDefaultKey {
    UINT16	type ACX_PACKED;
    UINT16	len ACX_PACKED;
    UINT8	action ACX_PACKED;
    UINT8	keySize ACX_PACKED;
    UINT8	defaultKeyNum ACX_PACKED;
    UINT8	key[29] ACX_PACKED;	/* check this! was Key[19]. */
} ie_dot11WEPDefaultKey_t;

typedef struct acx111WEPDefaultKey {
    UINT8	MacAddr[ETH_ALEN] ACX_PACKED;
    UINT8	action ACX_PACKED;
    UINT16	reserved ACX_PACKED;
    UINT8	keySize ACX_PACKED;
    UINT8	type ACX_PACKED;
    UINT8	index ACX_PACKED;
    UINT8	defaultKeyNum ACX_PACKED;
    UINT8	counter[6] ACX_PACKED;
    UINT8	key[29] ACX_PACKED;	/* check this! was Key[19]. */
} acx111WEPDefaultKey_t;

typedef struct ie_dot11WEPDefaultKeyID {
    UINT16	type ACX_PACKED;
    UINT16	len ACX_PACKED;
    UINT8	KeyID ACX_PACKED;
} ie_dot11WEPDefaultKeyID_t;

typedef struct acx100_wep_mgmt {
    UINT8	MacAddr[ETH_ALEN] ACX_PACKED;
    UINT16	Action ACX_PACKED;
    UINT16	KeySize ACX_PACKED;
    UINT8	Key[29] ACX_PACKED; /* 29*8 == 232bits == WEP256 */
} acx100_wep_mgmt_t;

/* a helper struct for quick implementation of commands */
typedef struct GenericPacket {
	UINT8 bytes[32] ACX_PACKED;
} GenericPacket_t;

typedef struct  defaultkey {
	UINT8 num ACX_PACKED;
} defaultkey_t;

typedef struct acx_ie_generic {
	UINT16 type ACX_PACKED;
	UINT16 len ACX_PACKED;
	union data {
		struct GenericPacket gp ACX_PACKED;
		/* struct wep wp ACX_PACKED; */
		struct associd asid ACX_PACKED;
		struct defaultkey dkey ACX_PACKED;
	} m ACX_PACKED;
} acx_ie_generic_t;

/* Config Option structs */

typedef struct co_antennas {
    UINT8	type ACX_PACKED;
    UINT8	len ACX_PACKED;
    UINT8	list[2] ACX_PACKED;
} co_antennas_t;

typedef struct co_powerlevels {
    UINT8	type ACX_PACKED;
    UINT8	len ACX_PACKED;
    UINT16	list[8] ACX_PACKED;
} co_powerlevels_t;

typedef struct co_datarates {
    UINT8	type ACX_PACKED;
    UINT8	len ACX_PACKED;
    UINT8	list[8] ACX_PACKED;
} co_datarates_t;

typedef struct co_domains {
    UINT8	type ACX_PACKED;
    UINT8	len ACX_PACKED;
    UINT8	list[6] ACX_PACKED;
} co_domains_t;

typedef struct co_product_id {
    UINT8	type ACX_PACKED;
    UINT8	len ACX_PACKED;
    UINT8	list[128] ACX_PACKED;
} co_product_id_t;

typedef struct co_manuf_id {
    UINT8	type ACX_PACKED;
    UINT8	len ACX_PACKED;
    UINT8	list[128] ACX_PACKED;
} co_manuf_t;

typedef struct co_fixed {
    UINT8	type ACX_PACKED;
    UINT8	len ACX_PACKED;
    char	NVSv[8] ACX_PACKED;
    UINT8	MAC[6] ACX_PACKED;
    UINT16	probe_delay ACX_PACKED;
    UINT32	eof_memory ACX_PACKED;
    UINT8	dot11CCAModes ACX_PACKED;
    UINT8	dot11Diversity ACX_PACKED;
    UINT8	dot11ShortPreambleOption ACX_PACKED;
    UINT8	dot11PBCCOption ACX_PACKED;
    UINT8	dot11ChannelAgility ACX_PACKED;
    UINT8	dot11PhyType ACX_PACKED;
/*    UINT8	dot11TempType ACX_PACKED;
    UINT8	num_var ACX_PACKED;           seems to be erased     */
} co_fixed_t;


typedef struct acx_configoption {
    co_fixed_t			configoption_fixed ACX_PACKED;
    co_antennas_t		antennas ACX_PACKED;
    co_powerlevels_t		power_levels ACX_PACKED;
    co_datarates_t		data_rates ACX_PACKED;
    co_domains_t		domains ACX_PACKED;
    co_product_id_t		product_id ACX_PACKED;
    co_manuf_t			manufactor ACX_PACKED;
} acx_configoption_t;



void acx_stop_queue(netdevice_t *dev, char *msg);
int acx_queue_stopped(netdevice_t *dev);
void acx_start_queue(netdevice_t *dev, char *msg);
void acx_wake_queue(netdevice_t *dev, char *msg);
void acx_carrier_off(netdevice_t *dev, char *msg);
void acx_carrier_on(netdevice_t *dev, char *msg);
void acx_schedule(long timeout);
int acx_reset_dev(netdevice_t *dev);
void acx_join_bssid(wlandevice_t *priv);
int acx_init_mac(netdevice_t *dev, UINT16 init);
void acx_set_reg_domain(wlandevice_t *priv, unsigned char reg_dom_id);
void acx_set_timer(wlandevice_t *priv, UINT32 time);
void acx_update_capabilities(wlandevice_t *priv);
UINT16 acx_read_eeprom_offset(wlandevice_t *priv, UINT16 addr,
					UINT8 *charbuf);
UINT16 acx_read_eeprom_area(wlandevice_t *priv);
UINT16 acx_write_eeprom_offset(wlandevice_t *priv, UINT16 addr,
					UINT16 len, UINT8 *charbuf);
UINT16 acx_read_phy_reg(wlandevice_t *priv, UINT16 reg, UINT8 *charbuf);
UINT16 acx_write_phy_reg(wlandevice_t *priv, UINT16 reg, UINT8 value);
void acx_start(wlandevice_t *priv);
void acx_reset_mac(wlandevice_t *priv);
/*@null@*/ firmware_image_t *acx_read_fw( const char *file, UINT32 *size);
int acx_upload_fw(wlandevice_t *priv);
int acx_write_fw(wlandevice_t *priv, const firmware_image_t *apfw_image, UINT32 offset);
int acx_validate_fw(wlandevice_t *priv, const firmware_image_t *apfw_mage, UINT32 offset);
void acx100_set_wepkey(wlandevice_t *priv);
void acx111_set_wepkey(wlandevice_t *priv);
int acx_init_wep(wlandevice_t *priv);
void acx_initialize_rx_config(wlandevice_t *priv, INT setting);
void acx_update_card_settings(wlandevice_t *priv, int init, int get_all, int set_all);
int acx_ioctl_old(netdevice_t *dev, struct ifreq *ifr, int cmd);
void acx100_scan_chan(wlandevice_t *priv);
void acx100_scan_chan_p(wlandevice_t *priv, acx100_scan_t *s);
void acx111_scan_chan_p(wlandevice_t *priv, struct acx111_scan *s);
void acx111_scan_chan(wlandevice_t *priv);
int acx_load_radio(wlandevice_t *priv);
void acx_read_configoption(wlandevice_t *priv);
UINT16 acx_proc_register_entries(struct net_device *dev);
UINT16 acx_proc_unregister_entries(struct net_device *dev);
void acx_update_dot11_ratevector(wlandevice_t *priv);
void acx_update_peerinfo(wlandevice_t *priv, struct peer *peer, struct bss_info *bsspeer);


int acx111_get_feature_config(wlandevice_t *priv, struct ACX111FeatureConfig *config);
int acx111_set_feature_config(wlandevice_t *priv, struct ACX111FeatureConfig *config);

#endif /* __ACX_ACX100_HELPER_H */
