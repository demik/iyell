/*
 *  cmd.h
 *  iyell
 *
 *  Created by Michel DEPEIGE on 05/06/2008.
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

#ifndef CMD_H
#define CMD_H

/* prototypes */
void	cmd_check_irc_msg(struct evbuffer *out, char *who, char *what);
void	cmd_switch(struct evbuffer *out, char *who, char *what, char *where);

void	cmd_ping(struct evbuffer *out, int argc, char **argv, char *who, char *where);
void	cmd_stats(struct evbuffer *out, int argc, char **argv, char *who, char *where);
void	cmd_timestamp(struct evbuffer *out, int argc, char **argv, char *who, char *where);
void	cmd_uptime(struct evbuffer *out, int argc, char **argv, char *who, char *where);

#endif
