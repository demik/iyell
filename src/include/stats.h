/*
 *  stats.h
 *  iyell
 *
 *  Created by Michel DEPEIGE on 03/06/2008.
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

#ifndef STATS_H
#define STATS_H

/* statistics struct */

typedef struct	statistics {
	time_t	boot_timestamp;
	long	srv_in;
	long	srv_out;
	long	msg_in;
	long	msg_out;
	long	lines;
	long	bad_msg;
	long	oe_msg;		/* offset error */
	long	good_msg;
}		stats_t;

extern stats_t	gstats;

/* functions */
int	stats_init(void);

/* macros */
#define STATS_INC_SRV_IN(x)	gstats.srv_in += x
#define STATS_INC_SRV_OUT(x)	gstats.srv_out += x

#define STATS_INC_MSG_IN(x)	gstats.msg_in += x
#define STATS_INC_MSG_OUT(x)	gstats.msg_out += x

#define STATS_INC_LINES(x)	gstats.lines++
#define STATS_INC_BMSG(x)	gstats.bad_msg++
#define STATS_INC_OEMSG(x)	gstats.oe_msg++
#define STATS_INC_GMSG(x)	gstats.good_msg++

#endif /* STATS_H */
