/*
 *  io.c
 *  iyell/quotes
 *
 *  Created by Michel DEPEIGE on 18/02/2009.
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
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
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
struct event		ev_in, ev_out;
struct evbuffer		*in, *out;

/* prototypes */
void	io_read(int fd, short event, void *arg);
void	io_write(int fd, short event, void *arg);

void	cmd_parse(struct evbuffer *in, struct evbuffer *out);

/* functions */

/*
 * Main loop
 * init events & friends
 * read / write stdin & stdout
 */

void	io_loop()
{
	/* init */
	in = evbuffer_new();
	out = evbuffer_new();

	if (in == NULL || out == NULL) {
		log_err("[i] cannot init buffers: %s\n", strerror(errno));
		return ;
	}

	event_set(&ev_in, fileno(stdin), EV_READ, io_read, &ev_in);
	event_add(&ev_in, NULL);

	/* event loop */
	if (event_dispatch() < 0)
		log_err("[i] event_dispatch() error: %s\n", strerror(errno));

	/* cleanup */
	evbuffer_free(in);
	evbuffer_free(out);
}

/*
 * I/O callbacks
 */

void	io_read(int fd, short event, void *arg)
{
	int	ret;
	char	tmp[BUFF_SIZE];

	event = 0;
	ret = read(fd, tmp, BUFF_SIZE);

	if (ret <= 0) {
		log_err("[i] error %i while reading data: %s\n",
			ret, strerror(errno));
		return ;
	}

	evbuffer_add(in, tmp, ret);
	cmd_parse(out, in);

	/* reschedule this event */
	if (event_add((struct event *)arg, NULL) == -1)
		log_err("[i] event_add() error: %s\n", strerror(errno));
}

void	io_write(int fd, short event, void *arg)
{
	int	ret;

	assert(EVBUFFER_LENGTH(out) != 0);
	event = 0;

	ret = write(fd, EVBUFFER_DATA(out), EVBUFFER_LENGTH(out));

	if (ret <= 0) {
		log_err("[i] error %i while writing data: %s\n",
			ret, strerror(errno));
		return ;
	}
	else {
		evbuffer_drain(out, ret);
	}

	/* if buffer is free, do nothing */
	if (EVBUFFER_LENGTH(out) > 0)
		if (event_add((struct event *)arg, NULL) == -1)
			log_err("[i] event_add() error: %s\n", strerror(errno));
}

/*
 * will eb called with SIGINT, SIGTERM, ...
 */

 void	io_shutdown(void)
 {
	if (event_del(&ev_in) == -1)
		log_err("[i] event_del() error: %s\n", strerror(errno));
	if (event_del(&ev_out) == -1)
		log_err("[i] event_del() error: %s\n", strerror(errno));
 }

/*
 * strip nick in the output buffer if possible
 * (-s flag)
 */

void	io_strip_nick(struct evbuffer *out, char *nick, char *channel)
{
	size_t	len;

	assert(channel != NULL);
	assert(nick != NULL);

	/* channel == empty => private, no strip */
	if (channel[0] == '\0')
		return ;

	len = strlen(nick);
	/* should not happen, but who knows */
	if (len == 0)
		return ;

	/* remove the nick from the buffer (ie : drain len bytes) */
	evbuffer_drain(out, len);
}

/*
 * put the EV_WRITE flag on ev_out
 */

void	io_toggle_write()
{
	assert(EVBUFFER_LENGTH(out) > 0);

        if (event_del(&ev_out) == -1)
                log_err("[n] cannot event_del(): %s\n", strerror(errno));
        event_set(&ev_out, fileno(stdout), EV_WRITE, io_write, &ev_out);
	if (event_add(&ev_out, NULL) == -1)
		log_err("[n] cannot event_add(): %s\n", strerror(errno));
}
