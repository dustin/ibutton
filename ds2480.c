/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds2480.c,v 1.1 1999/09/20 02:52:52 dustin Exp $
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

	mlan->mode = MODSEL_COMMAND;
	mlan->speed = SPEEDSEL_FLEX;

	mlan->setbaud(mlan, PARMSET_9600);
	mlan->cbreak(mlan);
	msDelay(2);
	mlan->flush(mlan);
	
	/* OK, get ready to send shite */
	sendpacket[0] = 0xC1;
	if(mlan->write(mlan, 1, sendpacket) != 1)
		return(FALSE);

	/* Set up all the stuff we needs. */
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_SLEW | PARMSET_Slew1p65Vus;
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_WRITE1LOW | PARMSET_Write12us;
	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_SAMPLEOFFSET |
							PARMSET_SampOff7us;

	sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_PARMREAD |
							(PARMSEL_BAUDRATE >> 3);

	sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_BIT | mlan->baud | BITPOL_ONE;

	mlan->flush(mlan);

	if(mlan->write(mlan, sendlen, sendpacket)) {
		if(mlan->read(mlan, 5, readbuffer) == 5) {
			if (((readbuffer[3] & 0xF1) == 0x00) &&
				((readbuffer[3] & 0x0E) == mlan->baud) &&
				((readbuffer[4] & 0xF0) == 0x90) &&
				((readbuffer[4] & 0x0C) == mlan->baud)) {
				return(TRUE);
			}
		}
	}

	return(FALSE);
}

int _ds2480_changebaud(MLan *mlan, uchar newbaud)
{
	int rt=FALSE;
	uchar readbuffer[5],sendpacket[5],sendpacket2[5];
	int sendlen=0,sendlen2=0;

	if(mlan->baud == newbaud) {
		return(TRUE);
	} else {
		if(mlan->mode != MODSEL_COMMAND) {
			mlan->mode=MODSEL_COMMAND;
			sendpacket[sendlen++] = MODE_COMMAND;
		} 

		sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_BAUDRATE | newbaud;
		mlan->flush(mlan);

		if(mlan->write(mlan, sendlen, sendpacket)) {
			msDelay(5);

			mlan->setbaud(mlan, newbaud);
			msDelay(5);
			sendpacket2[sendlen2++] = CMD_CONFIG | PARMSEL_PARMREAD |
									(PARMSEL_BAUDRATE >> 3);
			mlan->flush(mlan);
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
