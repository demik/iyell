/*
 *  conf_ini.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 02/10/08.
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
#define _BSD_SOURCE
#define __BSD_VISIBLE 1
#define _DARWIN_C_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include "conf.h"	
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "main.h"

#define	SEC_NONE	0
#define SEC_GLOBAL	1
#define SEC_CMD		2
#define	SEC_HOOKS	3
#define	SEC_FORWARD	4
#define	SEC_TRANSMIT	5

/* prototypes */
void	conf_ini_check_parameter(char *str, hash_t *mem, int line);
short	conf_ini_check_section(char *str, int line);
int	conf_ini_read(FILE *f, conf_t *conf);
int	conf_ini_save(conf_t *mem, char *file);
int	conf_ini_save_section(hash_t *mem, FILE *out);

/* load an init file */
int	conf_ini_read(FILE *f, conf_t *conf)
{
	char		tmp[BUFF_SIZE];
	short		section = SEC_NONE;
	unsigned int	line = 0;

	while (fgets(tmp, BUFF_SIZE, f) != NULL) {
		/* skip comments */
		if (tmp[0] == '#' || tmp[0] == '\r' || tmp[0] == '\n' || \
		    tmp[0] == ';' || tmp[0] == '\0')
			goto next;
		/* check for sections */
		if (tmp[0] == '[') {
			section = conf_ini_check_section(tmp, line);
			goto next;
		}

		switch (section) {
			case SEC_GLOBAL: {
				conf_ini_check_parameter(tmp, conf->global, line);	
				break;
			}
			case SEC_CMD: {
				conf_ini_check_parameter(tmp, conf->cmd, line);
				break;
			}
			case SEC_HOOKS: {
				conf_ini_check_parameter(tmp, conf->hooks, line);
				break;
			}
			case SEC_FORWARD: {
				conf_ini_check_parameter(tmp, conf->forward, line);
				break;
			}
			case SEC_TRANSMIT: {
				conf_ini_check_parameter(tmp, conf->transmit, line);
				break;
			}
			default: {
				log_msg("[c] line %i outside known section: %s", line, tmp);
			}
		}
next:
		line++;
	}
	return 0;
}

/*
 * check if an entry is valid$
 * if valid : insert entry in hash table
 * if not : print a warning
 * return nothing
 */

void	conf_ini_check_parameter(char *str, hash_t *mem, int line)
{
	char		*data[MAX_TOKENS];
	char		*key, *token;
#ifdef HAVE_STRTOK_R
	char		*saveptr;
#endif
	unsigned short	i;
	size_t		len;

	/* remove comments */
	str = strsep(&str, ";");

	/* split key and data */
	key = strsep(&str, " =\t");
	if (str == NULL) {
		log_err("[c] invalid parameter declaration line %i: %s", line, key);
		return ;
	}

	/* split data */
	for (i = 0; i < (MAX_TOKENS - 1); i++, str = NULL) {
		#ifdef HAVE_STRTOK_R
		token = strtok_r(str, "=\t\r,\n", &saveptr);
		#else           
		token = strtok(str, "=\t\r,\n");
		#endif
		if (token == NULL)
			break;

		/* skip spaces before token */
		while (*token == ' ')
			token++;

		/* remove spaces after token */
		len = strlen(token);
		while (--len && (token[len] == ' '))
			token[len] = '\0';

		data[i] = token;
	}
	data[i] = NULL;

	/* consistency checks */
	if (i < 1) {
		log_err("[c] missing data or empty line %i\n", line);
		return ;
	}

	/* insert data */
	for (i = 0; data[i] != NULL; i++) {
		hash_text_insert(mem, key, data[i]);
	}
}

/* 
 * check if a section is valid
 * return: the section id.
 */

short	conf_ini_check_section(char *str, int line)
{
	char	*sections[] = { NULL, "global", "cmd", "hooks", "forward", "transmit"};
	short	i;
	char	*name;

	str++;
	name = strsep(&str, "]");
	for (i = 1; i < 6; i++) {
		if (strcmp(name, sections[i]) == 0) {
			return i;
		}
	}

	/* if we are here, then there is a syntax error */
	log_err("[c] invalid section declaration line %i: %s\n", line, name);
	return SEC_NONE;
}

/*
 *	save the current configuration to the specified file
 *	usefeull for a gui, unused otherwise
 */

int	conf_ini_save(conf_t *mem, char *file)
{
	FILE		*out;

	out = fopen(file, "w+");
	if (!out)
		return -1;

	fwrite((void *)"[global]\n", 9, 1, out);
	conf_ini_save_section(mem->global, out);
	fwrite((void *)"\n", 1, 1, out);
	fwrite((void *)"[cmd]\n", 6, 1, out);
	conf_ini_save_section(mem->cmd, out);
	fwrite((void *)"\n", 1, 1, out);
	fwrite((void *)"[hooks]\n", 6, 1, out);
	conf_ini_save_section(mem->hooks, out);
	fwrite((void *)"\n", 1, 1, out);
	fwrite((void *)"[forward]\n", 10, 1, out);
	conf_ini_save_section(mem->forward, out);
	fwrite((void *)"\n", 1, 1, out);
	fwrite((void *)"[transmit]\n", 9, 1, out);
	conf_ini_save_section(mem->transmit, out);
	fwrite((void *)"\n", 1, 1, out);
	fclose(out);

	return 0;
}

int	conf_ini_save_section(hash_t *mem, FILE *out)
{
	char		**data;
	struct record_s	*recs;
	unsigned int	c, i, j;

	if (! mem)
		return -1;

	recs = mem->records;
	for (i = 0, c = 0; c < mem->records_count; i++) {
		if (recs[i].hash != 0 && recs[i].key) {
			c++;
			fwrite((void *)recs[i].key, strlen(recs[i].key), 1, out);
			fwrite((void *)" = ", 3, 1, out);
			data = recs[i].value;
			for (j = 0; data[j] != NULL; j++) {
				fwrite((void *)data[j], strlen(data[j]), 1, out);
				if (data[j + 1] != NULL)
					fwrite((void *)", ", 2, 1, out);
			}
			fwrite((void *)"\n", 1, 1, out);
		}
	}

	return 0;
}
