/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds2480.c,v 1.8 1999/12/09 08:40:34 dustin Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <mlan.h>

int _ds2480_detect(MLan *mlan)
{
	uchar sendpacket[10],readbuffer[10];
	int sendlen=0;

	assert(mlan);

	mlan_debug(mlan, 2, ("Calling ds2480_detect\n"));

	mlan->mode = MODSEL_COMMAND;
	mlan->speed = SPEEDSEL_FLEX;
	mlan->baud = PARMSET_9600;

	mlan->setbaud(mlan, mlan->baud);
	mlan->cbreak(mlan);
	mlan->msDelay(mlan, 5);
	mlan->flush(mlan);
	
	/* OK, get ready to send shite */
	sendpacket[0] = 0xC1;
	if(mlan->write(mlan, 1, sendpacket) != 1) {
		mlan_debug(mlan, 2, ("Returning from ds2480_detect(1) - FALSE\n"));
		return(FALSE);
	}

	/* Set up all the stuff we needs. */
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_SLEW | mlan->slew_rate;
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_WRITE1LOW | PARMSET_Write12us;
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_SAMPLEOFFSET |
							PARMSET_SampOff7us;

	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_PARMREAD |
							(PARMSEL_BAUDRATE >> 3);

	sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_BIT | mlan->baud | BITPOL_ONE;

	if(mlan->write(mlan, sendlen, sendpacket)) {
		if(mlan->read(mlan, 5, readbuffer) == 5) {
			if (((readbuffer[3] & 0xF1) == 0x00) &&
				((readbuffer[3] & 0x0E) == mlan->baud) &&
				((readbuffer[4] & 0xF0) == 0x90) &&
				((readbuffer[4] & 0x0C) == mlan->baud)) {
				mlan_debug(mlan, 2, ("Returning from ds2480_detect - TRUE\n"));
				return(TRUE);
			}
		}
	}

	mlan_debug(mlan, 2, ("Returning from ds2480_detect(2) - FALSE\n"));
	return(FALSE);
}

int _ds2480_changebaud(MLan *mlan, uchar newbaud)
{
	int rt=FALSE;
	uchar readbuffer[5],sendpacket[5],sendpacket2[5];
	int sendlen=0,sendlen2=0;

	assert(mlan);

	if(mlan->baud == newbaud) {
		return(TRUE);
	} else {
		if(mlan->mode != MODSEL_COMMAND) {
			mlan->mode=MODSEL_COMMAND;
			sendpacket[sendlen++] = MODE_COMMAND;
		} 

		sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_BAUDRATE | newbaud;

		if(mlan->write(mlan, sendlen, sendpacket)) {
			mlan->msDelay(mlan, 10);

			mlan->setbaud(mlan, newbaud);
			mlan->msDelay(mlan, 10);
			sendpacket2[sendlen2++] = CMD_CONFIG | PARMSEL_PARMREAD |
									(PARMSEL_BAUDRATE >> 3);
			if(mlan->write(mlan, sendlen2,sendpacket2)) {
				if(mlan->read(mlan, 1, readbuffer) == 1) {
					if (((readbuffer[0] & 0x0E)
						== (sendpacket[sendlen-1] & 0x0E))) {
						rt = TRUE;
					}
				}
			}
		} else {
			/* If the top write failed */
			rt = FALSE;
		}
	}

	if(rt != TRUE) {
		mlan->ds2480detect(mlan);
	}

	return(mlan->baud);
}
