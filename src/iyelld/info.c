/*
 *  info.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 15/10/07.
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
#define __BSD_VISIBLE 1
#define _DARWIN_C_SOURCE
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <openssl/rc4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#include "main.h"
#include "irc.h"
#include "stats.h"

/* defines */
#define	DEFAULT_OFFSET	900

/* prototypes */
static int     	find_target_bots(char *type, char *message);
static int	find_target_channels(struct evbuffer *out, char *type, char *message);
void		info_forward(struct evbuffer *out, char *type, char *message);
short		info_check_timestamp(char *t);
void		info_cipher(unsigned char *data, unsigned char *key, size_t len);
void		info_direct_forward(struct evbuffer *out, char *type, char *message);
void		info_rc4_cipher(unsigned char *data, unsigned char *key, size_t len);
void		info_parse(struct evbuffer *out, struct evbuffer *in);
void		info_transmit(char *bot, char *type, char *message);
void		dgram_parse(struct evbuffer *out, struct evbuffer *in);
static char	*udp_get_key(void);

/*
 * this function decrypt the UDP packet, check if it is correct (via info_parse())
 * and send the data to the appropriate channels if needed (via info_forward());
 */

void	dgram_parse(struct evbuffer *out, struct evbuffer *in)
{
	info_rc4_cipher(EVBUFFER_DATA(in), (unsigned char *)udp_get_key(), EVBUFFER_LENGTH(in));
	info_parse(out, in);
}

/*
 * check timestamp validity
 * *t = a null terminated timestamp ascii string
 * - return 1 if timestamp is valid
 * - return 0 of not
* - return -1 on error
 */

short	info_check_timestamp(char *t)
{
	time_t	current, offset, packet;
	char	*tmp;

	/* fetch configuration */
	tmp = hash_text_get_first(g_conf.global, "offset");
	if ((current = time(NULL)) == -1)
		return -1;
	if (tmp == NULL)
		offset = DEFAULT_OFFSET;
	else
		offset = strtol(tmp, NULL, 10);
	if (offset == 0)
		return -1;

	/* check packet timestamp syntax */
	packet = strtol(t, NULL, 10);
	if (packet == 0)
		return -1;

	if (labs(current - packet) > offset)
		return 0;

	return 1;
}

/* cipher switching stuff */
void	info_cipher(unsigned char *data, unsigned char *key, size_t len)
{
	info_rc4_cipher(data, key, len);
}

/* cipher irc4 stuff */
void	info_rc4_cipher(unsigned char *data, unsigned char *key, size_t len)
{
	RC4_KEY		rc4_key;
	unsigned char	tmp[len + 1];

	RC4_set_key(&rc4_key, strlen((char *)key), key);
	tmp[len] = '\0';
	memcpy(tmp, data, len);
	RC4(&rc4_key, len, tmp, data);
}

/* check if the packet is valid */
void	info_parse(struct evbuffer *out, struct evbuffer *in)
{
  	char		*tmp;
	size_t		len = 0;
	char		*ap;
	short		ret;
	char		*ts;

	/* if the bot is not connected, drop messages */
	if (! (g_mode & CONNECTED)) {
		log_msg("[i] bot not connected. ignoring message\n");
		evbuffer_drain(in, EVBUFFER_LENGTH(in));
		return ;
	}

	/* check if there is one line in the buffer */
	if ((tmp = evbuffer_readline(in)) == NULL) {
		log_err("[i] bad packet, dropping\n");
		goto error;
	}

parseline:
	/* check if the magic is here */
	len = strlen(tmp);
	ap = strsep(&tmp, ":");
	if (ap != NULL) {
		/* split the timestamp and the message, ts = timestamp, tmp = message */
		ts = strsep(&tmp, ":");

		/* check if the timestamp is here */
		if (ts == NULL || (strlen(ts) == 0)) {
			log_err("[i] no timestamp, dropping\n");
			free(ap);
			goto error;
		}

		/* check if the message is correct */
		if (tmp == NULL || (strlen(tmp) == 0)) {
			log_err("[i] no message, dropping\n");
			free(ap);
			goto error;
		}

		/* we have something, find what it is, send it to IRC ! */
		errno = 0;
		ret = info_check_timestamp(ts);
		switch (ret) {
			case 0: {
				STATS_INC_OEMSG(x);
				log_err("[i] timestamp offset exceeded, dropping\n");
				break;
			}
			case 1: {
				info_forward(out, ap, tmp);
				break;
			}
			default: {
				log_err("[i] error while validating timestamp: %s\n",
					errno ? strerror(errno) : "parse error");
				free(ap);
				goto error;
			}
		}
	}
	else {
		/* crappy packet */
		log_err("[i] no info type, dropping\n");
		goto error;
	}
	free(ap);
	evbuffer_drain(in, len + 1);
	STATS_INC_GMSG(x);

	/* check for another line */
	if (EVBUFFER_LENGTH(in) > 0) {
		if ((tmp = evbuffer_readline(in)) == NULL) {
			log_err("[i] bad packet, dropping\n");
			evbuffer_drain(in, EVBUFFER_LENGTH(in));
			return;
		}
		goto parseline;
	}
	return ;
error:
	evbuffer_drain(in, EVBUFFER_LENGTH(in));
	STATS_INC_BMSG(x);
	return ;
}

/*
 * just find target channel(s) and bot(s)/
 *
 * subfunctions :
 * - find_target_bots()
 * - find_target_channels()
 */

void	info_forward(struct evbuffer *out, char *type, char *message)
{
	int	ret = 0;

	assert(type != NULL);

	/* direct recipient */
	if (type[0] == '=') {
		info_direct_forward(out, type + sizeof(char), message);
		return ;
	}

	/* bot or target */
	ret += find_target_channels(out, type, message);
	ret += find_target_bots(type, message);
	if (ret == -2)
		log_err("[i] unknow information type\n");
}

/*
 * forward message to recipient
 * and message - recipient: message
 */

void	info_direct_forward(struct evbuffer *out, char *type, char *message)
{
	if (! hash_text_is_true(g_conf.global, "allow_direct")) {
		log_msg("[i] direct recipient not allowed, dropping\n");
		return ;
	}

	if (type[0] == '\0') {
		log_err("[i] incomplete recipient, dropping\n");
		return ;
	}

	if (g_mode & VERBOSE)
		printf("[i] forwarding message \"%s\" to recipient \"%s\"\n",
	        message, type);

	/* standard mode */
	irc_cmd_privmsg(out, type, message);
}

/*
 * search in the configuration for eligibles bots and transmit the message
 * return -1 if nothing found, 0 otherwise.
 * C99 variable lengh array used
 */

static int	find_target_bots(char *type, char *message)
{
	char		**tmp;
	unsigned int	i;

	/* get the bots for this information */
	tmp = hash_get(g_conf.transmit, type);
	if (tmp == NULL)
		return -1;

	if (g_mode & VERBOSE)
		printf("[i] forwarding message \"%s\" to bot(s)\n", message);
	for (i = 0; tmp[i] != NULL; i++) {
		info_transmit(tmp[i], type, message);
	}
	return 0;
}

/*
 * search in the configuration for eligible channels for this message type.
 * return -1 if nothing found, 0 otherwise.
 * C99 variable lengh array used
 */

static int	find_target_channels(struct evbuffer *out, char *type, char *message)
{
	char		**tmp;
	unsigned int	i;

	/* get the channels for this information */
	tmp = hash_get(g_conf.forward, type);
	if (tmp == NULL)
		return -1;

	if (g_mode & VERBOSE)
		printf("[i] forwarding message \"%s\" to channel(s)\n", message);
	/* send it */
	for (i = 0; tmp[i] != NULL; i++) {
		irc_cmd_privmsg(out, tmp[i], message);
	}
	return 0;
}

/*
 * Simple fonction for getting the udp key
 */

static char	*udp_get_key()
{
	char	*tmp;
	char	*key = NULL;

	tmp = hash_text_get_first(g_conf.global, "udp_key");
	key = tmp;
	return (key);
}

/*
 * Rebuild an UDP packet and try to send it
 * message format : "type:timestamp:message:\n"
 *
 * bot address is formated like [ip.of.the.bot]:port
 */

void	info_transmit(char *bot, char *type, char *message)
{
	char		*host = NULL, *port = NULL;
	size_t		len;
	int		fd;

	len = sizeof(time_t) + 1 + strlen(type) + 1 + strlen(message) + 1;
	unsigned char	msg[len + 1];

	host = malloc(strlen(bot) * sizeof(char));
	port = malloc(strlen(bot) * sizeof(char));
	if (port == NULL || host == NULL) {
		log_err("[i] malloc() error: %s\n", strerror(errno));
		return ;
	}

	if (sscanf(bot, "[%[^]] %*[^':']:%s", host, port) != 2) {
		free(host);
		free(port);
		log_err("[i] invalid bot declaration: %s\n", bot);
		return ;
	}

	/* build message and send it */
	sprintf((char *)msg, "%ld:%s:%s\n", (long)time(NULL), type, message);
	info_rc4_cipher(msg, (unsigned char *)udp_get_key(), len);
	fd = net_create_udp_socket(host, port);
	if (fd == -1)
		return ;
	if (send(fd, msg, len, 0) == -1)
		log_err("[i] send() error: %s\n", strerror(errno));
	close(fd);
	free(host);
	free(port);
}
