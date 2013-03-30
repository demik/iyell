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
#include "quotes.h"

/* global variables needed for getopt */
extern char	*optarg;
extern int	optind, opterr, optopt;
char		*g_database;
unsigned int	g_mode;

/* prototypes */
void	usage(void);
void	version(void);

#define	OPTION_STRING	"df:ghlsvV"

/* check arg, set variables as necessary, and return */
int	checkopt(int argc, char **argv)
{
	int	c;
	int	option_index = 0;
	
	g_database = NULL;
	
	while (1)
	{
		static struct option long_options[] = {
		{"debug", 0, 0, 'd'},
		{"file", 1, 0, 'f'},
		{"global", 1, 0, 'g'},
		{"help", 0, 0, 'h'},
		{"log", 0, 0, 'l'},
		{"strip", 0, 0, 's'},
		{"version", 0, 0, 'V'},
		{"verbose", 0, 0, 'v'},
		{0, 0, 0, 0}
		};
		
		c = getopt_long(argc, argv, OPTION_STRING,
				long_options, &option_index);
		
		if (c == -1)
			break;
		
		switch (c) {
			case 'd':
				g_mode |= DEBUG;
				break;
			case 'f':
				g_database = optarg;
				break;
			case 'g':
				g_mode |= GLOBAL;
				break;
			case 'h':
				usage();
				break;
			case 'l':
				g_mode |= SYSLOG;
				break;
			case 's':
				g_mode |= STRIP;
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

	if (g_database == NULL)
		return -1;
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
	printf("iyell/quotes version %s\n", VERSION);
	printf("iyell/quotes <-f file> [-o] [--options]\n");
	printf("\t -d --debug\t debug mode: print errors on stderr.\n");
	printf("\t -f --file f\t use file as database. (required)\n");
	printf("\t -g --global\t global database mode.\n");
	printf("\t -h --help\t this screen.\n");
	printf("\t -l --log\t log to syslog.\n");
	printf("\t -s --strip\t strip nick from answers if possible.\n");
	printf("\t -v --verbose\t verbose mode.\n");
	printf("\t -V --version\t show version and exit.\n");
	exit(NOERROR);
}

/* print version and exit */
void	version(void)
{
	printf("iyell/quotes version %s\n", VERSION);
	exit(NOERROR);
}
