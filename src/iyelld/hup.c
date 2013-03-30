/*
 *  misc.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 25/10/07.
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
#include <errno.h>
#include <signal.h>
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
#include "hooks.h"
#include "irc.h"
#include "misc.h"
#include "sched.h"
#include "tools.h"

/* defines */

/* globals */
static struct event	ev_hup;

/* prototypes */
static void	hup_check_channels(char **old, char **new);
static void	hup_check_nick(char *old, char *new);
static void	hup_free_saved_channels(char **tab);
void            hup_handler(int sig, short event, void *arg);
static char	**hup_save_channels();
static char	*hup_save_nick();
void		hup_update_modes(conf_t *conf);

/*
 * Check for new channels
 * Compares old and new channels and join/part if needed
 */

static void	hup_check_channels(char **old, char **new) 
{
	int     n, o, found;
	
	/* searching for new channels */
	for (n = 0; new[n] != NULL; n++) {
		found = 0;
		for (o = 0; old[o] != NULL; o++) {
			if (strcmp(new[n], old[o]) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			irc_cmd_join(ircout, new[n]);
		}
	}
	
	/* searching for olds channels */
	for (o = 0; old[o] != NULL; o++) {
		found = 0;
		for (n = 0; new[n] != NULL; n++) {
			if (strcmp(new[n], old[o]) == 0) {
				found = 1;
				break;
			}
		}
		if (!found) {
			irc_cmd_part(ircout, old[o]);
		}
	}
} 

/*
 * Compare old and new nicknames. Change current nick if needed
 */

static void	hup_check_nick(char *old, char *new)
{
	if (new == NULL) {
		log_err("[r] warning: no nickname in new configuration, using old one\n");
		if (hash_text_insert(g_conf.global, "nick", old) == -1) { 
			log_err("[r] error: cannot repair bad configuration\n");
			fatal("[r] exiting");
		}
		return ;
	}
	if (strcmp(new, old) == 0)
		return ;
	
	try_nick_change(-1, -1, ircout);
}

/*
 * free a previously saved array of channels
 */

static void	hup_free_saved_channels(char **tab)
{
	unsigned int	o;

	for (o = 0; tab[o] != NULL; o++) {
		free(tab[o]);
	}
	free(tab);
}
 
/*
 * on SIGHUP, reload the configuration file
 * join / part channels if needed
 */

void	hup_handler(int sig, short event, void *arg)
{
	char	**old_c;	/* old channels */
	char	**new_c;	/* new channels */
	char	*old_n;		/* old nickname */
	char	*new_n;		/* new nickname */

	event = 0;
	arg = NULL;
	
	/* print only of we got a signal */
	if (sig > 0)
		log_msg("[r] got signal %i, reloading configuration\n", sig);

	/* terminate active hooks */
	hooks_broadcast_sig(ghook, SIGTERM);
	
	/* save the old nickname */
	if ((old_n = hup_save_nick()) == NULL)
		return ;
	
	/* save the old channels settings */
	if ((old_c = hup_save_channels()) == NULL) {
		free(old_n);
		return ;
	}

	/* reload the configuration */
	conf_reinit(&g_conf);
	conf_read(&g_conf, g_file);
	
	/* get the new settings */
	new_c = hash_get(g_conf.global, "channels");
	if (new_c == NULL) {
		log_err("[r] cannot complete sighup\n");
		free(old_c);
		free(old_n);
		return ;
	}
	
	new_n = hash_text_get_first(g_conf.global, "nick");
	
	/* Search for diffs in channels, nick, etc... */
	hup_check_channels(old_c, new_c);	
	hup_check_nick(old_n, new_n);
	
	/* free stuff, etc */
	hup_free_saved_channels(old_c);
	free(old_n);
	
	/* update modes */
	hup_update_modes(&g_conf);
}

/* register SIGHUP event with libevent */
void	hup_register()
{
	signal_set(&ev_hup, SIGHUP, hup_handler, (void *)(&ev_hup));
	if (signal_add(&ev_hup, NULL) == -1)
		fatal("cannot register handler");
}

/*
 * Save old channels aray (it will be freed by conf_reinit())
 * technically strdup a null terminated char **
 */

static char	**hup_save_channels()
{
	char		**tmp;
	char		**saved;
	unsigned int	o;
	
	/* count channels for malloc */
	tmp = hash_get(g_conf.global, "channels");
	if (tmp == NULL) {
		log_err("[r] cannot complete SIGHUP: %s\n", strerror(errno));
		return NULL;
	}
	for (o = 0; tmp[o] != NULL; o++);
	saved = malloc((o + 1) * sizeof(char *));
	if (saved == NULL)
		return NULL;
	
	/* duplicate strings in new array */
	saved[o] = NULL;
	for (o = 0; tmp[o] != NULL; o++) {
		saved[o] = strdup(tmp[o]);
		if (saved[o] == NULL)
			fatal("[r] cannot strdup()");
	}
	return saved;
}

/*
 * Save nickname before reload
 * a strdup with some additional checks
 */

static char	*hup_save_nick()
{
	char	*saved;
	char	*tmp;

	tmp = hash_text_get_first(g_conf.global, "nick");
	if (tmp == NULL) { /* shouldn't happen */
		log_err("[r] cannot commplete SIGHUP: my nickname vanished !\n",
			strerror(errno));
		return NULL;	
	}
	
	saved = strdup(tmp);
	if (saved == NULL) {
		log_err("[r] cannot complete SIGHUP: %s\n", strerror(errno));
		return NULL;
	}
	return saved;
}

/*
 * Delete SIGHUP event
 */

void	hup_unregister()
{
	signal_del(&ev_hup);
}

/*
 * changes modes from the specified configuration
 */

void	hup_update_modes(conf_t *conf)
{
	g_mode &= ~COLOR;
	if (hash_text_is_true(conf->global, "color"))
		g_mode |= COLOR;
	g_mode &= ~SL3164_SILENT;
	if (hash_text_is_true(conf->global, "syslog_silent_drop"))
		g_mode |= SL3164_SILENT;
	g_mode &= ~HOOKS;
	if (hash_text_is_true(conf->global, "hooks"))
		g_mode |= HOOKS;
	g_mode &= ~SYSLOG;
	if (hash_text_is_true(conf->global, "syslog"))
		g_mode |= SYSLOG;
	g_mode &= ~THROTTLING;
  	if (hash_text_is_true(conf->global, "throttling"))
		g_mode |= THROTTLING;
}
