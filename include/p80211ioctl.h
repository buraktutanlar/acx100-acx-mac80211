/* include/p80211ioctl.h
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

#ifndef _P80211IOCTL_H
#define _P80211IOCTL_H

#include <p80211msg.h>

/*================================================================*/
/* Constants */

/*----------------------------------------------------------------*/
/* p80211 ioctl "request" codes.  See argument 2 of ioctl(2). */

#define P80211_IFTEST		(SIOCDEVPRIVATE + 0)
#define P80211_IFREQ		(SIOCDEVPRIVATE + 1)

/*----------------------------------------------------------------*/
/* Magic number, a quick test to see we're getting the desired struct */

#define P80211_IOCTL_MAGIC	(0x4a2d464dUL)

/*----------------------------------------------------------------*/
/* Netlink protocol numbers for the indication interface */

#define P80211_NL_SOCK_IND	NETLINK_USERSOCK

/*----------------------------------------------------------------*/
/* Netlink multicast bits for different types of messages */

#define P80211_NL_MCAST_GRP_MLME	BIT0	/* Local station messages */
#define P80211_NL_MCAST_GRP_SNIFF	BIT1	/* Sniffer messages */
#define P80211_NL_MCAST_GRP_DIST	BIT2	/* Distribution system messages */

/*================================================================*/
/* Macros */


/*================================================================*/
/* Types */

/*----------------------------------------------------------------*/
/* A ptr to the following structure type is passed as the third */
/*  argument to the ioctl system call when issuing a request to */
/*  the p80211 module. */

typedef struct p80211ioctl_req {
	char name[WLAN_DEVNAMELEN_MAX] __WLAN_ATTRIB_PACK__;
	void *data __WLAN_ATTRIB_PACK__;
	UINT32 magic __WLAN_ATTRIB_PACK__;
	UINT16 len __WLAN_ATTRIB_PACK__;
	UINT32 result __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ p80211ioctl_req_t;


/*================================================================*/
/* Extern Declarations */


/*================================================================*/
/* Function Declarations */


#endif				/* _P80211IOCTL_H */
