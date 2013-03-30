/*
 *  main.h
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

#ifndef __YELL_H__
#define __YELL_H__

/* global includes */
#include "conf.h"

/* irc stuff from network.c */
int	irc_connect(void);
void	irc_reconnect(int fd, short event, void *arg);
int	irc_disconnect(int socket);
void	irc_shutdown(int fd, short event, void *arg);


/* from openssl.c */
int     ssl_init(void);
int     ssl_handshake(int fd);

/* from hup.c */
void	hup_register(void);
void	hup_unregister(void);

/* from init.c */
int	init();

/* from info.c */
void	info_transmit(char *bot, char *type, char *message);

/* from log.c */
void	log_from_libevent(int level, char *str);
void	log_init(void);
void	log_deinit(void);
void	log_msg(char *msg, ...);
void	log_err(char *msg, ...);

/* from rfc3164.c */
#ifdef	_EVENT_H_
void	sl_parse(struct evbuffer *out, struct evbuffer *in);
#endif

/* from network.c */
int	net_create_udp_socket(char *host, char *port);
#ifdef	_EVENT_H_
void	net_force_output(struct event *ev);
#endif
int	net_loop(void);
void	net_throttled_write(int fd, short event, void *arg);

void	sig_set_handlers(void);

int	net_udp_start(int family, char *desc, char *setting);
int	net_udp_stop(int socket);
int	net_unix_start(void);
int	net_unix_stop(int socket);

/* global variables */
extern unsigned int	g_mode;		/* runing mode */
extern char		*g_file;	/* configuration file */
extern conf_t		g_conf;

/* gmode defines */
#define	DAEMON		(1 << 0)
#define	VERBOSE		(1 << 1)
#define	USE_SSL		(1 << 2)
#define	STARTING	(1 << 3)
#define SHUTDOWN	(1 << 4)
#define CONNECTED	(1 << 5)
#define NICKUSED	(1 << 6)
#define BANNED		(1 << 7)
#define	TODO		(NICKUSED + BANNED)
#define DEBUG		(1 << 8)
#define KEY		(1 << 9)
#define	SYSLOG		(1 << 10)
#define	FLOOD		(1 << 11)
#define THROTTLING	(1 << 12)
#define	THROTTLED	(1 << 13)
#define	LUA		(1 << 14)
#define	HOOKS		(1 << 15)
#define SL3164_SILENT	(1 << 16)
#define COLOR		(1 << 17)

/* errors code */
#define	NOERROR		0
#define	ERROR		-1
#define	NOSERVER	-2

/* others defines */
#define	BUFF_SIZE	4096
#ifndef VERSION
#define	VERSION		"0.7.0"
#endif

#endif
