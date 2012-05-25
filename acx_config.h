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
#ifndef _ACX_CONFIG_H_
#define _ACX_CONFIG_H_

#ifndef ACX_GIT_VERSION
#define ACX_RELEASE "v0.6.0"
#else
#define ACX_RELEASE "v0.6.0-g" ACX_GIT_VERSION
#endif

/* set to 0 if you don't want any debugging code to be compiled in */
/* set to 1 if you want some debugging */
/* set to 2 if you want extensive debug log */

#define ACX_DEBUG 2

//	L_LOCK|		/* locking debug log */
//	L_INIT|		/* special card initialization logging */
//	L_IRQ|		/* interrupt stuff */
//	L_ASSOC|	/* assocation (network join) and station log */
//	L_FUNC|		/* logging of function enter / leave */
//	L_XFER|		/* logging of transfers and mgmt */
//	L_DATA|		/* logging of transfer data */
//	L_DEBUG|	/* log of debug info */
//	L_IOCTL|	/* log ioctl calls */
//	L_CTL|		/* log of low-level ctl commands */
//	L_BUFR|		/* debug rx buffer mgmt (ring buffer etc.) */
//	L_XFER_BEACON|		/* also log beacon packets */
//	L_BUFT|			/* debug tx buffer mgmt (ring buffer etc.) */
//	L_USBRXTX|		/* debug USB rx/tx operations */
//	L_BUF|		    /* L_BUFR + L_BUFT */
//	L_ANY
#define ACX_DEFAULT_MSG (L_ASSOC|L_INIT)

/* assume 32bit I/O width (16bit is also compatible with Compact Flash) */
#define ACX_IO_WIDTH 32

#if (ACX_IO_WIDTH == 32)
# define IO_COMPILE_NOTE \
	"compiled to use 32bit I/O access. "		\
	"I/O timing issues might occur, such as "	\
	"non-working firmware upload. Report them\n"
#else
# define IO_COMPILE_NOTE \
	"compiled to use 16bit I/O access only (compatibility mode)\n"
#endif

/* Set this to 1 if you want monitor mode to use
 * phy header. Currently it is not useful anyway since we
 * don't know what useful info (if any) is in phy header.
 * If you want faster/smaller code, say 0 here */
#define WANT_PHY_HDR 0

/* whether to do Tx descriptor cleanup in softirq (i.e. not in IRQ
 * handler) or not. Note that doing it later does slightly increase
 * system load, so still do that stuff in the IRQ handler for now,
 * even if that probably means worse latency */
#define TX_CLEANUP_IN_SOFTIRQ 0

/* if you want very experimental 802.11 power save mode features */
#define POWER_SAVE_80211 0

/* if you want very early packet fragmentation bits and pieces */
#define ACX_FRAGMENTATION 0

#ifdef OW_20100613_OBSELETE_ACXLOCK_REMOVE
/* Locking: */
/* very talkative */
#define PARANOID_LOCKING 1
/* normal (use when bug-free) */
/* #define DO_LOCKING 1 */
/* else locking is disabled! */
#endif

/* 0 - normal mode */
/* 1 - development/debug: probe for IEs on modprobe */
#define CMD_DISCOVERY 0

#ifdef __LITTLE_ENDIAN
#define ENDIAN_STR "acx: running on a little-endian CPU\n"
#else
#define ENDIAN_STR "acx: running on a BIG-ENDIAN CPU\n"
#endif

#endif /* _ACX_CONFIG_H_ */
