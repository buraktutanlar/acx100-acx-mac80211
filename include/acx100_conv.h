/* include/acx100_conv.h
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
 * This code is based on elements which are
 * Copyright (C) 1999 AbsoluteValue Systems, Inc.  All Rights Reserved.
 * info@linux-wlan.com
 * http://www.linux-wlan.com
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

#ifndef _ACX100_CONV_H
#define _ACX100_CONV_H

/*============================================================================*
 * Constants                                                                  *
 *============================================================================*/

#define WLAN_ETHADDR_LEN	6
#define WLAN_IEEE_OUI_LEN	3

#define WLAN_ETHCONV_ENCAP	1
#define WLAN_ETHCONV_RFC1042	2
#define WLAN_ETHCONV_8021h	3

#define WLAN_MIN_ETHFRM_LEN	60
#define WLAN_MAX_ETHFRM_LEN	1514
#define WLAN_ETHHDR_LEN		14

/*============================================================================*
 * Types                                                                      *
 *============================================================================*/

/* local ether header type */
typedef struct wlan_ethhdr {
	UINT8 daddr[WLAN_ETHADDR_LEN] __WLAN_ATTRIB_PACK__;
	UINT8 saddr[WLAN_ETHADDR_LEN] __WLAN_ATTRIB_PACK__;
	UINT16 type __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ wlan_ethhdr_t;

/* local llc header type */
typedef struct wlan_llc {
	UINT8 dsap __WLAN_ATTRIB_PACK__;
	UINT8 ssap __WLAN_ATTRIB_PACK__;
	UINT8 ctl __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ wlan_llc_t;

/* local snap header type */
typedef struct wlan_snap {
	UINT8 oui[WLAN_IEEE_OUI_LEN] __WLAN_ATTRIB_PACK__;
	UINT16 type __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ wlan_snap_t;

/* FIXME: Circular include trick */
struct wlandevice;
struct txdescriptor;
struct rxhostdescriptor;

/*============================================================================*
 * Function Declarations                                                      *
 *============================================================================*/

int acx100_ether_to_txdesc(struct wlandevice *priv,
			   struct txdescriptor *txdesc, struct sk_buff *skb);
/*@null@*/ struct sk_buff *acx100_rxdesc_to_ether(struct wlandevice *priv,
				       struct rxhostdescriptor *rxdesc);
void acx100_rxdesc_to_txdesc(struct rxhostdescriptor *rxdesc,
			     struct txdescriptor *txdesc);

#endif /* _ACX100_CONV_H */
