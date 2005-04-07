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
#ifndef __ACX_ACX100_HELPER2_H
#define __ACX_ACX100_HELPER2_H

#define MAX_NUMBER_OF_SITE 31

typedef struct ssid {
	u8 element_ID ACX_PACKED;
	u8 length ACX_PACKED;
	u8 inf[32] ACX_PACKED;
} ssid_t;

typedef struct rates {
	u8 element_ID ACX_PACKED;
	u8 length ACX_PACKED;
	u8 sup_rates[8] ACX_PACKED;
} rates_t;

typedef struct fhps {
	u8 element_ID ACX_PACKED;
	u8 length ACX_PACKED;
	u16 dwell_time ACX_PACKED;
	u8 hop_set ACX_PACKED;
	u8 hop_pattern ACX_PACKED;
	u8 hop_index ACX_PACKED;
} fhps_t;

typedef struct dsps {
	u8 element_ID ACX_PACKED;
	u8 length ACX_PACKED;
	u8 curr_channel ACX_PACKED;
} dsps_t;

typedef struct cfps {
	u8 element_ID ACX_PACKED;
	u8 length ACX_PACKED;
	u8 cfp_count ACX_PACKED;
	u8 cfp_period ACX_PACKED;
	u16 cfp_max_dur ACX_PACKED;
	u16 cfp_dur_rem ACX_PACKED;
} cfps_t;

typedef struct challenge_text {
	u8 element_ID ACX_PACKED;
	u8 length ACX_PACKED;
	u8 text[253] ACX_PACKED;
} challenge_text_t;


/* Warning. Several types used in below structs are
** in fact variable length. Use structs with such fields with caution */
typedef struct auth_frame_body {
	u16 auth_alg ACX_PACKED;
	u16 auth_seq ACX_PACKED;
	u16 status ACX_PACKED;
	challenge_text_t challenge ACX_PACKED;
} auth_frame_body_t;

typedef struct assocresp_frame_body {
	u16 cap_info ACX_PACKED;
	u16 status ACX_PACKED;
	u16 aid ACX_PACKED;
	rates_t rates ACX_PACKED;
} assocresp_frame_body_t;

typedef struct reassocreq_frame_body {
	u16 cap_info ACX_PACKED;
	u16 listen_int ACX_PACKED;
	u8 current_ap[6] ACX_PACKED;
	ssid_t ssid ACX_PACKED;
	rates_t rates ACX_PACKED;
} reassocreq_frame_body_t;

typedef struct reassocresp_frame_body {
	u16 cap_info ACX_PACKED;
	u16 status ACX_PACKED;
	u16 aid ACX_PACKED;
	rates_t rates ACX_PACKED;
} reassocresp_frame_body_t;

typedef struct deauthen_frame_body {
	u16 reason ACX_PACKED;
} deauthen_frame_body_t;

typedef struct disassoc_frame_body {
	u16 reason ACX_PACKED;
} disassoc_frame_body_t;

typedef struct probereq_frame_body {
	ssid_t ssid ACX_PACKED;
	rates_t rates ACX_PACKED;
} probereq_frame_body_t;

typedef struct proberesp_frame_body {
	u8 timestamp[8] ACX_PACKED;
	u16 beacon_int ACX_PACKED;
	u16 cap_info ACX_PACKED;
	ssid_t ssid ACX_PACKED;
	rates_t rates ACX_PACKED;
	fhps_t fhps ACX_PACKED;
	dsps_t dsps ACX_PACKED;
	cfps_t cfps ACX_PACKED;
} proberesp_frame_body_t;

void acx_sta_list_init(wlandevice_t *priv);
void acx_set_status(wlandevice_t *priv, u16 status);
int acx_rx_ieee802_11_frame(wlandevice_t *priv, rxhostdesc_t *desc);
u32 acx_transmit_disassoc(client_t *arg_0, wlandevice_t *priv);
void acx_timer(unsigned long a);
void acx_complete_dot11_scan(wlandevice_t *priv);

#define COUNT_STATE_STR	6

static inline const char *acx_get_status_name(u16 status)
{
	extern const char * const g_wlan_state_str[COUNT_STATE_STR];
	return g_wlan_state_str[
		(status < COUNT_STATE_STR) ?
			status : COUNT_STATE_STR-1
		];
}

#endif /* __ACX_ACX100_HELPER2_H */
