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
#define ISTATUS_0_STARTED	0
#define ISTATUS_1_SCANNING	1
#define ISTATUS_2_WAIT_AUTH	2
#define ISTATUS_3_AUTHENTICATED	3
#define ISTATUS_4_ASSOCIATED	4
#define ISTATUS_5_UNKNOWN	5



/* a helper struct for quick implementation of commands */
typedef struct GenericPacket {
	UINT8 bytes[32];
} GenericPacket_t;

typedef struct InitPacket {
//    UINT16 vala;
//    UINT16 valb;
	UINT CodeStart;		//0x4
	UINT CodeEnd;		//0x8
	UINT WEPCacheStart;	//0xc
	UINT WEPCacheEnd;	//0x10
	UINT PacketTemplateStart;	//0x14
	UINT PacketTemplateEnd;	//0x18
	UINT valc;
} InitPacket_t;

typedef struct InitPacket2 {
//    char buf[0x30];
	UINT16 vald;
	UINT8 vale;
	UINT8 valf;
//              UINT PacketTemplateStart;//0x14
//              UINT PacketTemplateEnd;  //0x18
} InitPacket2_t;

#ifdef V1_VERSION
typedef struct ConfigWrite {
	UINT16 vala;		//0x4
	UINT16 valb;		//0x6
	UINT CodeStart;		//0x8
	UINT CodeEnd;		//0xc
	UINT8 valc;		//0x10
	UINT8 vald;		//0x11
	UINT8 val001;		//0x12
	UINT32 vale;		//0x16
	UINT valf;		//0x1a
	UINT valg;		//0x1e
	UINT8 valh;		//0x1f
	UINT8 vali;		//0x20
	UINT8 valj;		//0x21
} ConfigWrite_t;
typedef struct ConfigWrite2 {
	UINT16 vala;		//0x4
	UINT16 valb;		//0x6
	UINT CodeStart;		//0x8
	UINT CodeEnd;		//0xc
	UINT8 valc;		//0x10
	UINT8 vald;		//0x11
	UINT8 val001;		//0x12
	UINT32 vale;		//0x16
	UINT valf;		//0x1a
	UINT valg;		//0x1e
	UINT valh;		//0x1f
	UINT32 vali;		//0x20
	UINT32 val0x28;
//    UINT8 valj;     //0x21
} ConfigWrite2_t;
#else

/* I think these stucts changed (or where even wrong in version 1)
 */
typedef struct ConfigWrite {
	UINT16 vala;		//0x4
	UINT16 valb;		//0x6
	UINT CodeStart;		//0x8
	UINT CodeEnd;		//0xc
	UINT8 valc;		//0x10
	UINT8 vald;		//0x11
	UINT8 val001;		//0x12
	UINT8 val002;		//0x13
	UINT32 vale;		//0x14
	UINT valf;		//0x18
	UINT valg;		//0x1c
	UINT8 valh;		//0x20
/*    UINT8 vali;     //0x21
    UINT8 valj;     //0x22 */
} ConfigWrite_t;
typedef struct ConfigWrite2 {
	UINT16 vala;		//0x4
	UINT16 valb;		//0x6
	UINT CodeStart;		//0x8
	UINT CodeEnd;		//0xc
	UINT8 valc;		//0x10
	UINT8 vald;		//0x11
	UINT8 val001;		//0x12
	UINT8 val002;		//0x13
	UINT32 vale;		//0x14
	UINT valf;		//0x18
	UINT valg;		//0x1c
	UINT valh;		//0x20
	UINT32 vali;		//0x24
	UINT32 val0x28;		//0x28
} ConfigWrite2_t;
typedef struct ConfigWrite3 {
	UINT16 vala;		//0x4
	UINT16 valb;		//0x6
	UINT CodeStart;		//0x8
	UINT CodeEnd;		//0xc
	UINT8 valc;		//0x10
	UINT8 vald;		//0x11
	UINT8 val001;		//0x12
	UINT8 val002;		//0x13
	UINT32 vale;		//0x14
	UINT valf;		//0x18
	UINT valg;		//0x1c
	UINT valh;		//0x20
	UINT16 vali;		//0x24
	UINT16 valj;		//0x26
} ConfigWrite3_t;
#endif

typedef struct QueueConfig {
	UINT16 vala;
	UINT16 valb;
	UINT32 AreaSize;
	UINT32 RxQueueStart;
	UINT8  vald;
	UINT8  vale;
	UINT16 valf;
	UINT32 QueueEnd;
	UINT32 QueueEnd2;
	UINT32 TxQueueStart;
	UINT8  valj;
	UINT8  valk;
	UINT16 vall;
} QueueConfig_t;

typedef struct wep {
	UINT16 vala;

	UINT8 wep_key[MAX_KEYLEN];
	char key_name[0x16];
} wep_t;

typedef struct associd {
	UINT16 vala;
} associd_t;

typedef struct powermgmt {
	UINT8 a;
	UINT8 nul1;
	UINT8 b;
	UINT8 c;
	UINT16 nul2;
} powermgmt_t;

typedef struct defaultkey {
	UINT8 num;
} defaultkey_t;

typedef struct memmap {
	UINT16 type;
	UINT16 length;
	union data {
		struct GenericPacket gp;
		struct InitPacket ip;
		struct InitPacket2 ip2;
		struct ConfigWrite cw;
		struct ConfigWrite2 cw2;
		struct ConfigWrite3 cw3;
		struct QueueConfig qc;
		struct wep wp;
		struct associd asid;
		struct powermgmt power;
		struct defaultkey dkey;
	} m;
} memmap_t;

typedef struct sub3info {
	UINT8 size;
	UINT8 buf[0x4];
} sub3info_t;

typedef struct subsubinfo {
	UINT8 size;
	struct sub3info buf;
} subsubinfo_t;

typedef struct subinfo {
	UINT16 size;
	char buf[0x8];
	struct subsubinfo buf2;
	char buf3[0x8];
} subinfo_t;

typedef struct beaconinfo {
	UINT16 size;
	UINT16 status;
	UINT8 addr1[0x6];
	UINT8 addr2[0x6];
	UINT8 addr3[0x6];
	struct subinfo inf;
	UINT8 buf[0x1c];
} beaconinfo_t;

typedef struct beacon {
	UINT16 size;
	struct beaconinfo inf;
//  UINT16 vala;
} beacon_t;

typedef struct scan {
	UINT16 count; /* number of scans to do */
	UINT16 start_chan;
	UINT16 flags;
	UINT8 max_rate; /* max. probe rate */
	UINT8 options;
	UINT16 chan_duration;
	UINT16 max_probe_delay;
} scan_t;			/* length 0xc */

typedef struct tim {
	UINT16 size;
	char buf[0x100];
} tim_t;

typedef struct proberesp {
	UINT16 size;
	char buf[0x54];
} proberesp_t;

typedef struct probereq {
	UINT16 size;
	char buf[0x44];
} probereq_t;

typedef struct joinbss {
	UINT8 bssid[WLAN_ADDR_LEN];
	UINT16 beacon_interval;
	UINT8 val0x8;
	UINT8 val0x9;
	UINT8 val0xa;
	UINT8 val0xb;
	UINT8 capab_short;
	UINT8 macmode;
	UINT8 channel;
	UINT8 essid_len;
	char essid[0x20];	
} joinbss_t;

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
	p80211_hdr_a3_t a4;
	UINT8 val0x18[8];	/* this contains the Logical Link Control data
				   the first three bytes are the dsap, ssap and control
				   respectively. the following 3 bytes are either ieee_oui
				   or an RFC. See p80211conv.c/h for more. */
	UINT16 val0x20;
	UINT16 val0x22;
	UINT8 info[0x30];	/* 0x24 */
	/*
	 * info[1] is essid_len
	 * &info[2] is essid (max. 32 chars)
	 * FIXME: huh, then sizeof(info) == 0x18 cannot be correct! Adjusting
	 * to 0x30, which extends to end of this struct
	 */
//  p80211_hdr_a4_t b4;
} acxp80211_hdr_t;		/* size: 0x54 */


typedef struct acxp80211_packet {
	UINT16 size;		//this is probably to align it for memory mapping, the actual packet
	// should start with the packet.
	struct acxp80211_hdr hdr;
} acxp80211_packet_t;		/* size: 0x56 */


typedef struct {
    UINT32 chksum;
    UINT32 size;
    UINT8 data[1]; /* the byte array of the actual firmware... */
} firmware_image_t;

typedef struct {
    UINT32 offset;
    UINT32 len;
} radioinit_t;

void acx100_schedule(UINT32 timeout);
int acx100_reset_dev(netdevice_t * hw);
void acx100_join_bssid(wlandevice_t * hw);
int acx100_init_mac(netdevice_t * hw);
int acx100_set_defaults(wlandevice_t * hw);
void acx100_set_reg_domain(wlandevice_t *wlandev, unsigned char reg_dom_id);
int acx100_set_probe_response_template(wlandevice_t * hw);
int acx100_set_beacon_template(wlandevice_t * wlandev);
void acx100_set_timer(wlandevice_t * hw, UINT32 time);
void acx100_update_capabilities(wlandevice_t * hw);
unsigned int acx100_read_eeprom_offset(wlandevice_t * wlandev, UINT16 addr,
					unsigned char *charbuf);
void acx100_start(wlandevice_t * hw);
void acx100_reset_mac(wlandevice_t * wlandev);
firmware_image_t *acx100_read_fw( const char *file );
int acx100_upload_fw(wlandevice_t * wlandev);
int acx100_write_fw(wlandevice_t * wlandev, const firmware_image_t * apfw_image, UINT32 offset);
int acx100_validate_fw(wlandevice_t * wlandev, const firmware_image_t * apfw_mage, UINT32 offset );
int acx100_verify_init(wlandevice_t * wlandev);
int acx100_read_eeprom_area(wlandevice_t * wlandev);
void acx100_init_mboxes(wlandevice_t * hw);
int acx100_init_wep(wlandevice_t * wlandev, memmap_t * pt);
int acx100_init_packet_templates(wlandevice_t * hw, memmap_t * pt);
int acx100_init_max_beacon_template(wlandevice_t * wlandev);
int acx100_init_max_tim_template(wlandevice_t * hw);
int acx100_init_max_probe_response_template(wlandevice_t * hw);
int acx100_init_max_probe_request_template(wlandevice_t * hw);
int acx100_set_tim_template(wlandevice_t * acx);
int acx100_set_generic_beacon_probe_response_frame(wlandevice_t * hw,
						   acxp80211_hdr_t * txf);
void acx100_update_card_settings(wlandevice_t *wlandev, int init, int get_all, int set_all);
int acx100_ioctl_main(netdevice_t * dev, struct ifreq *ifr, int cmd);
void acx100_set_probe_request_template(wlandevice_t * skb);
void acx100_scan_chan(wlandevice_t *wlandev);
void acx100_scan_chan_p(wlandevice_t *wlandev, struct scan *s);
int acx100_set_rxconfig(wlandevice_t *hw);
int acx100_load_radio(wlandevice_t * hw);
int acx100_read_proc(char *page, char **start, off_t offset, int count,
		     int *eof, void *data);
int acx100_proc_output(char *buf, wlandevice_t * hw);
