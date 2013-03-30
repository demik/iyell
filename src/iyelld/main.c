/*
 *  main.c
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

#define _GNU_SOURCE
#define _BSD_SOURCE
#define __BSD_VISIBLE 1
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "posix+bsd.h"
#include <event.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "hooks.h"
#include "main.h"
#include "tools.h"

/* prototypes */
conf_t	g_conf;

int	main(int argc, char *argv[])
{
	struct event_base	*bot;

	/* init the program, check opt, init some stuff... */
	checkopt(argc, argv);
	
	if (init(&bot) == ERROR)
		exit(ERROR);

	/* switch to daemon mode if needed */
	if (g_mode & DAEMON)
		daemonize();

	/* connect to the server, init the listener, enter main loop */
	net_loop();

	/* close the program, free allocations, etc... */
	if (g_mode & HOOKS)
		hooks_shutdown();
#ifdef HAVE_EVENT_LOOPBREAK
	event_base_free(bot);
#endif
	log_msg("[-] exiting\n");
	log_deinit();

	conf_erase(&g_conf);
	return(NOERROR);
}

