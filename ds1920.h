/*
 * Copyright (c) Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds1920.h,v 1.1 2000/07/14 05:49:18 dustin Exp $
 */

#ifndef DS1920_H
#define DS1920_H 1

/* The structure we keep the DS1920 data in */

struct ds1920_data {
	float temp;
	int valid;

	char reading_f[80];
	char reading_c[80];
};

struct ds1920_data ds1920Sample(MLan *, uchar *);

#endif /* DS1920_H */
