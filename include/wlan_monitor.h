/* required structs for prism header emulation (monitor mode) */
#define WLAN_DEVNAMELEN_MAX 16

#define DIDmsg_lnxind_wlansniffrm		0x0041
#define DIDmsg_lnxind_wlansniffrm_hosttime	0x1041
#define DIDmsg_lnxind_wlansniffrm_mactime	0x2041
#define DIDmsg_lnxind_wlansniffrm_channel	0x3041
#define DIDmsg_lnxind_wlansniffrm_rssi		0x4041
#define DIDmsg_lnxind_wlansniffrm_sq		0x5041
#define DIDmsg_lnxind_wlansniffrm_signal	0x6041
#define DIDmsg_lnxind_wlansniffrm_noise		0x7041
#define DIDmsg_lnxind_wlansniffrm_rate		0x8041
#define DIDmsg_lnxind_wlansniffrm_istx		0x9041
#define DIDmsg_lnxind_wlansniffrm_frmlen	0xA041

    __WLAN_PRAGMA_PACK1__ typedef struct p80211item_uint32 {
	u32 did __WLAN_ATTRIB_PACK__;
	u16 status __WLAN_ATTRIB_PACK__;
	u16 len __WLAN_ATTRIB_PACK__;
	u32 data __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ p80211item_uint32_t;
__WLAN_PRAGMA_PACKDFLT__

typedef struct p80211msg_lnxind_wlansniffrm {
	uint32_t msgcode __WLAN_ATTRIB_PACK__;
	uint32_t msglen __WLAN_ATTRIB_PACK__;
	uint8_t devname[WLAN_DEVNAMELEN_MAX] __WLAN_ATTRIB_PACK__;
	p80211item_uint32_t hosttime __WLAN_ATTRIB_PACK__;
	p80211item_uint32_t mactime __WLAN_ATTRIB_PACK__;
	p80211item_uint32_t channel __WLAN_ATTRIB_PACK__;
	p80211item_uint32_t rssi __WLAN_ATTRIB_PACK__;
	p80211item_uint32_t sq __WLAN_ATTRIB_PACK__;
	p80211item_uint32_t signal __WLAN_ATTRIB_PACK__;
	p80211item_uint32_t noise __WLAN_ATTRIB_PACK__;
	p80211item_uint32_t rate __WLAN_ATTRIB_PACK__;
	p80211item_uint32_t istx __WLAN_ATTRIB_PACK__;
	p80211item_uint32_t frmlen __WLAN_ATTRIB_PACK__;
} p80211msg_lnxind_wlansniffrm_t;

#define P80211ENUM_truth_false				0
#define P80211ENUM_truth_true				1

#define P80211ENUM_resultcode_success			1
#define P80211ENUM_resultcode_invalid_parameters	2
#define P80211ENUM_resultcode_not_supported		3
#define P80211ENUM_resultcode_timeout			4
#define P80211ENUM_resultcode_too_many_req		5
#define P80211ENUM_resultcode_refused			6
#define P80211ENUM_resultcode_bss_already		7
#define P80211ENUM_resultcode_invalid_access		8
#define P80211ENUM_resultcode_invalid_mibattribute	9
#define P80211ENUM_resultcode_cant_set_readonly_mib	10
#define P80211ENUM_resultcode_implementation_failure	11
#define P80211ENUM_resultcode_cant_get_writeonly_mib	12

#define P80211ENUM_msgitem_status_data_ok		0
#define P80211ENUM_msgitem_status_no_value		1
#define P80211ENUM_msgitem_status_invalid_itemname	2
#define P80211ENUM_msgitem_status_invalid_itemdata	3
#define P80211ENUM_msgitem_status_missing_itemdata	4
#define P80211ENUM_msgitem_status_incomplete_itemdata	5
#define P80211ENUM_msgitem_status_invalid_msg_did	6
#define P80211ENUM_msgitem_status_invalid_mib_did	7
#define P80211ENUM_msgitem_status_missing_conv_func	8
#define P80211ENUM_msgitem_status_string_too_long	9
#define P80211ENUM_msgitem_status_data_out_of_range	10
#define P80211ENUM_msgitem_status_string_too_short	11
#define P80211ENUM_msgitem_status_missing_valid_func	12
#define P80211ENUM_msgitem_status_unknown		13
#define P80211ENUM_msgitem_status_invalid_did		14
#define P80211ENUM_msgitem_status_missing_print_func	15
