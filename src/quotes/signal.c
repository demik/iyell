/*
 *  signal.c
 *  iyell/quotes
 *
 *  Created by Michel DEPEIGE on 25/02/2009.
 *  Copyright (c) 2009 Michel DEPEIGE.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 */

#define _GNU_SOURCE
#define _BSD_SOURCE
#define __BSD_VISIBLE 1
#define _DARWIN_C_SOURCE 1
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <event.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "main.h"
#include "quotes.h"

/* heap */
static struct event     ev_int, ev_hup, ev_term;

/* prototypes */

/* functions */

/*
 * shutdown callback
 */

void	sig_shutdown(int sig, short event, void *arg)
{
	arg = NULL;
	event = 0;

	if (sig > 0)
		log_msg("[r] got signal %i, shuting down\n", sig);

	io_shutdown();
	sig_unset_handlers();
}

/*
 * register and unregister functions
 */

void	sig_set_handlers(void)
{
        signal_set(&ev_int, SIGINT, sig_shutdown, (void *)(&ev_int));
        if (signal_add(&ev_int, NULL) == -1)
                fatal("cannot register handler");
        signal_set(&ev_hup, SIGHUP, sig_shutdown, (void *)(&ev_hup));
        if (signal_add(&ev_hup, NULL) == -1)
                fatal("cannot register handler");
        signal_set(&ev_term, SIGTERM, sig_shutdown, (void *)(&ev_term));
        if (signal_add(&ev_term, NULL) == -1)
                fatal("cannot register handler");
}

void	sig_unset_handlers(void)
{
	signal_del(&ev_int);
	signal_del(&ev_hup);
	signal_del(&ev_term);

	/* remove timers too */
	quotes_stop_timers();
}
