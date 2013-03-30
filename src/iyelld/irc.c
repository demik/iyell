/*
 *  irc.c
 *  iyell 
 *
 *  Created by Michel DEPEIGE on 12/10/07.
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
#define _DARWIN_C_SOURCE
#include <assert.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#ifndef HAVE_ASPRINTF
#include "asprintf.h"
#endif
#include "main.h"
#include "cmd.h"
#include "ctcp.h"
#include "heap.h"
#include "irc.h"
#include "misc.h"
#include "sched.h"
#include "stats.h"
#include "tools.h"

/* static stuff */
static inline void	irc_code_split(char *str, char **who, char **chan);
static inline void	irc_msg_split(char *str, char **who, char **chan);

/* real stuff */
void	irc_parse(struct evbuffer *out, struct evbuffer *in)
{
	char		*tmp;
	size_t		len = 0;
	char		*ap;
	unsigned int	i = 0;
	
	/* table of functions pointers */
	struct s_message irc_messages[] = {
		{"PING", irc_cmd_pong},
		{0, 0}
	};
	
        if ((tmp = evbuffer_readline(in)) == NULL)
                return;
	
parseline:
	STATS_INC_LINES(x);
	len = strlen(tmp);

	/* display to sdtout */
	tmp[len] = '\0';
	if (g_mode & DEBUG)
		printf("<<< %s\n", tmp);
	
	/* if the line begin by a ":", then it's not a command */
	if (tmp[0] == ':') {
		/* check for CTCP */
		ap = memchr(tmp, 0x01, len);
		if (ap != NULL)
			ctcp_parse(out, tmp + 1);
		else
			irc_switch(out, tmp + 1);
		free(tmp);
	}
	else {
		/* find if something is interesting and call the function if needed */
		ap = strsep(&tmp, " :");
		if (ap != NULL) {
			for (i = 0; irc_messages[i].cmd != NULL; i++) {
				if (strcmp(irc_messages[i].cmd, ap) == 0) {
					irc_messages[i].fp(out, tmp);
					break;
				}
			}
			free(ap);
		}
	}
	
	/* check for another line */
	if (EVBUFFER_LENGTH(in) > 0) {
		if ((tmp = evbuffer_readline(in)) == NULL)
			return;
		goto parseline;
	}
}

/*
 * This function search for specific codes and messages
 * and lauch appropriate actions
 * return 1 if something found
 * return 0 if nothing found
 */

/*
 * how parsing works:
 *
 * input (*str) :iYell!iyell@blackbox.lostwave.net JOIN :#test
 * first step : tmp = str
 * second step : ap = iYell!iyell@blackbox.lostwave.net tmp = JOIN :#test
 * third step : ap = iYell!iyell@blackbox.lostwave.net, code = JOIN, tmp = :#test
 */

void	irc_switch(struct evbuffer *out, char *str)
{
	char		*tmp;
	char		*ap;
	char		*code;
	unsigned int	i = 0;
	
	/* table of functions pointers (codes) */
	struct s_message irc_codes[] = {
		{RPL_ENDOFMOTD, irc_send_join},
		{ERR_NICKNAMEINUSE, change_nick},
		{0, 0}
	};
	
	/* table of functions pointers (messages) */
	struct s_message irc_messages[] = {
		{"INVITE", irc_check_invite},
		{"KICK", irc_check_kick},
		{RPL_ENDOFNAMES, irc_send_sync},
		{ERR_BADCHANNELKEY, irc_wait_key},
		{ERR_BANNEDFROMCHAN, irc_got_banned},
		{ERR_INVITEONLYCHAN, irc_wait_invite},
		{0, 0}
	};

	tmp = str;
	ap = strsep(&tmp, " :");
	if (ap != NULL) {
		code = strsep(&tmp, " :");
		if (code != NULL) {

			/* check for "talk", on line commands */
			if (strcmp(code, "PRIVMSG") == 0) {
				cmd_check_irc_msg(out, ap, tmp);
				return ;
			}

			/* check for codes */
			for (i = 0; irc_codes[i].cmd != NULL; i++) {
				if (strcmp(irc_codes[i].cmd, code) == 0) {
					irc_codes[i].fp(out, NULL);
					return ;
				}
			}
			
			/* check for messages */
			for (i = 0; irc_messages[i].cmd != NULL; i++) {
				if (strcmp(irc_messages[i].cmd, code) == 0) {
					irc_messages[i].fp(out, tmp);
					return ;
				}
			}
		}
	}
}

/*
 * extract nick and channel from str
 */

static inline void irc_code_split(char *str, char **who, char **chan)
{	
	/* determine the channel */
	*who = strsep(&str, " ");
	if (*who == NULL) {
		log_err("[i] got garbage from server\n");
		return ;
	}
	*chan = strsep(&str, " :\r\n");
	if (*chan == NULL) {
		log_err("[i] got garbage from server\n");
		return ;
	}
}

static	inline void irc_msg_split(char *str, char **who, char **chan)
{
	*chan = strsep(&str, " ");
	if (*chan == NULL) {
		log_err("[i] got garbage from server\n");
		return ;
	}
	*who = strsep(&str, " :\r\n");
	if (*chan == NULL) {
		log_err("[i] got garbage from server\n");
		return ;
	}
}

/*
 * print something if invited needed
 */

void	irc_wait_invite(struct evbuffer *out, char *str)
{
	char	*chan = NULL, *who = NULL;
	
	out = NULL;

	irc_code_split(str, &who, &chan);
	log_msg("[i] I need an invite for %s\n", chan);
}

/*
 * print something if I need a key, or I have a bad key.
 */

void	irc_wait_key(struct evbuffer *out, char *str)
{
	char	*chan = NULL, *who = NULL;

	out = NULL;

	g_mode |= KEY;
	irc_code_split(str, &who, &chan);
	log_msg("[i] I need a key for %s\n", chan);
}

/*
 *i check if the specified channel is know, rejoin if needed
 */

void	irc_check_invite(struct evbuffer *out, char *str)
{
	char		*chan, **tmp, *who;
	size_t		len;
	unsigned int	i;

	tmp = hash_get(g_conf.global, "channels");
	if (tmp == NULL)
		return ;

	who = strsep(&str, " ");
	if (who == NULL) {
		log_err("[i] got garbage from server\n");
		return ;
	}
	strsep(&str, ":\r\n");
	if (str== NULL) {
		log_err("[i] got garbage from server\n");
		return ;
	}

	chan = str;
	len = strlen(chan);
	/* determine the channel */
	for (i = 0; tmp[i] != NULL; i++) {
		if (strcmp(tmp[i], chan) == 0 || ((strlen(tmp[i]) > len) && \
		    (tmp[i][len] == ' ') && (strncmp(tmp[i], chan, len) == 0))) {
			irc_cmd_join(out, tmp[i]);
			break;
		}
	}
}

/*
 * check if we got kicked, rejoin if needed
 */

void	irc_check_kick(struct evbuffer *out, char *str)
{
	char		*chan = NULL;
	char		*who = NULL;
	char		*me;
	char		**list;
	unsigned int	i;
	size_t		len;

	/* won't work in NICKUSED mode */
	if (g_mode & NICKUSED)
		return ;

        me = hash_text_get_first(g_conf.global, "nick");
	list = hash_get(g_conf.global, "channels");
	irc_msg_split(str, &who, &chan);
	len = strlen(chan);

	/* check if we have been kicked */
	if (strcmp(who, me) == 0) {
		log_msg("[i] got kicked from %s, rejoining\n", chan);
		for (i = 0; list[i] != NULL; i++) {
			if ((strcmp(list[i], chan) == 0) || (strlen(list[i]) > len \
			    && strncmp(list[i], chan, len) == 0 && list[i][len] == ' '))
				irc_cmd_join(out, list[i]);
				break ;
		}
	}
}

/*
 * switch the bot to banned mode
 */

void	irc_got_banned(struct evbuffer *out, char *str)
{
	char	*chan = NULL;
	char	*who = NULL;
	
	out = NULL;
	
	irc_code_split(str, &who, &chan);
	g_mode |= BANNED;
	log_msg("[i] I'm banned from %s :(\n", chan);

	schedule_rejoin_after_ban(180);
}

/*
 * NICK <nick>
 * this command send the nick command in the output buffer
 * use the default nick if no one is specified
 */

void	irc_cmd_nick(struct evbuffer *out, char *nick)
{
	char	*tmp = NULL;
	
	/* build the command */
	evbuffer_add(out, "NICK ", 5);
	if (nick == NULL) {
		tmp = hash_text_get_first(g_conf.global, "nick");
		evbuffer_add(out, tmp, strlen(tmp));
	}
	else {
		evbuffer_add(out, nick, strlen(nick));		
	}
	evbuffer_add(out, "\n", 1);
}

/*
 * This function send a notice
 * NOTICE <dest> <message>
 */

void	irc_cmd_notice(struct evbuffer *out, char *dest, char *message)
{
	evbuffer_add(out, "NOTICE ", 7);
	evbuffer_add(out, dest, strlen(dest));
	evbuffer_add(out, " :", 2);
	evbuffer_add(out, message, strlen(message));
	evbuffer_add(out, "\n", 1);
}

/*
 * This function  leave a channel
 * PART <channel> [channel2]
 */

void	irc_cmd_part(struct evbuffer *out, char *channel)
{
	evbuffer_add(out, "PART ", 5);
	log_msg("[i] leaving %s\n", channel);
	evbuffer_add(out, channel, strlen(channel));
	evbuffer_add(out, "\n", 1);
}

/*
 * this function send a ping request
 * PING <server1> [server2]
 */

void	irc_cmd_ping(struct evbuffer *out, char *target)
{
	evbuffer_add(out, "PING ", 5);
	evbuffer_add(out, target, strlen(target));
	evbuffer_add(out, "\n", 1);
}

/*
 * Send the password to the IRC server
 * PASS <pass>
 */

void	irc_cmd_pass(struct evbuffer *out)
{
	char	*tmp = NULL;
	
	/* build the command */
	tmp = hash_text_get_first(g_conf.global, "pass");
	if (tmp == NULL)
		return ;
	evbuffer_add(out, "PASS ", 5);
	evbuffer_add(out, tmp, strlen(tmp));
	evbuffer_add(out, "\n", 1);
}

/*
 * This function reply to a ping message
 * PONG <server> [server2]
 */

void	irc_cmd_pong(struct evbuffer *out, char *str)
{
	evbuffer_add(out, "PONG ", 5);
	if (*str == ':')
		str++;
	evbuffer_add(out, str, strlen(str));
	evbuffer_add(out, "\n", 1);
}

/*
 * This function send a private message
 * PRIVMSG <dest> <message>
 */

void	irc_cmd_privmsg(struct evbuffer *out, char *dest, char *message)
{
	assert(dest != NULL);
	assert(message != NULL);
	assert(out != NULL);

	evbuffer_add(out, "PRIVMSG ", 8);
	evbuffer_add(out, dest, strlen(dest));
	evbuffer_add(out, " :", 2);
	evbuffer_add(out, message, strlen(message));
	evbuffer_add(out, "\n", 1);
}

/*
 * This fonction send the QUIT command to the server
 * QUIT [message]
 */

void	irc_cmd_quit(struct evbuffer *out, char *message)
{
	char	*tmp;

	evbuffer_add(out, "QUIT", 4);
	if (message != NULL) {
		evbuffer_add(out, " :", 2);
		evbuffer_add(out, message, strlen(message));
	}
	else if ((tmp = hash_text_get_first(g_conf.global, "quit_message")) != NULL) {
		evbuffer_add(out, " :", 2);
		evbuffer_add(out, tmp, strlen(tmp));
	}
	evbuffer_add(out, "\n", 1);
}

/*
 * USER <user>
 * this command send the USER command in the output buffer
 * use the default user if no one is specified
 * use the default real name if no one found in configuration
 */

void	irc_cmd_user(struct evbuffer *out, char *user)
{
	char	*tmp = NULL;
	
	/* build the command */
	evbuffer_add(out, "USER ", 5);
	if (user == NULL) {
		
		/* check for something in the configuration or use default name */
		tmp = hash_text_get_first(g_conf.global, "username");
		if (tmp == NULL) {
			evbuffer_add(out, "iyell", 5);
		}
		else {
			evbuffer_add(out, tmp, strlen(tmp));
		}
	}
	else {
		evbuffer_add(out, user, strlen(user));
	}
	
	/* check for realname */
	tmp = hash_text_get_first(g_conf.global, "realname");
	if (tmp == NULL) {
		evbuffer_add(out, " 0 * :iyell\n", 12);
	}
	else {
		evbuffer_add(out, " 0 * :", 6);
		evbuffer_add(out, tmp, strlen(tmp));
		evbuffer_add(out, "\n", 1);
	}
}

/*
 * JOIN <channel> <key>, ...
 * this command send the JOIN command in the output buffer
 */

void	irc_cmd_join(struct evbuffer *out, char *channel)
{
	char	**tmp = NULL;
	int	i = 0;
	
	if (channel == NULL) {
		/* check if we have a channel to join */
		tmp = hash_get(g_conf.global, "channels");
		if (tmp == NULL || tmp[0] == NULL)
			return;
		
		/* build the command */
		evbuffer_add(out, "JOIN ", 5);
		while (tmp[i] != NULL) {
			evbuffer_add(out, tmp[i], strlen(tmp[i]));
			if (tmp[i + 1] != NULL)
				evbuffer_add(out, ",", 1);
			log_msg("[i] entering %s\n", tmp[i]);
			i++;
		}
	}
	else {
		evbuffer_add(out, "JOIN ", 5);
		log_msg("[i] entering %s\n", channel);
		evbuffer_add(out, channel, strlen(channel));
	}
	evbuffer_add(out, "\n", 1);
}

/*
 * Same as previous but mark the bot as connected
 */

void	irc_send_join(struct evbuffer *out, char *channel)
{
	channel = NULL;
	g_mode |= CONNECTED;
	irc_cmd_join(out, NULL);
}

/*
 * Send NICK + USER command.
 * Send also PERFORM.
 */

void	irc_send_helo(struct evbuffer *out)
{
	char		**tmp;
	unsigned int	i;

	irc_cmd_pass(out);
	irc_cmd_nick(out, NULL);
	irc_cmd_user(out, NULL);
	
	tmp = hash_get(g_conf.global, "perform");
	if (tmp != NULL) {
		for (i = 0; tmp[i] != NULL; i++) {
			irc_send_raw(out, tmp[i]);
		}
	}
}

/*
 * Send raw messages to the IRC server
 * basically a wrapper around the buffer append function
 */

void	irc_send_raw(struct evbuffer *out, char *str)
{
	evbuffer_add(out, str, strlen(str));
	evbuffer_add(out, "\n", 1);
}

/*
 * same but wrapper around evbuffer_add_vprintf
 */

void	irc_send_raw_vprintf(struct evbuffer *out, char *fmt, ...)
{
	va_list s;

	va_start(s, fmt);
	evbuffer_add_vprintf(out, fmt, s);
	va_end(s);
	evbuffer_add(out, "\n", 1);
}

/*
 * Send sync message to the channel
 */

void	irc_send_sync(struct evbuffer *out, char *str)
{
	char	*chan;
	char	*tmp;

	/* determine the channel */
	tmp = strsep(&str, " ");
	if (tmp == NULL) {
		log_err("[i] got garbage from server\n");
		return ;
	}
	chan = strsep(&str, " :\r\n");
	if (chan == NULL) {
		log_err("[i] got garbage from server\n");
		return ;
	}

	/* display only sync notice if set */
	if (! hash_text_is_true(g_conf.global, "sync_notice"))
		goto message;
	if (asprintf(&tmp, "sync with %s completed", chan) == -1) {
		log_err("[i] asprintf() error: %s\n", strerror(errno));
		goto message;
	}

	irc_cmd_notice(out, chan, tmp);
	free(tmp);

message:
	/* display a sync message if set */
	tmp = hash_text_get_first(g_conf.global, "sync_message");
	if (tmp != NULL)
		irc_cmd_privmsg(out, chan, tmp);

	/* logging */
	if (g_mode & VERBOSE)
		printf("[i] sync with %s completed\n", chan);
}

