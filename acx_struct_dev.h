/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2010
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
#ifndef _ACX_STRUCT_DEV_H_
#define _ACX_STRUCT_DEV_H_

#include "acx_struct_hw.h"

/*
 * BOM Debug / log functionality
 * ==================================================
 */
enum {
	L_LOCK		= (ACX_DEBUG>1)*0x0001,	/* locking debug log */
	L_INIT		= (ACX_DEBUG>0)*0x0002,	/* special card initialization logging */
	L_IRQ		= (ACX_DEBUG>0)*0x0004,	/* interrupt stuff */
	L_ASSOC		= (ACX_DEBUG>0)*0x0008,	/* assocation (network join) and station log */
	L_FUNC		= (ACX_DEBUG>1)*0x0020,	/* logging of function enter / leave */
	L_XFER		= (ACX_DEBUG>1)*0x0080,	/* logging of transfers and mgmt */
	L_DATA		= (ACX_DEBUG>1)*0x0100,	/* logging of transfer data */
	L_DEBUG		= (ACX_DEBUG>1)*0x0200,	/* log of debug info */
	L_IOCTL		= (ACX_DEBUG>0)*0x0400,	/* log ioctl calls */
	L_CTL		= (ACX_DEBUG>1)*0x0800,	/* log of low-level ctl commands */
	L_BUFR		= (ACX_DEBUG>1)*0x1000,	/* debug rx buffer mgmt (ring buffer etc.) */
	L_XFER_BEACON	= (ACX_DEBUG>1)*0x2000,	/* also log beacon packets */
	L_BUFT		= (ACX_DEBUG>1)*0x4000,	/* debug tx buffer mgmt (ring buffer etc.) */
	L_USBRXTX	= (ACX_DEBUG>0)*0x8000,	/* debug USB rx/tx operations */
	L_BUF		= L_BUFR + L_BUFT,
	L_ANY		= 0xffff
};

#if ACX_DEBUG
extern unsigned int acx_debug;
#else
enum { acx_debug = 0 };
#endif

// BOM Operations by writing to acx_diag
enum {
	ACX_DIAG_OP_RECALIB = 0x0001,
	ACX_DIAG_OP_PROCESS_TX_RX = 0x0002,
	ACX_DIAG_OP_REINIT_TX_BUF = 0x0003
};

/*
 * BOM Constants
 * ==================================================
 */

#define OK	0
#define NOT_OK	1

/* The supported chip models */
#define CHIPTYPE_ACX100		1
#define CHIPTYPE_ACX111		2

#define IS_ACX100(adev)	((adev)->chip_type == CHIPTYPE_ACX100)
#define IS_ACX111(adev)	((adev)->chip_type == CHIPTYPE_ACX111)

/* Supported interfaces */
#define DEVTYPE_PCI		0
#define DEVTYPE_USB		1
#define DEVTYPE_MEM		2

#if !(defined(CONFIG_ACX_MAC80211_PCI) || defined(CONFIG_ACX_MAC80211_USB) || defined(CONFIG_ACX_MAC80211_MEM))
#error Driver must include PCI and/or USB, MEM support. You selected neither.
#endif

#if defined(CONFIG_ACX_MAC80211_PCI)
 #if !(defined(CONFIG_ACX_MAC80211_USB) || defined(CONFIG_ACX_MAC80211_MEM))
  #define IS_PCI(adev)	1
 #else
  #define IS_PCI(adev)	((adev)->dev_type == DEVTYPE_PCI)
 #endif
#else
 #define IS_PCI(adev)	0
#endif

#if defined(CONFIG_ACX_MAC80211_USB)
 #if !(defined(CONFIG_ACX_MAC80211_PCI) || defined(CONFIG_ACX_MAC80211_MEM))
  #define IS_USB(adev)	1
 #else
  #define IS_USB(adev)	((adev)->dev_type == DEVTYPE_USB)
 #endif
#else
 #define IS_USB(adev)	0
#endif

#if defined(CONFIG_ACX_MAC80211_MEM)
 #if !(defined(CONFIG_ACX_MAC80211_PCI) || defined(CONFIG_ACX_MAC80211_USB))
  #define IS_MEM(adev)	1
 #else
  #define IS_MEM(adev)	((adev)->dev_type == DEVTYPE_MEM)
 #endif
#else
 #define IS_MEM(adev)	0
#endif


/* Driver defaults */
#define DEFAULT_DTIM_INTERVAL	10
/* used to be 2048, but FreeBSD driver changed it to 4096 to work properly
** in noisy wlans */
#define DEFAULT_MSDU_LIFETIME	4096
#define DEFAULT_RTS_THRESHOLD	2312	/* max. size: disable RTS mechanism */
#define DEFAULT_BEACON_INTERVAL	100

#define ACX100_BAP_DATALEN_MAX		4096
#define ACX100_RID_GUESSING_MAXLEN	2048	/* I'm not really sure */
#define ACX100_RIDDATA_MAXLEN		ACX100_RID_GUESSING_MAXLEN

// BOM 'After Interrupt' Commands 
#define ACX_AFTER_IRQ_CMD_STOP_SCAN	0x01
#define ACX_AFTER_IRQ_CMD_ASSOCIATE	0x02
#define ACX_AFTER_IRQ_CMD_RADIO_RECALIB	0x04
#define ACX_AFTER_IRQ_UPDATE_CARD_CFG	0x08
#define ACX_AFTER_IRQ_TX_CLEANUP	0x10
#define ACX_AFTER_IRQ_COMPLETE_SCAN	0x20
#define ACX_AFTER_IRQ_RESTART_SCAN	0x40

/*
 * BOM  Tx/Rx buffer sizes and watermarks
 * ==================================================
 *
 * This will alloc and use DMAable buffers of
 * WLAN_A4FR_MAXLEN_WEP_FCS * (RX_CNT + TX_CNT) bytes
 * RX/TX_CNT=32 -> ~150k DMA buffers
 * RX/TX_CNT=16 -> ~75k DMA buffers
 *
 * 2005-10-10: reduced memory usage by lowering both to 16
 */
#define RX_CNT 16
#define TX_CNT 16

/* we clean up txdescs when we have N free txdesc: */
#define TX_CLEAN_BACKLOG (TX_CNT/4)
#define TX_START_CLEAN (TX_CNT - TX_CLEAN_BACKLOG)
#define TX_EMERG_CLEAN 2
/* we stop queue if we have < N free txbufs: */
#define TX_STOP_QUEUE 3
/* we start queue if we have >= N free txbufs: */
#define TX_START_QUEUE 5

#define ACX_TX_QUEUE_MAX_LENGTH 20

/*
 * BOM Global data
 * ==================================================
 */
extern const u8 acx_bitpos2ratebyte[];
extern const u8 acx_bitpos2rate100[];

extern const u8 acx_reg_domain_ids[];
extern const char * const acx_reg_domain_strings[];
enum {
	acx_reg_domain_ids_len = 8
};

/*
 * BOM Main acx per-device data structure
 * ==================================================
 */
#define ACX_STATE_FW_LOADED	0x01
#define ACX_STATE_IFACE_UP	0x02

/* MAC mode (BSS type) defines
 * Note that they shouldn't be redefined, since they are also used
 * during communication with firmware */
#define ACX_MODE_0_ADHOC	0
#define ACX_MODE_1_UNUSED	1
#define ACX_MODE_2_STA		2
#define ACX_MODE_3_AP		3
/* These are our own inventions. Sending these to firmware
** makes it stop emitting beacons, which is exactly what we want
** for these modes */
#define ACX_MODE_MONITOR	0xfe
#define ACX_MODE_OFF		0xff
/* 'Submode': identifies exact status of ADHOC/STA host */
#define ACX_STATUS_0_STOPPED		0
#define ACX_STATUS_1_SCANNING		1
#define ACX_STATUS_2_WAIT_AUTH		2
#define ACX_STATUS_3_AUTHENTICATED	3
#define ACX_STATUS_4_ASSOCIATED		4

/* FIXME: this should be named something like struct acx_priv (typedef'd to
 * acx_priv_t) */

/* non-firmware struct, no packing necessary */
struct acx_device {
	/* most frequent accesses first (dereferencing and cache line!) */

	/*** Locking ***/
	struct mutex		mutex;
	spinlock_t		spinlock;

#ifdef OW_20100613_OBSELETE_ACXLOCK_REMOVE
#if defined(PARANOID_LOCKING) /* Lock debugging */
	const char		*last_sem;
	const char		*last_lock;
	unsigned long		sem_time;
	unsigned long		lock_time;
#endif
#endif

	/*** Linux network device ***/
	//struct device	*dev;		/* pointer to linux netdevice */

	/*** Device statistics ***/
	struct ieee80211_low_level_stats	ieee_stats;		/* wireless device statistics */

	/*** Device statistics ***/
	struct net_device_stats	stats;		/* net device statistics */

#ifdef WIRELESS_EXT
//	struct iw_statistics	wstats;		/* wireless statistics */
#endif
	struct ieee80211_hw	*ieee;
	// FIXME OW 20100616 rx_status is reported for each skb. Check if this field is really required
	struct ieee80211_rx_status rx_status;
	struct ieee80211_vif	*vif;

	/*** Power managment ***/
	struct pm_dev		*pm;		/* PM crap */

	/*** Management timer ***/
	struct timer_list	mgmt_timer;

	/*** Hardware identification ***/
	const char		*chip_name;
	u8			dev_type;
	u8			chip_type;
	u8			form_factor;
	u8			radio_type;
	u8			eeprom_version;

	/*** Config retrieved from EEPROM ***/
	char			cfgopt_NVSv[8];
	u16			cfgopt_NVS_vendor_offs;
	u8			cfgopt_MAC[6];
	u16			cfgopt_probe_delay;
	u32			cfgopt_eof_memory;
	u8			cfgopt_dot11CCAModes;
	u8			cfgopt_dot11Diversity;
	u8			cfgopt_dot11ShortPreambleOption;
	u8			cfgopt_dot11PBCCOption;
	u8			cfgopt_dot11ChannelAgility;
	u8			cfgopt_dot11PhyType;
	u8			cfgopt_dot11TempType;
	co_antennas_t		cfgopt_antennas;
	co_powerlevels_t	cfgopt_power_levels;
	co_datarates_t		cfgopt_data_rates;
	co_domains_t		cfgopt_domains;
	co_product_id_t		cfgopt_product_id;
	co_manuf_t		cfgopt_manufacturer;

	/*** Firmware identification ***/
	char		firmware_version[FW_ID_SIZE+1];
	u32		firmware_numver;
	u32		firmware_id;
	const u16	*ie_len;
	const u16	*ie_len_dot11;

	/*** Device state ***/
	u16		dev_state_mask;
	u8		led_power;		/* power LED status */
	u32		get_mask;		/* mask of settings to fetch from the card */
	u32		set_mask;		/* mask of settings to write to the card */
	u32		initialized:1;
	/* Barely used in USB case */
	u16		irq_status;
#ifdef UNUSED
	int		irq_savedstate;
#endif	
	int		irq_reason;
	u8		after_interrupt_jobs;	/* mini job list for doing actions after an interrupt occurred */

	struct work_struct	irq_work;	/* our task for after interrupt actions */

	unsigned int	irq;

	/*** scanning ***/
	u16		scan_count;		/* number of times to do channel scan */
	u8		scan_mode;		/* 0 == active, 1 == passive, 2 == background */
	u8		scan_rate;
	u16		scan_duration;
	u16		scan_probe_delay;
#if WIRELESS_EXT > 15
//	struct iw_spy_data	spy_data;	/* FIXME: needs to be implemented! */
#endif

	// TODO FIXME Fields previously defined in acx_mac80211.h. Review usage what and how
	int vif_type;
    /* Counter of active monitor interfaces. */
	// TODO FIXME Review if required / usage
    int vif_monitor;
    /* Is the card operating in AP, STA or IBSS mode? */
	// TODO FIXME Review if required / usage
    unsigned int vif_operating:1;

	/*** Wireless network settings ***/
	/* copy of the device address (ifconfig hw ether) that we actually use
	** for 802.11; copied over from the network device's MAC address
	** (ifconfig) when it makes sense only */
	u8		dev_addr[MAX_ADDR_LEN];
	u8		bssid[ETH_ALEN];	/* the BSSID after having joined */
	u8		ap[ETH_ALEN];		/* The AP we want, FF:FF:FF:FF:FF:FF is any */
	u16		aid;			/* The Association ID sent from the AP / last used AID if we're an AP */
	u16		mode;			/* mode from iwconfig */
	int		monitor_type;		/* ARPHRD_IEEE80211 or ARPHRD_IEEE80211_PRISM */
	u16		status;			/* 802.11 association status */
	u8		essid_active;		/* specific ESSID active, or select any? */
	u8		essid_len;		/* to avoid dozens of strlen() */
	/* INCLUDES \0 termination for easy printf - but many places
	** simply want the string data memcpy'd plus a length indicator!
	** Keep that in mind... */
	char		essid[IW_ESSID_MAX_SIZE+1];
	/* essid we are going to use for association, in case of "essid 'any'"
	** and in case of hidden ESSID (use configured ESSID then) */
	char		essid_for_assoc[IW_ESSID_MAX_SIZE+1];
	char		nick[IW_ESSID_MAX_SIZE+1]; /* see essid! */
	u8		channel;
	u8		reg_dom_id;		/* reg domain setting */
	u16		reg_dom_chanmask;

#ifdef UNUSED
	u16		auth_or_assoc_retries;
	u16		scan_retries;
	unsigned long	scan_start;		/* YES, jiffies is defined as "unsigned long" */
#endif

	/* MAC80211 Template Reference */
	struct sk_buff *beacon_skb;
	u8 *beacon_tim;
	int beacon_ready;
	/* stations known to us (if we're an ap) */
//	client_t	sta_list[32];		/* tab is larger than list, so that */
//	client_t	*sta_hash_tab[64];	/* hash collisions are not likely */
//	client_t	*ap_client;		/* this one is our AP (STA mode only) */

	// Mac80211 Tx_queue
	struct sk_buff_head tx_queue;
	struct work_struct tx_work;

#ifdef UNUSED
	int		dup_count;
	int		nondup_count;
	unsigned long	dup_msg_expiry;
	u16		last_seq_ctrl;		/* duplicate packet detection */
#endif

	/* 802.11 power save mode */
	u8		ps_wakeup_cfg;
	u8		ps_listen_interval;
	u8		ps_options;
	u8		ps_hangover_period;
	u32		ps_enhanced_transition_time;
	u32		ps_beacon_rx_time;

	/*** PHY settings ***/
	u8		fallback_threshold;
	u8		stepup_threshold;
	u16		rate_basic;
	u16		rate_oper;
	u16		rate_bcast;
	u16		rate_bcast100;
	u8		rate_auto;		/* false if "iwconfig rate N" (WITHOUT 'auto'!) */
	u8		preamble_mode;		/* 0 == Long Preamble, 1 == Short, 2 == Auto */
	u8		preamble_cur;

	u8		tx_enabled;
	u8		rx_enabled;
	int		tx_level_dbm;
	u8		tx_level_val;
	/* u8		tx_level_auto;		whether to do automatic power adjustment */

	unsigned long	recalib_time_last_success;
	unsigned long	recalib_time_last_attempt;
	int		recalib_failure_count;
	int		recalib_msg_ratelimit;
	int		retry_errors_msg_ratelimit;
	int 	recalib_auto;

	unsigned long	brange_time_last_state_change;	/* time the power LED was last changed */
	u8		brange_last_state;	/* last state of the LED */
	u8		brange_max_quality;	/* maximum quality that equates to full speed */

	u8		sensitivity;
	u8		antenna[2];		/* antenna settings */
	u8		ed_threshold;		/* energy detect threshold */
	u8		cca;			/* clear channel assessment */

	u16		rts_threshold;
	u16		frag_threshold;
	u32		short_retry;
	u32		long_retry;
	u16		msdu_lifetime;
	u16		listen_interval;	/* given in units of beacon interval */
	u32		beacon_interval;

	u16		capabilities;
	u8		rate_supported_len;
	u8		rate_supported[13];

	/*** Encryption settings (WEP) ***/
	u32		auth_alg;		/* used in transmit_authen1 */
	u8		wep_enabled;
	u8		wep_restricted;
	u8		wep_current_index;
	wep_key_t	wep_keys[DOT11_MAX_DEFAULT_WEP_KEYS];	/* the default WEP keys */

	key_struct_t	wep_key_struct[10];

	/*** Unknown ***/
	u8		dtim_interval;

	/*** Card Rx/Tx management ***/
	u16		rx_config_1;
	u16		rx_config_2;
	u16		memblocksize;
	unsigned int	tx_free;
	unsigned int	tx_head; /* keep as close as possible to Tx stuff below (cache line) */
	u16		phy_header_len;

/*************************************************************************
 *** PCI/USB/... must be last or else hw agnostic code breaks horribly ***
 *************************************************************************/

#ifdef CONFIG_ACX_MAC80211_MEM
	u32 acx_txbuf_start;
	int acx_txbuf_numblocks;
	u32 acx_txbuf_free; /* addr of head of free list          */
	int acx_txbuf_blocks_free; /* how many are still open            */
	queueindicator_t *acx_queue_indicator;
#endif

	/*** PCI stuff ***/
#if (defined(CONFIG_ACX_MAC80211_PCI) || defined(CONFIG_ACX_MAC80211_MEM))
	/* pointers to tx buffers, tx host descriptors (in host memory)
	** and tx descs in device memory */
	unsigned int	tx_tail;
	u8		*txbuf_start;
	txhostdesc_t	*txhostdesc_start;
	txdesc_t	*txdesc_start;	/* points to PCI-mapped memory */
	dma_addr_t	txbuf_startphy;
	dma_addr_t	txhostdesc_startphy;
	/* sizes of above host memory areas */
	unsigned int	txbuf_area_size;
	unsigned int	txhostdesc_area_size;

	unsigned int	txdesc_size;	/* size of txdesc; ACX111 = ACX100 + 4 */

	/* same for rx */
	unsigned int	rx_tail;
	rxbuffer_t	*rxbuf_start;
	rxhostdesc_t	*rxhostdesc_start;
	rxdesc_t	*rxdesc_start;
	/* physical addresses of above host memory areas */
	dma_addr_t	rxbuf_startphy;
	dma_addr_t	rxhostdesc_startphy;
	unsigned int	rxbuf_area_size;
	unsigned int	rxhostdesc_area_size;

	u8		need_radio_fw;
	u8		irqs_active;	/* whether irq sending is activated */

	const u16	*io;		/* points to ACX100 or ACX111 PCI I/O register address set */

#ifdef CONFIG_VLYNQ
	struct vlynq_device	*vdev;
#endif
	struct device *bus_dev;

// OW FIXME Type and ifdef mess ..
#ifdef ACX_MAC80211_PCI
	struct pci_dev	*pdev;
#endif
#ifdef ACX_MAC80211_MEM
	struct platform_device	*pdev;
#endif

// OW FIXME Type and ifdef mess ..
#ifdef ACX_MAC80211_PCI
	unsigned long	membase;
#endif
#ifdef ACX_MAC80211_MEM
	volatile u32	*membase;
#endif
	unsigned long	membase2;
#ifdef ACX_MAC80211_PCI
	void __iomem	*iobase;
#endif
#ifdef ACX_MAC80211_MEM
	volatile u32	*iobase;
#endif

	void __iomem	*iobase2;

	/* command interface */
	u8 __iomem	*cmd_area;
	u8 __iomem	*info_area;

	u16		irq_mask;		/* interrupt types to mask out (not wanted) with many IRQs activated */
	unsigned int	irq_loops_this_jiffy;
	unsigned long	irq_last_jiffies;
#endif

	/*** USB stuff ***/
#ifdef ACX_MAC80211_USB
	struct usb_device	*usbdev;

	rxbuffer_t	rxtruncbuf;

	usb_tx_t	*usb_tx;
	usb_rx_t	*usb_rx;

	int		bulkinep;	/* bulk-in endpoint */
	int		bulkoutep;	/* bulk-out endpoint */
	int		rxtruncsize;
#endif

};
// ---

static inline
acx_device_t* ieee2adev(struct ieee80211_hw *hw)
{
        return hw->priv;
}

#endif
