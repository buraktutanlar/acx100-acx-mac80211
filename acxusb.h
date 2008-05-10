#ifndef _ACXUSB_H_
#define _ACXUSB_H_

/*
 * acxusb.h: USB specific constants and structures.
 *
 * Copyright (c) 2008 Francis Galiegue <fgaliegue@gmail.com> for the ACX100
 * driver project.
 *
 * This file is licensed under the GPLv2. See the README file for details.
 */
/* Used for usb_txbuffer.desc field */
#define USB_TXBUF_TXDESC	0xA
/* Size of header (everything up to data[]) */
#define USB_TXBUF_HDRSIZE	14
typedef struct usb_txbuffer {
	u16	desc;
	u16	mpdu_len;
	u8	queue_index;
	u8	rate;
	u32	hostdata;
	u8	ctrl1;
	u8	ctrl2;
	u16	data_len;
	/* wlan packet content is placed here: */
	u8	data[30 + 2312 + 4]; /*WLAN_A4FR_MAXLEN_WEP_FCS]*/
} __attribute__ ((packed)) usb_txbuffer_t;

/* USB returns either rx packets (see rxbuffer) or
** these "tx status" structs: */
typedef struct usb_txstatus {
	u16	mac_cnt_rcvd;		/* only 12 bits are len! (0xfff) */
	u8	queue_index;
	u8	mac_status;		/* seen 0x20 on tx failure */
	u32	hostdata;
	u8	rate;
	u8	ack_failures;
	u8	rts_failures;
	u8	rts_ok;
//	struct ieee80211_tx_status txstatus;
//	struct sk_buff *skb;	
} __attribute__ ((packed)) usb_txstatus_t;

typedef struct usb_tx {
	unsigned	busy:1;
	struct urb	*urb;
	acx_device_t	*adev;
	/* actual USB bulk output data block is here: */
	usb_txbuffer_t	bulkout;
} usb_tx_t;

struct usb_rx_plain {
	unsigned	busy:1;
	struct urb	*urb;
	acx_device_t	*adev;
	rxbuffer_t	bulkin;
};

typedef struct usb_rx {
	unsigned	busy:1;
	struct urb	*urb;
	acx_device_t	*adev;
	rxbuffer_t	bulkin;
	/* Make entire structure 4k */
	u8 padding[4*1024 - sizeof(struct usb_rx_plain)];
} usb_rx_t;

#if 0
struct acx_device {
	/* most frequent accesses first (dereferencing and cache line!) */

	/*
	 * Locking 
	 */
	struct mutex		mutex;
	spinlock_t		spinlock;
	spinlock_t		irqlock;
	/*
	 * IRQ handling
	 */
	/* The IRQ we have inherited */
	unsigned int	irq;
	/* Are IRQs currently activated? FIXME: should get rid of this */
	u8		irqs_active;
	/* The interrupts we can acknowledge (see acx_irq.h) */
	u16		irq_mask;
	/* The mask of IRQs saved by the IRQ top half routine */
	u16		irq_saved_mask;
	/*
	 * FIXME: these ones should disappear
	 */
	unsigned int	irq_loops_this_jiffy;
	unsigned long	irq_last_jiffies;
	/* Barely used in USB case (FIXME?) */
	u16		irq_status;
	int		irq_reason; /* FIXME: should be u16 */
	/* Mask of jobs we have to schedule post interrupt */
	u8		after_interrupt_jobs;
	/* 
	 * Work queue for the bottom half. FIXME: only one, consider a
	 * delayed_work struct some day?
	 */
	struct work_struct	after_interrupt_task;
#if defined(PARANOID_LOCKING) /* Lock debugging */
	const char		*last_sem;
	const char		*last_lock;
	unsigned long		sem_time;
	unsigned long		lock_time;
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
	struct ieee80211_hw_mode	modes[2];
	struct ieee80211_rx_status rx_status;

	struct ieee80211_vif 	*vif;

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


	/*** scanning ***/
	u16		scan_count;		/* number of times to do channel scan */
	u8		scan_mode;		/* 0 == active, 1 == passive, 2 == background */
	u8		scan_rate;
	u16		scan_duration;
	u16		scan_probe_delay;
#if WIRELESS_EXT > 15
//	struct iw_spy_data	spy_data;	/* FIXME: needs to be implemented! */
#endif

	/*** Virtual interface struct ***/
	struct acx_interface interface;

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
	u16		auth_or_assoc_retries;
	u16		scan_retries;
	unsigned long	scan_start;		/* YES, jiffies is defined as "unsigned long" */


	/* MAC80211 Template Reference */
	struct sk_buff *beacon_cache;
	/* stations known to us (if we're an ap) */
//	client_t	sta_list[32];		/* tab is larger than list, so that */
//	client_t	*sta_hash_tab[64];	/* hash collisions are not likely */
//	client_t	*ap_client;		/* this one is our AP (STA mode only) */

	int		dup_count;
	int		nondup_count;
	unsigned long	dup_msg_expiry;
	u16		last_seq_ctrl;		/* duplicate packet detection */

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

	u8		tx_disabled;
	u8		tx_level_dbm;
	/* u8		tx_level_val; */
	/* u8		tx_level_auto;		whether to do automatic power adjustment */

	unsigned long	recalib_time_last_success;
	unsigned long	recalib_time_last_attempt;
	int		recalib_failure_count;
	int		recalib_msg_ratelimit;
	int		retry_errors_msg_ratelimit;

	unsigned long	brange_time_last_state_change;	/* time the power LED was last changed */
	u8		brange_last_state;	/* last state of the LED */
	u8		brange_max_quality;	/* maximum quality that equates to full speed */

	u8		sensitivity;
	u8		antenna;		/* antenna settings */
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

	/*** Encryption Replacement for mac80211 ***/
	struct acx_key	key[54];
	u16 security_offset;
	u8 default_key_idx;


	/*** Unknown ***/
	u8		dtim_interval;

	/*** Card Rx/Tx management ***/
	u16		rx_config_1;
	u16		rx_config_2;
	u16		memblocksize;
	unsigned int	tx_free;
	unsigned int	tx_head; /* keep as close as possible to Tx stuff below (cache line) */
	u16		phy_header_len;
	
	/*
	 * Bus specific (USB here) stuff.
	 */

	/* hack to let common code compile. FIXME */
	dma_addr_t	rxhostdesc_startphy;

	struct usb_device	*usbdev;

	rxbuffer_t	rxtruncbuf;

	usb_tx_t	*usb_tx;
	usb_rx_t	*usb_rx;

	int		bulkinep;	/* bulk-in endpoint */
	int		bulkoutep;	/* bulk-out endpoint */
	int		rxtruncsize;

};
#endif

#endif /* _ACXUSB_H_ */

