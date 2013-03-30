/*
 *  events.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 11/07/2008.
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
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include "posix+bsd.h"
#include <event.h>
#include "heap.h"
#include "main.h"
#include "misc.h"
#include "sched.h"

/*
 * For libevent 1.3 
 */

#ifndef HAVE_EVENT_LOOPBREAK
int	event_loopbreak(void);
#endif

/*
 * nick change scheduling
 * will call try_nick_change() (misc.c)
 */

void	schedule_regain_nick(int seconds)
{
	schedule_nick_change(seconds);
}

void	schedule_nick_change(int seconds)
{
	struct timeval	todo_timeout;
	
	if (seconds <= 0)
		seconds = 180;
	if (g_mode & VERBOSE)
		log_msg("[m] will try to change/regain nick in %i second(s)\n", seconds);
	todo_timeout.tv_sec = seconds;
	todo_timeout.tv_usec = 42;
	if (event_once(-1, EV_TIMEOUT, try_nick_change, ircout, &todo_timeout) == -1)
		perror("[m] cannot event_add()");
}

/*
 * reconnection stuff
 * schedule a reconnection after a disconnection
 */

void	schedule_reconnection(int seconds)
{
	struct timeval	rec_delay;

	if (g_mode & SHUTDOWN) {
		log_msg("[m] shutdown in progress, aborting reconnection\n");
		event_loopbreak();
		return ;
	}
	if (seconds <= 0)
		seconds = 30;
	if (g_mode & VERBOSE)
		log_msg("[m] will try to reconnect in %i second(s)\n", seconds);
	rec_delay.tv_sec = seconds;
	rec_delay.tv_usec = 42;
	if (event_once(-1, EV_TIMEOUT, irc_reconnect, NULL, &rec_delay) == -1)
		perror("[m] cannot event_add()");
}

/*
 * ban rejoin stuff
 * will call try_rejoin_after_ban() (misc.c)
 */

void	schedule_rejoin_after_ban(int seconds)
{
	struct timeval	todo_timeout;
	
	if (seconds <= 0)
		seconds = 180;
	if (g_mode & VERBOSE)
		log_msg("[m] will try to rejoin banned channels in %i seconds\n", seconds);
	todo_timeout.tv_sec = seconds;
	todo_timeout.tv_usec = 42;
	if (event_once(-1, EV_TIMEOUT, try_rejoin_after_ban, ircout, &todo_timeout) == -1)
		perror("[m] cannot event_add()");
}

/*
 * schedule output
 * used by irc_throttled_write()
 */

void    schedule_irc_write(int seconds)
{
	struct timeval	write_timeout;

	if (seconds <= 0)
		seconds = 1;
	write_timeout.tv_sec = seconds;
	write_timeout.tv_usec = 42;
	if (event_once(-1, EV_TIMEOUT, net_throttled_write, NULL, &write_timeout) == -1) {
		log_err("[m] cannot add event, disabling throttling temporary");
		net_throttled_write(-1, EV_TIMEOUT, NULL);
	}
}

/*
 * shutdown
 * will call irc_shutdown() (network.c)
 */

 void	schedule_shutdown()
 {
	struct timeval	shutdown_timeout;

	if (g_mode & VERBOSE)
		log_msg("[m] will shutdown soon...\n");
	shutdown_timeout.tv_sec = 1;
	shutdown_timeout.tv_usec = 42;

	if (! (g_mode & CONNECTED)) {
		event_loopbreak();
		return ;
	}
	if (event_once(-1, EV_TIMEOUT, irc_shutdown, ircout, &shutdown_timeout) == -1)
		perror("[m] cannot event_add()");
 }
