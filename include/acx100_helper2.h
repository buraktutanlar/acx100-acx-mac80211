/* include/acx100_helper2.h
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


typedef struct acx100_frame {
	char val0x0[0x26];
	UINT16 val0x26;
	acx100_addr3_t fr;
} acx100_frame_t;

typedef struct alloc_p80211mgmt_req_t {
	union {
		wlan_fr_mgmt_t mgmt;
		wlan_fr_assocreq_t assocreq;
		wlan_fr_reassocreq_t reassocreq;
		wlan_fr_assocresp_t assocresp;
		wlan_fr_reassocresp_t reassocresp;
		wlan_fr_beacon_t beacon;
		wlan_fr_disassoc_t disassoc;
		wlan_fr_authen_t authen;
		wlan_fr_deauthen_t deauthen;
		wlan_fr_proberesp_t proberesp;
	} a;
} alloc_p80211_mgmt_req_t;

typedef struct client {
	UINT16 aid;		/* association ID */
	char address[WLAN_ADDR_LEN];	/* 0x2 */
	UINT8 val0x8;
	UINT8 used;		/* 0x9 */
	UINT16 val0xa;
	UINT16 auth_alg;
	UINT16 val0xe;
	UINT8 *val0x10;		// points to some data, don't know what yet
	UINT32 unkn0x14;
	UINT8 val0x18[0x80];	/* 0x18, used by acx100_process_authen() */
	UINT16 val0x98;
	UINT16 val0x9a;
	UINT8 pad5[8];		/* 0x9c */
	struct client *next;	/* 0xa4 */
} client_t;

typedef struct rxhostdescriptor rxhostdescriptor_t;
typedef struct rxbuffer rxbuffer_t;

#define MAX_NUMBER_OF_SITE 31

typedef struct ssid {
	UINT8 element_ID;
	UINT8 length;
	UINT8 inf[32];
} ssid_t;

typedef struct rates {
	UINT8 element_ID;
	UINT8 length;
	UINT8 sup_rates[8];
} rates_t;

typedef struct fhps {
	UINT8 element_ID;
	UINT8 length;
	UINT16 dwell_time;
	UINT8 hop_set;
	UINT8 hop_pattern;
	UINT8 hop_index;
} fhps_t;

typedef struct dsps {
	UINT8 element_ID;
	UINT8 length;
	UINT8 curr_channel;
} dsps_t;

typedef struct cfps {
	UINT8 element_ID;
	UINT8 length;
	UINT8 cfp_count;
	UINT8 cfp_period;
	UINT16 cfp_max_dur;
	UINT16 cfp_dur_rem;
} cfps_t;

typedef struct challenge_text {
	UINT8 element_ID;
	UINT8 length;
	UINT8 text[253];
} challenge_text_t;



typedef struct auth_frame_body {
	UINT16 auth_alg;
	UINT16 auth_seq;
	UINT16 status;
	challenge_text_t challenge;
} auth_frame_body_t;

typedef struct assocresp_frame_body {
	UINT16 cap_info;
	UINT16 status;
	UINT16 aid;
	rates_t rates;
} assocresp_frame_body_t;

typedef struct reassocreq_frame_body {
	UINT16 cap_info;
	UINT16 listen_int;
	UINT8 current_ap[6];
	ssid_t ssid;
	rates_t rates;
} reassocreq_frame_body_t;

typedef struct reassocresp_frame_body {
	UINT16 cap_info;
	UINT16 status;
	UINT16 aid;
	rates_t rates;
} reassocresp_frame_body_t;

typedef struct deauthen_frame_body {
	UINT16 reason;
} deauthen_frame_body_t;

typedef struct disassoc_frame_body {
	UINT16 reason;
} disassoc_frame_body_t;

typedef struct probereq_frame_body {
	ssid_t ssid;
	rates_t rates;
} probereq_frame_body_t;

typedef struct proberesp_frame_body {
	UINT8 timestamp[8];
	UINT16 beacon_int;
	UINT16 cap_info;
	ssid_t ssid;
	rates_t rates;
	fhps_t fhps;
	dsps_t dsps;
	cfps_t cfps;
} proberesp_frame_body_t;

typedef struct TxData {
	UINT16 frame_control;	/* 0x0 */
	UINT16 duration_id;	/* 0x2 */
	UINT8 da[WLAN_ADDR_LEN];	/* 0x4 */
	UINT8 sa[WLAN_ADDR_LEN];	/* 0xa */
	UINT8 bssid[WLAN_BSSID_LEN];	/* 0x10 */
	UINT16 sequence_control;	/* 0x16 */
	union {
		auth_frame_body_t auth;
		deauthen_frame_body_t deauthen;
		/* assocreq_frame_body_t does not exist, since it
		 * contains variable-length members, thus it's no static
		 * struct */
		assocresp_frame_body_t assocresp;
		reassocresp_frame_body_t reassocreq;
		reassocresp_frame_body_t reassocresp;
		disassoc_frame_body_t disassoc;
		probereq_frame_body_t probereq;
		proberesp_frame_body_t proberesp;
		char * raw[2400-24];
	} body;
} TxData;			/* size: 2400 */

void acx100_client_sta_list_init(void);
int acx100_rx_ieee802_11_frame(wlandevice_t *wlandev, rxhostdescriptor_t *desc);
int acx100_process_disassoc(wlan_fr_disassoc_t * req, wlandevice_t * hw);
int acx100_process_assocresp(wlan_fr_assocresp_t * req, wlandevice_t * hw);
int acx100_process_reassocresp(wlan_fr_reassocresp_t * req, wlandevice_t * hw);
int acx100_process_disassociate(wlan_fr_disassoc_t * req, wlandevice_t * hw);
int acx100_process_deauthenticate(wlan_fr_deauthen_t * req, wlandevice_t * hw);
int acx100_process_authen(wlan_fr_authen_t * req, wlandevice_t * hw);

int acx100_process_deauthen(wlan_fr_deauthen_t * req, wlandevice_t * hw);
void acx100_process_probe_response(struct rxbuffer *mmt, wlandevice_t * hw,
			  acxp80211_hdr_t * hdr);
void acx100_timer(unsigned long a);
void acx100_complete_dot11_scan(wlandevice_t * wlandev);
char *acx100_get_status_name(int status);
void acx100_set_status(wlandevice_t *hw, int status);
void acx100_ibssid_gen(wlandevice_t * wlandev, unsigned char *p_out);

extern void acx100_rx(struct rxhostdescriptor *rxdesc, wlandevice_t *wlandev);

void acx100_gen_challenge(challenge_text_t *);
void acx100_get_random(UINT8 *, UINT32);

int acx100_transmit_authen4(wlan_fr_authen_t * arg_0, wlandevice_t * wlandev);
int acx100_transmit_authen3(wlan_fr_authen_t * arg_0, wlandevice_t * wlandev);
int acx100_transmit_authen2(wlan_fr_authen_t * arg_0, client_t * sta_list,
		      wlandevice_t * wlandev);
int acx100_transmit_authen1(wlandevice_t * wlandev);
int acx100_transmit_assoc_req(wlandevice_t * wlandev);
UINT32 acx100_transmit_disassoc(client_t * arg_0, wlandevice_t * wlandev);
int acx100_transmit_deauthen(char *a, client_t * arg_4, wlandevice_t * hw,
		      int valb);
UINT32 acx100_transmit_assocresp(wlan_fr_assocreq_t * arg_0,
			  wlandevice_t * wlandev);
UINT32 acx100_transmit_reassocresp(wlan_fr_reassocreq_t * arg_0,
			    wlandevice_t * wlandev);

client_t *acx100_sta_list_alloc(UINT8 * address);
client_t *acx100_sta_list_add(UINT8 * address);
client_t *acx100_get_sta_list(char *address);
