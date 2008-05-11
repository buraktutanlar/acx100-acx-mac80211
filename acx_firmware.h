#ifndef _ACX_FIRMWARE_H_
#define _ACX_FIRMWARE_H_

/*
 * acx_firmware.h: firmware-related structures and constants.
 *
 * Copyright (c) 2008 Francis Galiegue <fgaliegue@gmail.com> for the ACX100
 * driver project.
 *
 * This file is licensed under the GPL version 2. See the README file for
 * details.
 */

/*
 * The original comments talk about "firmware statistics". I take it this means
 * that you can read from these structures and gather values.
 */

/* 
 * The original comment defined this constant as "a random 100 bytes more to
 * catch firmware versions which provide a bigger struct".
 *
 * Err, FIXME! Does that mean that this area could actually be written to beyond
 * our knowledge???
 *
 */
#define FW_STATS_FUTURE_EXTENSION	100

typedef struct firmware_image {
	u32	chksum;
	u32	size;
	u8	data[1]; /* the byte array of the actual firmware... */
} __attribute__ ((packed)) firmware_image_t;

typedef struct fw_stats_tx {
	u32	tx_desc_of;
} __attribute__ ((packed)) fw_stats_tx_t;

typedef struct fw_stats_rx {
	u32	rx_oom;
	u32	rx_hdr_of;
	u32	rx_hw_stuck; /* old: u32	rx_hdr_use_next */
	u32	rx_dropped_frame;
	u32	rx_frame_ptr_err;
	u32	rx_xfr_hint_trig;
	u32	rx_aci_events; /* later versions only */
	u32	rx_aci_resets; /* later versions only */
} __attribute__ ((packed)) fw_stats_rx_t;

typedef struct fw_stats_dma {
	u32	rx_dma_req;
	u32	rx_dma_err;
	u32	tx_dma_req;
	u32	tx_dma_err;
} __attribute__ ((packed)) fw_stats_dma_t;

typedef struct fw_stats_irq {
	u32	cmd_cplt;
	u32	fiq;
	u32	rx_hdrs;
	u32	rx_cmplt;
	u32	rx_mem_of;
	u32	rx_rdys;
	u32	irqs;
	u32	tx_procs;
	u32	decrypt_done;
	u32	dma_0_done;
	u32	dma_1_done;
	u32	tx_exch_complet;
	u32	commands;
	u32	rx_procs;
	u32	hw_pm_mode_changes;
	u32	host_acks;
	u32	pci_pm;
	u32	acm_wakeups;
} __attribute__ ((packed)) fw_stats_irq_t;

typedef struct fw_stats_wep {
	u32	wep_key_count;
	u32	wep_default_key_count;
	u32	dot11_def_key_mib;
	u32	wep_key_not_found;
	u32	wep_decrypt_fail;
	u32	wep_pkt_decrypt;
	u32	wep_decrypt_irqs;
} __attribute__ ((packed)) fw_stats_wep_t;

typedef struct fw_stats_pwr {
	u32	tx_start_ctr;
	u32	no_ps_tx_too_short;
	u32	rx_start_ctr;
	u32	no_ps_rx_too_short;
	u32	lppd_started;
	u32	no_lppd_too_noisy;
	u32	no_lppd_too_short;
	u32	no_lppd_matching_frame;
} __attribute__ ((packed)) fw_stats_pwr_t;

typedef struct fw_stats_mic {
	u32	mic_rx_pkts;
	u32	mic_calc_fail;
} __attribute__ ((packed)) fw_stats_mic_t;

typedef struct fw_stats_aes {
	u32	aes_enc_fail;
	u32	aes_dec_fail;
	u32	aes_enc_pkts;
	u32	aes_dec_pkts;
	u32	aes_enc_irq;
	u32	aes_dec_irq;
} __attribute__ ((packed)) fw_stats_aes_t;

typedef struct fw_stats_event {
	u32	heartbeat;
	u32	calibration;
	u32	rx_mismatch;
	u32	rx_mem_empty;
	u32	rx_pool;
	u32	oom_late;
	u32	phy_tx_err;
	u32	tx_stuck;
} __attribute__ ((packed)) fw_stats_event_t;

/* mainly for size calculation only */
typedef struct fw_stats {
	u16			type;
	u16			len;
	fw_stats_tx_t		tx;
	fw_stats_rx_t		rx;
	fw_stats_dma_t		dma;
	fw_stats_irq_t		irq;
	fw_stats_wep_t		wep;
	fw_stats_pwr_t		pwr;
	fw_stats_mic_t		mic;
	fw_stats_aes_t		aes;
	fw_stats_event_t	evt;
	u8			_padding[FW_STATS_FUTURE_EXTENSION];
} fw_stats_t;

/* Firmware version struct */

typedef struct fw_ver {
	u16	cmd;
	u16	size;
	char	fw_id[20];
	u32	hw_id;
} __attribute__ ((packed)) fw_ver_t;

#define FW_ID_SIZE 20

#endif /* _ACX_FIRMWARE_H_ */
