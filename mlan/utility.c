/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: utility.c,v 1.1 2002/01/23 23:56:04 dustin Exp $
 *
 * This file deals with stuff that's missing on some systems.
 */

#include <stdio.h>
#include <stdarg.h>
#include <strings.h>

#include "utility.h"

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

/*
 * arch-tag: 206E5026-13E5-11D8-ABCC-000393CFE6B8
 */
