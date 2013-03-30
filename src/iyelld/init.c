/*
 *  init.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 10/10/07.
 *  Copyright (c) 2007 Michel DEPEIGE.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#include "heap.h"
#include "hooks.h"
#include "main.h"
#include "misc.h"
#include "stats.h"
#include "conf.h"

/* prototypes */
void		check_id(void);
int		init(struct event_base **bot);
static void	init_flags();

/* functions */
void	check_id()
{
	if (getuid() == 0 || getgid() == 0)
		fprintf(stderr, "[i] warning: running as root !\n");
}

/*
 * main init
 */

int	init(struct event_base **bot)
{
	char	**tmp = NULL;
	
	conf_init(&g_conf);
	conf_read(&g_conf, g_file);
	
	/* check for basic stuff */
	check_id();
	tmp = hash_get(g_conf.global, "nick");
	if (tmp == NULL || tmp[0] == NULL) {
		fprintf(stderr, "[i] no nickname set in configuration, exiting\n");
		return ERROR;
	}
	
	/* check for UDP configuration */
	tmp = hash_get(g_conf.global, "udp_port");
	if (tmp == NULL || tmp[0] == NULL) {
		fprintf(stderr, "[i] no UDP port set, exiting\n");
		return ERROR;
	}

	/* check for UDP key (RC4) */
	if (hash_text_get_first(g_conf.global, "udp_key") == NULL) {
		fprintf(stderr, "[i] no UDP key set, exiting\n");
		return ERROR;
	}
	
	/* check for SSL mode */
	if (hash_text_is_true(g_conf.global, "ssl"))
		g_mode |= USE_SSL;

	/* init libevents */
	*bot = event_init();
	if (*bot == NULL) {
		fprintf(stderr, "[i] cannot init libevent, exiting\n");
		return ERROR;
	}
	if (g_mode & VERBOSE) {
		printf("[i] using libevent %s, mechanism: %s\n",
			event_get_version(), event_get_method());
	}
	
	/* set handlers */
	sig_set_handlers();

	/* misc init */	
	log_init();
	hooks_init(&ghook);
	stats_init();
	init_flags();

	timeout.tv_sec = 240;
	timeout.tv_usec = 42;
	g_mode |= STARTING;
	
	log_msg("[i] iyell version %s started.\n", VERSION);
	return NOERROR;
}

/* set global glags */
static void	init_flags()
{
	/* colors in IRC */
	if (! hash_text_is_false(g_conf.global, "color"))
		g_mode |= COLOR;

	/* drop unknow PRI silently */
	if (hash_text_is_true(g_conf.global, "syslog_silent_drop"))
		g_mode |= SL3164_SILENT;

	/* hooks activated flag */
	if (hash_text_is_true(g_conf.global, "hooks"))
		g_mode |= HOOKS;

	/* syslog logging flag */
	if (hash_text_is_true(g_conf.global, "syslog"))
		g_mode |= SYSLOG;

	/* IRC throttling flag */
	if (hash_text_is_true(g_conf.global, "throttling")) { 
		g_mode |= THROTTLING;
		if (g_mode & VERBOSE)
			log_msg("[i] throttling irc output\n");
	}
}
