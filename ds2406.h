/*
 * Copyright (c) Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds2406.h,v 1.1 2000/09/17 09:20:11 dustin Exp $
 */

#ifndef DS2406_H
#define DS2406_H 1

#define DS2406SwitchA 0x20
#define DS2406SwitchB 0x40

#define DS2406_OFF 0
#define DS2406_ON  1

/* Raw stuff */
int getDS2406Status(MLan *mlan, uchar *serial);
int setDS2406Status(MLan *mlan, uchar *serial, int value);

/* higher level stuff */
int setDS2406Switch(MLan *mlan, uchar *serial, int switchPort, int onoff);

#endif /* DS2406_H */
