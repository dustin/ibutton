/*
 * Copyright (c) Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds1921.h,v 1.2 2000/07/14 05:49:19 dustin Exp $
 */

#ifndef DS1921_H
#define DS1921_H 1

/* 128 bytes of histogram */
#define HISTOGRAM_SIZE 64
/* 2k of temperature */
#define SAMPLE_SIZE 2048

/* The structure we keep the DS1921 data in */

struct ds1921_data {
	struct {

		/* Realtime clock */
		struct {
			int year;
			int month;
			int date;
			int day;
			int hours;
			int minutes;
			int seconds;
		} clock;

		/* Mission timestamp */
		struct {
			int year;
			int month;
			int date;
			int hours;
			int minutes;
		} mission_ts;

		/* Current status */
		int status;
		/* Current control info */
		int control;

		/* Sample rate */
		int sample_rate;

		/* Temperature alarms */
		float low_alarm;
		float high_alarm;

		/* Delay, in minutes, until mission start */
		int mission_delay;

		/* Counters */
		/* Mission status counter */
		int mission_s_counter;
		/* Device status counter */
		int device_s_counter;

	} status;

	int histogram[HISTOGRAM_SIZE];
	/* sample data, and the number of samples */
	int n_samples;
	float samples[SAMPLE_SIZE];

	/* Textual summary */
};

struct ds1921_data getDS1921Data(MLan *mlan, uchar *serial);
void printDS1921(struct ds1921_data d);

#endif /* DS1921_H */
