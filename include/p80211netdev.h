/* include/p80211netdev.h
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
 * This code is based on elements which are
 * Copyright (C) 1999 AbsoluteValue Systems, Inc.  All Rights Reserved.
 * info@linux-wlan.com
 * http://www.linux-wlan.com
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

#ifndef _LINUX_P80211NETDEV_H
#define _LINUX_P80211NETDEV_H

/*================================================================*/
/* Constants */

#define WLAN_DEVICE_CLOSED	0
#define WLAN_DEVICE_OPEN	1

#define WLAN_MACMODE_NONE	0
#define WLAN_MACMODE_IBSS_STA	1
#define WLAN_MACMODE_ESS_STA	2
#define WLAN_MACMODE_ESS_AP	3

/* MSD States */
#define WLAN_MSD_START			-1
#define WLAN_MSD_DRIVERLOADED		0
#define WLAN_MSD_HWPRESENT_PENDING	1
#define WLAN_MSD_HWFAIL			2
#define WLAN_MSD_HWPRESENT		3
#define WLAN_MSD_FWLOAD_PENDING		4
#define WLAN_MSD_FWLOAD			5
#define WLAN_MSD_RUNNING_PENDING	6
#define WLAN_MSD_RUNNING		7

#ifndef ETH_P_ECONET
#define ETH_P_ECONET   0x0018	/* needed for 2.2.x kernels */
#endif

#define ETH_P_80211_RAW        (ETH_P_ECONET + 1)

#ifndef ARPHRD_IEEE80211
#define ARPHRD_IEEE80211 801	/* kernel 2.4.6 */
#endif

#ifndef ARPHRD_IEEE80211_PRISM	/* kernel 2.4.18 */
#define ARPHRD_IEEE80211_PRISM 802
#endif

/*--- NSD Capabilities Flags ------------------------------*/
#define P80211_NSDCAP_HARDWAREWEP           0x01	/* hardware wep engine */
#define P80211_NSDCAP_TIEDWEP               0x02	/* can't decouple en/de */
#define P80211_NSDCAP_NOHOSTWEP             0x04	/* must use hardware wep */
#define P80211_NSDCAP_PBCC                  0x08	/* hardware supports PBCC */
#define P80211_NSDCAP_SHORT_PREAMBLE        0x10	/* hardware supports */
#define P80211_NSDCAP_AGILITY               0x20	/* hardware supports */
#define P80211_NSDCAP_AP_RETRANSMIT         0x40	/* nsd handles retransmits */
#define P80211_NSDCAP_HWFRAGMENT            0x80	/* nsd handles frag/defrag */
#define P80211_NSDCAP_AUTOJOIN              0x100	/* nsd does autojoin */
#define P80211_NSDCAP_NOSCAN                0x200	/* nsd can scan */

/*================================================================*/
/* Macros */

/*================================================================*/
/* Types */

/* Received frame statistics */
typedef struct p80211_frmrx_t {
	UINT32 mgmt;
	UINT32 assocreq;
	UINT32 assocresp;
	UINT32 reassocreq;
	UINT32 reassocresp;
	UINT32 probereq;
	UINT32 proberesp;
	UINT32 beacon;
	UINT32 atim;
	UINT32 disassoc;
	UINT32 authen;
	UINT32 deauthen;
	UINT32 mgmt_unknown;
	UINT32 ctl;
	UINT32 pspoll;
	UINT32 rts;
	UINT32 cts;
	UINT32 ack;
	UINT32 cfend;
	UINT32 cfendcfack;
	UINT32 ctl_unknown;
	UINT32 data;
	UINT32 dataonly;
	UINT32 data_cfack;
	UINT32 data_cfpoll;
	UINT32 data__cfack_cfpoll;
	UINT32 null;
	UINT32 cfack;
	UINT32 cfpoll;
	UINT32 cfack_cfpoll;
	UINT32 data_unknown;
	UINT32 decrypt;
	UINT32 decrypt_err;
} p80211_frmrx_t;

struct macaddr {
	UINT32 mac[8];
	UINT16 size;
};


#ifdef WIRELESS_EXT
/* called by /proc/net/wireless */
struct iw_statistics *p80211wext_get_wireless_stats(netdevice_t * dev);
/* wireless extensions' ioctls */
int p80211wext_support_ioctl(netdevice_t * dev, struct ifreq *ifr,
			     int cmd);
#if WIRELESS_EXT > 12
extern struct iw_handler_def p80211wext_handler_def;
#endif
#endif				/* wireless extensions */

/* WEP stuff */
#define NUM_WEPKEYS 4
#define MAX_KEYLEN 32

#define HOSTWEP_DEFAULTKEY_MASK (BIT1|BIT0)
#define HOSTWEP_DECRYPT  BIT4
#define HOSTWEP_ENCRYPT  BIT5
#define HOSTWEP_PRIVACYINVOKED BIT6
#define HOSTWEP_EXCLUDEUNENCRYPTED BIT7

typedef struct wep_key {
	UINT32 index;
	UINT16 size;
	UINT8 key[256];
	UINT16 strange_filler;
} wep_key_t; /* size = 264 bytes (33*8) FIXME: we don't have size 264!! or is there 2 bytes beyond the key? (strange_filler) */

/* FIXME: acx100_addr3_t should probably actually be discarded in favour
 * of the identical linux-wlan-ng p80211_hdr_t */
typedef struct acx100_addr3 {
	/* IEEE 802.11-1999.pdf chapter 7 might help */
	UINT16 frame_control __WLAN_ATTRIB_PACK__;	/* 0x00, wlan-ng name */
	UINT16 duration_id __WLAN_ATTRIB_PACK__;	/* 0x02, wlan-ng name */
	char address1[0x6] __WLAN_ATTRIB_PACK__;	/* 0x04, wlan-ng name */
	char address2[0x6] __WLAN_ATTRIB_PACK__;	/* 0x0a */
	char address3[0x6] __WLAN_ATTRIB_PACK__;	/* 0x10 */
	UINT16 sequence_control __WLAN_ATTRIB_PACK__;	/* 0x16 */
	UINT8 *val0x18;
	struct sk_buff *val0x1c;
	struct sk_buff *val0x20;
} acx100_addr3_t;

typedef struct TIWLAN_DC {	/* V3 version */
	struct wlandevice *wlandev;

	UINT val0x4;		//spacing
	
	UINT ui32ACXTxQueueStart;	/* 0x8, official name */
	UINT ui32ACXRxQueueStart;	/* 0xc */

	UINT8 *pTxBufferPool;	/* 0x10 */
	dma_addr_t TxBufferPoolPhyAddr;	/* 0x14 */
	UINT TxBufferPoolSize;	/* 0x18 */

	struct txdescriptor *pTxDescQPool;	/* V13POS 0x1c, official name */
	UINT tx_pool_count;	/* 0x20 indicates # of ring buffer pool entries */
	UINT tx_head;		/* 0x24 current ring buffer pool member index */
	UINT tx_tail;		/* 0x34,pool_idx2 is not correct, I'm
				 * just using it as a ring watch
				 * official name */

	struct framehdr *pFrameHdrQPool;	/* 0x28 */
	UINT FrameHdrQPoolSize;	/* 0x2c */
	dma_addr_t FrameHdrQPoolPhyAddr;	/* 0x30 */

	UINT val0x38;		/* 0x38, official name */

	struct txhostdescriptor *pTxHostDescQPool;	/* V3POS 0x3c, V1POS 0x60 */
	UINT TxHostDescQPoolSize;	/* 0x40 */
	UINT TxHostDescQPoolPhyAddr;	/* 0x44 */

	UINT val0x48;		/* 0x48 */
	UINT val0x4c;

	struct rxdescriptor *pRxDescQPool;		/* V1POS 0x74, V3POS 0x50 */
	UINT rx_pool_count;		/* V1POS 0x78, V3POS 0X54 */
	UINT rx_tail;	/* 0x6c */

	UINT val0x50;		/* V1POS:0x50, some size */
	UINT val0x54;		/* 0x54, official name */

	struct rxhostdescriptor *pRxHostDescQPool;	/* 0x58, is it really rxdescriptor? */
	UINT RxHostDescQPoolSize;	/* 0x5c */
	UINT RxHostDescQPoolPhyAddr;	/* 0x60, official name. */

	UINT val0x64;		/* 0x64, some size */

	UINT *pRxBufferPool;	/* *rxdescq1; 0x70 */
	UINT RxBufferPoolPhyAddr;	/* *rxdescq2; 0x74 */
	UINT RxBufferPoolSize;
} TIWLAN_DC;

typedef struct iS {
	char address[WLAN_BSSID_LEN];	/* 0x0 */
	UINT16 cap;		/* 0x6 */
	int size; /* 0x8 */ ;
	char essid[WLAN_BSSID_LEN];	/* 0xc */
	char val0x12[0x1a];
	UINT16 fWEPPrivacy;	/* 0x2c */
	char val0x2e[0x2e];
	char supp_rates[0x8];
	UINT32 channel;		/* 0x64; strange, this is accessed as UINT16 once. oh well, probably doesn't matter */
	char val0x68[0x10];
	UINT32 sir;		/* 0x78; Standard IR */
	UINT32 snr;		/* 0x7c; Signal to Noise Ratio */
	char val0x80[4];
} iS_t;				//132 0x84

typedef struct key_struct {
	UINT8 addr[WLAN_ADDR_LEN]; /* 0x00 */
	UINT16 filler1; /* 0x06 */
	UINT32 filler2; /* 0x08 */
	UINT32 index; /* 0x0c */
	UINT16 len; /* 0x10 */
	UINT8 key[29]; /* 0x12; is this long enough??? */
} key_struct_t; /* size = 276. FIXME: where is the remaining space?? */

/* WLAN device type */
typedef struct wlandevice {
	struct wlandevice *next;	/* 00 link for list of devices */
	struct acx100 *hw;	/* 04 private data for MSD */
	struct net_device *prev_nd;	/* 08 private data for MSD */

	struct timer_list mgmt_timer;

	/* PCMCIA crap */
	void *card;
	int broken_cor_reset;
	int (*hard_reset)(struct wlandevice *);
	/* end PCMCIA crap */

	/* Hardware config */
	int hw_unavailable; /* indicates whether the hardware has been suspended or ejected or so */
	UINT irq;		/* 0c */
	UINT16 irq_mask;	/* mask indicating the interrupt types that are masked (not wanted) V3POS 2418, V1POS 23f4 */

	UINT membase;		/* 10 */
	UINT membase2;		/* 14 */
	UINT pvMemBaseAddr1;	/* V3POS 2448, V1POS 2424 */
	UINT pvMemBaseAddr2;	/* V3POS 2434, V1POS 2410 official name */
	UINT iobase;		/* 18 */
	UINT iobase2;		/* 1c */
	UINT8 form_factor;
	UINT8 radio_type;
	unsigned char eeprom_version;

	UINT8 dev_addr[MAX_ADDR_LEN];	/* V3POS 2340, V1POS 22f0 (or was it 22f8?) */

	UINT16 rx_config_1;	/* V3POS 2820, V1POS 27f8 */
	UINT16 rx_config_2;	/* V3POS 2822, V1POS 27fa */

	unsigned char firmware_version[20];
	UINT32 firmware_numver;
	UINT32 firmware_id;

	/*** ACX100 command interface ***/
	UINT16 cmd_type;	/* V3POS 2508, V1POS 24e0 */
	UINT16 cmd_status;	/* V3POS 250a, V1POS 24e2 */
	UINT32 CommandParameters;	/* FIXME: used to be an array UINT*0x88, but it should most likely be *one* UINT32 instead, pointing to the cmd param memory. V3POS 268c, V1POS 2664 */
	UINT InfoParameters;	/* V3POS 2814, V1POS 27ec */

	/*** PHY settings ***/
#if EXPERIMENTAL_VER_0_3
	unsigned char tx_level_dbm;
	unsigned char tx_level_val;
#endif
	/* tx power settings */
	unsigned char pow;
	/* antenna settings */
	unsigned char antenna[4 + ACX100_RID_DOT11_CURRENT_ANTENNA_LEN];
	/* ed threshold settings */
	unsigned char ed_threshold[4 + ACX100_RID_DOT11_ED_THRESHOLD_LEN];
	/* cca settings */
	unsigned char cca[4 + ACX100_RID_DOT11_CURRENT_CCA_MODE_LEN];

	/*** Linux device management ***/
	/* netlink socket */
	/* queue for indications waiting for cmd completion */
	/* Linux netdevice and support */
	netdevice_t *netdev;	/* ptr to linux netdevice */
	struct net_device_stats stats;	/* 20 */
	/* which is:
	   rx_packets; 20
	   tx_packets; 24
	   rx_bytes;   28
	   tx_bytes;   2c
	   ...       - 7b
	   hmm, or given that net_device_stats is so big: maybe it does NOT
	   contain a complete net_device_stats???
	 */
	UINT8 state;		/* 0x7c */
	UINT8 ifup; /* whether the device is up */
	int monitor; /* whether the device is in monitor mode or not */

/* compatibility to wireless extensions */
#ifndef WIRELESS_EXT
#error "you forgot to include linux/wireless.h"
#endif

#ifdef WIRELESS_EXT
	struct iw_statistics wstats;
#endif

	/*** wireless settings ***/
	UINT32 mode;		/* V3POS 80; that's the MAC mode we want */
	UINT8 bitrateval;	/* V3POS b8, V1POS ba */
	char essid[0x20];	/* V3POS 84; essid */
	char essid_active;	/* specific ESSID active, or select any? */
	char nick[IW_ESSID_MAX_SIZE];

	/* reg domain settings */
	unsigned char reg_dom_id;
	UINT16 reg_dom_chanmask;

	UINT8 address[WLAN_ADDR_LEN]; /* V1+3POS: a6 */ /* the random BSSID of the station we're associated to (or even our own one in case we're the one to establish the network) */
	UINT16 channel;		/* V3POS 22f0, V1POS b8 */

	char preamble_mode; /* 0 == Long Preamble, 1 == Short, 2 == Auto */
	char preamble_flag; /* 0 == Long Preamble, 1 == Short */

	/* PM crap */
	struct pm_dev *pm;
	/* end PM crap */

	UINT32 short_retry;	/* V3POS 204, V1POS 20c */
	UINT32 long_retry;	/* V3POS 208, V1POS 210 */
	UINT32 msdu_lifetime;	/* V3POS 20c, V1POS 214 */
	UINT32 auth_alg;	/* V3POS 228, V3POS 230, used in transmit_authen1 */
	UINT16 listen_interval;	/* V3POS 250, V1POS 258, given in units of beacon interval */
	UINT32 beacon_interval;	/* V3POS 2300, V1POS c8 */

	UINT16 capabilities;	/* V3POS b0 */
	unsigned char capab_short;	/* V3POS 1ec, V1POS 1f4 */
	unsigned char capab_pbcc;	/* V3POS 1f0, V1POS 1f8 */
	unsigned char capab_agility;	/* V3POS 1f4, V1POS 1fc */
	char rate_spt_len;	/* V3POS 1243, V1POS 124b */
	char rate_support1[5];	/* V3POS 1244, V1POS 124c */
	char rate_support2[5];	/* V3POS 1254, V1POS 125c */


	/*** encryption settings ***/
	UINT32 wep_restricted;  /* V3POS c0 */
	UINT32 wep_enabled;
	UINT32 wep_current_index;	/* V3POS 254, V1POS 25c not sure about this */
	wep_key_t wep_keys[NUM_WEPKEYS];	/* V3POS 268 (it is NOT 260, but 260 plus offset 8!), V1POS 270 */
	key_struct_t wep_key_struct[10]; /* V3POS 688 */
	int hostwep;

	/*** network status ***/
	/* FIXME: this should be cleaned up */
	UINT8 iStable;		/* V3POS 1264, V1POS 126c; original name, "Scan Table" - most likely number of peers found in range */
	struct iS val0x126c[0x20];	/* V3POS 126c, V1POS 1274 */
	struct iS station_assoc; /* the station we're currently associated to */

	char val0x2302[6];	/* V3POS 2302, V1POS ca */
	char val0x2324[0x8];	/* V3POS 2324 */

	UINT32 macmode;		/* 0xac; 0 == Ad-Hoc, 2 == infrastructure, using a wlan-ng name here! This is the mode we're currently in */
	char essid_for_assoc[0x20]; /* the ESSID we are going to use for association, in case of "essid 'any'" and in case of hidden ESSID (use configured ESSID then) */
	UINT8 bssid[WLAN_ADDR_LEN];	/* V3POS b2, using a wlan-ng name here! */
	UINT iStatus;		/* original name. V3POS 234c, V1POS 2304 */// all in the same
	UINT unknown0x2350;	/* V3POS 2350, V1POS 2308 *///structure

	/*** card Rx/Tx management ***/
	TIWLAN_DC dc;		/* V3POS 2380, V1POS 2338 */

	UINT TxQueueNo;		/* V3POS 24dc, V1POS 24b4 */
	UINT RxQueueNo;		/* V3POS 24f4, V1POS 24cc */

	UINT ACXTxQueueStart;
	UINT16 memblocksize;	/* V3POS 2354, V1POS 230c */
	int TxQueueFree;
	UINT32 val0x24e4;	/* V3POS 24e4, V1POS 24bc */
	UINT32 val0x24e8;	/* V3POS 24e8, V1POS 24c0 */

	struct rxhostdescriptor *RxHostDescPoolStart;	/* V3POS 24f8, V1POS 24d0 */
	UINT32 val0x24fc;	/* V3POS 24fc, V1POS 24d4 */
	UINT32 val0x2500;	/* V3POS 2500, V1POS 24d8 */

	/*** helper stuff ***/
	UINT8 scan_retries;	/* V3POS 2826, V1POS 27fe */
	UINT8 auth_assoc_retries;	/* V3POS 2827, V1POS 27ff */
	spinlock_t lock; /* our new locking variable */

	/* Request/Confirm i/f state (used by p80211) */
//	UINT32 request_pending;	/* flag, access atomically */
//	p80211msg_t *curr_msg;
//	struct timer_list reqtimer;
//	wait_queue_head_t reqwq;

//	acx100_metacmd_t *cmds;

	/* device methods (init by MSD, used by p80211 */
	int (*open) (struct wlandevice *wlandev);
	int (*close) (struct wlandevice *wlandev);
	void (*reset) (struct wlandevice *wlandev);
	int (*txframe) (struct wlandevice *wlandev, struct sk_buff *skb);
	int (*mlmerequest) (struct wlandevice *wlandev, p80211msg_t *msg);
	void (*hwremovedfn) (struct wlandevice *wlandev);

#ifdef CONFIG_PROC_FS
	/* Procfs support */
	struct proc_dir_entry *procdir;
	struct proc_dir_entry *procwlandev;
#endif

	/* 802.11 device statistics */
//	struct p80211_frmrx_t rx;

} wlandevice_t __WLAN_ATTRIB_PACK__;

/* WEP stuff */
int wep_change_key(wlandevice_t * wlandev, int keynum, UINT8 * key,
		   int keylen);
int wep_decrypt(wlandevice_t * wlandev, UINT8 * buf, UINT32 len,
		int key_override, UINT8 * iv, UINT8 * icv);
int wep_encrypt(wlandevice_t * wlandev, UINT8 * buf, UINT8 * dst,
		UINT32 len, int keynum, UINT8 * iv, UINT8 * icv);


/*================================================================*/
/* Externs */

/*================================================================*/
/* Function Declarations */

void p80211netdev_startup(void);
void p80211netdev_shutdown(void);
int wlan_setup(wlandevice_t * wlandev);
int wlan_unsetup(wlandevice_t * wlandev);
int register_wlandev(wlandevice_t * wlandev);
int unregister_wlandev(wlandevice_t * wlandev);
void p80211netdev_hwremoved(wlandevice_t * wlandev);

/*================================================================*/
/* Function Definitions */

static inline void p80211netdev_stop_queue(wlandevice_t * wlandev)
{
	if (!wlandev)
		return;
	if (!wlandev->netdev)
		return;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,38) )
	wlandev->netdev->tbusy = 1;
	wlandev->netdev->start = 0;
#else
	netif_stop_queue(wlandev->netdev);
#endif
}

static inline void p80211netdev_start_queue(wlandevice_t * wlandev)
{
	if (!wlandev)
		return;
	if (!wlandev->netdev)
		return;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,38) )
	wlandev->netdev->tbusy = 0;
	wlandev->netdev->start = 1;
#else
	netif_start_queue(wlandev->netdev);
#endif
}

static inline void p80211netdev_wake_queue(wlandevice_t * wlandev)
{
	if (!wlandev)
		return;
	if (!wlandev->netdev)
		return;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,3,38) )
	wlandev->netdev->tbusy = 0;
	mark_bh(NET_BH);
#else
	netif_wake_queue(wlandev->netdev);
#endif
}

/********************************************************************/
/* Locking and synchronization functions                            */
/********************************************************************/

/* These functions *must* be inline or they will break horribly on
 * SPARC, due to its weird semantics for save/restore flags. extern
 * inline should prevent the kernel from linking or module from
 * loading if they are not inlined. */
extern inline int acx100_lock(wlandevice_t *wlandev,
                               unsigned long *flags)
{
        spin_lock_irqsave(&wlandev->lock, *flags);
        if (wlandev->hw_unavailable) {
                printk(KERN_DEBUG "acx100_lock() called with hw_unavailable (dev=%p)\n",
                       wlandev->netdev);
                spin_unlock_irqrestore(&wlandev->lock, *flags);
                return -EBUSY;
        }
        return 0;
}

extern inline void acx100_unlock(wlandevice_t *wlandev,
                                  unsigned long *flags)
{
        spin_unlock_irqrestore(&wlandev->lock, *flags);
}

#endif
