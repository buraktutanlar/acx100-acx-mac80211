/* include/acx100mgmt.h
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

#ifndef _acx100MGMT_H
#define _acx100MGMT_H

/*=============================================================*/
/*------ Constants --------------------------------------------*/

#define	MAX_GRP_ADDR		(32)
#define	MAX_acx100_GRP_ADDR	(16)

#define MM_SAT_PCF		(BIT14)
#define MM_GCSD_PCF		(BIT15)
#define MM_GCSD_PCF_EB		(BIT14 | BIT15)

#define WLAN_STATE_STOPPED	0	/* Network is not active. */
#define WLAN_STATE_STARTED	1	/* Network has been started. */

#define WLAN_AUTH_MAX           60	/* Max. # of authenticated stations. */
#define WLAN_ACCESS_MAX		60	/* Max. # of stations in an access list. */
#define WLAN_ACCESS_NONE	0	/* No stations may be authenticated. */
#define WLAN_ACCESS_ALL		1	/* All stations may be authenticated. */
#define WLAN_ACCESS_ALLOW	2	/* Authenticate only "allowed" stations. */
#define WLAN_ACCESS_DENY	3	/* Do not authenticate "denied" stations. */

#define WLAN_COMMENT_MAX	80	/* Max. length of user comment string. */

/*=============================================================*/
/*------ Macros -----------------------------------------------*/


/*=============================================================*/
/*------ Types and their related constants --------------------*/

typedef struct acx100_authlist {
	UINT cnt;
	UINT8 addr[WLAN_AUTH_MAX][WLAN_ADDR_LEN];
	UINT8 assoc[WLAN_AUTH_MAX];
} acx100_authlist_t;

typedef struct acx100_accesslist {
	UINT modify;
	UINT cnt;
	UINT8 addr[WLAN_ACCESS_MAX][WLAN_ADDR_LEN];
	UINT cnt1;
	UINT8 addr1[WLAN_ACCESS_MAX][WLAN_ADDR_LEN];
} acx100_accesslist_t;

typedef struct acx100_priv {
#if (WLAN_HOSTIF == WLAN_PCMCIA)
	dev_node_t node;
	dev_link_t *cs_link;
#elif (WLAN_HOSTIF==WLAN_PLX || WLAN_HOSTIF==WLAN_PCI || WLAN_HOSTIF==WLAN_USB)
	char name[IFNAMSIZ];
#endif

	/* Timer to allow for the deferred processing of linkstatus messages */
	struct work_struct link_tq;
	UINT16 link_status;

	/* Structure for MAC data */
	/* acx100_t *hw; */

	/* Component Identities */
	acx100_compident_t ident_nic;
	acx100_compident_t ident_pri_fw;
	acx100_compident_t ident_sta_fw;
	acx100_compident_t ident_ap_fw;
	UINT16 mm_mods;

	/* Supplier compatibility ranges */
	acx100_caplevel_t cap_sup_mfi;
	acx100_caplevel_t cap_sup_cfi;
	acx100_caplevel_t cap_sup_pri;
	acx100_caplevel_t cap_sup_sta;
	acx100_caplevel_t cap_sup_ap;

	/* Actor compatibility ranges */
	acx100_caplevel_t cap_act_pri_cfi;	/* pri f/w to controller interface */
	acx100_caplevel_t cap_act_sta_cfi;	/* sta f/w to controller interface */
	acx100_caplevel_t cap_act_sta_mfi;	/* sta f/w to modem interface */
	acx100_caplevel_t cap_act_ap_cfi;	/* ap f/w to controller interface */
	acx100_caplevel_t cap_act_ap_mfi;	/* ap f/w to modem interface */

	/* PDA */
	UINT8 pda[ACX100_PDA_LEN_MAX];
	acx100_pdrec_t *pdrec[ACX100_PDA_RECS_MAX];
	UINT npdrec;

	/* The following are dot11StationConfigurationTable mibitems
	   maintained by the driver; there are no acx100 RID's */
	UINT32 dot11_desired_bss_type;
	UINT32 dot11_disassoc_reason;
	UINT8 dot11_disassoc_station[WLAN_ADDR_LEN];
	UINT32 dot11_deauth_reason;
	UINT8 dot11_deauth_station[WLAN_ADDR_LEN];
	UINT32 dot11_auth_fail_status;
	UINT8 dot11_auth_fail_station[WLAN_ADDR_LEN];

	/* Group Addresses - right now, there are up to a total
	   of MAX_GRP_ADDR group addresses */
	UINT8 dot11_grp_addr[MAX_GRP_ADDR][WLAN_ADDR_LEN];
	UINT dot11_grpcnt;

	/* Channel Info request results (AP only) */
	struct {
		atomic_t done;
		UINT8 count;
		acx100_ChInfoResult_t results;
	} channel_info;

	/* State variables */
	UINT presniff_port_type;
	UINT16 presniff_wepflags;

	int ap;			/* AP flag: 0 - Station, 1 - Access Point. */
	int state;		/* Network state: 0 - Stopped, 1 - Started. */
	int log;		/* Log flag: 0 - No, 1 - Log events. */

	acx100_authlist_t authlist;	/* Authenticated station list. */
	UINT accessmode;	/* Access mode. */
	acx100_accesslist_t allow;	/* Allowed station list. */
	acx100_accesslist_t deny;	/* Denied station list. */
	UINT32 psusercount;	/* Power save user count. */
	acx100_CommTallies32_t tallies;	/* Communication tallies. */
	UINT8 comment[WLAN_COMMENT_MAX + 1];	/* User comment */

	acx100_InfFrame_t *scanresults;
} acx100_priv_t;


/*=============================================================*/
/*------ Static variable externs ------------------------------*/

#if (WLAN_HOSTIF != WLAN_USB)
extern int acx100_bap_timeout;
#endif
extern int acx100_debug;
extern int acx100_irq_evread_max;

#endif
