/*
 *  hash_text.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 02/10/2008.
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

#define	INITIAL_CAPACITY	20

/* misc hash stuff */
static inline unsigned int strhash(const char *str)
{
	int c;
	int hash = 5381;
	while ((c = *str++))
		hash = hash * 33 + c;
	return hash == 0 ? 1 : hash;
}

/*
	Insert a key and the associated data in the configuration.
	return 0 if no problems.
	return -1 if problems
*/

int	hash_text_insert(hash_t *mem, char *key, char *data)
{
	char		**hash_data;
	char		*new_data, *new_key = NULL;
	int		ret;
	struct record_s	*recs;
	unsigned int	i, c, code, count = 0;

	if (! mem)
		return -1;

	/* save new data */
	new_data = strdup(data);
	if (new_data == NULL)
		return -1;
	hash_data = hash_get(mem, key);

	/* data does not exist, create a new aray as hash value */
	if (hash_data == NULL) {
		new_key = strdup(key);
		if (new_key == NULL)
			return -1;
		hash_data = malloc(2 * sizeof(char *));
		hash_data[0] = new_data;
		hash_data[1] = NULL;
		ret = hash_add(mem, new_key, hash_data);
		if (ret)
			return ret;
	}
	/* data for this key already exist */
	/* append data at the end of the array */
	else {
		count = hash_text_count_data(mem, key);
		hash_data = realloc(hash_data, (2 * sizeof(char *)) + \
			       count * sizeof(char *));
		if (hash_data == NULL)
			return -1;
		hash_data[count] = new_data;
		hash_data[count + 1] = NULL;

		/* find the old allocated key */
		recs = mem->records;
		code = strhash(key);
		for (i = 0, c = 0; c < 1; i++) {
			if ((code == recs[i].hash) && recs[i].key && \
			    strcmp(key, recs[i].key) == 0) {
				c++;
				new_key = (char *)recs[i].key;
			}
		}

		hash_remove(mem, key);
		ret = hash_add(mem, new_key, hash_data);
		if (ret)
			return ret;
	}

	return 0;
}

/*
	Delete a specific key in the current configuration.
*/

void	hash_text_delete(hash_t *mem, char *key)
{
	char		**data;
	/* malloced key in hash table */
	char		*m_key = NULL;
	struct record_s	*recs;
	unsigned int	c, code, i;

	/* check if we have something */
	if (! mem)
		return ;

	/* backup the key pointer for free() */
	recs = mem->records;
	code = strhash(key);
	for (i = 0, c = 0; c < 1; i++) {
		if ((code == recs[i].hash) && recs[i].key && \
		    strcmp(key, recs[i].key) == 0) {
			c++;
			m_key = (char *)recs[i].key;
		}
	}

	/* remove from hash table and free values */
	data = hash_remove(mem, key);
	if (data == NULL)
		return ;
	for (i = 0; data[i] != NULL; i++)
		free(data[i]);
	free(data);
	free(m_key);
}

/*
	Remove the current configuration from memory
	usefull before quiting, or before reloading the conf.
*/

void	hash_text_erase(hash_t *mem)
{
	char		**data;
	struct record_s	*recs;
	unsigned int	c, i, j;

	/* check if we have something */
	if (! mem)
		return ;

	recs = mem->records;
	for (i = 0, c = 0; c < mem->records_count; i++) {
		if (recs[i].hash != 0 && recs[i].key) {
			c++;
			free((char *)recs[i].key);
			data = recs[i].value;
			for (j = 0; data[j] != NULL; j++)
				free(data[j]);
			free(data);
		}
	}
	hash_destroy(mem);
}

/*
	Dump the current configuration in stdout
	usefull for the test stuff program
*/

void	hash_text_dump(hash_t *mem)
{
	char		**data;
	struct record_s	*recs;
	unsigned int	c, i, j;

	/* check if we have something */
	if (! mem)
		return ;

	recs = mem->records;
	for (i = 0, c = 0; c < mem->records_count; i++) {
		if (recs[i].hash != 0 && recs[i].key) {
			c++;
			printf("%s: ", recs[i].key);
			data = recs[i].value;
			for (j = 0; data[j] != NULL; j++)
				printf("\"%s\" ", data[j]);
			printf("\n");
		}
	}
}

/*
	Count the number of results for a specified key
*/

int	hash_text_count_data(hash_t *mem, char *key)
{
	int		found = 0;
	unsigned int	i;
	char		**data;

	if (!mem)
		return 0;

	data = hash_get(mem, key);
	if (data == NULL)
		return found;
	for (i = 0; data[i] != NULL; i++)
		found++;
	return found;
}

/*
	this fonction is used to get settings
	return the first data for *key
 */

char	*hash_text_get_first(hash_t *mem, char *key)
{
	char	**data;

	if (mem == NULL)
		return NULL;
	data = hash_get(mem, key);
	if (data == NULL)
		return NULL;
	return data[0];
}

/*
	this function return 1 if they data for the key
	is either "no", "0", "false" or "non"
	others values are considered as true
 	return 0 if setting unavaible or malloc error
 */

int	hash_text_is_false(hash_t *mem, char *key)
{
	char	*tmp;

	tmp = hash_text_get_first(mem, key);
	if (tmp == NULL)
		return 0;
	if ((strcmp(tmp, "no") == 0) || (strcmp(tmp, "false") == 0) || \
	    (strcmp(tmp, "0") == 0) || (strcmp(tmp, "non") == 0)) {
		return 1;
	}
	return 0;
}

/*
	this function return 1 if they data for the key
	is either "yes", "1", "true" or "oui"
	others values are considered as false
 	return 0 if setting unavaible or malloc error
 */

int	hash_text_is_true(hash_t *mem, char *key)
{
	char	*tmp;

	tmp = hash_text_get_first(mem, key);
	if (tmp == NULL)
		return 0;
	if ((strcmp(tmp, "yes") == 0) || (strcmp(tmp, "true") == 0) || \
	    (strcmp(tmp, "1") == 0) || (strcmp(tmp, "oui") == 0)) {
		return 1;
	}
	return 0;
}

