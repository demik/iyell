/*
 *  network.c
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

/* include */
#define __EXTENSIONS__ /* SunOS */
#define _BSD_SOURCE
#define __BSD_VISIBLE 1
#include <arpa/inet.h>
#include <assert.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#include "ctcp.h"
#include "heap.h"
#include "irc.h"
#include "main.h"
#include "misc.h"
#include "sched.h"
#include "stats.h"

/*
 * Some usefull stuff
 */

#define TIMEOUT_IRC	&timeout

void		net_dgram4_read(int fd, short event, void *arg);
void		net_dgram6_read(int fd, short event, void *arg);
void		net_irc_cleanup(int fd);
static void	net_irc_event(int fd, short event, void *arg);
void		net_irc_read(int fd, short event, void *arg);
void		net_irc_timeout(int fd, short event, void *arg);
void		net_irc_write(int fd, short event, void *arg);
void		net_irc_throttled_write(int fd, short event, void *arg);
void		net_force_output(struct event *ev);
void		net_sl4_read(int fd, short event, void *arg);
void		net_sl6_read(int fd, short event, void *arg);
void		net_unix_read(int fd, short event, void *arg);

static int	start_listeners(void);
static void	stop_listeners(void);

void		info_parse(struct evbuffer *out, struct evbuffer *in);
void		dgram_parse(struct evbuffer *out, struct evbuffer *in);

/* for libevent 1.3 */
#ifndef HAVE_EVENT_LOOPBREAK
int	event_loopbreak(void);
#endif

/* variables */
struct evbuffer	*ircin, *ircout;		/* IO buffers */
struct evbuffer	*dgram_in, *syslog_in, *unix_sock;

extern SSL             *ssl;
extern SSL_CTX         *ctx;

int	ssl_smart_shutdown(SSL *ssl);
int	ssl_recv(SSL *ssl, void *buf, size_t len);
int	ssl_send(SSL *ssl, void *buf, size_t len);

struct event	ev_irc;			/* IRC event structure */
struct event	ev_syslog4;		/* syslog event structure */
struct event	ev_syslog6;		/* syslog event structure */
struct event	ev_udp4;		/* UDP event structure */
struct event	ev_udp6;		/* UDP event structure */
struct event	ev_unix;		/* Unix event structure */

/*
 * Switch functions for IRC (read, write, timeout...)
 */

static void	net_irc_event(int fd, short event, void *arg)
{
	if (event & EV_READ) {
		net_irc_read(fd, event, arg);
		return ;
	}
	if (event & EV_TIMEOUT) {
		net_irc_timeout(fd, event, arg);
		event |= EV_WRITE;
	}
	if (event & EV_WRITE) {
		if (g_mode & THROTTLING)
			net_irc_throttled_write(fd, event, arg);
		else
			net_irc_write(fd, event, arg);
	}
}

/*
 * read from the IRC server
 */

void	net_irc_read(int fd, short event, void *arg)
{
	int		ret;
	char		tmp[BUFF_SIZE];

	event = 0;
	if (g_mode & USE_SSL)
		ret = ssl_recv(ssl, tmp, BUFF_SIZE);
	else
		ret = recv(fd, tmp, BUFF_SIZE, 0);
	if (ret <= 0) {
		log_err("[n] error %i while receiving data: %s\n",
			ret, strerror(errno));
		net_irc_cleanup(fd);
		return ;
	}
	STATS_INC_SRV_IN(ret);
	evbuffer_add(ircin, tmp, ret);
	irc_parse(ircout, ircin);

	/* if something in the output buffer, add EV_WRITE */
	if (EVBUFFER_LENGTH(ircout) > 0) { 
		event_set(arg, fd, EV_READ | EV_WRITE, net_irc_event, arg);
	}
        /* reschedule this event */
        if (event_add((struct event *)arg, TIMEOUT_IRC) == -1)
		perror("[n] cannot event_add()");
}

/*
 * Write to the IRC socket
 */

void	net_irc_write(int fd, short event, void *arg)
{
	int	ret;
	char	tmp[BUFF_SIZE];

	assert(EVBUFFER_LENGTH(ircout) != 0);
	event = 0;
	/* print output buffer */
	if (g_mode & DEBUG) {
		snprintf(tmp, ((EVBUFFER_LENGTH(ircout)) + 1 < BUFF_SIZE ? \
				(EVBUFFER_LENGTH(ircout) + 1) : BUFF_SIZE), \
			"%s", EVBUFFER_DATA(ircout));
		printf(">>> %s", tmp);
	}

	/* send data SSL or raw */
	if (g_mode & USE_SSL)
		ret = ssl_send(ssl, EVBUFFER_DATA(ircout), EVBUFFER_LENGTH(ircout));
	else 
		ret = send(fd, EVBUFFER_DATA(ircout), EVBUFFER_LENGTH(ircout), 0);
	if (ret <= 0) {
		log_err("[n] error %i while sending data: %s\n",
		ret, strerror(errno));
		net_irc_cleanup(fd);
		return ;
	}
	else {
		evbuffer_drain(ircout, ret);
	}
	STATS_INC_SRV_OUT(ret);

	/* if shutdown, don't register the event again */
	if (g_mode & SHUTDOWN) {
		event_loopbreak();
		return ;
	}

	/* if buffer is free, remove EV_WRITE */
	if (EVBUFFER_LENGTH(ircout) == 0)
		event_set(arg, fd, EV_READ, net_irc_event, arg);

        /* reschedule this event */
        if (event_add((struct event *)arg, TIMEOUT_IRC) == -1)
		perror("[n] cannot event_add()");
}

/*
 * Write to the IRC socket, throttled version
 *
 * idea: allow a burst of 3 lines
 * if more, throttle data, one line every second max.
 */

void	net_irc_throttled_write(int fd, short event, void *arg)
{
	int		buffer_count = 0;
	int		ret;
	unsigned int	i;
	unsigned char	*data;
	char		tmp[BUFF_SIZE];
	static unsigned int burst_count = 0;	/* if > 3, throttling needed */
	time_t		current;
	static time_t	last = 0;

	assert(EVBUFFER_LENGTH(ircout) != 0);
	event = 0;
	current = time(NULL);
	data = EVBUFFER_DATA(ircout);
	if (current == -1) {
		log_err("[n] error with time(): %s, sending without throttling\n",
			strerror(errno));
		net_irc_write(fd, event, arg);
		return ;
	}

	/* count lines in output buffer */
	for (i = 0; i < EVBUFFER_LENGTH(ircout); i++)
		if (*(data + i) == '\n')
			buffer_count++;

	/* reset burst counter if needed */
	if (current - last > 3)
		burst_count = 0;
	
	/* send at most (4 - burst_count) lines => find the first lines lenght */
	for (i = 0; (i < EVBUFFER_LENGTH(ircout) && (burst_count < 4)); i++) {
		if (*(data + i) == '\n') {
			burst_count++;
		}
	}

	/* at this point, i = number of bytes to be send. if 0, we are throttled */
	if (i == 0)
		goto nothing;

	/* print output */
	if (g_mode & DEBUG) {
		snprintf(tmp, (i + 1 < BUFF_SIZE ? (i + 1) : BUFF_SIZE), \
			"%s", EVBUFFER_DATA(ircout));
		printf(">>> %s", tmp);
	}

	/* send data SSL or raw */
	if (g_mode & USE_SSL)
		ret = ssl_send(ssl, EVBUFFER_DATA(ircout), i);
	else 
		ret = send(fd, EVBUFFER_DATA(ircout), i, 0);
	if (ret <= 0) {
		log_err("[n] error %i while sending data: %s\n",
		ret, strerror(errno));
		net_irc_cleanup(fd);
		return ;
	}
	else {
		evbuffer_drain(ircout, ret);
	}
	STATS_INC_SRV_OUT(ret);

	/* if shutdown, don't register the event again */
	if (g_mode & SHUTDOWN) {
		event_loopbreak();
		return ;
	}

nothing:
	/* remove EV_WRITE, schedule output later if needed */
	event_set(arg, fd, EV_READ, net_irc_event, arg);
	if (EVBUFFER_LENGTH(ircout) > 0 && ! (g_mode & THROTTLED)) {
		g_mode |= THROTTLED;
		schedule_irc_write(1);
	}

	/* set variables for next call */
	if ((burst_count > 0) && ((current - last) >= 1))
		burst_count--;
	last = current;

        /* reschedule this event */
        if (event_add((struct event *)arg, TIMEOUT_IRC) == -1)
		perror("[n] cannot event_add()");
}

/*
 * Search WTF is doing the server
 */

void	net_irc_timeout(int fd, short event, void *arg)
{
	event = 0;
	fd = 0;
	arg = 0;

	log_msg("[n] something strange, sending self ping.\n");
	ctcp_self_ping(ircout);
}

/*
 * shutdown callback
 */

void	irc_shutdown(int fd, short event, void *out)
{
	static short	send = 0;

	event = 0;
	fd = 0;

	if (send)
		return ;

	irc_cmd_quit(out, NULL);
	net_force_output(&ev_irc);
	send = 1;
}

/*
 * Try to purge output IRC buffer (delete / readd event)
 */

void	net_force_output(struct event *ev) 
{
	int	fd;

	fd = ev->ev_fd;
	if (event_del(ev) == -1) {
		log_err("[n] cannot event_del(): %s\n", strerror(errno));
		return ;
	}
	event_set(ev, fd, EV_READ | EV_WRITE, net_irc_event, ev);

        if (event_add(ev, TIMEOUT_IRC) == -1) { 
		log_err("[n] cannot event_add(): %s\n", strerror(errno));
	}

}

/* wrapper for net_force_output called by a timer if throttled irc write */
void	net_throttled_write(int fd, short event, void *arg)
{
	fd = 0;
	event = 0;
	arg = NULL;

	g_mode &= ~THROTTLED;
	if (EVBUFFER_LENGTH(ircout) > 0)
		net_force_output(&ev_irc);
}

/*
 * 4 functions below are used to read from UDP sockets
 * dgram4 and dgram6 are iyell UDP listeners
 * sl4 and sl6 are syslog UDP listeners
 */

void	net_dgram4_read(int fd, short event, void *arg)
{
	int			ret;
	struct sockaddr_in	client;
	socklen_t		client_len;
	char			tmp[BUFF_SIZE];

	event = 0;
	arg = 0;

	client_len = sizeof(client);
	ret = recvfrom(fd, tmp, BUFF_SIZE, 0, (struct sockaddr *)&client, &client_len);
	if (ret <= 0) {
		perror("[n] error in recvfrom()");
		net_udp_stop(fd);
		evbuffer_drain(dgram_in, EVBUFFER_LENGTH(dgram_in));
		return ;
	}
	STATS_INC_MSG_IN(ret);
	evbuffer_add(dgram_in, tmp, ret);

	/* print information avout UDP */
	log_msg("[i] UDP packet from %s (len: %i)\n", 
	       inet_ntop(client.sin_family, &client.sin_addr, tmp, BUFF_SIZE), ret);
	dgram_parse(ircout, dgram_in);

	/* if something in the output buffer, send it */
	if (EVBUFFER_LENGTH(ircout) > 0) 
		net_force_output(&ev_irc);
}

void	net_dgram6_read(int fd, short event, void *arg)
{
	int			ret;
	struct sockaddr_in6	client6;
	socklen_t		client_len6;
	char			tmp[BUFF_SIZE];
	
	event = 0;
	arg = 0;
	
	client_len6 = sizeof(client6);
	ret = recvfrom(fd, tmp, BUFF_SIZE, 0, (struct sockaddr *)&client6, &client_len6);
	if (ret <= 0) {
		perror("[n] error in recvfrom()");
		net_udp_stop(fd);
		evbuffer_drain(dgram_in, EVBUFFER_LENGTH(dgram_in));
		return ;
	}
	STATS_INC_MSG_IN(ret);
	evbuffer_add(dgram_in, tmp, ret);

	/* print information avout UDP */
	log_msg("[i] UDP packet from %s (len: %i)\n", 
	       inet_ntop(client6.sin6_family, &client6.sin6_addr, tmp, BUFF_SIZE), ret);
	dgram_parse(ircout, dgram_in);

	/* if something in the output buffer, send it */
	if (EVBUFFER_LENGTH(ircout) > 0) 
		net_force_output(&ev_irc);
}

void	net_sl4_read(int fd, short event, void *arg)
{
	int			ret;
	struct sockaddr_in	client;
	socklen_t		client_len;
	char			tmp[BUFF_SIZE];

	event = 0;
	arg = 0;

	client_len = sizeof(client);
	ret = recvfrom(fd, tmp, BUFF_SIZE, 0, (struct sockaddr *)&client, &client_len);
	if (ret <= 0) {
		perror("[n] error in recvfrom()");
		net_udp_stop(fd);
		evbuffer_drain(syslog_in, EVBUFFER_LENGTH(syslog_in));
		return ;
	}
	STATS_INC_MSG_IN(ret);
	evbuffer_add(syslog_in, tmp, ret);

	/* print information avout UDP */
	log_msg("[n] syslog packet from %s (len: %i)\n", 
	       inet_ntop(client.sin_family, &client.sin_addr, tmp, BUFF_SIZE), ret);
	sl_parse(ircout, syslog_in);

	/* if something in the output buffer, send it */
	if (EVBUFFER_LENGTH(ircout) > 0) 
		net_force_output(&ev_irc);
}

void	net_sl6_read(int fd, short event, void *arg)
{
	int			ret;
	struct sockaddr_in6	client6;
	socklen_t		client_len6;
	char			tmp[BUFF_SIZE];
	
	event = 0;
	arg = 0;
	
	client_len6 = sizeof(client6);
	ret = recvfrom(fd, tmp, BUFF_SIZE, 0, (struct sockaddr *)&client6, &client_len6);
	if (ret <= 0) {
		perror("[n] error in recvfrom()");
		net_udp_stop(fd);
		evbuffer_drain(syslog_in, EVBUFFER_LENGTH(syslog_in));
		return ;
	}
	STATS_INC_MSG_IN(ret);
	evbuffer_add(syslog_in, tmp, ret);

	/* print information avout UDP */
	log_msg("[i] syslog packet from %s (len: %i)\n", 
	       inet_ntop(client6.sin6_family, &client6.sin6_addr, tmp, BUFF_SIZE), ret);
	sl_parse(ircout, syslog_in);

	/* if something in the output buffer, send it */
	if (EVBUFFER_LENGTH(ircout) > 0) 
		net_force_output(&ev_irc);
}
/*
 * read from the Unix socket
 */

void	net_unix_read(int fd, short event, void *arg)
{
	int	ret;
	char	tmp[BUFF_SIZE];

	event = 0;
        struct event *ev = arg;
	/* Reschedule this event */
	event_add(ev, NULL);
				
	ret = read(fd, tmp, BUFF_SIZE);
	if (ret < 0) {
		net_unix_stop(fd);
		evbuffer_drain(unix_sock, EVBUFFER_LENGTH(unix_sock));
		return ;
	}
	if (ret == 0)
		return ;

	if (g_mode & VERBOSE)
		log_msg("[n] data from unix socket (len: %i)\n", ret);
	STATS_INC_MSG_IN(ret);
	evbuffer_add(unix_sock, tmp, ret);
	info_parse(ircout, unix_sock);

	/* if something in the output buffer, send it */
	if (EVBUFFER_LENGTH(ircout) > 0) 
		net_force_output(&ev_irc);
}

/*
 * main loop fonction, set events etc
 */

int	net_loop(void)
{
	int		ret;

	/* Init buffers */
	dgram_in = evbuffer_new();
        ircin = evbuffer_new();
	ircout = evbuffer_new();
	syslog_in = evbuffer_new();
	unix_sock = evbuffer_new();
	if ((dgram_in == NULL) || (unix_sock == NULL) || \
	    (ircin == NULL) || (ircout == NULL) || (syslog_in == NULL))
		goto shutdown;

	/* Initlialise the listeners */ 
	ret = start_listeners();
	if (ret == ERROR)
		goto shutdown;

	/* if SSL is needed, init SSL */
	if (g_mode & USE_SSL)
		if (ssl_init() == -1)
			return -1;
	
	/* Initialise the IRC IO stuff */
	ret = irc_connect();
	if (ret == NOSERVER)
		return NOSERVER;

	/* we have something */
	if (ret > 0) {
		/* SSL Handshake if needed */
		if (g_mode & USE_SSL)
			if (ssl_handshake(ret) == -1) 
				goto shutdown;
	
		irc_send_helo(ircout);
	}
	else {
		schedule_reconnection(30);
	}
	
	if (g_mode & STARTING)
		g_mode &= ~STARTING;

	event_set(&ev_irc, ret, EV_READ, net_irc_event, &ev_irc);
	event_add(&ev_irc, NULL);

	/* event loop */
	event_dispatch();

shutdown:
        /* close listeners */
        stop_listeners();
	irc_disconnect(ev_irc.ev_fd);

	evbuffer_free(dgram_in);
	evbuffer_free(ircin);
	evbuffer_free(ircout);
	evbuffer_free(syslog_in);
	evbuffer_free(unix_sock);
	if ((g_mode & USE_SSL) && !(g_mode & STARTING)) {
		SSL_free(ssl);
		SSL_CTX_free(ctx);
		ERR_free_strings();		/* WTB */
		EVP_cleanup();			/* SSL_library_cleanup() */
		CRYPTO_cleanup_all_ex_data();	/*     */
	}
	
	return 0;
}

/*
 * Open a socket to the IRC sever and return the fd.
 * - return -1 on error
 */

int	irc_connect(void)
{
	int			s;
	char			**tmp = NULL;
	struct addrinfo		hints, *res, *res0;
	int			error;
	char			*port;

	if (g_mode & USE_SSL)
		log_msg("[n] connecting to server (SSL)...\n");
	else
		log_msg("[n] connecting to server...\n");

	/* 
	 * we ask the configuration for server name and port 
	 * then set the structures.
	 */
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	tmp = hash_get(g_conf.global, "port");
	if (tmp == NULL || tmp[0] == NULL) {
		log_msg("[n] cannot read port from configuration, using default\n");
		port = "irc";
	}
	else {
		port = tmp[0];
	}
	
	tmp = hash_get(g_conf.global, "server");
	if (tmp == NULL || tmp[0] == NULL) {
		log_err("[n] cannot find any servers, exiting\n");
		return NOSERVER;
	}
	
	if ((error = getaddrinfo(tmp[0], port, &hints, &res0))) {
		log_err("[n] host not found: %s (%s)\n",
			tmp[0], gai_strerror(error));
		return ERROR;
	}
	
	log_msg("[n] trying to connect to %s:%s...\n", tmp[0], port);

	/*
	 * try to connect
	 */
	
	s = -1;
	for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype,
			   res->ai_protocol);
		if (s < 0) {
			continue;
		}
		
		if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
			close(s);
			s = -1;
			continue;
		}
		
		break;  /* here we go */
	}
	
	if (s < 0) {
		log_err("[n] cannot connect to server: %s\n", strerror(errno));
		return ERROR;
	}

	/* print server IP address */
	switch (res->ai_family) {
		case AF_INET: {
			char ntop[INET_ADDRSTRLEN];
			
			if (inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ntop, INET_ADDRSTRLEN)) 
				log_msg("[n] connected to %s:%s (%i)\n", ntop, port, s);
			else
				log_err("[n] inet_ntop() error: %s\n", strerror(errno));
			break;
		}
		case AF_INET6: {
			char ntop[INET6_ADDRSTRLEN];

			if (inet_ntop(res->ai_family, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, ntop, INET6_ADDRSTRLEN))
				log_msg("[n] connected to [%s]:%s (%i)\n", ntop, port, s);
			else
				log_err("[n] inet_ntop() error: %s\n", strerror(errno));	
			break;
		}
		default: 
			log_msg("[n] getaddrinfo() error: %s\n", strerror(EAFNOSUPPORT));
	}
	
	freeaddrinfo(res0);
	return s;
}

/*
 * This function close the connection to the IRC server
 */

int	irc_disconnect(int socket)
{
	if (! (g_mode & CONNECTED))
		return 0;
	log_msg("[n] closing connection to server (%i)\n", socket);
	g_mode &= ~CONNECTED;
	if (close(socket) == -1)
		perror("[n] error while closing the IRC socket");

	/* delete event */
	if (event_del(&ev_irc) == -1)
		log_err("[n] event_del() error: %s\n", strerror(errno));
	return 0;
}

/*
 * clean IRC stuff after a disconnection
 */

void	net_irc_cleanup(int socket)
{
	irc_disconnect(socket);
	evbuffer_drain(ircin, EVBUFFER_LENGTH(ircin));
	evbuffer_drain(ircout, EVBUFFER_LENGTH(ircout));

	/* if shutdown, never reconnect */
	if (g_mode & SHUTDOWN) {
		event_loopbreak();
		return ;
	}
	schedule_reconnection(15);	
}

/*
 * this function try to reconnect to the IRC server
 */

void	irc_reconnect(int fd, short event, void *arg)
{
	fd = 0;
	event = 0;
	arg = NULL;

	int	ret;

	if ((ret = irc_connect()) == ERROR) {
		schedule_reconnection(60);
		return ;
	}

	if (ret == NOSERVER) {
		log_err("[n] cannot find any servers, exiting\n");
		event_loopbreak();
		return ;
	}

	event_set(&ev_irc, ret, EV_READ, net_irc_event, &ev_irc);
	event_add(&ev_irc, NULL);

	if (g_mode & USE_SSL)
		if (ssl_handshake(ret) == -1) {
			net_irc_cleanup(ret);
			schedule_reconnection(60);
			return ;
		}
			
	irc_send_helo(ircout);
}

/*
 * Start listeners if needed.
 * Starting IPv6 first help on Linux, where an IPv6 socket can also bind
 * the "associated" IPv4 address
 */

int	start_listeners()
{
	int	ret;

	/* Initlialise the syslog listener(s) if enabled */
	if (hash_text_is_true(g_conf.global, "syslog_listener")) {
		ret = net_udp_start(AF_INET6, "syslog", "syslog_port");
		if (ret != ERROR) {
			event_set(&ev_syslog6, ret, EV_READ | EV_PERSIST, \
				  net_sl6_read, &ev_syslog6);
			event_add(&ev_syslog6, NULL);
		}

		ret = net_udp_start(AF_INET, "syslog", "syslog_port");
		if (ret != ERROR) {
			event_set(&ev_syslog4, ret, EV_READ | EV_PERSIST, \
				  net_sl4_read, &ev_syslog4);
			event_add(&ev_syslog4, NULL);
		}
	}

	/* Initlialise the UDP listener(s) if enabled */
	if (hash_text_is_true(g_conf.global, "udp_listener")) {
		ret = net_udp_start(AF_INET6, "UDP", "udp_port");
		if (ret != ERROR) {
			event_set(&ev_udp6, ret, EV_READ | EV_PERSIST, \
				  net_dgram6_read, &ev_udp6);
			event_add(&ev_udp6, NULL);
		}

		ret = net_udp_start(AF_INET, "UDP", "udp_port");
		if (ret != ERROR) {
			event_set(&ev_udp4, ret, EV_READ | EV_PERSIST, \
				  net_dgram4_read, &ev_udp4);
			event_add(&ev_udp4, NULL);
		}
	}
	
	/* Initlialise the UNIX listener if needed */
	if (hash_text_is_true(g_conf.global, "unix_listener")) {
		ret = net_unix_start();
		if (ret != ERROR) {
			event_set(&ev_unix, ret, EV_READ, net_unix_read, &ev_unix);
			event_add(&ev_unix, NULL);
		}
	}
	
	return NOERROR;
}

/*
 * Stop listeners if needed
 */

void	stop_listeners()
{
	if (hash_text_is_true(g_conf.global, "syslog_listener")) {
		net_udp_stop(ev_syslog4.ev_fd);
		net_udp_stop(ev_syslog6.ev_fd);
	}
	if (hash_text_is_true(g_conf.global, "udp_listener")) {
		net_udp_stop(ev_udp4.ev_fd);
		net_udp_stop(ev_udp6.ev_fd);
	}
	if (hash_text_is_true(g_conf.global, "unix_listener")) {
		net_unix_stop(ev_unix.ev_fd);
	}
}

/*
 * This fonction bind one listener to the specified UNIX socket.
 * - return the file descriptor on success
 * - return -1 on error
 */

int	net_unix_start()
{
	char			*path;
	int			b, s = ERROR;
	struct sockaddr_un	sun;

	if (g_mode & VERBOSE)
		log_msg("[n] starting unix listener...\n");

	/* get the path from the configuration */
	path = hash_text_get_first(g_conf.global, "unix_path");
	if (path == NULL) {
		log_err("[n] cannot read unix path from configuration\n");
		return ERROR;
	}

	if (strlen(path) > (sizeof(sun.sun_path) - sizeof(char))) {
		log_err("[n] path too long for unix socket (max is %d)\n", \
			sizeof(sun.sun_path) - 1);
		return ERROR;
	}

	/* create socket */
	if ((s = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
		log_err("[n] socket() error: %s\n", strerror(errno));
		return ERROR;
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, path, sizeof(sun.sun_path) - 1);

	if ((b = bind(s, (struct sockaddr *)&sun, sizeof(sun))) < 0 ) {
		log_err("[n] bind() error: %s\n", strerror(errno));
		close(s);
		return ERROR;
	}




	log_msg("[n] listening Unix: %s (%i)\n", path, s);
	return s;
}

/*
 * This fonction close the specified unix socket.
 */

int	net_unix_stop(int socket)
{
  char	*path;

	if (socket <= 0)
		return 0;
	log_msg("[n] stoping the Unix listener (%i)\n", socket);
	if (close(socket) == -1)
		perror("[n] error while closing Unix socket");
        path = hash_text_get_first(g_conf.global, "unix_path");
	if (path != NULL) {
		if (unlink(path) == -1)
		log_err("[n] cannot unlink Unix socket: %s\n", strerror(errno));
	}
	return 0;
}

/*
 * This fonction bind one listener to the specified UDP port.
 *
 * family is the layer 3 protocol family (AF_INET or AF_INET6)
 * desc is the stuff used on printf (ie: UDP, syslog, foo...)
 * setting is the key in configuration
 * - return -1 on error
 * - return the fd associated to the socket on success
 */

int	net_udp_start(int family, char *desc, char *setting)
{
	int			b, error, s;
	struct addrinfo		hints, *server;
	char			*port;
	
	assert(setting != NULL);
	if (desc == NULL)
		desc = "UDP";

	if (g_mode & VERBOSE)
		log_msg("[n] starting %s listener...\n", desc);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	
	/* get the port from the configuration */
	port = hash_text_get_first(g_conf.global, setting);
	if (port == NULL) {
		log_err("[n] cannot read %s port from configuration\n", desc);
		return ERROR;
	}

	/* bind and listen socker */
	if ((error = getaddrinfo(NULL, port, &hints, &server))) {
		log_err("[n] getaddrinfo() error: %s\n", gai_strerror(error));
		return ERROR;
	}
	if ((s = socket(server->ai_family, server->ai_socktype, server->ai_protocol)) < 0 ) {
		log_err("[n] socket() error: %s\n", strerror(errno));
		return ERROR;
	}
	if ((b = bind(s, server->ai_addr, server->ai_addrlen)) < 0 ) {
		log_err("[n] bind() error: %s\n", strerror(errno));
		return ERROR;
	}
	freeaddrinfo(server);

	/* display listening information */
	switch (family) {
		case AF_INET: {
			log_msg("[n] listening %s/IPv4 on port %s (%i)\n", desc, port, s);
			break ;
		}
		case AF_INET6: {
			log_msg("[n] listening %s/IPv6 on port %s (%i)\n", desc, port, s);
			break ;
		}
		default: {
			log_msg("[n] listening %s on port %s (%i)\n", port, s);
		}
	}

	return s;
}

/*
 * This fonction close the specified udp socket.
 */

int	net_udp_stop(int socket)
{
	if (socket <= 0)
		return 0;
	log_msg("[n] stoping listener (%i)\n", socket);
	if (close(socket) == -1)
		log_err("[n] error while closing socket\n");
	return 0;
}

/*
 * Open an udp sockket and return a fd
 */

int	net_create_udp_socket(char *host, char *port)
{
	int			s;
	struct addrinfo		hints, *res, *res0;
	int			error;
	char			*cause;

	/* 
	 * we ask the configuration for server name and port 
	 * then set the structures.
	 */
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	
	if (g_mode & VERBOSE)
		log_msg("[n] creating UDP socket to [%s]:%s...\n", host, port);

	if ((error = getaddrinfo(host, port, &hints, &res0))) {
		log_err("[n] host not found: %s (%s), exiting\n",
			host, gai_strerror(error));
		return NOSERVER;
	}

	/*
	 * try to connect
	 */
	
	s = -1;
	for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype,
			   res->ai_protocol);
		if (s < 0) {
			cause = "[n] socket() error";
			continue;
		}
		
		if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
			cause = "[n] connect() error";
			close(s);
			s = -1;
			continue;
		}
		
		break;  /* here we go */
	}
	
	if (s < 0) {
		log_err("[n] cannot create UDP socket to %s: %s", host, strerror(errno));
		return -1;
	}
	freeaddrinfo(res0);
	return s;
}

