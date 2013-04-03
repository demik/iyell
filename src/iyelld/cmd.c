/*
 *  cmd.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 05/06/2008.
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
#define _DARWIN_C_SOURCE
#include <assert.h>
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
#include "cmd.h"
#include "conf.h"
#include "heap.h"
#include "hooks.h"
#include "irc.h"
#include "main.h"
#include "stats.h"

/* structures */
typedef struct	cmd_msg_s {
        const char      *cmd;
        void            (*fp)(struct evbuffer *out, \
			      int argc, char **argv, \
			      char *who, char *where);
}		cmd_msg_td;

/* prototypes */
void			cmd_build_args(char *str, int *argc, char **argv);
void			cmd_search_hooks(int argc, char **argv, char *who, char *where);

static inline char	*get_nick_from_mask(char *mask);

/* defines */
#define	STATUS_SIZE	16

/*
 * check if the message from IRC is for me or not
 * split the message, find where it comes from, and what the sender want from me
 */

void	cmd_check_irc_msg(struct evbuffer *out, char *who, char *str)
{
	char	*me, *where;
	size_t	me_len, str_len;

	/* find where and what */
	where = strsep(&str, " :");

	/* sanity checks */
	if (str == NULL)
		return ;
	str++;

	me = hash_text_get_first(g_conf.global, "nick");	
	if (me == NULL)
		return ; /* if we are here, then WTF */

	/* privates messages management */
	if (strcmp(where, me) == 0) { 
		if (! hash_text_is_true(g_conf.global, "allow_private_cmd"))
			return ;
		cmd_switch(out, get_nick_from_mask(who), str, NULL);
		return ;
	}

	me_len = strlen(me);
	str_len = strlen(str);
	if ((me_len + 2) > str_len)
		return ; 
	if (strncmp(str, me, me_len) == 0) {
		if (str[me_len] == ':' || str[me_len] == '>' || str[me_len] == ' ')
			cmd_switch(out, who, str + me_len + 1, where);
	}
}

/*
 * build argc argv from str
 * usefull for some commands
 */

#define	CMD_DELIM	" \t\r\n"

void	cmd_build_args(char *str, int *argc, char **argv)
{
	int	i;
#ifdef HAVE_STRTOK_R
	char	*saveptr
#endif
	char	*token;

	for (i = 1; i < MAX_TOKENS; i++, str = NULL) {
		#ifdef HAVE_STRTOK_R
		token = strtok_r(str, CMD_DELIM, &saveptr);
		#else
		token = strtok(str, CMD_DELIM);
		#endif
		if (token == NULL)
			break;
		argv[i - 1] = token;
	}
	argv[i - 1] = NULL;
	*argc = i - 1;
}

/*
 * Fin the right function for this command
 * if who == NULL -> private message, respond in "where"
 */
 
void	cmd_switch(struct evbuffer *out, char *who, char *what, char *where)
{
	char		*tab[MAX_TOKENS];
	int		argc = 0;
	unsigned int	i;
	static time_t	last = 0, current;
	struct cmd_msg_s cmd_tab[] = {
		{"ping", cmd_ping},
		{"stats", cmd_stats},
		{"timestamp", cmd_timestamp},
		{"uptime", cmd_uptime},
		{0, 0}
	};

	while (*what == ' ' || *what == ':' || *what == '>')
		what++;
 	cmd_build_args(what, &argc, tab);
	assert(argc > 0);
	/* seems we have everything, search for know command */
	for (i = 0; cmd_tab[i].cmd != NULL; i++) {
		if (strcmp(cmd_tab[i].cmd, what) == 0) {
        		current = time(NULL);
        		if (current == -1)
                		return ;
        		if ((current - last) < 1)
                		return ;
        		last = current;
			cmd_tab[i].fp(out, argc, tab, who, where);
			return ;
		}
	}

	if (g_mode & HOOKS)
		cmd_search_hooks(argc, tab, who, where);
}

/*
 * this function is similar to cmd_switch, but search if valid
 * command hook exist.
 *
 * this one is rather complex. It parses the hash table,
 * searching for keys that mau be valid command hooks.
 */

void	cmd_search_hooks(int argc, char **argv, char *who, char *where)
{
	struct record_s *recs;
	unsigned int    c, i;

	/* check if we have something */
	assert(g_conf.cmd != NULL);

	recs = g_conf.cmd->records;
	for (i = 0, c = 0; c < g_conf.cmd->records_count; i++) {
		if (recs[i].hash != 0 && recs[i].key) {
			c++;

			/* check if cmd looks like a registered hook */
			if (strcmp(argv[0], recs[i].key) == 0 && \
			    ! hash_text_is_true(g_conf.cmd, (char *)recs[i].key) && \
			    ! hash_text_is_false(g_conf.cmd, (char *)recs[i].key))
				hooks_cmd_switch(argc, argv, who, where);
		}
	}	
}

/* === LIVE COMMANDS BELOW === */


/*
 * get "bot" from "bot!user@hostname"
 * if error, return the original string
 */

static inline char	*get_nick_from_mask(char *mask)
{
	char	*nick;

	nick = strsep(&mask, "!");
	return nick;
}

/*
 * very basic command : just reply "pong"
 * if who == NULL, send pong to where
 */

void	cmd_ping(struct evbuffer *out, int argc, char **argv, char *who, char *where)
{
	char	*tmp = NULL;

	argc = 0;
	argv = NULL;
	if (! hash_text_is_true(g_conf.cmd, "ping"))
		return;
	if (where) { 
        	asprintf(&tmp, "%s: pong", get_nick_from_mask(who));
        	if (tmp == NULL)
                	return;
		irc_cmd_privmsg(out, where, tmp);
		free(tmp);
	}
	else {
		irc_cmd_privmsg(out, who, "pong");
	}
}

/*
 * send internal stats
 * I/O stats, messages, errors...
 */

void    cmd_stats(struct evbuffer *out, int argc, char **argv, char *who, char *where)
{
	char	*tmp = NULL;

	if (! hash_text_is_true(g_conf.cmd, "stats"))
		return ;
	argc = 0;
	argv = NULL;

	if (where) 
		asprintf(&tmp, "%s: \2[\2irc\2]\2 %ld KiB in, %ld KiB out, %ld lines " \
			 "\2[\2msg\2]\2 %ld KiB in, %ld KiB out, %ld offset exceeded, " \
			 "%ld ok, %ld error(s)",
			 get_nick_from_mask(who), gstats.srv_in / 1024, \
			 gstats.srv_out / 1024, gstats.lines, \
			 gstats.msg_in / 1024, gstats.msg_out / 1024, \
			 gstats.oe_msg, gstats.good_msg, gstats.bad_msg);
	else
		asprintf(&tmp, "\2[\2irc\2]\2 %ld KiB in, %ld KiB out, %ld lines " \
			 "\2[\2msg\2]\2 %ld KiB in, %ld KiB out, %ld offset exceeded, " \
			 "%ld ok, %ld error(s)",
			 gstats.srv_in / 1024, \
			 gstats.srv_out / 1024, gstats.lines, \
			 gstats.msg_in / 1024, gstats.msg_out / 1024, \
			 gstats.oe_msg, gstats.good_msg, gstats.bad_msg);

	if (tmp == NULL)
		return ;
	if (where)
		irc_cmd_privmsg(out, where, tmp);
	else
		irc_cmd_privmsg(out, who, tmp);
	free(tmp);
}

/*
 * build status in the form of [..T.s.b]
 * check include/main.h	for values
 *
 * disabled 	.
 * BANNED	b
 * CONNECTED	c
 * DAEMON	d
 * FLOOD	f
 * HOOKS	h
 * KEY		K
 * SYSLOG       l
 * NICKUSED	n
 * USE_SSL	s
 * VERBOSE	v
 *
 */

static inline void	cmd_build_modes(char *tab)
{
	unsigned int	i;
	char		on[] = "\2[\2bcdfhklnsv\2]\2";
	int	mode[] = {0xffffffff, 0xffffffff, 0xffffffff, BANNED, \
			  CONNECTED, DAEMON, FLOOD, HOOKS, KEY, \
			  SYSLOG, NICKUSED, USE_SSL, VERBOSE, \
			  0xffffffff, 0xffffffff, 0xffffffff};

	tab[STATUS_SIZE - 1] = '\0';
	for (i = 1; i < (STATUS_SIZE - 3); i++)
		tab[i] = '.';
	for (i = 0; i < (STATUS_SIZE - 1); i++) {
		if (g_mode & mode[i])
			tab[i] = on[i];
	}	
}

/*
 * Convert a timestamp to an human readable value
 */

void	cmd_timestamp(struct evbuffer *out, int argc, char **argv, char *who, char *where)
{
	char		*endptr;
	char		output[BUFF_SIZE];
	time_t		val;
	struct tm	time_s;
	int		len;

	if (argc < 2)
		return ;
	if (! hash_text_is_true(g_conf.cmd, "timestamp"))
		return ;

	len = 0;
	/* check if it's a private message */
	if (where != NULL)
		len = snprintf(output, BUFF_SIZE, "%s: ", get_nick_from_mask(who));

	if (len >= BUFF_SIZE || len < 0)
		return ;
	val = strtol(argv[1], &endptr, 10);
	
	/* check for errors */
	if ((errno == ERANGE) || (errno != 0 && val == 0)) {
		if (g_mode & VERBOSE)
			log_err("[c] strtol() error: %s\n", strerror(errno));
		return ;
	}
	if (endptr == argv[1])
		return ;

	if (localtime_r(&val, &time_s) == NULL) {
		log_err("[c] localtime() error: %s\n", strerror(errno));
		return ;
	}
	if (strftime(output + len, BUFF_SIZE - len, "%c", &time_s) == 0) {
		log_err("[c] strftime() error: %s\n", strerror(errno));
		return ;
	}

	/* output generated, send it */
	if (where == NULL)
		irc_cmd_privmsg(out, who, output);
	else
		irc_cmd_privmsg(out, where, output);
}

/*
 * return eturn end bot uptime
 */

void    cmd_uptime(struct evbuffer *out, int argc, char **argv, char *who, char *where)
{
	char		*tmp = NULL;
	long		uptime;
	unsigned int	days = 0, hours = 0, minutes = 0;
	time_t		seconds;
	struct tm	*current;
	char		modes[STATUS_SIZE];	

	if (gstats.boot_timestamp == 0) {
		log_err("[c] boot_timestamp incorect, cannot build uptime\n");
		return ;
	}
	if (! hash_text_is_true(g_conf.cmd, "uptime"))
		return ;
	argc = 0;
	argv = NULL; 
	if ((time(&seconds)) == -1) {
		log_err("[c] time() error: %s\n", strerror(errno));
		return ;
	}
	current = localtime(&seconds);
	cmd_build_modes(modes);

	/* find day(s) / hour(s) / minute(s) */
	uptime = seconds - gstats.boot_timestamp;
	days = uptime / (60 * 60 * 24); 
	minutes = uptime / 60;
	hours = (minutes / 60) % 24;

	if (days) {
		if (hours)
			asprintf(&tmp, "%2d:%02d  up %d day%s, %2d:%02d:%02d flags: %s", \
				 current->tm_hour, current->tm_min, \
				 days, (days != 1) ? "s" : "", hours, \
				 minutes % 60, (int)(uptime % 60), modes);
		else
			asprintf(&tmp, "%2d:%02d  up %d day%s, %d min, %ld sec. flags: %s", \
				 current->tm_hour, current->tm_min, \
				 days, (days != 1) ? "s" : "", minutes, uptime % 60, modes);
	}
	else if (hours) {
		asprintf(&tmp, "%2d:%02d  up %2d:%02d:%02d flags: %s", \
			 current->tm_hour, current->tm_min, \
			 hours, minutes % 60, (int)(uptime % 60), modes);
	}
	else if (minutes) {
		asprintf(&tmp, "%2d:%02d  up %d min, %ld sec. flags: %s", \
			 current->tm_hour, current->tm_min, \
			 minutes % 60, uptime % 60, modes);
	}
	else {
		asprintf(&tmp, "%2d:%02d  up %ld sec. flags: %s", \
			 current->tm_hour, current->tm_min, \
			 uptime % 60, modes);
	}
	
	/* send result */
	if (tmp == NULL)
		return ;

	/* private or public message */
	if (where != NULL)
		irc_cmd_privmsg(out, where, tmp);
	else
		irc_cmd_privmsg(out, who, tmp);
	free(tmp);
}

