/*
 *  hooks.c
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

#define __BSD_VISIBLE 1
#define _BSD_SOURCE 1
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#include "irc.h"
#include "heap.h"
#include "hooks.h"
#include "main.h"
#include "tools.h"

/* global */
hook_t	*ghook;

/* static prototypes */
static void	hooks_build_argv(char *cmd, char **exec);
static char	**hooks_mangle_oneshot_argv(char **exec, int argc, char **argv, \
					  char *who, char *where);
static void	hooks_send(struct hook_s *h, int argc, char **argv, \
				char *who, char *where);

/* functions */
void	hooks_destroy(hook_t **h);
void	hooks_dump(hook_t *h);
short	hooks_parse_cmd(char *line);

/*
 * Hooks function management below
 * basicaly a chained list
 * the hook_t is allocated by hooks_cmd_*();
 */

int	hooks_add(hook_t **first, hook_t *new)
{
	assert(new != NULL);

	/* build data */

	/* add to the list */
	new->next = *first;
	*first = new;

	return 0;
}

/*
 * broadcast a specified signal to all active child
 * can be a piped hook or a standard hook.
 */

void	hooks_broadcast_sig(hook_t *h, int sig)
{
	if (g_mode & VERBOSE)
		log_msg("[h] broadcasting signal %i to every hook\n");

	while (h != NULL) {
		if (kill(h->pid, sig) == -1)
			log_err("[h] kill() error: %s\n", strerror(errno));
		h = h->next;
	}
}

/*
 * build on array of arguments for a child
 *
 * example: test -f foo
 * result[0] = test, result[1] = -f, result[2] = foo
 */

static void	hooks_build_argv(char *cmd, char **result)
{
	unsigned int	i;
#ifdef HAVE_STRTOK_R
	char		*saveptr;
#endif

	/* split cmd and args from path	*/
	for (i = 0; i < MAX_TOKENS; i++, cmd = NULL) {
#ifdef HAVE_STRTOK_R
		result[i] = strtok_r(cmd, " \t\r\n", &saveptr);
#else
		result[i] = strtok(cmd, " \t\r\n");
#endif
		if (result[i] == NULL)
			break ;
	}
	result[MAX_TOKENS - 1] = NULL;
}

/*
 * execute switch function.
 * check is the hook is valid
 * determine the type of hook :
 * - simple command : launch & grab stdout
 * - pipe : launch if not launched and grad stdout
 */

void	hooks_cmd_switch(int argc, char **argv, char *who, char *where)
{
	char		**list;
	unsigned int	i;
	char		*exec[MAX_TOKENS];
	char		*cmd;
	struct hook_s	*h;

	assert(argv != NULL);
	assert(argv[0] != NULL);

	list = hash_get(g_conf.cmd, argv[0]);

	for (i = 0; list[i] != NULL; i++) {
		/*
		 * looks like a dual simplex pipe (stdin & stdout from child )
		 * we send data throug stdin and receive data from stdout
		 */

		if (list[i][0] == '|') {
			/* find if the hook is launched first */
			if ((h = hooks_search(ghook, list[i])) == NULL) {
				cmd = strdup(list[i]);
				if (cmd == NULL) {
					log_err("[h] strdup() error: %s\n", strerror(errno));
					continue ;
				}

				hooks_build_argv(cmd + sizeof(char), exec);

				/* shound't happend */
				assert(exec[0] != NULL);
				h = hooks_cmd_pipe(list[i], exec);
				free(cmd);
			}

			/* send data */
			hooks_send(h, argc, argv, who, where);
			continue ;
		}

		/* looks like an executable */
		else {
			if ((cmd = strdup(list[i])) == NULL) {
				log_err("[h] strdup() error: %s\n", strerror(errno));
				continue ;
			}
			hooks_build_argv(cmd, exec);

			/* shound't happend */
			assert(exec[0] != NULL);

			hooks_cmd_oneshot(list[i], exec, argc, argv, who, where);
			free(cmd);
			continue ;
		}

		/* unknow stuff */
		log_err("[h] unknow file type for %s, skiping\n", argv[0]);
	}
}

/*
 * execute an external command wich is defined as oneshot:
 * the bot will pass arguments via execv.
 * the child shound send us a result to stdout, and exit.
 */

void	hooks_cmd_oneshot(char *cmd, char **exec, int argc,
			  char **argv, char *who, char *where)
{
	pid_t		pid;
	int		pfp[2];
	struct hook_s	*h;
	char		**tmp;

	if (pipe(pfp) == -1) {
		log_err("[h] pipe() error: %s\n", strerror(errno));
		return ;
	}

	/* the callback will carry this structure, so we need to allocate it */
	h = malloc(sizeof(hook_t));
	if (h == NULL) {
		log_err("[h] malloc() error: %s\n", strerror(errno));
		return ;
	}

	/* init buffers */
	h->read = bufferevent_new(pfp[0], hooks_read, NULL, hooks_oneshot_error, h);
	h->write = NULL;
	h->data = evbuffer_new();
	if (h->read == NULL || h->data == NULL) {
		close(pfp[0]);
		close(pfp[1]);
		log_err("[h] buffer initialisation error: %s\n", strerror(errno));
		free(h);
		return ;
	}
	bufferevent_enable(h->read, EV_READ);

	/* here we go */
	pid = fork();
	if (pid < 0) {
		close(pfp[0]);
		close(pfp[1]);
		bufferevent_disable(h->read, EV_READ);
		log_err("[h] fork() error: %s\n", strerror(errno));
		free(h);
		return ;
	}
	if (pid == 0) {
		/* close unused read end */
		if (close(pfp[0]) == -1) {
			log_err("[h] close() error: %s\n", strerror(errno));
			exit(-1);
		}

		tmp = hooks_mangle_oneshot_argv(exec, argc, argv, who, where);
		log_msg("[h] about to exec %s\n", exec[0]);

		/* redirect stdout to pipe */
		if (dup2(pfp[1], 1) == -1) {
			log_err("[h] dup2() error: %s\n", strerror(errno));
			exit(-1);
		}

		execvp(exec[0], tmp);
		log_err("[h] cannot execute %s: %s\n",
			exec[0], strerror(errno));
		free(tmp);
		exit(errno);
	}
	else {
		h->path = cmd;
		h->pid = pid;
		hooks_add(&ghook, h);

		/* close unused write end */
		if (close(pfp[1]) == -1)
			log_err("[h] close() error: %s\n", strerror(errno));
	}
}

/*
 * execute an external command wich is defined as pipe:
 * the bot will pass arguments via the redirected child stdin.
 * the child shound send us a result to stdout, and stay alive
 * waitinh for other data
 */

hook_t	*hooks_cmd_pipe(char *cmd, char **exec)
{
	pid_t			pid;
	int			pfp_in[2];
	int			pfp_out[2];
	struct hook_s		*h;

	if (pipe(pfp_in) == -1 || pipe(pfp_out) == -1) {
		log_err("[h] pipe() error: %s\n", strerror(errno));
		return NULL;
	}

	/* the callback will carry this structure, so we need to allocate some memory */
	h = malloc(sizeof(hook_t));
	if (h == NULL) {
		log_err("[h] malloc() error: %s\n", strerror(errno));
		return NULL;
	}

	/* init buffers */
	h->read = bufferevent_new(pfp_in[0], hooks_read, NULL, hooks_pipe_error, h);
	h->write = bufferevent_new(pfp_out[1], NULL, hooks_write, hooks_pipe_error, h);
	h->data = evbuffer_new();
	if (h->read == NULL || h->write == NULL || h->data == NULL) {
		close(pfp_in[0]);
		close(pfp_in[1]);
		close(pfp_out[0]);
		close(pfp_out[1]);
		log_err("[h] buffer initialisation error: %s\n", strerror(errno));
		free(h);
		return NULL;
	}
	bufferevent_enable(h->read, EV_READ);
	bufferevent_enable(h->write, EV_WRITE);

	/* here we go */
	pid = fork();
	if (pid < 0) {
		close(pfp_in[0]);
		close(pfp_in[1]);
		close(pfp_out[0]);
		close(pfp_out[1]);
		log_err("[h] fork() error: %s\n", strerror(errno));
		free(h);
		return NULL;
	}
	if (pid == 0) {
		/* close unuseds ends */
		if (close(pfp_in[0]) == -1 || close(pfp_out[1] == -1)) {
			log_err("[h] close() error: %s\n", strerror(errno));
			exit(-1);
		}

		log_msg("[h] about to exec %s\n", exec[0]);
		/* redirect stdin & stdout to pipe */
		if (dup2(pfp_in[1], 1) == -1 || dup2(pfp_out[0], 0) == -1) {
			log_err("[h] dup2() error: %s\n", strerror(errno));
			exit(-1);
		}

		execvp(exec[0], exec);
		log_err("[h] cannot execute %s: %s\n",
			exec[0], strerror(errno));

		exit(errno);
	}
	else {
		h->path = cmd;
		h->pid = pid;
		hooks_add(&ghook, h);

		/* close unused ends */
		if (close(pfp_in[1]) == -1)
			log_err("[h] close() error: %s\n", strerror(errno));
		if (close(pfp_out[0]) == -1)
			log_err("[h] close() error: %s\n", strerror(errno));
	}
	return h;
}

/*
 * count active or presumed active hooks
 */

int	hooks_count(hook_t *h)
{
	int	count = 0;

	while (h != NULL) {
		count++;
		h = h->next;
	}

	return count;
}

/*
 * delete a spificif hook by path
 * - return 0 if ok
 * - return -1 on error
 */

int	hooks_delete(hook_t **first, hook_t *trash)
{
	hook_t	*current, *previous;

	for (previous = NULL, current = *first; current != NULL;
	     previous = current, current = current->next) {
		if (current == trash) {
			if (previous == NULL)
				*first = current->next;
			else
				previous->next = current->next;
			free(current);
			return 0;
		}
	}
	return 1;
}

/* free hooks chained list */
void	hooks_destroy(hook_t **h)
{
	hook_t	*tmp;

	while (*h != NULL) {
		tmp = *h;
		*h = (*h)->next;
		free(tmp);
	}
	*h = NULL;
}

/* dump hooks chained list to stdout, debug only */
void	hooks_dump(hook_t *h)
{
	for (; h != NULL; h = h->next) {
		printf("hook: %s, pid: %i, r: Ox%lx, w: 0x%lx, d: 0x%lx\n",
			h->path, h->pid, (long)h->read,
			(long)h->write, (long)h->data);
	}
}

/* init hooks chained list */
void	hooks_init(hook_t **first)
{
	*first = NULL;
}

/*
 * mangle an existing array of arguments for a oneshot child
 * example:
 * who = nick!user@host
 * where = #channel
 * exec = something like "/usr/bin/test.sh" "-f" foo"...
 * args = something like "test bar stuff"
 * argc = 3
 *
 * result will be something like
 * argv[0] = test, argv[1] = -f, argv[2] = foo, argv[3] = nick!user@host
 * argv[4] = #channel, argv[5] = stuff, argv[6] = stuff, argv[7] = NULL;
 *
 * or in text:
 * test -f foo nick!user@host #channel bar stuff
 */

static	char	**hooks_mangle_oneshot_argv(char **exec, int argc, char **argv, char *who, char *where)
{
	unsigned int	e;	/* exec count */
	unsigned int	i;	/* loop counter */
	char		**tmp;	/* result */

	/* sanity checks */
	assert(argv != NULL);
	assert(argc > 0);

	/* count **exec */
	for (e = 0; exec[e] != NULL; e++);

	/*
	 * build argv
	 * size of array = 2 (who + where) + e (child argc) + argc + 1 (NULL)
	 */
	tmp = malloc((2 + e + argc + 1) * sizeof(char *));
	if (tmp == NULL) {
		log_err("[h] malloc() error: %s\n", strerror(errno));
		fatal("[h] cannot launch hook, aborting");
	}

	tmp[0] = argv[0];
	/* adds tokens to argv */
	for (i = 1; i < e; i++) {
		tmp[i] = exec[i];
	}

	/* add who + where to argc */
	tmp[e + 0] = who;
	tmp[e + 1] = where;

	for (i = 1; i < (unsigned int)argc; i++) {
		tmp[e + i + 1] = argv[i];
	}
	tmp[e + i + 1] = NULL;

	return tmp;
}
/*
 * parse a line coming from a hook
 * if first character == : => raw IRC data
 * else
 * - data[0] = recipient
 * - data[1] = destination channel
 * - data[2] = message
 *
 * return 0 on succes
 * return -1 on error
 */

short	hooks_parse_cmd(char *line)
{
	unsigned int	i;
	short		seen;
	char		*channel = NULL;
	char		*message = NULL;
	char		*recipient = NULL;

	/* raw IRC data */
	if (line[0] == '\x01') {
		irc_send_raw(ircout, line + sizeof(char));
		if (line[1] != '\0')
			return 0;
		else
			return -1;
	}

	/*
	 * normal data
	 * format is "recipient:#channel:message'
	 */
	recipient = line;
	for (i = 0, seen = 0; line[i] != '\0' && seen < 2; i++) {
		if (line[i] == ':') {
			line[i] = '\0';
			seen++;
			if (seen == 1)
				channel = line + i + sizeof(char);
			else
				message = line + i + sizeof(char);
		}
	}

	/* we always need 3 tokens */
	if (channel == NULL || message == NULL || message[0] == '\0') {
		if (g_mode & VERBOSE)
			log_err("[h] incomplete line %s from hook\n", line);
		return -1;
	}

	/* strip recipient, in can it's a nick!user@host instead of a nick */
	for (i = 0; recipient[i] != '\0'; i++) {
		if (recipient[i] == '!') {
			recipient[i] = '\0';
			break;
		}
	}

	/* send back data */
	if (channel[0] == '\0') {
		irc_cmd_privmsg(ircout, recipient, message);
		return 0;
	}

	if (recipient[0] == '\0')
		irc_cmd_privmsg(ircout, channel, message);
	else
		irc_send_raw_vprintf(ircout, "PRIVMSG %s :%s: %s", \
				     channel, recipient, message);
	return 0;
}

/*
 * read callback
 */
void	hooks_read(struct bufferevent *ev, void *arg)
{
	size_t	len;
	char	buff[BUFF_SIZE];
	char	*line = NULL;
	hook_t	*h = (hook_t *)arg;

	assert(arg != NULL);
	len = bufferevent_read(ev, buff, BUFF_SIZE);

	/* move data from the event buffer to the parse queue */
	if (len > 0) {
		evbuffer_add(h->data, buff, len);
		line = evbuffer_readline(h->data);
		if (line == NULL)
			return ;

		/* on verbose mode, print buffer */
		if (g_mode & DEBUG)
			printf("#<# %s\n", line);

		/* if not connected, drop data */
		if (hooks_parse_cmd(line) == 0 && (g_mode & CONNECTED))
			net_force_output(&ev_irc);
		free(line);
	}
	else {
		hooks_generic_error(ev, -1, arg);
	}
}

/*
 * write callback
 */

void	hooks_write(struct bufferevent *ev, void *arg)
{
	arg = NULL;
	ev = NULL;
}

/*
 * errors callbacks
 */

/* generic version, used on read and write errors */
void	hooks_generic_error(struct bufferevent *ev, short what, void *arg)
{
	hook_t	*h = (struct hook_s *)arg;

	assert(arg != NULL);
	if (h->path[0] == '|')
		hooks_oneshot_error(ev, what, arg);
	else
		hooks_pipe_error(ev, what, arg);
}

/* oneshot (simplex) specific version */
void	hooks_oneshot_error(struct bufferevent *ev, short what, void *arg)
{
	hook_t  *h = (struct hook_s *)arg;

	if (what == 0)
		return ;
	ev = NULL;
	bufferevent_disable(h->read, EV_READ);
	bufferevent_free(h->read);
	evbuffer_free(h->data);
	hooks_delete(&ghook, h);
}

/* pipe (dual simplex) specific version */
void	hooks_pipe_error(struct bufferevent *ev, short what, void *arg)
{
	hook_t  *h = (struct hook_s *)arg;

	ev = NULL;
	what = 0;
	bufferevent_disable(h->read, EV_READ);
	bufferevent_disable(h->write, EV_WRITE);
	bufferevent_free(h->read);
	bufferevent_free(h->write);
	evbuffer_free(h->data);
	hooks_delete(&ghook, h);
}

/*
 * search if a specific hook is launched  
 *
 * return valued:
 * - a pointer to a hook_s struct if something is found
 * - NULL otherwise
 */

hook_t	*hooks_search(hook_t *h, char *path)
{
	for (; h != NULL; h = h->next) {
		if (strcmp(h->path, path) == 0)
			return h;
	}

	return NULL;
}

/*
 * send data to a piped hook.
 */

void	hooks_send(hook_t *h, int argc, char **argv, char *who, char *where)
{
	int	i;

	/* who can't be null */
	assert(who != NULL);

	bufferevent_write(h->write, who, strlen(who));
	bufferevent_write(h->write, ":", sizeof(char));
	if (where != NULL)
		bufferevent_write(h->write, where, strlen(where));
	bufferevent_write(h->write, ":", sizeof(char));
	for (i = 0; i < argc; i++) {
		bufferevent_write(h->write, argv[i], strlen(argv[i]));
		if (i + 1 < argc)
			bufferevent_write(h->write, " ", sizeof(char));
	}
	bufferevent_write(h->write, "\n", sizeof(char));
}

/*
 * event loop similar to net_loop
 * it will be called after IRC QUIT and listeners shutdown
 * so it will manage only hooks I/O
 */

void	hooks_shutdown(void)
{
	char		anim[] = "-\\|/";
	unsigned int	i;

	/* first printf */
	if (! isatty(fileno(stdout)))
		log_msg("[h] waiting for hooks to terminate...\n");

	/* step is 0,1 sec */
	for (i = 0; i < HOOKS_TIMEOUT_SEC * 10 && hooks_count(ghook); i++) {
		printf("[h] waiting for hooks to terminate %c\r", anim[i % 4]);
		fflush(NULL);
		usleep(100000);
	}

	/* timeout */
	if (i == (HOOKS_TIMEOUT_SEC * 10))
	{
		if (isatty(fileno(stdout)))
			printf("[h] waiting for hooks to terminate... timeout\n");
		else
			log_msg("[h] ... timeout\n");
		return ;
	}

	/* all hooks shutdown */
	if (isatty(fileno(stdout)))
		printf("[h] waiting for hooks to terminate... done\n");
	else
		log_msg("[h] ... done\n");
}
