/*
 * Copyright (c) Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds2438.h,v 1.1 2001/08/29 09:27:55 dustin Exp $
 */
 
#ifndef DS2438_H
#define DS2438_H 1

/* Structure for storing data from a DS2438 battery monitor/humidity sensor */
struct ds2438_data {
	float Vdd;
	float Vad;
	float humidity;
	float temp;
	int valid;
};

/* Get the data for a given device */
struct ds2438_data getDS2438Data(MLan *mlan, uchar *serial);

#endif /* DS2438_H */
