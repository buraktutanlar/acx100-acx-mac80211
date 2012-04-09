
// usb.h - included directly by common.c

/*
 * BOM Prototypes
 * ... static and also none-static for overview reasons (maybe not best practice ...)
 * ==================================================
 */

#if defined CONFIG_ACX_MAC80211_USB // by Makefile, Kbuild

#include <linux/usb.h>

// Logging

// Data Access

// Firmware, EEPROM, Phy
int acxusb_read_phy_reg(acx_device_t * adev, u32 reg, u8 * charbuf);
int acxusb_write_phy_reg(acx_device_t * adev, u32 reg, u8 value);
// static void acxusb_read_eeprom_version(acx_device_t * adev);
// static int acxusb_boot(struct usb_device *usbdev, int is_tnetw1450, int *radio_type);
// static inline int acxusb_fw_needs_padding(firmware_image_t *fw_image, unsigned int usb_maxlen);

// CMDs (Control Path)
int acxusb_issue_cmd_timeo_debug(acx_device_t * adev, unsigned cmd, void *buffer, unsigned buflen, unsigned timeout, const char *cmdstr);

// Init, Configure (Control Path)
// static int acxusb_fill_configoption(acx_device_t * adev);

// Other (Control Path)

// Proc, Debug
#ifdef UNUSED
static void dump_device(struct usb_device *usbdev);
static void dump_config_descriptor(struct usb_config_descriptor *cd);
static void dump_device_descriptor(struct usb_device_descriptor *dd);
#endif

// Rx Path
// static void acxusb_complete_rx(struct urb *);
// static void acxusb_poll_rx(acx_device_t * adev, usb_rx_t * rx);

// Tx Path
tx_t *acxusb_alloc_tx(acx_device_t *adev);
void acxusb_dealloc_tx(tx_t * tx_opaque);
void *acxusb_get_txbuf(acx_device_t * adev, tx_t * tx_opaque);
void acxusb_tx_data(acx_device_t *adev, tx_t *tx_opaque, int wlanpkt_len, struct ieee80211_tx_info *ieeectl, struct sk_buff *skb);

// Irq Handling, Timer
void acxusb_irq_work(struct work_struct *work);

// Mac80211 Ops
// static int acxusb_op_start(struct ieee80211_hw *);
// static void acxusb_op_stop(struct ieee80211_hw *);

// Helpers
// static void acxusb_unlink_urb(struct urb *urb);

// Driver, Module
// static int acxusb_probe(struct usb_interface *intf, const struct usb_device_id *devID);
// static void acxusb_disconnect(struct usb_interface *intf);

int __init acxusb_init_module(void);
void __exit acxusb_cleanup_module(void);

#else  // !CONFIG_ACX_MAC80211_USB stubs

static inline int acxusb_read_phy_reg(acx_device_t * adev, u32 reg, u8 * charbuf)
{
	return 0;
}
static inline int acxusb_write_phy_reg(acx_device_t * adev, u32 reg, u8 value)
{
	return 0;
}

static inline int acxusb_issue_cmd_timeo_debug(acx_device_t * adev, unsigned cmd,
					void *buffer, unsigned buflen,
					unsigned timeout, const char *cmdstr)
{
	return 0;
}

static inline tx_t *acxusb_alloc_tx(acx_device_t *adev)
{
	return (tx_t*) NULL;
}

static inline void acxusb_dealloc_tx(tx_t * tx_opaque)
{}

static inline void *acxusb_get_txbuf(acx_device_t * adev, tx_t * tx_opaque)
{
	return (void*) NULL;
}

static inline void acxusb_tx_data(acx_device_t *adev, tx_t *tx_opaque, int wlanpkt_len,
				struct ieee80211_tx_info *ieeectl, struct sk_buff *skb)
{}

static inline void acxusb_irq_work(struct work_struct *work)
{}

static inline int __init acxusb_init_module(void)
{
	return 0;
}

static inline void __exit acxusb_cleanup_module(void)
{}

#endif

