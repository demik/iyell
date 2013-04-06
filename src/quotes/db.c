/*
 *  db.c
 *  iyell/quotes
 *
 *  Created by Michel DEPEIGE on 24/02/2009.
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

#define _GNU_SOURCE
#define _BSD_SOURCE
#define __BSD_VISIBLE 1
#define _DARWIN_C_SOURCE 1
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sqlite3.h>
#include <event.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "main.h"
#include "quotes.h"

/* heap */
sqlite3		*db = NULL;

/* prototypes */

/*
 * queries
 *
 * _C defines are used for per channel quotes (!GLOBAL)
 */

#define DB_CHECK_CONFIG "CREATE TABLE IF NOT EXISTS config " \
			"(setting TEXT, value TEXT, num INTEGER)"
#define DB_CHECK_QUOTES	"CREATE TABLE IF NOT EXISTS quotes ( data TEXT, " \
			"time DATE DEFAULT (datetime('now','localtime')), "\
			"nick TEXT, channel TEXT)"

#define DB_ADD		"INSERT INTO quotes VALUES('%q', datetime('now','localtime'), '%q', '%q')"
#define DB_CFG_RANDOM	"SELECT value, num FROM config"
#define DB_DEL		"SELECT data FROM quotes WHERE rowid = '%q'; " \
			"DELETE FROM quotes WHERE rowid = '%q'"
#define DB_DEL_C	"SELECT data FROM quotes WHERE rowid = '%q' AND channel = '%q'; " \
			"DELETE FROM quotes WHERE rowid = '%q' AND channel = '%q';"
#define DB_COUNT	"SELECT COUNT('data') FROM quotes"
#define DB_COUNT_C	DB_COUNT " WHERE channel = '%q'"
#define DB_ID		"SELECT data FROM quotes WHERE rowid = '%q'"
#define DB_ID_C		DB_ID " AND channel = '%q'"
#define DB_INFO		"SELECT rowid, time, nick, channel FROM quotes " \
			"WHERE rowid = '%q' LIMIT 1"
#define DB_INFO_C	"SELECT rowid, time, nick, channel FROM quotes " \
			"WHERE rowid = '%q' AND channel = '%q' LIMIT 1"
#define DB_INTERVAL	"SELECT count(num), num FROM config WHERE setting = 'random' AND value = '%q'"
#define DB_LIKE		"SELECT data, rowid, COUNT(rowid) FROM quotes " \
			"WHERE data LIKE '%q' LIMIT 1"
#define DB_LIKE_C	"SELECT data, rowid, COUNT(rowid) FROM quotes " \
			"WHERE data LIKE '%q' AND channel = '%q' LIMIT 1"
#define DB_RANDOM	"SELECT data FROM quotes WHERE data GLOB '%q' "\
			"ORDER BY RANDOM() LIMIT 1"
#define DB_RANDOM_C	"SELECT data FROM quotes WHERE data GLOB '%q' AND channel = '%q' "\
			"ORDER BY RANDOM() LIMIT 1"
#define DB_SEARCH	"SELECT data, rowid, COUNT(rowid) FROM quotes " \
			"WHERE data GLOB '%q' LIMIT 1"
#define DB_SEARCH_C	"SELECT data, rowid, COUNT(rowid) FROM quotes " \
			"WHERE data GLOB '%q' AND channel = '%q' LIMIT 1"

/* functions */

/* add an entry */
char	*db_add(char *nick, char *channel, char *args)
{
	char	*query;
	char	*sql_error = NULL;

	query = sqlite3_mprintf(DB_ADD, args, nick, channel);
	sqlite3_exec(db, query, NULL, NULL, &sql_error);
	sqlite3_free(query);
	return sql_error;
}

/* db config setting : random quotes */
char	*db_config_random()
{
	char    *sql_error = NULL;

	sqlite3_exec(db, DB_CFG_RANDOM, quotes_random_cb, NULL, &sql_error);
	return sql_error;
}

/* count quotes stored */
char	*db_count_quotes(char *nick, char *channel, char *args)
{
	char	*source[2];
	char	*sql_error = NULL;

	args = NULL;
	source[0] = nick;
	source[1] = channel;
	if (g_mode & GLOBAL) {
		sqlite3_exec(db, DB_COUNT, cmd_count_cb, source, &sql_error);
	}
	else {
		char *query = sqlite3_mprintf(DB_COUNT_C, channel);
		sqlite3_exec(db, query, cmd_count_cb, source, &sql_error);
		sqlite3_free(query);
	}
	return sql_error;
}

/* delete an entry */
char	*db_del(char *nick, char *channel, char *args)
{
	char	*query;
	char	*source[2];
	char	*sql_error = NULL;

	source[0] = nick;
	source[1] = channel;
	if (g_mode & GLOBAL)
		query = sqlite3_mprintf(DB_DEL, args, args);
	else
		query = sqlite3_mprintf(DB_DEL_C, args, channel, args, channel);
	sqlite3_exec(db, query, cmd_del_cb, source, &sql_error);
	sqlite3_free(query);
	return sql_error;
}

/*
 * memory usage stuff
 * used by cmd_memory() in cmd.c for memory usage information
 */

short	db_get_memory_highwater(char **unit)
{
	sqlite3_int64	mem;
	short		s;
	char		*u[] = { "B", "KiB", "MiB" };

	mem = sqlite3_memory_highwater(0);
	for (s = 0; s < 2 && mem > 1024; s++) {
		mem /= 1024;
	}

	*unit = u[s];
	return (short)mem;
}

short	db_get_memory_used(char **unit)
{
	sqlite3_int64	mem;
	short		s;
	char		*u[] = { "B", "KiB", "MiB" };

	mem = sqlite3_memory_used();
	for (s = 0; s < 2 && mem > 1024; s++) {
		mem /= 1024;
	}

	*unit = u[s];
	return (short)mem;
}

/* display quote by id */
char	*db_id(char *nick, char *channel, char *args)
{
	char	*query;
	char	*source[2];
	char	*sql_error = NULL;

	if (g_mode & GLOBAL)
		query = sqlite3_mprintf(DB_ID, args);
	else
		query = sqlite3_mprintf(DB_ID_C, args, channel);
	source[0] = nick;
	source[1] = channel;
	sqlite3_exec(db, query, cmd_data_cb, source, &sql_error);
	sqlite3_free(query);
	return sql_error;
}

/* search a specific quote */
char	*db_info(char *nick, char *channel, char *args)
{
	char	*query;
	char	*source[2];
	char	*sql_error = NULL;

	if (g_mode & GLOBAL)
		query = sqlite3_mprintf(DB_INFO, args);
	else
		query = sqlite3_mprintf(DB_INFO_C, args, channel);
	source[0] = nick;
	source[1] = channel;
	sqlite3_exec(db, query, cmd_info_cb, source, &sql_error);
	sqlite3_free(query);
	return sql_error;
}

/* search a specific quote */
char	*db_interval(char *nick, char *channel, char *args)
{
	char	*query;
	char	*source[2];
	char	*sql_error = NULL;

	query = sqlite3_mprintf(DB_INTERVAL, args);
	source[0] = nick;
	source[1] = channel;
	sqlite3_exec(db, query, cmd_interval_cb, source, &sql_error);
	sqlite3_free(query);
	return sql_error;
}

/* open and check database consistency */
short	db_init(char *file)
{
	char	*error = NULL;

	if (sqlite3_open(file, &db)) {
		log_err("[d] cannot init database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return ERROR;
	}

	/* check if quote and config tables exists */
	if (g_mode & VERBOSE)
		log_msg("[d] checking database consistency...\n");

	if (sqlite3_exec(db, DB_CHECK_QUOTES, NULL, NULL, &error)) {
		log_err("[d] SQL error: %s\n", error);
		sqlite3_free(error);
		sqlite3_close(db);
		return ERROR;
	}

	if (sqlite3_exec(db, DB_CHECK_CONFIG, NULL, NULL, &error)) {
		log_err("[d] SQL error: %s\n", error);
		sqlite3_free(error);
		sqlite3_close(db);
		return ERROR;
	}

	return NOERROR;
}

/* search a specific quote (using LIKE) */
char	*db_like(char *nick, char *channel, char *args)
{
	char	*query;
	char	*source[2];
	char	*sql_error = NULL;

	if (g_mode & GLOBAL)
		query = sqlite3_mprintf(DB_LIKE, args);
	else
		query = sqlite3_mprintf(DB_LIKE_C, args, channel);
	source[0] = nick;
	source[1] = channel;
	sqlite3_exec(db, query, cmd_search_cb, source, &sql_error);
	sqlite3_free(query);
	return sql_error;
}

/* close database */
void	db_end(void)
{
	if (g_mode & VERBOSE)
		log_msg("[d] closing database\n");
	sqlite3_close(db);
}

/* a wrapper around sqlite3_last_insert_rowid(sqlite3*) */
uintmax_t	db_last_insert_id()
{
	return (uintmax_t)sqlite3_last_insert_rowid(db);
}

/* print out a random quote */
char	*db_random(char *nick, char *channel, char *args)
{
	char	*query;
	char	*source[2];
	char	*sql_error = NULL;

	if (g_mode & GLOBAL)
		query = sqlite3_mprintf(DB_RANDOM, args);
	else
		query = sqlite3_mprintf(DB_RANDOM_C, args, channel);
	source[0] = nick;
	source[1] = channel;
	sqlite3_exec(db, query, cmd_data_cb, source, &sql_error);
	sqlite3_free(query);
	return sql_error;
}

/* search a specific quote */
char	*db_search(char *nick, char *channel, char *args)
{
	char	*query;
	char	*source[2];
	char	*sql_error = NULL;

	if (g_mode & GLOBAL)
		query = sqlite3_mprintf(DB_SEARCH, args);
	else
		query = sqlite3_mprintf(DB_SEARCH_C, args, channel);
	source[0] = nick;
	source[1] = channel;
	sqlite3_exec(db, query, cmd_search_cb, source, &sql_error);
	sqlite3_free(query);
	return sql_error;
}

