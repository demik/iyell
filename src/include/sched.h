/*
 *  sched.h
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

#ifndef SCHED_H
#define SCHED_H

void	schedule_nick_change(int seconds);
void	schedule_reconnection(int seconds);
void	schedule_regain_nick(int seconds);
void	schedule_irc_write(int seconds);
void	schedule_rejoin_after_ban(int seconds);
void	schedule_shutdown(void);

#endif
