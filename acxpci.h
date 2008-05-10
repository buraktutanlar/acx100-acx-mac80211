#ifndef _ACXPCI_H_
#define _ACXPCI_H_

/*
 * acxpci.h: PCI related constants and structures.
 *
 * Copyright (c) 2008 Francis Galiegue <fgaliegue@gmail.com> for the ACX100
 * driver project.
 *
 * This file is licensed under the GPLv2. See the README file for details.
 */

#include "acx_config.h" /* For PARANOID_LOCKING */
#include "acx_mac80211.h" /* For struct acx_interface */

/*
 * MMIO registers
 */

/* Register I/O offsets */
#define ACX100_EEPROM_ID_OFFSET	0x380

/*
 * Please add further ACX hardware register definitions only when
 * it turns out you need them in the driver, and please try to use firmware
 * functionality instead, since using direct I/O access instead of letting the
 * firmware do it might confuse the firmware's state machine.
 */

/* ***** ABSOLUTELY ALWAYS KEEP OFFSETS IN SYNC WITH THE INITIALIZATION
** OF THE I/O ARRAYS!!!! (grep for '^IO_ACX') ***** */

/*
 * NOTE about ACX_IO_IRQ_REASON: this register is CLEARED ON READ.
 */
#define	ACX_IO_SOFT_RESET		0
#define	ACX_IO_SLV_MEM_ADDR 		1
#define	ACX_IO_SLV_MEM_DATA 		2
#define	ACX_IO_SLV_MEM_CTL 		3
#define	ACX_IO_SLV_END_CTL 		4
/*
 * Original code said that the following is the "function event mask". Whatever
 * that means.
 */
#define	ACX_IO_FEMR 			5
#define	ACX_IO_INT_TRIG 		6
#define	ACX_IO_IRQ_MASK 		7
#define	ACX_IO_IRQ_STATUS_NON_DES 	8
#define	ACX_IO_IRQ_REASON 		9
#define	ACX_IO_IRQ_ACK 			10
#define	ACX_IO_HINT_TRIG 		11
#define	ACX_IO_ENABLE 			12
#define	ACX_IO_EEPROM_CTL 		13
#define	ACX_IO_EEPROM_ADDR 		14
#define	ACX_IO_EEPROM_DATA 		15
#define	ACX_IO_EEPROM_CFG 		16
#define	ACX_IO_PHY_ADDR 		17
#define	ACX_IO_PHY_DATA 		18
#define	ACX_IO_PHY_CTL 			19
#define	ACX_IO_GPIO_OE 			20
#define	ACX_IO_GPIO_OUT 		21
#define	ACX_IO_CMD_MAILBOX_OFFS 	22
#define	ACX_IO_INFO_MAILBOX_OFFS 	23
#define	ACX_IO_EEPROM_INFORMATION 	24
#define	ACX_IO_EE_START 		25
#define	ACX_IO_SOR_CFG 			26
#define	ACX_IO_ECPU_CTRL 		27
/* ***** ABSOLUTELY ALWAYS KEEP OFFSETS IN SYNC WITH THE INITIALIZATION
** OF THE I/O ARRAYS!!!! (grep for '^IO_ACX') ***** */

/* Values for ACX_IO_INT_TRIG register: */
/* inform hw that rxdesc in queue needs processing */
#define INT_TRIG_RXPRC		0x08
/* inform hw that txdesc in queue needs processing */
#define INT_TRIG_TXPRC		0x04
/* ack that we received info from info mailbox */
#define INT_TRIG_INFOACK	0x02
/* inform hw that we have filled command mailbox */
#define INT_TRIG_CMD		0x01

/*
 * In-hardware TX/RX structures
 */

struct txhostdesc {
	acx_ptr	data_phy;			/* 0x00 [u8 *] */
	u16	data_offset;			/* 0x04 */
	u16	reserved;			/* 0x06 */
	u16	Ctl_16;	/* 16bit value, endianness!! */
	u16	length;			/* 0x0a */
	acx_ptr	desc_phy_next;		/* 0x0c [txhostdesc *] */
	acx_ptr	pNext;			/* 0x10 [txhostdesc *] */
	u32	Status;			/* 0x14, unused on Tx */
/* From here on you can use this area as you want (variable length, too!) */
	u8	*data;
	struct ieee80211_tx_status txstatus;
	struct sk_buff *skb;	

} __attribute__ ((packed));

struct rxhostdesc {
	acx_ptr	data_phy;			/* 0x00 [rxbuffer_t *] */
	u16	data_offset;			/* 0x04 */
	u16	reserved;			/* 0x06 */
	u16	Ctl_16;			/* 0x08; 16bit value, endianness!! */
	u16	length;			/* 0x0a */
	acx_ptr	desc_phy_next;		/* 0x0c [rxhostdesc_t *] */
	acx_ptr	pNext;			/* 0x10 [rxhostdesc_t *] */
	u32	Status;			/* 0x14 */
/* From here on you can use this area as you want (variable length, too!) */
	rxbuffer_t *data;
} __attribute__ ((packed));

/*
 * acx_device structure: the ->priv part of the ieee80211_hw structure
 */

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
	
/*************************************************************************
 *** PCI/USB/... must be last or else hw agnostic code breaks horribly ***
 *************************************************************************/
 	/*
	 * Bus specific (PCI here) code.
	 *
	 * The original header file said "must be last or else hw agnostic code
	 * breaks horribly" - huh?
	 */

	/* hack to let common code compile. FIXME */
	dma_addr_t	rxhostdesc_startphy;

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
	client_t	*txc[TX_CNT];
	u16		txr[TX_CNT];

	/* same for rx */
	unsigned int	rx_tail;
	rxbuffer_t	*rxbuf_start;
	rxhostdesc_t	*rxhostdesc_start;
	rxdesc_t	*rxdesc_start;
	/* physical addresses of above host memory areas */
	dma_addr_t	rxbuf_startphy;
	/* dma_addr_t	rxhostdesc_startphy; */
	unsigned int	rxbuf_area_size;
	unsigned int	rxhostdesc_area_size;

	u8		need_radio_fw;

	const u16	*io;		/* points to ACX100 or ACX111 PCI I/O register address set */

#ifdef CONFIG_PCI
	struct pci_dev	*pdev;
#endif
#ifdef CONFIG_VLYNQ
	struct vlynq_device	*vdev;
#endif
	struct device *bus_dev;
	unsigned long	membase;
	unsigned long	membase2;
	void __iomem	*iobase;
	void __iomem	*iobase2;
	/* command interface */
	u8 __iomem	*cmd_area;
	u8 __iomem	*info_area;
};
#endif

#endif /* _ACXPCI_H_ */
