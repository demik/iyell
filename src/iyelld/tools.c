/*
 *  tools.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 10/10/07.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "main.h"
#include "tools.h"

/* global variables needed for getopt */
extern char	*optarg;
extern int	optind, opterr, optopt;
char		*g_file;
unsigned int	g_mode;

#define OPTION_STRING   "bc:dvVh"

/* check arg, set variables as necessary, and return */
int	checkopt(int argc, char **argv)
{
	int	c;
	int	option_index = 0;

	g_file = NULL;

	while (1)
	{
		static struct option long_options[] = {
		{"background", 0, 0, 'b'},
		{"config", 1, 0, 'c'},
		{"debug", 0, 0, 'd'},
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'V'},
		{"verbose", 0, 0, 'v'},
		{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, OPTION_STRING,
				long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'b':
				g_mode |= DAEMON;
				break;
			case 'c':
				g_file = optarg;
				break;
			case 'd':
				g_mode |= DEBUG;
				break;
			case 'h':
				usage();
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
	printf("iyelld version %s\n", VERSION);
	printf("iyelld [-c file] [-v] [-h]\n");
	printf("\t -b --background\tgo background (daemon mode).\n");
	printf("\t -c --config f\t use f as configuration file.\n");
	printf("\t -d --debug\t debug mode.\n");
	printf("\t -h --help\t this screen.\n");
	printf("\t -v --verbose\t verbose mode.\n");
	printf("\t -V --version\t show version and exit.\n");
	exit(NOERROR);
}

/* print version and exit */
void	version(void)
{
	printf("iyelld version %s\n", VERSION);
	exit(NOERROR);
}

/*
 * Daemonize the current process
 * Check if we are in daemon mode first.
 */

void	daemonize(void)
{
	pid_t pid, sid;

	/* already a daemon */
	if (getppid() == 1)
		return;

	/* duplicate and quit the parrent */
	pid = fork();
	if (pid < 0)
		fatal("cannot fork()");
	if (pid > 0)
		exit(NOERROR);

	umask(0);

	sid = setsid();
	if (sid < 0)
		fatal("cannot setsid()");

	if ((chdir("/")) < 0)
		fatal("cannot chdir()");

	/* redirect everything to the void */
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
}
