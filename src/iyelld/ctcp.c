/*
 *  ctcp.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 19/10/07.
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
#define __BSD_VISIBLE 1
#define _DARWIN_C_SOURCE
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#ifndef HAVE_ASPRINTF
#include "asprintf.h"
#endif
#include "main.h"
#include "ctcp.h"
#include "irc.h"
#include "tools.h"

/* structures */
typedef struct cmd_ctcp_s {
        const char      *cmd;
        void            (*fp)(struct evbuffer *out, char *from, char *args);
}               cmd_ctcp_t;

/* functions */

void	ctcp_parse(struct evbuffer *out, char *line)
{
	char		*from = NULL;
	char		*cmd;
	char		*args;
	char		*tmp;
	unsigned int	i;
	
	/* table of functions pointers */
	struct cmd_ctcp_s ctcp_cmd[] = {
		{"CLIENTINFO", ctcp_cmd_clientinfo},
		{"PING", ctcp_cmd_ping},
		{"TIME", ctcp_cmd_time},
		{"USERINFO", ctcp_cmd_userinfo},
		{"VERSION", ctcp_cmd_version},
		{0, 0}
	};

	/* ignore everything but requests */
	if (strstr(line, "PRIVMSG") == NULL)
		return;
	
	/* searching for CTCP "magic" */
	tmp = line;
	tmp = memchr(line, 0x01, strlen(line));
	if (tmp[1] != '\0')
		tmp++;
	
	/* searching for CTCP command */
	cmd = strsep(&tmp, " \001");
	if (cmd == NULL)
		return ;
	
	/* searching for args */
	args = strsep(&tmp, "\001");
	
	tmp = line;
	/* searching for sender, CTCP type and args */
	from = strsep(&tmp, "!");
	if (from == NULL)
		return ;
	
	/* seems we have everything, search for know command */
	for (i = 0; ctcp_cmd[i].cmd != NULL; i++) {
		if (strcmp(ctcp_cmd[i].cmd, cmd) == 0) {
			ctcp_cmd[i].fp(out, from, args);
			break;
		}
	}
}

/*
 * CTCP reply, similar to irc_cmd_privmsg();
 */

void	ctcp_request(struct evbuffer *out, char *dest, char *message)
{
	evbuffer_add(out, "PRIVMSG ", 8);
	evbuffer_add(out, dest, strlen(dest));
	evbuffer_add(out, " :\x01", 3);
	evbuffer_add(out, message, strlen(message));
	evbuffer_add(out, "\x01\n", 2);	
}

/*
 * CTCP reply, similar to irc_cmd_notice();
 */

void	ctcp_reply(struct evbuffer *out, char *dest, char *message)
{
	evbuffer_add(out, "NOTICE ", 7);
	evbuffer_add(out, dest, strlen(dest));
	evbuffer_add(out, " :\x01", 3);
	evbuffer_add(out, message, strlen(message));
	evbuffer_add(out, "\x01\n", 2);	
}
 
/*
 * CTCP CLIENTINFO
 * reply what we are able to manage
 */

void	ctcp_cmd_clientinfo(struct evbuffer *out, char *from, char *args)
{
	args = NULL;
	if (g_mode & VERBOSE)
		printf("[c] CTCP CTCPINFO from %s\n", from);
	ctcp_reply(out, from, "CTCPINFO PING VERSION TIME USERINFO CLIENTINFO");
}

/*
 * CTCP PING
 * reply PING [arg] 
 */

void	ctcp_cmd_ping(struct evbuffer *out, char *from, char *args)
{
	char	*tmp = NULL;
	
	if (g_mode & VERBOSE)
		printf("[c] CTCP PING from %s\n", from);
	if (args == NULL || strlen(args) <= 1) {
		ctcp_reply(out, from, "PING");
	}
	else {
		asprintf(&tmp, "PING %s", args);
		if (tmp == NULL)
			return;
		ctcp_reply(out, from, tmp);
		free(tmp);
	}
}

/*
 * ping myself 
 */

void	ctcp_self_ping(struct evbuffer *out)
{
	char	*to;
	
	to = hash_text_get_first(g_conf.global, "nick");
	if (to == NULL)
		perror("[c] cannot allocate memory.\n");
	ctcp_request(out, to, "PING");
}

/*
 * CTCP TIME : reply the current timestamp
 * TIME 1192837268
 */

void	ctcp_cmd_time(struct evbuffer *out, char *from, char *args)
{
	char	*tmp = NULL;
	
	args = NULL;
	asprintf(&tmp, "TIME %u", (unsigned int)time(NULL));
	if (tmp == NULL)
		return;
	if (g_mode & VERBOSE)
		printf("[c] CTCP TIME from %s\n", from);
	ctcp_reply(out, from, tmp);
	free(tmp);
}

/*
 * CTCP USERINFO
 * just reply what we are
 */

void	ctcp_cmd_userinfo(struct evbuffer *out, char *from, char *args)
{
	args = NULL;
	if (g_mode & VERBOSE)
		printf("[c] CTCP USERINFO from %s\n", from);
	ctcp_reply(out, from, "USERINFO iyell, bot");
}

/* 
 * CTCP VERSION
 * reply <client> <version> <infos>
 */

void	ctcp_cmd_version(struct evbuffer *out, char *from, char *args)
{
	char		*tmp = NULL;
	struct utsname	name;
	
	/* cleaning the structure */
	memset(&name, '\0', sizeof(name));
	args = NULL;
	
	/* gathering information */
	if (uname(&name) == -1)
		return;
	
	/* building reply */
	asprintf(&tmp, "VERSION iyell v%s - running on %s %s / %s", VERSION, \
		 name.sysname, name.release, name.machine);
	if (tmp == NULL)
		return;

	if (g_mode & VERBOSE)
		printf("[c] CTCP VERSION from %s\n", from);
	ctcp_reply(out, from, tmp);
	free(tmp);
}
