/*
 *  misc.h
 *  iyell
 *
 *  Created by Michel DEPEIGE on 25/10/07.
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

#ifndef MISC_H
#define MISC_H

/* prototypes */
void	change_nick(struct evbuffer *out, char *nick);

void	try_nick_change(int fd, short event, void *out);
void	try_rejoin_after_ban(int fd, short event, void *out);

#endif
