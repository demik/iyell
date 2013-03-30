/*
 *  asprintf.c
 *  
 *
 *  Created by Michel DEPEIGE on 01/04/08.
 *  2013 lostwave.net.
 *  This code can be used under the BSD license.
 *
 */

#define _BSD_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "config.h"

#ifndef HAVE_ASPRINTF
int	asprintf(char **strp, const char *format, ...)
{
	va_list	arg;
	char		*str;
	int		size;
	int		ret;

	va_start(arg, format);
	size = vsnprintf(NULL, 0, format, arg);
	size++;
	va_start(arg, format);
	
	/* linux style: don't set strp to NULL */
	if (size < 1)
		return -1;
	str = malloc(size);
	if (str == NULL) {
		va_end(arg);
		return -1;
	}
	ret = vsnprintf(str, size, format, arg);
	va_end(arg);
	
	*strp = str;
	return ret;
}
#endif
