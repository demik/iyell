/*
 *  stats.c
 *
 *
 *  Created by Michel DEPEIGE on 01/06/2008.
 *  Copyright (c) 2008 Michel DEPEIGE.
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
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#include "main.h"
#include "stats.h"

/* on heap */
stats_t	gstats;

/* functions */
static void	stats_clear(stats_t *stats);


/*
 * Clear a stats_t structure;
 */

static void	stats_clear(stats_t *stats)
{
	stats->srv_in = 0;
	stats->srv_out = 0;
	stats->msg_in = 0;
	stats->msg_out = 0;
	stats->lines = 0;
	stats->bad_msg = 0;
	stats->oe_msg = 0;
	stats->good_msg = 0;
}

/*
 * init internal statistics
 * - init boot timestamp
 * - reset data
 * - check / init rrdtool
 */

int	stats_init()
{
	time_t	boot;

	stats_clear(&gstats);
	boot = time(NULL);
	if (boot == -1) {
		log_err("[t] time() error: %s\n", strerror(errno));
		boot = 0;
	}
	gstats.boot_timestamp = boot;
	return	NOERROR;
}
