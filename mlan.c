/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: mlan.c,v 1.1 1999/09/20 02:52:52 dustin Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <mlan.h>

extern void _com_setbaud(MLan *mlan, int new_baud);
extern void _com_flush(MLan *mlan);
extern int _com_write(MLan *mlan, int outlen, uchar *outbuf);
extern int _com_read(MLan *mlan, int inlen, uchar *inbuf);
extern void _com_cbreak(MLan *mlan);
extern int _ds2480_detect(MLan *mlan);
extern int _ds2480_changebaud(MLan *mlan, uchar newbaud);

MLan *mlan_init(char *port, int baud_rate)
{
	MLan *mlan;

	assert(port);

	mlan=calloc(1, sizeof(MLan));
	assert(mlan);

	/* Serial functions */
	mlan->setbaud=_com_setbaud;
	mlan->flush=_com_flush;
	mlan->write=_com_write;
	mlan->read=_com_read;
	mlan->cbreak=_com_cbreak;

	/* DS2480 functions */
	mlan->ds2480detect=_ds2480_detect;
	mlan->ds2480changebaud=_ds2480_changebaud;

	mlan->debug=0;
	mlan->mode = MODSEL_COMMAND;
	mlan->baud = PARMSET_9600;
	mlan->speed = SPEEDSEL_FLEX;
	mlan->level = MODE_NORMAL;

	mlan->fd=open(port, O_RDWR|O_NOCTTY|O_NONBLOCK);
	if(mlan->fd < 0) {
		free(mlan);
		/* Short-circuit on failure */
		return(NULL);
	}

	/* Set the baud rate */
	mlan->setbaud(mlan, baud_rate);

	return(mlan);
}
