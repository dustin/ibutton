/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: utility.h,v 1.1 2002/01/23 23:56:05 dustin Exp $
 *
 * This file deals with stuff that's missing on some systems.
 */

#ifndef UTILITY_H
#define UTILITY_H 1

#if !defined(HAVE_VSNPRINTF)
# if defined(HAVE_VSPRINTF)
#  define vsnprintf(a, b, c, d) vsprintf(a, c, d)
# else
#  error No vsnprintf *OR* vsprintf?  Call your vendor.
# endif
#endif

/* If there's no snprintf, make one */
#if !defined(HAVE_SNPRINTF)
int snprintf(char *s, size_t n, const char *format, ...);
#endif

#endif /* UTILITY_H */

/*
 * arch-tag: 20729E67-13E5-11D8-9BD6-000393CFE6B8
 */
