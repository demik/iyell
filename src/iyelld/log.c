/*
 *  log.c
 *  iyell
 *
 *  Created by Michel DEPEIGE on 29/03/2008.
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

#define _BSD_SOURCE 1
#define __BSD_VISIBLE 1
#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <syslog.h>
#include <stdarg.h>
#include "main.h"

/*
 * Init all log used, openlog (syslog), open files, etc
 */

void	log_init()
{
	openlog("iyell", LOG_PID, LOG_USER);
}

/*
 * cleanup everything before exiting
 */

void	log_deinit()
{
	closelog();
}

/*
 * log messages to stdout, log and syslog if choosen
 */

void	log_msg(char *msg, ...)
{
	va_list	s;
	
	if (g_mode & SYSLOG) {
		va_start(s, msg);
		vsyslog(LOG_NOTICE, msg + 4, s);
		va_end(s);
	}
	va_start(s, msg);
	vfprintf(stdout, msg, s);
	va_end(s);
}

void	log_err(char *msg, ...)
{
	va_list	s;
	
	if (g_mode & SYSLOG) {
		va_start(s, msg);
		vsyslog(LOG_ERR, msg + 4, s);
		va_end(s);
	}
	va_start(s, msg);
	vfprintf(stderr, msg, s);
	va_end(s);
}
