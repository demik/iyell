/*
 *  cmd.c
 *  iyell/quotes
 *
 *  Created by Michel DEPEIGE on 01/03/2009.
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
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <event.h>
#include <sqlite3.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "main.h"
#include "quotes.h"

extern struct evbuffer	*out;	/* for callbacks */

/* defines */
#define	NICK	0
#define	CHANNEL	1

/* prototypes */
void		cmd_help(struct evbuffer *out, char *nick, char *channel, char *args);
void		cmd_memory(struct evbuffer *out, char *nick, char *channel, char *args);
void		cmd_switch(struct evbuffer *out, char *nick, char *channel, char *cmd, char *args);
static void	cmd_toggle_write(struct evbuffer *out, char *nick, char *channel);

/* structures */
typedef struct  cmd_quote_s {
        const char      *cmd;
        void            (*fp)(struct evbuffer *out, \
                              char *nick, char *channel, char *args);
}               cmd_quote_t;

/* functions */

/* add stuff */
void	cmd_add(struct evbuffer *out, char *nick, char *channel, char *args)
{
	char	*sql_error;

	if (args == NULL || strlen(args) == 0) {
		evbuffer_add_printf(out, "%s:%s:%s add <something>\n", \
				    nick, channel, g_name);
		cmd_toggle_write(out, nick, channel);
		return ;
	}
	if ((sql_error = db_add(nick, channel, args)) != NULL) {
		evbuffer_add_printf(out, "%s:%s:error: %s\n", \
				    nick, channel, sql_error);
		sqlite3_free(sql_error);
	}
	else {
		evbuffer_add_printf(out, "%s:%s:quote added successfully, " \
				    "id: %ju\n", nick, channel, \
				    db_last_insert_id());
	}
	cmd_toggle_write(out, nick, channel);
}

/* count quotes in db */
void	cmd_count(struct evbuffer *out, char *nick, char *channel, char *args)
{
	char	*sql_error;

	if ((sql_error = db_count_quotes(nick, channel, args)) != NULL) {
		evbuffer_add_printf(out, "%s:%s:error: %s\n", \
				    nick, channel, sql_error);
		sqlite3_free(sql_error);
		cmd_toggle_write(out, nick, channel);
	}
}

int	cmd_count_cb(void *tab, int argc, char **argv, char **col_name)
{
	char	**source;

	col_name = NULL;
	if (argc != 1)
		return 0;

	source = tab;
	evbuffer_add_printf(out, "%s:%s:total quotes in database: %s\n", \
			    source[NICK], source[CHANNEL], argv[0] ? argv[0] : "n/a");
	cmd_toggle_write(out, source[NICK], source[CHANNEL]);
	return 0;
}

/* delete a quote by id */
void	cmd_del(struct evbuffer *out, char *nick, char *channel, char *args)
{
	char	*sql_error;

	if (args == NULL || strlen(args) == 0) {
		evbuffer_add_printf(out, "%s:%s:%s del <id>\n", \
				    nick, channel, g_name);
		cmd_toggle_write(out, nick, channel);
		return ;
	}
	if ((sql_error = db_del(nick, channel, args)) != NULL) {
		evbuffer_add_printf(out, "%s:%s:error: %s\n", \
				    nick, channel, sql_error);
		sqlite3_free(sql_error);
		cmd_toggle_write(out, nick, channel);
	}
}

int	cmd_del_cb(void *tab, int argc, char **argv, char **col_name)
{
	char	**source;

	col_name = NULL;
	if (argc != 1)
		return 0;

	source = tab;
	evbuffer_add_printf(out, "%s:%s:quote deleted successfully (%s)\n", \
			    source[NICK], source[CHANNEL], argv[0] ? argv[0] : "n/a");
	cmd_toggle_write(out, source[NICK], source[CHANNEL]);
	return 0;
}

/*
 * generic data output callback
 * used by:
 * - cmd_id
 * - cmd_random
 */

int	cmd_data_cb(void *tab, int argc, char **argv, char **col_name)
{
	char	*format;
	char	**source;

	col_name = NULL;
	if (argc != 1)
		return 0;

	source = tab;
	if (source[NICK] == NULL)
		source[NICK] = "";

	/* if the quote begins with £, then it's an action (/me) */
	if (argv[0] && argv[0][0] == '+') {
		argv[0]++;
		/* Raw IRC !*/
		format = "\1%PRIVMSG %s%s :\1ACTION %s\1\n";
		source[NICK] = "";
	}
	else {
		format = "%s:%s:%s\n";
	}

	/* output stuff */
	evbuffer_add_printf(out, format, \
			    source[NICK], source[CHANNEL], argv[0] ? argv[0] : "n/a");
	cmd_toggle_write(out, source[NICK], source[CHANNEL]);
	return 0;
}

/*
 * helper function
 * fill strip nick from nick!user@host and duplicate it on a new allocated string
 * return a pointer to the allocated string on success
 * return NULL on error
 */

static char	*cmd_dup_nick(char *str)
{
	char	*nick;
	size_t	len;

	if (str == NULL)
		return NULL;
	for (len = 0; (str[len] != '!') && (str[len] != '\0'); len++);
	nick = malloc((len + 1) * sizeof(char));
	if (nick == NULL) {
		log_err("[c] malloc() error: %s\n", strerror(errno));
		return NULL;
	}
	strncpy(nick, str, len);
	nick[len] = '\0';
	return nick;
}

/* help command */
void	cmd_help(struct evbuffer *out, char *nick, char *channel, char *args)
{
	args = NULL;

	evbuffer_add_printf(out, "%s:%s:add, count, del, id, info, " \
				 "interval, like, help, memory, random, " \
				 "search, status\n", nick, channel);
	cmd_toggle_write(out, nick, channel);
}

/*
 * Display info for a specific rowid:
 * - creation time
 * - author
 * - origin channel
 */

void	cmd_info(struct evbuffer *out, char *nick, char *channel, char *args)
{
	char	*sql_error;

	if (args == NULL || strlen(args) == 0) {
		evbuffer_add_printf(out, "%s:%s:%s info <something>\n", \
				    nick, channel, g_name);
		cmd_toggle_write(out, nick, channel);
		return ;
	}
	if ((sql_error = db_info(nick, channel, args)) != NULL) {
		evbuffer_add_printf(out, "%s:%s:error: %s\n", \
				    nick, channel, sql_error);
		sqlite3_free(sql_error);
		cmd_toggle_write(out, nick, channel);
	}
}

int	cmd_info_cb(void *tab, int argc, char **argv, char **col_name)
{
	char	*nick;
	char	**source;

	col_name = NULL;
	source = tab;

	/* nothing found or database b0rked */
	if (argc < 4) {
		evbuffer_add_printf(out, "%s:%s:nothing found\n", \
				    source[NICK], source[CHANNEL]);
		return 0;
	}

	/* strip nick */
	nick = cmd_dup_nick(argv[2]);

	/* output stuff */
	evbuffer_add_printf(out, "%s:%s:quote id %s added on %s by %s, at %s\n", \
			    source[NICK], source[CHANNEL], argv[0] ? argv[0] : "n/a", \
			    argv[3] ? argv[3] : "n/a", nick ? nick : "n/a", \
			    argv[1] ? argv[1] : "n/a");
	if (nick != NULL)
		free(nick);
	cmd_toggle_write(out, source[NICK], source[CHANNEL]);
	return 0;
}

/* display a quote by id */
void	cmd_id(struct evbuffer *out, char *nick, char *channel, char *args)
{
	char	*sql_error;

	if (args == NULL || strlen(args) == 0) {
		evbuffer_add_printf(out, "%s:%s:%s id <id>\n", \
				    nick, channel, g_name);
		cmd_toggle_write(out, nick, channel);
		return ;
	}
	if ((sql_error = db_id(nick, channel, args)) != NULL) {
		evbuffer_add_printf(out, "%s:%s:error: %s\n", \
				    nick, channel, sql_error);
		sqlite3_free(sql_error);
		cmd_toggle_write(out, nick, channel);
	}
}

/* display "randstuff" interval for the specified channel */
void	cmd_interval(struct evbuffer *out, char *nick, char *channel, char *args)
{
	char	*sql_error;

	args = NULL;
	if ((sql_error = db_interval(nick, channel, channel)) != NULL) {
		evbuffer_add_printf(out, "%s:%s:error: %s\n", \
				    nick, channel, sql_error);
		sqlite3_free(sql_error);
		cmd_toggle_write(out, nick, channel);
	}
}

int	cmd_interval_cb(void *tab, int argc, char **argv, char **col_name)
{
	char	**source;

	col_name = NULL;
	if (argc < 2)
		return 0;

	source = tab;
	if (argv[1] != NULL)
		evbuffer_add_printf(out, "%s:%s:interval for this channel is " \
				    "%s second(s)\n", source[NICK], source[CHANNEL], \
				    argv[1] ? argv[1] : "n/a");
	else
		evbuffer_add_printf(out, "%s:%s:no interval is set for this channel\n", \
				    source[NICK], source[CHANNEL]);
	cmd_toggle_write(out, source[NICK], source[CHANNEL]);
	return 0;
}

/* search function (using SQL LIKE) */
void	cmd_like(struct evbuffer *out, char *nick, char *channel, char *args)
{
	char	*sql_error;

	if (args == NULL || strlen(args) == 0) {
		evbuffer_add_printf(out, "%s:%s:%s like <something>\n", \
				    nick, channel, g_name);
		cmd_toggle_write(out, nick, channel);
		return ;
	}
	if ((sql_error = db_like(nick, channel, args)) != NULL) {
		evbuffer_add_printf(out, "%s:%s:error: %s\n", \
				    nick, channel, sql_error);
		sqlite3_free(sql_error);
		cmd_toggle_write(out, nick, channel);
	}
}


/* display how much memory is using sqlite */
void	cmd_memory(struct evbuffer *out, char *nick, char *channel, char *args)
{
	char	*u = NULL;

	args = NULL;
	evbuffer_add_printf(out, "%s:%s:sql cur/max: %hi %s/%hi %s\n", \
			    nick, channel, \
			    db_get_memory_used(&u), u, db_get_memory_highwater(&u), u);
	cmd_toggle_write(out, nick, channel);
}

/*
 * Parsing function
 * input form:
 * "nick:channel:command args for this one"
 * both nick and channel can be null, BUT not both at one.
 */

void	cmd_parse(struct evbuffer *out, struct evbuffer *in)
{
	unsigned short	check;
	unsigned int	i;
	char		*tmp;
	char		*nick;
	char		*channel = NULL;
	char		*cmd = NULL;
	char		*args = NULL;

	if ((tmp = evbuffer_readline(in)) == NULL)
                return;
parseline:
	/* custom strtok */
	nick = tmp;
	for (check = 0, i = 0; (check < 2) && (tmp[i] != '\0'); i++) {
		if (tmp[i] == ':') {
			check++;
			tmp[i] = '\0';
			if (check == 1)
				channel = tmp + i + 1;
			else
				cmd = tmp + i + 1;
		}
	}

	if (check < 2 || cmd[0] == '\0' || \
	    (strlen(nick) == 0 && strlen(channel) == 0)) {
		log_err("[i] got crap from bot\n");
		goto next;
	}

	/* remove front spaces */
	for (; *cmd == ' '; cmd++);

	/* skip 'argv[0]' (quotes) */
	for (i = 0; (*cmd != ' ') && (*cmd != '\0'); cmd++);

	/* remove spaces (again) */
	for (; *cmd == ' '; cmd++);

	/* split command ans args */
	for (i = 0; cmd[i] != '\0'; i++) {
		if (cmd[i] == ' ') {
			cmd[i] = '\0';
			args = cmd + i + 1;
			break ;
		}
	}

	cmd_switch(out, nick, channel, cmd, args);

next:
	free(tmp);

	/* check for another line */
	if (EVBUFFER_LENGTH(in) > 0) {
		if ((tmp = evbuffer_readline(in)) == NULL)
			return;
		goto parseline;
	}
}

/* pint a random quote */
void	cmd_random(struct evbuffer *out, char *nick, char *channel, char *args)
{
	char	*sql_error;

	if (args == NULL || strlen(args) == 0)
		args = "*";
	if ((sql_error = db_random(nick, channel, args)) != NULL) {
		evbuffer_add_printf(out, "%s:%s:error: %s\n", \
				    nick, channel, sql_error);
		sqlite3_free(sql_error);
		cmd_toggle_write(out, nick, channel);
	}
}

void	cmd_status(struct evbuffer *out, char *nick, char *channel, char *args)
{
	args = NULL;

	evbuffer_add_printf(out, "%s:%s:iyell/quotes: " VERSION ", "\
			    "sqlite: %s, database: %s\n", \
			    nick, channel, sqlite3_libversion(), \
			    basename(g_database));
	cmd_toggle_write(out, nick, channel);
}

/* add stuff */
void	cmd_search(struct evbuffer *out, char *nick, char *channel, char *args)
{
	char	*sql_error;

	if (args == NULL || strlen(args) == 0) {
		evbuffer_add_printf(out, "%s:%s:%s search <something>\n", \
				    nick, channel, g_name);
		cmd_toggle_write(out, nick, channel);
		return ;
	}
	if ((sql_error = db_search(nick, channel, args)) != NULL) {
		evbuffer_add_printf(out, "%s:%s:error: %s\n", \
				    nick, channel, sql_error);
		sqlite3_free(sql_error);
		cmd_toggle_write(out, nick, channel);
	}
}

int	cmd_search_cb(void *tab, int argc, char **argv, char **col_name)
{
	char	**source;
	long	count = 0;

	col_name = NULL;
	source = tab;

	/* nothing found or database b0rked */
	if (argc != 3) {
		log_err("[c] got garbage from db\n");
		return 0;
	}

	/* convert count from str to int */
	count = strtol(argv[2], (char **)NULL, 10);

	/* output stuff */
	if (count == 0) {
		evbuffer_add_printf(out, "%s:%s:nothing found\n", \
				    source[NICK], source[CHANNEL]);
	}
	else if (count == 1) {
		evbuffer_add_printf(out, "%s:%s:%s id: %s\n", \
			    source[NICK], source[CHANNEL], argv[0] ? argv[0] : "n/a", \
			    argv[1] ? argv[1] : "n/a");
	}
	else {
		evbuffer_add_printf(out, "%s:%s:%s id: %s, %s results\n", \
			    source[NICK], source[CHANNEL], argv[0] ? argv[0] : "n/a", \
			    argv[1] ? argv[1] : "n/a", argv[2]);
	}
	cmd_toggle_write(out, source[NICK], source[CHANNEL]);
	return 0;
}

/*
 * command witching stuff
 * a simple function pointer table parser.
 */

void	cmd_switch(struct evbuffer *out, char *nick, char *channel, char *cmd, char *args)
{
	unsigned short	s;

	/* snity checks */
	assert(nick != NULL);
	assert(channel != NULL);
	assert(cmd != NULL);

	struct cmd_quote_s	cmd_tab[] = {
		{"add", cmd_add},
		{"count", cmd_count},
		{"del", cmd_del},
		{"id", cmd_id},
		{"info", cmd_info},
		{"like", cmd_like},
		{"interval", cmd_interval},
		{"help", cmd_help},
		{"memory", cmd_memory},
		{"random", cmd_random},
		{"search", cmd_search},
		{"status", cmd_status},
		{0, 0}
	};

	/* hook called without command (ex: iyell: quote) */
	if (strlen(cmd) == 0)
		return ;

	for (s = 0; cmd_tab[s].cmd != NULL; s++) {
		if (strcmp(cmd_tab[s].cmd, cmd) == 0) {
			cmd_tab[s].fp(out, nick, channel, args);
			return ;
		}
	}
}

static void cmd_toggle_write(struct evbuffer *out, char *nick, char *channel)
{
	if (g_mode & STRIP)
		io_strip_nick(out, nick, channel);
	io_toggle_write();
}

