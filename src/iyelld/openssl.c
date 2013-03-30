/*
 *  openssl.c
 *  iyell  
 *
 *  Created by Michel DEPEIGE on 09/09/2008.
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

/* include */
#define _BSD_SOURCE
#define __BSD_VISIBLE 1
#include <arpa/inet.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#include "heap.h"
#include "main.h"
#include "misc.h"

/* variables */
SSL		*ssl = NULL;			/* SSL stuff */
SSL_CTX		*ctx = NULL;			/* SSL context */

/* prototypes */
int	ssl_recv(SSL *ssl, void *buf, size_t len);
int	ssl_send(SSL *ssl, void *buf, size_t len);
int	ssl_smart_shutdown(SSL *ssl);

/*
 * init the SSL library
 */

int	ssl_init(void)
{
	SSL_load_error_strings();
	SSL_library_init();
	ctx = SSL_CTX_new(SSLv23_client_method());

	if (! ctx) {
		ERR_print_errors_fp(stderr);
		log_err("[o] cannot init openssl\n");
		return -1;
	}
	
	return NOERROR;
}

/* ssl shutdown */

int	ssl_smart_shutdown(SSL *ssl)
{
	unsigned int	i;
	int		ret;

	ret = 0;
	for (i = 0; i < 4; ++i)
		if ((ret = SSL_shutdown(ssl)))
			break;
	return ret;
}

/*
 * handshake for SSL connections
 */

int	ssl_handshake(int fd)
{
	if (g_mode & STARTING)
		ssl = SSL_new(ctx);
	else
		SSL_clear(ssl);

	if (g_mode & VERBOSE)
		log_msg("[o] performing SSL handshake\n");
	
	if (! (SSL_set_fd(ssl, fd))) {
		fprintf(stderr, "[o] unable to set ssl fd\n");
		SSL_clear(ssl);
		return -1;
	}

	if (! (SSL_connect(ssl))) {
		fprintf(stderr, "[o] unable to complete ssl handshake\n");
		SSL_clear(ssl);
		return -1;
	}
	return NOERROR;
}

/*
 * SSL I/O stuff
 */

int	ssl_recv(SSL *ssl, void *buf, size_t len)
{
	int	ret, ssl_err;

	ret = SSL_read(ssl, buf, len);
	if (ret <= 0) {
		switch ((ssl_err = SSL_get_error(ssl, len))) {
		case SSL_ERROR_SYSCALL:
			if ((errno == EWOULDBLOCK) ||
			    (errno == EAGAIN) || (errno == EINTR)) {
			case SSL_ERROR_WANT_READ:
			case SSL_ERROR_WANT_WRITE:
				errno = EWOULDBLOCK;
				return -1;
			}
		default:
			log_err("[o] SSL_read() exited with SSL error: %i (%s)\n",
				SSL_get_error(ssl, ret),
				ERR_error_string(SSL_get_error(ssl, ret), NULL));
		}
	}
	return ret;
}

int	ssl_send(SSL *ssl, void *buf, size_t len)
{
	int	ret, ssl_err;

	ret = SSL_write(ssl, buf, len);
	if (ret <= 0) {
		switch ((ssl_err = SSL_get_error(ssl, len))) {
		case SSL_ERROR_SYSCALL:
			if ((errno == EWOULDBLOCK) ||
			    (errno == EAGAIN) || (errno == EINTR)) {
			case SSL_ERROR_WANT_WRITE:
			case SSL_ERROR_WANT_READ:
				errno = EWOULDBLOCK;
				return -1;
			}
		default:
			log_err("[o] SSL_write() exited with SSL error: %i (%s)\n",
				SSL_get_error(ssl, ret),
				ERR_error_string(SSL_get_error(ssl, ret), NULL));
		}
	}
	return ret;
}
