#ifndef _ACX_DEBUG_H_
#define _ACX_DEBUG_H_

/*
 * acx_debug.h: logging constants and helpers
 *
 * Copyright (c) 2008, the ACX100 project (http://acx100.sourceforge.net).
 *
 * See the README file for licensing.
*/

/*
 * ACX_DEBUG:
 * set to 0 if you don't want any debugging code to be compiled in
 * set to 1 if you want some debugging
 * set to 2 if you want extensive debug log
 */
#define ACX_DEBUG 2
#define ACX_DEFAULT_MSG (L_INIT|L_IRQ|L_ASSOC)

#if ACX_DEBUG
extern unsigned int acx_debug;
#else
enum { acx_debug = 0 };
#endif

#endif /* _ACX_DEBUG_H_ */
