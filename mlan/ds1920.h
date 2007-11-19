/*
 * Copyright (c) Dustin Sallings <dustin@spy.net>
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

struct ds1920_data getDS1920Data(MLan *, uchar *);
void printDS1920(struct ds1920_data);
int setDS1920Params(MLan *mlan, uchar *serial, struct ds1920_data d);

#define ds1920temp_convert_out(a) ( ((float)(a)/2.0) - 0.25)
#define ds1920temp_convert_in(a) ( (int) (((a)+0.25)*2) )

#endif /* DS1920_H */
