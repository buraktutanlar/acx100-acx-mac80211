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
#endif /* ACX_USB */
#endif /* _ACXUSB_H_ */

