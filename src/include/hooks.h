/*
 *  hooks.h
 *  iyell
 *
 *  Created by Michel DEPEIGE on 28/10/2008.
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

#ifndef HOOKS_H
#define HOOKS_H

/* defines **/
#define HOOKS_TIMEOUT_SEC 15

/* structures */
typedef	struct	hook_s {
	int			pid;
	char			*path;
	struct bufferevent	*read;
	struct bufferevent	*write;
	struct evbuffer		*data;
	struct hook_s		*next;
}		hook_t;

extern	hook_t	*ghook;

/* prototypes */
int	hooks_add(hook_t **first, hook_t *new);
void	hooks_broadcast_sig(hook_t *h, int sig);
int	hooks_count(hook_t *h);
void	hooks_cmd_switch(int argc, char **argv, char *who, char *where);
void	hooks_cmd_oneshot(char *cmd, char **exec, int argc, char **argv, char *who, char *where);
hook_t	*hooks_cmd_pipe(char *cmd, char **exec);
int	hooks_delete(hook_t **first, hook_t *trash);
void	hooks_init(hook_t **first);
hook_t	*hooks_search(hook_t *first, char *path);
void	hooks_shutdown(void);

/* prototypes callbacks */
void	hooks_read(struct bufferevent *ev, void *arg);
void	hooks_write(struct bufferevent *ev, void *arg);
void	hooks_generic_error(struct bufferevent *ev, short what, void *arg);
void	hooks_oneshot_error(struct bufferevent *ev, short what, void *arg);
void	hooks_pipe_error(struct bufferevent *ev, short what, void *arg);

#endif
