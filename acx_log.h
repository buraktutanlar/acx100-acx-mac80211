#ifndef _ACX_LOG_H_
#define _ACX_LOG_H_

/*
 * acx_log.h: logging constants and functions.
 *
 * Copyright (c) 2008, Francis Galiegue <fgaliegue@gmail.com> for the ACX100
 * driver project.
 *
 * This file is licensed under GPL version 2.
 */

/*
 * For KERN_*, and printk()
 */
#include <linux/kernel.h>

/*
 * The acx_debug.h file defines the log level and default log mask.
 */
#include "acx_debug.h"

#define ACX_LOG_LEVEL ACX_DEBUG

/*
 * TODO() and FIXME() macros
 * these macros that inform the user (via printk) if we have possibly
 * broken or incomplete code
 */

#ifdef TODO
# undef TODO
#endif
#define TODO() \
	do { \
		printk(KERN_INFO "TODO: Incomplete code in %s() at %s:%d\n", \
			__FUNCTION__, __FILE__, __LINE__); \
	} while (0)

#ifdef FIXME
# undef FIXME
#endif
#define FIXME() \
	do { \
		printk(KERN_INFO "FIXME: Possibly broken code in %s() at %s:%d\n", \
			__FUNCTION__, __FILE__, __LINE__); \
	} while (0)

/*
 * Helpers to log MAC addresses
 */

#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC(bytevector) \
	((unsigned char *)bytevector)[0], \
	((unsigned char *)bytevector)[1], \
	((unsigned char *)bytevector)[2], \
	((unsigned char *)bytevector)[3], \
	((unsigned char *)bytevector)[4], \
	((unsigned char *)bytevector)[5]

/*
 * What to log.
 */
#define L_LOCK		0x0001	/* Locking */
#define L_INIT		0x0002	/* Card initialization */
#define L_IRQ		0x0004	/* Interrupt handling */
#define L_ASSOC		0x0008	/* Assocation (network join) and station log */
#define L_FUNC		0x0010	/* Function enter/leave */
#define L_XFER		0x0020	/* TX management */
#define L_DATA		0x0040	/* Data transfer */
#define L_IOCTL		0x0080	/* Log ioctl calls */
#define L_CTL		0x0100	/* Log of low-level ctl commands */
#define L_BUFR		0x0200	/* Debug rx buffer mgmt (ring buffer etc.) */
#define L_XFER_BEACON	0x0400	/* Also log beacon packets */
#define L_BUFT		0x0800	/* Debug tx buffer mgmt (ring buffer etc.) */
#define L_USBRXTX	0x1000	/* Debug USB rx/tx operations */
#define L_BUF	 	(L_BUFR|L_BUFT)	
#define L_REALLYVERBOSE	0x2000	/* Flood me, baby! */
#define L_ANY		0xffff

#define ACX_DEFAULT_MSG (L_INIT|L_ASSOC|L_FUNC|L_IRQ)
/*
 * Log levels.
 */
#define LOG_WARNING	0
#define LOG_INFO	1
#define LOG_DEBUG	2

#define MAX_LOG_LEVEL	2

/*
 * Function declarations.
 *
 * The acx_log_dump() function also dumps a buffer taken as an argument.
 */

void acx_log(int level, int what, const char *fmt, ...);
void acx_log_dump(int level, int what, const void *buf, ssize_t buflen,
	const char *fmt, ...);

/*
 * This one needs to be a macro! We don't want nested va_start()/va_end()
 * calls...
 */

#define acx_log_ratelimited(level, what, fmt...) do { \
	if (!printk_ratelimit()) \
		acx_log(level, what, fmt); \
} while (0)

#if ACX_LOG_LEVEL == 2

#define __FUNCTION_ENTER	0
#define __FUNCTION_EXIT		1
#define __FUNCTION_EXIT_WITHARG 2

void __function_enter_exit(const char *, int, int);

#define FN_ENTER do { \
	__function_enter_exit(__func__, __FUNCTION_ENTER, 0); \
} while (0)

#define FN_EXIT0 do { \
	__function_enter_exit(__func__, __FUNCTION_EXIT, 0); \
} while (0)

#define FN_EXIT1(retcode) do { \
	__function_enter_exit(__func__, __FUNCTION_EXIT_WITHARG, retcode); \
} while (0)

#else

#define FN_ENTER do {} while(0)
#define FN_EXIT do {} while(0)
#define FN_EXIT1(retcode) do {} while(0)

#endif /* ACX_LOG_LEVEL == 2 */

#endif /* _ACX_LOG_H_ */
