/* simple C client for iyell */

/* version 008 */

#define __EXTENSIONS__ /* SunOS */
#define _GNU_SOURCE
#define __BSD_VISIBLE 1
#define _DARWIN_C_SOURCE
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <openssl/rc4.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#ifndef HAVE_ASPRINTF
#include "asprintf.h"
#endif
#include "main.h"

/* prototypes */
int     checkopt(int *argc, char ***argv);
void	read_password(char *buffer, size_t size);
int	send_message(char *str);
int	send_unix(char *str, int len);
int	send_udp(char *str, int len);

/* variables */
extern char	*g_address;
extern char	*g_port;
extern char	*g_intro;
extern char	*g_type;

int	main(int argc, char *argv[])
{
	int		i = 0;
	
	/* check args */
	if (checkopt(&argc, &argv))
		fprintf(stderr, "[-] warning: trailing arguments\n");

	if (argc == 0) {
		fprintf(stderr, "[-] no messages ?!\n");
		exit(NOERROR);
	}
	/* basic checks */
	if (g_type == NULL) {
		fprintf(stderr, "[-] no message type specified\n");
		exit(ERROR);
	}

	if (g_address[0] != '/' && g_file == NULL) {
		fprintf(stderr, "[-] no password file specified\n");
		exit(ERROR);
	}

	for (i = 0; i < argc; i++) {
		if (g_mode & DEBUG) 
			printf("[+] Sending message: %s\n", argv[i]);

		send_message(argv[i]);	
	}
	return 0;
}

/*
 * read a password from g_file
 * return a null temrinated string on success
 * exit with -1 on error
 */

void	read_password(char *buffer, size_t size)
{
	FILE	*f;
	char	*search;

	f = fopen(g_file, "r");
	if (f == NULL) {
		fprintf(stderr, "[-] fopen() error: %s\n", strerror(errno));
		exit(ERROR);
	}
	if (fgets(buffer, size, f) == NULL) {
		fprintf(stderr, "[-] fgets() error: %s\n", strerror(errno));
		fclose(f);
		exit(ERROR);
	}

	/* cleanup new line if foud */
	search = strchr(buffer, '\n');
	if (search != NULL)
		*search = '\0';

	fclose(f);
}

/*
 * This fonction build the buffer and then call the appropriate
 * send_* depending on Unix/UDP mode
 */

int	send_message(char *str)
{
	int		len;
	char		password[BUFF_SIZE];
	char		*msg, *tmp;
	RC4_KEY		key;

	if (g_intro == NULL)
		len = asprintf(&msg, "%s:%ld:%s\n", g_type, time(NULL), str);
	else
		len = asprintf(&msg, "%s:%ld:%s %s\n", g_type, time(NULL), g_intro, str);
	if (len == -1) {
		fprintf(stderr, "[i] asprintf() error: %s\n", strerror(errno));
		return ERROR;
	}

	if (g_address[0] == '/') {
		send_unix(msg, len);
	}
	else {
		read_password(password, BUFF_SIZE);
		tmp = malloc(len * sizeof(char));
		if (tmp == NULL) {
			fprintf(stderr, "[-] malloc() error: %s\n", strerror(errno));
			free(msg);
			return ERROR;
		}

		/* set the key */
		RC4_set_key(&key, strlen(password), (unsigned char *)password);

		/* crypt the stuff */
	  	RC4(&key, len, (unsigned char *)msg, (unsigned char *)tmp);

		send_udp(tmp, len);
	  	free(tmp);
	}

	free(msg);
	return NOERROR;
}

/*
 * Crypt data and send it to the remote bot specified in 
 * g_address and g_port if appropriate
 *
 * - str = string to send
 * - len = lengh of this string
 *
 * return 0 on success and -1 on error
 */

int	send_udp(char *str, int len)
{
	int			error, s;
	struct addrinfo         hints, *res, *res0;

	/*
	 * initialise hints
	 * AF_UNSPEC is for IPv4 and IPv6 
	 */

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((error = getaddrinfo(g_address, g_port, &hints, &res0))) {
		fprintf(stderr, "[-] host not found: %s (%s)\n",
			g_address, gai_strerror(error));
		exit(ERROR);
	}

	s = -1;
	for (res = res0; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype,
		res->ai_protocol);
		if (s < 0) {
			continue;
		}

		break;
	}

	/* Find IP address */
	switch (res->ai_family) {
		case AF_INET: {
			char ntop[INET_ADDRSTRLEN];

			if (inet_ntop(res->ai_family, &((struct sockaddr_in *)res->ai_addr)->sin_addr, \
				      ntop, INET_ADDRSTRLEN) == NULL) {
				fprintf(stderr, "[-] inet_ntop() error: %s\n", strerror(errno));
				break;
			}

			if (sendto(s, str, len, 0, res->ai_addr, res->ai_addrlen) == -1)
				fprintf(stderr, "[-] sendto() error: %s\n", strerror(errno));
			if (g_mode & VERBOSE)
				printf("[+] sending to host [%s]:%s\n", ntop, g_port);
			break;
		}
		case AF_INET6: {
			char ntop[INET6_ADDRSTRLEN];

			if (inet_ntop(res->ai_family, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, \
				      ntop, INET6_ADDRSTRLEN) == NULL) {
				fprintf(stderr, "[-] inet_ntop() error: %s\n", strerror(errno));
				break;
			}

			if (sendto(s, str, len, 0, res->ai_addr, res->ai_addrlen) == -1)
				fprintf(stderr, "[-] sendto() error: %s\n", strerror(errno));
			if (g_mode & VERBOSE)
				printf("[+] sending to host [%s]:%s\n", ntop, g_port);
			break;
		}
		default:
		       fprintf(stderr, "[n] getaddrinfo() error: %s\n", strerror(EAFNOSUPPORT));
	}

	return close(s);
}

int	send_unix(char *str, int len)
{
        int			s = ERROR;
        struct sockaddr_un	sun;

	if (strlen(g_address) > (sizeof(sun.sun_path) - sizeof(char))) {
		fprintf(stderr, "[-] path too long for unix socket (max is %ld)\n", \
		        sizeof(sun.sun_path) - 1);
	        exit(ERROR);
	}

	/* create socket */
	if ((s = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
		fprintf(stderr, "[-] socket() error: %s\n", strerror(errno));
		exit(ERROR);
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, g_address, sizeof(sun.sun_path) - 1);

	if (connect(s, (struct sockaddr *)&sun, sizeof(sun)) < 0) {
		fprintf(stderr, "[-] connect() error: %s\n", strerror(errno));	
		exit(ERROR);
	}

	if (send(s, str, len, 0) == -1) {
		fprintf(stderr, "[-] send() error: %s\n", strerror(errno));
		exit(ERROR);
	}

	return close(s);
}
