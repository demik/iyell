/*
 *  asprintf.h
 *
 *
 *  Created by Michel DEPEIGE on 01/04/08.
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

#ifndef ASPRINTH_H
#define ASPRINTH_H
#ifndef HAVE_ASPRINTF
int	asprintf(char **strp, const char *format, ...);
#endif	/* HAVE_ASPRINTF */
#endif	/* ASPRINTH_H */
