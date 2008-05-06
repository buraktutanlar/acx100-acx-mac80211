#ifndef _ACX_DEBUG_H_
#define _ACX_DEBUG_H_

/*
 * acx_log.h: logging constants and helpers
 *
 * Copyright (c) 2008, the ACX100 project (http://acx100.sourceforge.net).
 *
 * See the README file for licensing.
*/

#include "acx.h"

/***********************************************************************
** Debug / log functionality
*/
enum {
	L_LOCK		= (ACX_DEBUG>1)*0x0001,	/* locking debug log */
	L_INIT		= (ACX_DEBUG>0)*0x0002,	/* special card initialization logging */
	L_IRQ		= (ACX_DEBUG>0)*0x0004,	/* interrupt stuff */
	L_ASSOC		= (ACX_DEBUG>0)*0x0008,	/* assocation (network join) and station log */
	L_FUNC		= (ACX_DEBUG>1)*0x0020,	/* logging of function enter / leave */
	L_XFER		= (ACX_DEBUG>1)*0x0080,	/* logging of transfers and mgmt */
	L_DATA		= (ACX_DEBUG>1)*0x0100,	/* logging of transfer data */
	L_DEBUG		= (ACX_DEBUG>1)*0x0200,	/* log of debug info */
	L_IOCTL		= (ACX_DEBUG>0)*0x0400,	/* log ioctl calls */
	L_CTL		= (ACX_DEBUG>1)*0x0800,	/* log of low-level ctl commands */
	L_BUFR		= (ACX_DEBUG>1)*0x1000,	/* debug rx buffer mgmt (ring buffer etc.) */
	L_XFER_BEACON	= (ACX_DEBUG>1)*0x2000,	/* also log beacon packets */
	L_BUFT		= (ACX_DEBUG>1)*0x4000,	/* debug tx buffer mgmt (ring buffer etc.) */
	L_USBRXTX	= (ACX_DEBUG>0)*0x8000,	/* debug USB rx/tx operations */
	L_BUF		= L_BUFR + L_BUFT,
	L_ANY		= 0xffff
};

#if ACX_DEBUG
extern unsigned int acx_debug;
#else
enum { acx_debug = 0 };
#endif

/***********************************************************************
** LOGGING
**
** - Avoid SHOUTING needlessly. Avoid excessive verbosity.
**   Gradually remove messages which are old debugging aids.
**
** - Use printk() for messages which are to be always logged.
**   Supply either 'acx:' or '<devname>:' prefix so that user
**   can figure out who's speaking among other kernel chatter.
**   acx: is for general issues (e.g. "acx: no firmware image!")
**   while <devname>: is related to a particular device
**   (think about multi-card setup). Double check that message
**   is not confusing to the average user.
**
** - use printk KERN_xxx level only if message is not a WARNING
**   but is INFO, ERR etc.
**
** - Use printk_ratelimited() for messages which may flood
**   (e.g. "rx DUP pkt!").
**
** - Use log() for messages which may be omitted (and they
**   _will_ be omitted in non-debug builds). Note that
**   message levels may be disabled at compile-time selectively,
**   thus select them wisely. Example: L_DEBUG is the lowest
**   (most likely to be compiled out) -> use for less important stuff.
**
** - Do not print important stuff with log(), or else people
**   will never build non-debug driver.
**
** Style:
** hex: capital letters, zero filled (e.g. 0x02AC)
** str: dont start from capitals, no trailing periods ("tx: queue is stopped")
*/
#if ACX_DEBUG > 1

void log_fn_enter(const char *funcname);
void log_fn_exit(const char *funcname);
void log_fn_exit_v(const char *funcname, int v);

#define FN_ENTER \
	do { \
		if (unlikely(acx_debug & L_FUNC)) { \
			log_fn_enter(__func__); \
		} \
	} while (0)

#define FN_EXIT1(v) \
	do { \
		if (unlikely(acx_debug & L_FUNC)) { \
			log_fn_exit_v(__func__, v); \
		} \
	} while (0)
#define FN_EXIT0 \
	do { \
		if (unlikely(acx_debug & L_FUNC)) { \
			log_fn_exit(__func__); \
		} \
	} while (0)

#else

#define FN_ENTER do {} while(0)
#define FN_EXIT1(v) do {} while(0)
#define FN_EXIT0 do {} while(0)

#endif /* ACX_DEBUG > 1 */


#if ACX_DEBUG

#define log(chan, args...) \
	do { \
		if (acx_debug & (chan)) \
			printk(args); \
	} while (0)
#define printk_ratelimited(args...) printk(args)

#else /* Non-debug build: */

#define log(chan, args...)
/* Standard way of log flood prevention */
#define printk_ratelimited(args...) \
do { \
	if (printk_ratelimit()) \
		printk(args); \
} while (0)

#endif /* ACX_DEBUG */ 

#endif /* _ACX_DEBUG_H_ */