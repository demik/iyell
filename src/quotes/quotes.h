/*
 *  quotes.h
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

#ifndef QUOTES_H
#define QUOTES_H

/*
 * defines
 * first 24 bits are reserved to the core FLAGS
 * custom flags are 24-32
 */

#define	STRIP	(1 << 25)
#define	GLOBAL	(1 << 26)	/* per channel mode */

/* heap */
extern char	*g_database;
extern char	*g_name;

/* prototypes */
/* from cmd.c */
int	cmd_count_cb(void *source, int argc, char **argv, char **col_name);
int	cmd_data_cb(void *source, int argc, char **argv, char **col_name);
int	cmd_del_cb(void *source, int argc, char **argv, char **col_name);
int	cmd_info_cb(void *source, int argc, char **argv, char **col_name);
int	cmd_interval_cb(void *source, int argc, char **argv, char **col_name);
int	cmd_search_cb(void *source, int argc, char **argv, char **col_name);

/* from db.c */
char		*db_add(char *nick, char *channel, char *args);
char		*db_config_random(void);
char		*db_count_quotes(char *nick, char *channel, char *args);
char		*db_del(char *nick, char *channel, char *args);
char		*db_id(char *nick, char *channel, char *args);
char		*db_info(char *nick, char *channel, char *args);
char		*db_interval(char *nick, char *channel, char *args);
char		*db_like(char *nick, char *channel, char *args);
short		db_get_memory_highwater(char **unit);
short		db_get_memory_used(char **unit);
short		db_init(char *file);
void		db_end(void);
uintmax_t	db_last_insert_id();
char		*db_random(char *nick, char *channel, char *args);
char		*db_search(char *nick, char *channel, char *args);

/* from io.c */
void	io_loop(void);
void	io_shutdown(void);
#ifdef _EVENT_H_
void	io_strip_nick(struct evbuffer *out, char *nick, char *channel);
#endif
void	io_toggle_write(void);

/* from main.c */
int	quotes_random_cb(void *source, int argc, char **argv, char **col_name);
void	quotes_stop_timers(void);

/* from opt.c */
void    fatal(char *str);

/* from signal.c */
void	sig_set_handlers(void);
void	sig_unset_handlers(void);

#endif
