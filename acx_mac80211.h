/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008
 * The ACX100 Open Source Project <acx100-devel@lists.sourceforge.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _ACX_MAC80211_H_
#define _ACX_MAC80211_H_

#include <net/mac80211.h>
#include <linux/nl80211.h>

struct acx_key {
	u8 enabled:1;
	u8 algorithm;
};

struct acx_interface {
        /* Opaque ID of the operating interface (!= monitor
         * interface) from the ieee80211 subsystem.
         * Do not modify.
         */
        int if_id;
        /* MAC address. */
        u8 *mac_addr;
        /* Current BSSID (if any). */
        const u8 *bssid;

        /* Interface type. (IEEE80211_IF_TYPE_XXX) */
        int type;
        /* Counter of active monitor interfaces. */
        int monitor;
        /* Is the card operating in AP, STA or IBSS mode? */
        unsigned int operating:1;
        /* Promisc mode active?
         * Note that (monitor != 0) implies promisc.
         */
        unsigned int promisc:1;
};
#ifdef TODO
# undef TODO
#endif
#define TODO()  \
        do {                                                                            \
                printk(KERN_INFO "TODO: Incomplete code in %s() at %s:%d\n",        \
                       __FUNCTION__, __FILE__, __LINE__);                               \
        } while (0)

#ifdef FIXME
# undef FIXME
#endif
#define FIXME()  \
        do {                                                                            \
                printk(KERN_INFO "FIXME: Possibly broken code in %s() at %s:%d\n",  \
                       __FUNCTION__, __FILE__, __LINE__);                               \
        } while (0)

#define ACX_MODE_NOTADHOC 0xFFFF
#define ACX_MODE_PROMISC 0x5

/** Rate values **/

#define ACX_CCK_RATE_1MB            0
#define ACX_CCK_RATE_2MB            1
#define ACX_CCK_RATE_5MB            2
#define ACX_CCK_RATE_11MB           3
#define ACX_OFDM_RATE_6MB           4
#define ACX_OFDM_RATE_9MB           5
#define ACX_OFDM_RATE_12MB          6
#define ACX_OFDM_RATE_18MB          7
#define ACX_OFDM_RATE_24MB          8
#define ACX_OFDM_RATE_36MB          9
#define ACX_OFDM_RATE_48MB          10
#define ACX_OFDM_RATE_54MB          11

#endif /* _ACX_MAC80211_H_ */
