/*
 *  rfc3164.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 07/04/2009.
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
#define _DARWIN_C_SOURCE
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#ifndef HAVE_ASPRINTF
#include "asprintf.h"
#endif
#include "irc.h"
#include "main.h"

/* arrays */

const char	*facilities[] = {
	"kern",
	"user",
	"mail",
	"daemon",
	"auth",
	"syslog",
	"lpr",
	"news",
	"uucp",
	"cron",
	"auth",
	"ftp",
	"ntp",
	"authpriv",
	"authpriv",
	"cron",
	"local0",
	"local1",
	"local2",
	"local3",
	"local4",
	"local5",
	"local6",
	"local7"
};

const char	*severities[] = {
	"emerg",
	"alert",
	"crit",
	"err",
	"warning",
	"notice",
	"info",
	"debug"
};

/* defines */
#define RFC3164_MAX	1024
#define RFC3164_TAG	32
#define RFC3164_PRI_TXT	16

/* Colors for syslog severities */

const char	*c_severities[] = {
	"5",	/* emerg */
	"5",	/* alert */
	"4",	/* crit */
	"7",	/* err */
	"8",	/* warning */
	"9",	/* notice */
	"11",	/* info */
	"15"	/* debug */
};

/* syslog SCANF */
/* <123>su: pouet */
#define	RFC3164_SCANF_SHORT	"<%hu>%32[^:]: %1024[^\n]"
/* <13>May 11 12:29:48 itoto: test */
#define	RFC3164_SCANF_DATE	"<%hu>%*s %*[0-9] %*[0-9:] %32[^:]: %1024[^\n]"
/* <13>Feb  5 17:32:18 10.0.0.9 su: dklsfksdlmfsdlkfsdlmds */
#define	RFC3164_SCANF_HEADER	"<%hu>%*s %*[0-9] %*[0-9:] %[^ :] %32[^:]: %1024[^\n]"

/* prototypes */
static short	sl_find_target_channels(struct evbuffer *out, unsigned short facility, \
				        unsigned short severity, char *tag, char *msg);
static short	sl_find_target_bots(unsigned short facility, unsigned short severity, \
				    char *tag, char *msg);
void		sl_forward(struct evbuffer *out, unsigned short pri, char *tag, char *msg);
static void	sl_forward_with_host(struct evbuffer *out, unsigned short pri, \
				     char *tag, char *msg, char *host);

/* find a target bot, build a classic iyell message and send it */
static short	sl_find_target_bots(unsigned short facility, unsigned short severity, \
				    char *tag, char *msg)
{
	char		**tmp;
	char		*data;
	char		search[RFC3164_PRI_TXT + 1];
	char		type[RFC3164_PRI_TXT + 1];
	unsigned int	i;
	unsigned short	found;

	data = NULL;
	found = 0;
	sprintf(type, "%s.%s", facilities[facility], severities[severity]);
	if (g_mode & COLOR)
		asprintf(&data, "\3%s\2[\2\3%s\3%s\2]\2\3 %s", \
			 c_severities[severity], tag, c_severities[severity], msg);
	else
		asprintf(&data, "[%s] %s", tag, msg);

	if (data == NULL) {
		log_err("[r] asprintf() error: %s\n", strerror(errno));
		return 0;
	}

	/* everything */
	tmp = hash_get(g_conf.transmit, "*.*");
	if (tmp != NULL) {
		found = 1;

		for (i = 0; tmp[i] != NULL; i++) {
			info_transmit(tmp[i], type, data);
		}
	}

	/* all severities */
	sprintf(search, "%s.*", facilities[facility]);
	tmp = hash_get(g_conf.transmit, search);
	if (tmp != NULL) {
		found = 1;

		for (i = 0; tmp[i] != NULL; i++) {
			info_transmit(tmp[i], type, data);
		}
	}

	/* all facilities */
	sprintf(search, "*.%s", severities[severity]);
	tmp = hash_get(g_conf.transmit, search);
	if (tmp != NULL) {
		found = 1;

		for (i = 0; tmp[i] != NULL; i++) {
			info_transmit(tmp[i], type, data);
		}
	}

	/* this facility and this severity. I mean, THIS! */
	sprintf(search, "%s.%s", facilities[facility], severities[severity]);
	tmp = hash_get(g_conf.transmit, search);
	if (tmp != NULL) {
		found = 1;

		for (i = 0; tmp[i] != NULL; i++) {
			info_transmit(tmp[i], type, data);
		}
	}

	/* cleanup and exit */
	free(data);
	if (! found)
		return -1;

	if (g_mode & VERBOSE)
		log_msg("[r] forwarded message with PRI %s.%s TAG %s to bot(s)\n", 
			facilities[facility], severities[severity], tag);
	return 0;
		return -1;
}

/* find a target channel, build a message and forward it */
static short	sl_find_target_channels(struct evbuffer *out, unsigned short facility, \
					unsigned short severity, char *tag, char *msg)
{
	char		**tmp;
	char		*data;
	char		search[RFC3164_PRI_TXT + 1];
	unsigned int	i;
	unsigned short	found;

	data = NULL;
	found = 0;
	if (g_mode & COLOR)
		asprintf(&data, "\3%s\2[\2\3%s\3%s\2]\2\3 %s", \
			 c_severities[severity], tag, c_severities[severity], msg);
	else
		asprintf(&data, "[%s] %s", tag, msg);

	if (data == NULL) {
		log_err("[r] asprintf() error: %s\n", strerror(errno));
		return 0;
	}

	/* everything */
	tmp = hash_get(g_conf.forward, "*.*");
	if (tmp != NULL) {
		found = 1;

		for (i = 0; tmp[i] != NULL; i++) {
			irc_cmd_privmsg(out, tmp[i], data);
		}
	}

	/* all severities */
	sprintf(search, "%s.*", facilities[facility]);
	tmp = hash_get(g_conf.forward, search);
	if (tmp != NULL) {
		found = 1;

		for (i = 0; tmp[i] != NULL; i++) {
			irc_cmd_privmsg(out, tmp[i], data);
		}
	}

	/* all facilities */
	sprintf(search, "*.%s", severities[severity]);
	tmp = hash_get(g_conf.forward, search);
	if (tmp != NULL) {
		found = 1;

		for (i = 0; tmp[i] != NULL; i++) {
			irc_cmd_privmsg(out, tmp[i], data);
		}
	}

	/* this facility and this severity. I mean, THIS! */
	sprintf(search, "%s.%s", facilities[facility], severities[severity]);
	tmp = hash_get(g_conf.forward, search);
	if (tmp != NULL) {
		found = 1;

		for (i = 0; tmp[i] != NULL; i++) {
			irc_cmd_privmsg(out, tmp[i], data);
		}
	}

	/* cleanup and exit */
	free(data);
	if (! found)
		return -1;

	if (g_mode & VERBOSE)
		log_msg("[r] forwarded message with PRI %s.%s TAG %s to channel(s)\n", 
			facilities[facility], severities[severity], tag);
	return 0;
}

/*
 * forwad function
 * try to find valid targets (channels or others bots), for the specified 
 * syslog message.
 * will use sl_wind_*
 */

void	sl_forward(struct evbuffer *out, unsigned short pri, char *tag, char *msg)
{
	unsigned short	facility, severity;
	short		ret;
	char		*tmp;

	assert(msg != NULL);

	ret = 0;
	severity = pri % 8;
	facility = pri / 8;

	if (severity > 7 || facility > 23) {
		log_err("[r] incorrect PRI %hu (facility %hu, severity %hu)\n",
			pri, facility, severity);
		return ;
	}

	/* cleanup tag (ie: /usr/sbin/cron[31501] => cron) */
	tmp = strchr(tag, '[');
	if (tmp != NULL)
		*tmp = '\0';

	if (tag[0] == '/') {
		tmp = basename(tag);
		if (tmp == NULL) {
			log_err("[r] basename() error: %s\n", strerror(errno));
			return ;
		}
		strncpy(tag, tmp, strlen(tag));
	}

        /* bot or target */
	ret += sl_find_target_channels(out, facility, severity, tag, msg);
	ret += sl_find_target_bots(facility, severity, tag, msg);
	if (ret == -2 && ! (g_mode & SL3164_SILENT))
		log_err("[r] no settings found for PRI %hu (%s.%s)\n",
			pri, facilities[facility], severities[severity]);
}

/*
 * same as above, but replaces tag by tag@host
 */

static void	sl_forward_with_host(struct evbuffer *out, unsigned short pri, \
				     char *tag, char *msg, char *host)
{
	size_t	len;
	char	*tmp;

	/* cleanup a bit */
	tmp = strchr(tag, '[');
	if (tmp != NULL)
		*tmp = '\0';
	
	/* compute len and allocate C99 VLA */
	len = strlen(tag) + strlen(host) + 2;	/* 2 => '.' + '\0' */
	if(len > RFC3164_MAX) {
		log_err("[r] syslog message too long\n");
		return ;
	}

	char	tag_wth_host[len];
	sprintf(tag_wth_host, "%s@%s", tag, host);
	sl_forward(out, pri, tag_wth_host, msg);
}

/*
 * syslog main parse function
 */

void	sl_parse(struct evbuffer *out, struct evbuffer *in)
{
	char		host[RFC3164_MAX + 1];
	char		tag[RFC3164_TAG + 1];
	char		msg[RFC3164_MAX + 1];
	unsigned short	pri, s;
	char		*line, *header;
	const char	*month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", \
				    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	size_t		len;

	/* if the bot is not connected, drop messages */
	if (! (g_mode & CONNECTED)) {
		log_msg("[r] bot not connected. ignoring message\n");
		evbuffer_drain(in, EVBUFFER_LENGTH(in));
		return ;
	}

	/* grab buffer content */
	assert(EVBUFFER_LENGTH(in) > 0);
	len = EVBUFFER_LENGTH(in);
	line = malloc((EVBUFFER_LENGTH(in) + 1) * sizeof(char));
	if (line == NULL) {
		log_err("[r] malloc() error: %s\n", strerror(errno));
		goto error;
	}
	memcpy(line, EVBUFFER_DATA(in), len);
	evbuffer_drain(in, EVBUFFER_LENGTH(in));
	line[len] = '\0';

	/* chop */
	if (line[len - 1] == '\n')
		line[len - 1] = '\0';

	/* useful for debug */
	if (g_mode & DEBUG) {
		printf("*<* %s\n", line);
	}

	/* determine if header or not */
	header = NULL;
	for (s = 0; (s < 5) && (line[s] != '\0'); s++) {
		if (line[s] == '>') {
			header = line + ((s + 1) * sizeof(char));
			break ;
		}
	}

	if (line[s] == '\0') {
		log_err("[r] bad syslog packet, dropping\n");
		goto next;
	}
	if (header != NULL && header[0] == '\0') {
		log_err("[r] bad syslog packet, dropping\n");
		goto next;
	}

	/* check if the TIMESTAMP looks valid to rfc3164 */
	if (header != NULL) {
		for (s = 0; s < 12; s++) {
			if (strncmp(month[s], header, 3) == 0)
				break ;
		}
	
		/* nothing found */
		if (s == 12)
			header = NULL;
	}

	if (header != NULL) {
		/* header exists */
		if (sscanf(line, RFC3164_SCANF_HEADER, &pri, host, tag, msg) == 4) {
			if (hash_text_is_true(g_conf.global, "syslog_show_host"))
				sl_forward_with_host(out, pri, tag, msg, host);
			else
				sl_forward(out, pri, tag, msg);	
		}
		/* if it fails, try with a date only header */
		else if (sscanf(line, RFC3164_SCANF_DATE, &pri, tag, msg) == 3) {
			sl_forward(out, pri, tag, msg);
		}
		else {
			log_err("[r] bad syslog packet, dropping\n");
		}
	}
	else {
		/* header does not exists */
		if (sscanf(line, RFC3164_SCANF_SHORT, &pri, tag, msg) == 3) {
			sl_forward(out, pri, tag, msg);			
		}
		else {
			log_err("[r] bad syslog packet, dropping\n"); 
		}
	}

next:
	free(line);
	return ;

error:
	log_err("[r] bad syslog packet, dropping\n");
	evbuffer_drain(in, EVBUFFER_LENGTH(in));
	return ;
}

