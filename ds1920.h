/*
 * Copyright (c) Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds1920.h,v 1.2 2000/07/14 19:35:32 dustin Exp $
 */

#ifndef DS1920_H
#define DS1920_H 1

/* The structure we keep the DS1920 data in */

struct ds1920_data {
	float temp;
	int valid;

	float temp_hi;
	float temp_low;

	char reading_f[80];
	char reading_c[80];
};

struct ds1920_data ds1920Sample(MLan *, uchar *);
void printDS1920(struct ds1920_data);
int setDS1920Params(MLan *mlan, uchar *serial, struct ds1920_data d);

#endif /* DS1920_H */
