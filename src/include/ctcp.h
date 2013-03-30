/*
 *  ctcp.h
 *  iyell
 *
 *  Created by Michel DEPEIGE on 19/10/07.
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

#ifndef CTCP_H
#define CTCP_H

/* prototypes */
void	ctcp_parse(struct evbuffer *out, char *line);
void	ctcp_request(struct evbuffer *out, char *dest, char *message);
void	ctcp_reply(struct evbuffer *out, char *dest, char *message);
void	ctcp_cmd_clientinfo(struct evbuffer *out, char *from, char *args);
void	ctcp_cmd_ping(struct evbuffer *out, char *from, char *args);
void	ctcp_cmd_time(struct evbuffer *out, char *from, char *args);
void	ctcp_cmd_userinfo(struct evbuffer *out, char *from, char *args);
void	ctcp_cmd_version(struct evbuffer *out, char *from, char *args);
void	ctcp_self_ping(struct evbuffer *out);

#endif
