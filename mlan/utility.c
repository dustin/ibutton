/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * This file deals with stuff that's missing on some systems.
 */

#include <stdio.h>
#include <stdarg.h>
#include <strings.h>

#include "utility.h"
#include "mlan-defines.h"

#include <assert.h>

#if !defined(HAVE_SNPRINTF)
int
snprintf(char *s, size_t n, const char *format,...)
{
	va_list ap;
	va_start(ap, format);
	vsnprintf(s, n - 1, format, ap);
	va_end(ap);
	/* Make sure we haven't overstepped */
	assert(strlen(s)<n);
}
#endif

/* Get the serial number as a string */
char*
get_serial(uchar *serial)
{
	static char r[(MLAN_SERIAL_SIZE * 2) + 1];
	char *map="0123456789ABCDEF";
	int i=0, j=0;
	assert(serial);
	for(i=0; i<MLAN_SERIAL_SIZE; i++) {
		r[j++]=map[((serial[i] & 0xf0) >> 4)];
		r[j++]=map[(serial[i] & 0x0f)];
	}
	r[j]=0x00;
	return(r);
}
