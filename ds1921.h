/*
 * Copyright (c) Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds1921.h,v 1.3 2000/07/14 06:18:22 dustin Exp $
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
	char summary[80];
};

#define BIT(a) (1<<a)

/* bitmasks for checking the status */
#define STATUS_REALTIME_ALARM BIT(0)
#define STATUS_HI_ALARM BIT(1)
#define STATUS_LOW_ALARM BIT(2)
#define STATUS_UNUSED BIT(3)
#define STATUS_SAMPLE_IN_PROGRESS BIT(4)
#define STATUS_MISSION_IN_PROGRESS BIT(5)
#define STATUS_MEMORY_CLEARED BIT(6)
#define STATUS_TEMP_CORE_BUSY BIT(7)

/* Bitmasks for control */
#define CONTROL_TIMER_ALARM_ENABLED BIT(0)
#define CONTROL_HI_ALARM_ENABLED BIT(1)
#define CONTROL_LOW_ALARM_ENABLED BIT(2)
#define CONTROL_ROLLOVER_ENABLED BIT(3)
#define CONTROL_MISSION_ENABLED BIT(4)
#define CONTROL_UNUSED BIT(5)
#define CONTROL_MEMORY_CLR_ENABLED BIT(6)
#define CONTROL_RTC_OSC_DISABLED BIT(7)

/* prototypes */
struct ds1921_data getDS1921Data(MLan *mlan, uchar *serial);
void printDS1921(struct ds1921_data d);

#endif /* DS1921_H */
