/*
 *  tools.h
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

#ifndef CONF_H_
#define CONF_H_

#include "hash.h"

/* defines */

#define TRUE		1
#define FALSE		0
#define MAX_TOKENS      128
#if !defined (_WIN32) && !defined (_WIN64)
#       ifdef   HAVE_CONFIG_H
/* PACKAGE_SYSCONF_DIR is defined by autoconf in config.h */
#       define  DEFAULT_CONF    PACKAGE_SYSCONF_DIR "iyell.ini"
#       else
#       define  DEFAULT_CONF    "./iyell.ini"
#       endif
#else
#define DEFAULT_CONF    "C:\\iyell.ini"
#endif
#define CONF_ENV        "IYELL_CONF"
#define CONF_HOME       "/.iyell.ini"

/* structures */
typedef struct conf_s {
	hash_t	*global;
	hash_t	*cmd;
	hash_t	*hooks;
	hash_t	*forward;
	hash_t	*transmit;
}               conf_t;

/* hash structures */
struct record_s {
	unsigned int hash;
	const char *key;
	void *value;
};

struct hash_s {
	struct record_s *records;
	unsigned int records_count;
	unsigned int size_index;
};

/* prototypes */

void    conf_dump(conf_t *conf);
void    conf_dump_section(hash_t *mem);
void    conf_erase(conf_t *mem);
int     conf_save(conf_t *mem, char *file);
char    **conf_get(conf_t *mem, char *key);
int	conf_init(conf_t *mem);
int	conf_reinit(conf_t *mem);
int     conf_read(conf_t *mem, char *file);

/* hash+ stuff */
int	hash_text_insert(hash_t *mem, char *key, char *data);
void	hash_text_delete(hash_t *mem, char *key);
void	hash_text_erase(hash_t *mem);
void	hash_text_dump(hash_t *mem);
int	hash_text_count_data(hash_t *mem, char *key);
char	*hash_text_get_first(hash_t *mem, char *key);
int	hash_text_is_false(hash_t *mem, char *key);
int	hash_text_is_true(hash_t *mem, char *key);

#endif
