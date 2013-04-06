/*
 *  opt.c
 *  iyell/quotes
 *
 *  Created by Michel DEPEIGE on 18/02/2009.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "main.h"

/* global variables needed for getopt */
extern char	*optarg;
extern int	optind, opterr, optopt;
unsigned int	g_mode;
char		*g_address, *g_message, *g_port;
char		*g_intro, *g_file, *g_type;

/* prototypes */
void	usage(void);
void	version(void);

#define	OPTION_STRING	"a:di:f:hp:t:vV"

/* check arg, set variables as necessary, and return */
int	checkopt(int *argc, char ***argv)
{
	int	c;
	int	option_index = 0;

	g_address = "localhost";
	g_file = NULL;
	g_intro = NULL;
	g_message = NULL;
	g_port = "25879";
	g_type = NULL;

	while (1)
	{
		static struct option long_options[] = {
		{"address", 1, 0, 'a'},
		{"debug", 0, 0, 'd'},
		{"help", 0, 0, 'h'},
		{"intro", 1, 0, 'i'},
		{"file", 1, 0, 'f'},
		{"port", 1, 0, 'p'},
		{"type", 1, 0, 't'},
		{"version", 0, 0, 'V'},
		{"verbose", 0, 0, 'v'},
		{0, 0, 0, 0}
		};

		c = getopt_long(*argc, *argv, OPTION_STRING,
				long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'a':
				g_address = optarg;
				break;
			case 'd':
				g_mode |= DEBUG;
				break;
			case 'f':
				g_file = optarg;
				break;
			case 'i':
				g_intro = optarg;
				break;
			case 'h':
				usage();
				break;
			case 'p':
				g_port = optarg;
				break;
			case 't':
				g_type = optarg;
				break;
			case 'V':
				version();
				break;
			case 'v':
				g_mode |= VERBOSE;
				break;
			default:
				break;
		}
	}

     	*argc -= optind;
        *argv += optind;

	return 0;
}

/* display an error with perror and exit the program */
void    fatal(char *str)
{
	perror(str);
	exit(-1);
}

/* print usage and exit */
void	usage(void)
{
	printf("iyell client version %s\n", VERSION);
	printf("iyell [-o] [--options] <message> [another_message]\n");
	printf("\t -a --address\t address of remote bot.\n");
	printf("\t -d --debug\t debug mode.\n");
	printf("\t -h --help\t this screen.\n");
	printf("\t -i --intro\t leading text added to messages.\n");
	printf("\t -f --file\t password file. Specify this for UDP.\n");
	printf("\t -p --port\t remote bot port.\n");
	printf("\t -t --type\t type of message.\n");
	printf("\t -v --verbose\t verbose mode.\n");
	printf("\t -V --version\t show version and exit.\n");
	exit(NOERROR);
}

/* print version and exit */
void	version(void)
{
	printf("iyell client version %s\n", VERSION);
	exit(NOERROR);
}
