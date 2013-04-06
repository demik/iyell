/*
 *  misc.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 25/10/07.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#include "heap.h"
#include "main.h"
#include "irc.h"
#include "sched.h"
#include "tools.h"

/* structure for timeout */
struct timeval		timeout;

/* prototypes */
void	change_nick(struct evbuffer *out, char *nick);
void	try_nick_change(int fd, short event, void *out);
void	try_rejoin_after_ban(int fd, short event, void *out);

/*
 * this function help the bot to register if the nick is used
 * and try to regain the nick set.
 * *nick isn't used yet, It's here for future improvement.
 */

void	change_nick(struct evbuffer *out, char *nick)
{
	size_t	len;
	int	mod;

	/* mark our nick as used, if so we will try to /nick later */
	if (! (g_mode & NICKUSED))
		schedule_regain_nick(180);
	g_mode |= NICKUSED;

	/*
	 * this code help to choose a temporary nickname while connecting
	 * to the server
	 */

	if (! (g_mode & CONNECTED)) {
		/* we need this if we don't want to flood the server */
		sleep(1);

		/* compute the nick and try it */
		nick = strdup(hash_text_get_first(g_conf.global, "nick"));
		if (nick == NULL)
			fatal("cannot strdup()");
		len = strlen(nick);
		mod = time(NULL);
		mod %= (len - 1);
		nick[mod] += (mod + 1);
		if ((nick[mod] < 'A' && nick[mod] > 'z') || \
		    (nick[mod] > 'Z' && nick[mod] < 'a')) {
			nick[mod] = '_';
		}
		if (g_mode & VERBOSE)
			log_msg("[m] nick used, trying with %s\n", nick);
		irc_cmd_nick(out, nick);
		free(nick);
	}
}

/*
 * nick change/regain callback
 * try to /nick
 */

void	try_nick_change(int fd, short event, void *out)
{
	fd = 0;
	event = 0;
	if (g_mode & VERBOSE)
		log_msg("[m] trying to change/regain my nickname\n");
	irc_cmd_nick(out, NULL);
	g_mode &= ~NICKUSED;
}

/*
 * ban rejoin callback and scheduler
 */

void	try_rejoin_after_ban(int fd, short event, void *out)
{
	fd = 0;
	event = 0;

	irc_send_join(out, NULL);
	g_mode &= ~BANNED;
}
