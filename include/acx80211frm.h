/* include/acx80211frm.h
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

void acx_mgmt_decode_ibssatim(wlan_fr_ibssatim_t *f);
void acx_mgmt_encode_ibssatim(wlan_fr_ibssatim_t *f);
void acx_mgmt_decode_assocreq(wlan_fr_assocreq_t *f);
void acx_mgmt_decode_assocresp(wlan_fr_assocresp_t *f);
void acx_mgmt_decode_authen(wlan_fr_authen_t *f);
void acx_mgmt_decode_beacon(wlan_fr_beacon_t *f);
void acx_mgmt_decode_deauthen(wlan_fr_deauthen_t *f);
void acx_mgmt_decode_disassoc(wlan_fr_disassoc_t *f);
void acx_mgmt_decode_probereq(wlan_fr_probereq_t *f);
void acx_mgmt_decode_proberesp(wlan_fr_proberesp_t *f);
void acx_mgmt_decode_reassocreq(wlan_fr_reassocreq_t *f);
void acx_mgmt_decode_reassocresp(wlan_fr_reassocresp_t *f);
void acx_mgmt_encode_assocreq(wlan_fr_assocreq_t *f);
void acx_mgmt_encode_assocresp(wlan_fr_assocresp_t *f);
void acx_mgmt_encode_authen(wlan_fr_authen_t *f);
void acx_mgmt_encode_beacon(wlan_fr_beacon_t *f);
void acx_mgmt_encode_deauthen(wlan_fr_deauthen_t *f);
void acx_mgmt_encode_disassoc(wlan_fr_disassoc_t *f);
void acx_mgmt_encode_probereq(wlan_fr_probereq_t *f);
void acx_mgmt_encode_proberesp(wlan_fr_proberesp_t *f);
void acx_mgmt_encode_reassocreq(wlan_fr_reassocreq_t *f);
void acx_mgmt_encode_reassocresp(wlan_fr_reassocresp_t *f);
