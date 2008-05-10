#ifndef _ACX_DEBUG_H_
#define _ACX_DEBUG_H_

/*
 * acx_debug.h: logging constants and helpers
 *
 * Copyright (c) 2008, Francis Galiegue <fgaliegue@gmail.com> for the ACX100
 * driver project.
 *
 * This file is licensed under GPL version 2.
*/

/*
 * ACX_DEBUG:
 * set to 0 if you don't want any debugging code to be compiled in
 * set to 1 if you want some debugging
 * set to 2 if you want extensive debug log
 */
#define ACX_DEBUG 2

#if ACX_DEBUG
extern unsigned int acx_debug;
#else
enum { acx_debug = 0 };
#endif

#endif /* _ACX_DEBUG_H_ */
