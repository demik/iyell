/*
 *  main.c
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
#include <errno.h>
#include <libgen.h>
#include <stdint.h>
#include <stdlib.h>
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

/* prototypes */
int	checkopt(int argc, char **argv);
void	usage(void);		/* from opt.c */

void    cmd_random(struct evbuffer *out, char *nick, char *channel, char *args);

void		quotes_random_timeout(int fd, short event, void *arg);
static void	quotes_start_timers(void);

extern struct evbuffer  *out;   /* for callbacks */
char			*g_name; /* global name */

/* structures */
typedef struct	random_s {
	char		*channel;
	struct event	ev;
	struct timeval	timeout;
}		random_t;

/* functions */

int	main(int argc, char *argv[])
{
	struct event_base	*bot;

	/* build name */
	assert(argv[0] != NULL);
	g_name = basename(argv[0]);
	if (g_name == NULL) {
		log_err("[r] basename() error: %s\n", strerror(errno));
		g_name = "quotes";
	}

	/* checks args and database specification */
	if (checkopt(argc, argv) == -1) {
		log_err("[-] no database specified\n");
		usage();
	}

	if (g_mode & VERBOSE)
		log_msg("[+] using %s as database\n", g_database);

	/* init logsi, db */
	log_init();
	
	if ((bot = event_init()) == NULL)
		fatal("[-] cannot init libevent, exiting\n");
		
	if (g_mode & VERBOSE) {
		log_msg("[+] using libevent %s, mechanism: %s\n",
		event_get_version(), event_get_method());
	}
															
	sig_set_handlers();
	if (db_init(g_database) == ERROR)
		goto fail;

	/* misc init */
	quotes_start_timers();

	/* main loop */
	io_loop();

	/* deinit everything */
	db_end();
fail:
	sig_unset_handlers();
        event_base_free(bot);
	log_deinit();
	return(NOERROR);
}

/*
 * random callback
 * called by quotes_start_timers()
 * fetch configuration from the config table in the sqlite db.
 */

/* pointer on heap used to manage a timer array */
static struct random_s	**random_tab; 

int	quotes_random_cb(void *unused, int argc, char **argv, char **col_name)
{
	unsigned int	i, t;
	char		*channel, *endptr;

	unused = NULL;
	col_name = NULL;

	/* impossible, sanity check */
	if (argc % 2)
		return 0;

	/*
	 * argv[0] == channel, argv[1] == timer, argv[2] == channel, etc
	 * thus the number of timers = argc / 2
	 * add one more, the table should be NULL terminated.
	 */

	random_tab = malloc(((argc / 2) + 1) * sizeof(struct random_s *));
	if (random_tab == NULL) {
		log_err("[-] malloc() error: %s\n", strerror(errno));
		return 0;
	}

	for (i = 0, t = 0; i < (unsigned int)argc; i += 2, t++) {
		channel = strdup(argv[i]);
		if (channel == NULL) {
			log_err("[-] strdup() error: %s\n", strerror(errno));
			t--;
			continue;
		}

		/* 
		 * allocate memory for the channels tring, the timeout, and the 
		 * event structure
		 */

		random_tab[t] = malloc(sizeof(struct random_s));

		if (random_tab[t] == NULL) {
			log_err("[-] malloc() error: %s\n", strerror(errno));
			t--;
			free(channel);
			continue;
		}

		/* if 0, generate an assert in libevent :/ */
		random_tab[t]->timeout.tv_usec = 1;
		random_tab[t]->timeout.tv_sec = strtol(argv[i + 1], &endptr, 10);

		if (random_tab[t]->timeout.tv_sec < 0) {
			log_err("[-] incorect random interval for channel %s\n",
				argv[i]);
			t--;
			continue ;
		}

		random_tab[t]->channel = channel;
		evtimer_set(&(random_tab[t]->ev), quotes_random_timeout, random_tab[t]);
		evtimer_add(&(random_tab[t]->ev), &(random_tab[t]->timeout));
	}

	random_tab[t] = NULL;

	if (g_mode & VERBOSE)
		log_msg("[+] %i timer(s) in configuration, %i successfully registered\n",
			argc / 2, t);
	return 0;
}

/*
 * initialise misc timers
 * usefull for random stuff
 */

static void	quotes_start_timers()
{
	if (g_mode & VERBOSE)
		log_msg("[+] initializing timers\n");

	random_tab = NULL;

	/* random timers */
	db_config_random();
}

void	quotes_stop_timers()
{
	unsigned int	i;

	if (random_tab == NULL)
		return ;

	if (g_mode & VERBOSE)
		log_msg("[+] removing timers\n");

	/* remove all registered random timers */
	for (i = 0; random_tab[i] != NULL; i++) {
		free(random_tab[i]->channel);
		evtimer_del(&(random_tab[i]->ev));
		free(random_tab[i]);
	}

	free(random_tab);
	random_tab = NULL;
}

/*
 * callback which is called every xx seconds
 * output a random entry to the specified channel
 */

void	quotes_random_timeout(int fd, short event, void *arg)
{
	struct random_s	*tmp;

	fd = -1;
	tmp = arg;

	if (event & EV_TIMEOUT)
		cmd_random(out, NULL, tmp->channel, NULL);
	evtimer_set(&(tmp->ev), quotes_random_timeout, arg);
	evtimer_add(&(tmp->ev), &(tmp->timeout));
}
