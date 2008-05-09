/*
 * log.c: logging framework.
 *
 * This file should disappear some day, when the driver is known to work
 * reliably. Until then, this will contain all logging routines used everywhere
 * in the code.
 *
 * Copyright (c) 2008, Francis Galiegue <fgaliegue@gmail.com> for the ACX100
 * driver project.
 *
 * This file is licensed under the GPL version 2.
 */
#include <linux/jiffies.h>
#include <linux/module.h> /* Needed for MODULE_* */

#include "acx_config.h"
#include "acx_log.h"

/*
 * Forward declarations
 */

static void acx_dump_bytes(const char *prefix, const void *data,
	ssize_t len);

static const char *const printk_levels[MAX_LOG_LEVEL + 1] = {
	KERN_WARNING,
	KERN_INFO,
	KERN_DEBUG
};

/*
 * "debug" module parameter, only if ACX_DEBUG is set.
 *
 * If not set, statically define it to ACX_DEFAULT_MSG.
 */
#if ACX_DEBUG
unsigned int acx_debug = ACX_DEFAULT_MSG;
/* parameter is 'debug', corresponding var is acx_debug */
module_param_named(debug, acx_debug, uint, 0);
MODULE_PARM_DESC(debug, "Debug level mask (see L_xxx constants)");
#else
static unsigned int acx_debug = ACX_DEFAULT_MSG;
#endif


/**
 * acx_log: the logging function
 * @level: what level to log (LOG_WARNING, LOG_INFO or LOG_DEBUG).
 * @what: what channel to log (any of the L_* values defined in acx_log.h).
 * @fmt: the format string, and its arguments if any.
 *
 */

void acx_log(int level, int what, const char *fmt, ...)
{
	va_list args;
	const char *printk_level;

	if (level > ACX_LOG_LEVEL)
		return;
	if (!(what & acx_debug))
		return;
	
	/*
	 * FIXME: this shouldn't be necessary, but I don't rely on luck
	 */
	if (level > MAX_LOG_LEVEL)
		level = MAX_LOG_LEVEL;
	
	printk_level = printk_levels[level];
	va_start(args, fmt);

	printk("%sacx: ", printk_level);
	vprintk(fmt, args);
	va_end(args);

	return;
}

/**
 * acx_log_dump(): logs a message, and dumps a buffer. Basically, this is
 * acx_log(), and a call to acx_dump_bytes() below.
 * @level: see acx_log().
 * @what: see acx_log().
 * @buf: the buffer to dump.
 * @buflen: the length of the buffer to dump.
 */

void acx_log_dump(int level, int what, const void *buf, ssize_t buflen,
	const char *fmt, ...)
{
	va_list args;
	const char *printk_level;

	if (level > ACX_LOG_LEVEL)
		return;
	if (!(what & acx_debug))
		return;
	
	/*
	 * FIXME: this shouldn't be necessary, but I don't rely on luck
	 */
	if (level > MAX_LOG_LEVEL)
		level = MAX_LOG_LEVEL;
	
	printk_level = printk_levels[level];
	va_start(args, fmt);
	
	acx_log(level, what, fmt, args);
	acx_dump_bytes(printk_level, buf, buflen);
}
/**
 * acx_log_ratelimited: like acx_log(), but rate limited via printk_ratelimit().
 */
void acx_log_ratelimited(int level, int what, const char *fmt, ...)
{
	va_list args;
	
	if (printk_ratelimit())
		return;

	va_start(args, fmt);
	acx_log(level, what, fmt, args);
	va_end(args);
}

/**
 * acx_dump_bytes: hex dump of a buffer
 * @printk_prefix: the KERN_* char constant, passed to this function by
 * acx_log().
 * @buf: the buffer to dump.
 * @buflen: the length of the buffer.
 *
 * This function is static: it's not supposed to be called from anywhere else
 * than this file. There is no "acx:" prefix here.
 */
static void acx_dump_bytes(const char *printk_prefix, const void *data,
	ssize_t len)
{
        const u8 *ptr = (const u8 *)data;
        unsigned int size = 0;
	/*
	 * buf holds:
	 * 	- the printk prefix (3 bytes);
	 *	- the size printed as "0x%08X" (10 bytes);
	 *	- the following semicolon (1 bytes);
	 *	- 16 bytes printed as " %02X" (48 bytes);
	 *	- the final '\0' (1 byte).
	 */
	char buf[63], *p;
 
        printk("%s--- BEGIN DUMP (%d bytes) ---\n", printk_prefix,
		(int) len);
 
        if (len <= 0)
                return;
 
	goto inside;

        do {
		p += sprintf(p, " %02X", *ptr);
		size++, ptr++;
		if (size % 16)
			continue;
		printk("%s\n", buf);
inside:
		p = buf;
		p += sprintf(p, "%s0x%08X:", printk_prefix, size);
	} while (size < len);

	if (size % 16)
		printk("%s\n", buf);

        printk("%s--- END DUMP ---\n", printk_prefix);
}
/*
 * Only in case of heavy debugging
 */

#if ACX_LOG_LEVEL == 2

/**
 * __function_enter_exit: display entering/exiting of a function
 * @fname: the function name.
 * @enter_exit: 0 on enter, 1 on exit, 2 on exit with return value to be
 * printed.
 * @retcode: the return code to be printed if enter_exit is 2.
 */

#define DEBUG_TSC	0

#if DEBUG_TSC
#define TIMESTAMP(d) unsigned long d; rdtscl(d)
#else
#define TIMESTAMP(d) unsigned long d = jiffies
#endif

/*
 * MAX_INDENT is the size of the spaces[] string below.
 */
#define MAX_INDENT	10
void __function_enter_exit(const char *fname, int enter_exit,
	int retcode)
{
	static int indent = 0;
	static const char spaces[] = "          ";
	const char *p = spaces + MAX_INDENT;
	/*
	 * Note that we MUST "declare" TIMESTAMP last: in case DEBUG_TSC is set,
	 * an rdtscl() is done on the argument, and, well, that's C.
	 */
	TIMESTAMP(stamp);
	stamp = stamp % 1000000;
	
	switch (enter_exit) {
		case __FUNCTION_ENTER:
			if (indent < MAX_INDENT)
				indent++;
			break;
		case __FUNCTION_EXIT:
		case __FUNCTION_EXIT_WITHARG:
			/* Nothing */
			break;
		default: /* Meh? */
			return;
	}

	p -= indent;

	switch (enter_exit) {
		case __FUNCTION_ENTER:
			acx_log(LOG_DEBUG, L_FUNC, "%08ld %s-> %s\n",
				stamp, p, fname);
			break;
		case __FUNCTION_EXIT:
			acx_log(LOG_DEBUG, L_FUNC, "%08ld %s<- %s\n",
				stamp, p, fname);
			break;
		case __FUNCTION_EXIT_WITHARG:
			acx_log(LOG_DEBUG, L_FUNC, "%08ld %s<- %s: %08X\n",
				stamp, p, fname, retcode);
	}

	/*
	 * The below test is enough: we already sanitized away illegal values of
	 * enter_exit at the beginning.
	 */
	if (enter_exit != __FUNCTION_ENTER)
		indent--;

}

#endif /* ACX_LOG_LEVEL == 2 */

