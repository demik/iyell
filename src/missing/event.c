/*
 *  event.c
 *
 *
 *  Created by Michel DEPEIGE on 29/05/2009.
 *  2013 lostwave.net.
 *  This code can be used under the BSD license.
 *
 */

#define __BSD_VISIBLE 1
#define _BSD_SOURCE 1
#define _DARWIN_C_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "posix+bsd.h"
#include <event.h>

#ifndef HAVE_EVENT_LOOPBREAK
/*
 * this add a missing event_loopbreak() for libevent 1.3
 * using event_loopexit();
 * useful for  Debian Lenny and others
 */

extern struct event_base *current_base;

/* prototypes */
int     	event_loopbreak(void);
static int	event_loopbreak_missing(struct event_base *event_base);

/* real stuff */

int	event_loopbreak(void)
{
	return (event_loopbreak_missing(current_base));
}

static int	event_loopbreak_missing(struct event_base *event_base)
{
	struct timeval	missing;

	if (event_base == NULL)
		return (-1);

	missing.tv_sec = 0;
	missing.tv_usec = 0;
	return event_loopexit(&missing);
}

#endif
