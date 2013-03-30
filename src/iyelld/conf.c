/*
 *  conf.c
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

#ifdef	HAVE_CONFIG_H
#include	"config.h"
#endif
#include	<fcntl.h>
#include	<stdio.h>
#include	<stdint.h>
#include	<stdlib.h>
#define _GNU_SOURCE
#include	<string.h>
#include	<unistd.h>
#include	<errno.h>
#include	"conf.h"
#include	"main.h"

#define	MAIN_INITIAL_CAPACITY	16
#define	SUB_INITIAL_CAPACITY	4

/* prototypes */
char	*conf_search(void);
int	conf_ini_read(FILE *conf, conf_t *mem);
int	conf_lua_read(char *conf, conf_t *mem);

/*
 *	Remove the current configuration from memory
 *	usefull before quiting, or before reloading the conf.
 */

void	conf_erase(conf_t *conf)
{
	hash_text_erase(conf->global);
	hash_text_erase(conf->cmd);
	hash_text_erase(conf->hooks);
	hash_text_erase(conf->forward);
	hash_text_erase(conf->transmit);
}

/*
 *	reinit configuration
 */

int	conf_reinit(conf_t *conf)
{
	conf_erase(conf);
	return conf_init(conf);
}

/*
 *	Dump the current configuration in stdout
 *	usefull for the test stuff program
 */

void	conf_dump(conf_t *conf)
{
	conf_dump_section(conf->global);
	conf_dump_section(conf->cmd);
	conf_dump_section(conf->hooks);
	conf_dump_section(conf->forward);
	conf_dump_section(conf->transmit);
}

void	conf_dump_section(hash_t *mem)
{
	char		**data;
	struct record_s	*recs;
	unsigned int	c, i, j;

	printf("===\n");
	/* check if we have something */
	if (! mem)
		return ;

	recs = mem->records;
	for (i = 0, c = 0; c < mem->records_count; i++) {
		if (recs[i].hash != 0 && recs[i].key) {
			c++;
			printf("--- %s: ", recs[i].key);
			data = recs[i].value;
			for (j = 0; data[j] != NULL; j++)
				printf("\"%s\" ", data[j]);
			printf("\n");
		}
	}
}

/*
 *	Read the configuration from the file
 *	return 0 if no errors
 *	return -1 on errors
 */

int	conf_init(conf_t *conf)
{
	hash_t	*hash;

	conf->global = NULL;
	hash = hash_new(MAIN_INITIAL_CAPACITY);
	if (hash == NULL)
		return -1;
	conf->global = hash;

	conf->cmd = NULL;
	hash = hash_new(SUB_INITIAL_CAPACITY);
	if (hash == NULL)
		return -1;
	conf->cmd = hash;

	conf->hooks = NULL;
	hash = hash_new(SUB_INITIAL_CAPACITY);
	if (hash == NULL)
		return -1;
	conf->hooks = hash;

	conf->forward = NULL;
	hash = hash_new(SUB_INITIAL_CAPACITY);
	if (hash == NULL)
		return -1;
	conf->forward = hash;

	conf->transmit = NULL;
	hash = hash_new(SUB_INITIAL_CAPACITY);
	if (hash == NULL)
		return -1;
	conf->transmit = hash;

	return 0;
}

/*
 * search for existing configuration files
 * return an allocated string containing the path of the configuration file
 * on error: return NULL
 */

char	*conf_search(void)
{
	FILE	*conf;
	char	*tmp;
	char	*dup;

	/* if no file given, try env, then home directy, then system side */
	tmp = getenv(CONF_ENV);
	if (tmp != NULL) {
		dup = strdup(tmp);
		if (dup != NULL) {
			conf = fopen(dup, "r");
			if (conf) {
				fclose(conf);	
				return dup;
			}
			free(dup);
		}
	}
#if !defined (_WIN32) && !defined (_WIN64)
	tmp = getenv("HOME");
	if (tmp != NULL) {
		dup = strdup(tmp);
		if (dup != NULL) {
			tmp = malloc((strlen(dup) + \
					strlen(CONF_HOME) + 1) * sizeof(*tmp));
			if (tmp != NULL) {
				strcpy(tmp, dup);
				strcpy(tmp + strlen(dup), CONF_HOME);
				conf = fopen(tmp, "r");
				free(tmp);
				if (conf) {
					fclose(conf);
					return dup;
				}
				free(dup);
			}
			else {
				free(dup);
			}
		}
	}
#endif
	dup = strdup(DEFAULT_CONF);
	if (dup == NULL) {
		log_err("[c] strdup() error: %s\n", strerror(errno));
		return NULL;
	}
	conf = fopen(dup, "r");
	if (conf) {
		fclose(conf);
		return dup;
	}
	return NULL;
}

int	conf_read(conf_t *mem, char *file)
{
	FILE	*conf;
	char	*found = NULL;
	int	error = 0;

	/* if no file given, try env, then home directy, then system side */
	if (file == NULL) {
		found = conf_search();
		if (found == NULL) {
			log_err("[c] cannot find any configuration file\n");
			return -1;
		}
	}

	if (file == NULL)
		file = found;
	conf = fopen(file, "r");
	if (conf == NULL) {
		log_err("[c] cannot open configuration file: %s\n", strerror(errno));
		return -1;
	}
	if (g_mode & VERBOSE)
		log_msg("[c] using configuration file: %s\n", file);

#ifdef HAVE_LIBLUA
	if (g_mode & LUA)
		error = conf_lua_read(file, mem);
	else
#endif
		error = conf_ini_read(conf, mem);
	fclose(conf);
	free(found);
	return 0;
}
