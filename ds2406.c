/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds2406.c,v 1.1 2000/09/17 09:20:09 dustin Exp $
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
#include <commands.h>
#include <ds2406.h>

int
setDS2406Status(MLan *mlan, uchar *serial, int value)
{
	uchar send_buffer[16];
	int send_cnt=0;

	assert(mlan);
	assert(serial);

	/* Make sure it's a ds2406 */
	assert(serial[0] == 0x12);

	/* Zero */
	memset(&send_buffer, 0x00, sizeof(send_buffer));

	/* We're going to do this send manually since we can't send to a page. */

	/* Access the device */
	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS2406\n"));
		return(FALSE);
	}

	send_buffer[send_cnt++]=DS2406WRITE_STATUS;
	send_buffer[send_cnt++]=7;
	send_buffer[send_cnt++]=0;
	send_buffer[send_cnt++]=value;
	send_buffer[send_cnt++]=0;
	send_buffer[send_cnt++]=0;

	mlan_debug(mlan, 3, ("Writing DS2406 status.\n"));
	if(! (mlan->block(mlan, FALSE, send_buffer, send_cnt))) {
		printf("Error writing to scratchpad!\n");
		return(FALSE);
	}

	/* Give it some power */
	mlan_debug(mlan, 3, ("Strong pull-up.\n"));
	if(mlan->setlevel(mlan, MODE_STRONG5) != MODE_STRONG5) {
		printf("Strong pull-up failed.\n");
		return(FALSE);
	}

	/* Give it some time to think */
	mlan->msDelay(mlan, 500);

	/* Turn off the hose */
	mlan_debug(mlan, 3, ("Disabling strong pull-up.\n"));
	if(mlan->setlevel(mlan, MODE_NORMAL) != MODE_NORMAL) {
		printf("Disabling strong pull-up failed.\n");
		return(FALSE);
	}

	return(TRUE);
}

/* Get a temperature reading from a DS1920 */
int getDS2406Status(MLan *mlan, uchar *serial)
{
	uchar send_block[30];
	int send_cnt=0, i; 

	assert(mlan);
	assert(serial);

	/* Make sure this is a DS2406 */
	assert(serial[0] == 0x12);

	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS2406\n"));
		return(0);
	}

	/* Command to read the temperature */
	send_block[send_cnt++] = DS2406READ_STATUS;
	send_block[send_cnt++]=0x00;
	send_block[send_cnt++]=0x00;
	/* We'll be reading 8 bytes of memory plus a 2 byte CRC */
	for(i=0; i<10; i++) {
		send_block[send_cnt++]=0xff;
	}

	if(! (mlan->block(mlan, FALSE, send_block, send_cnt)) ) {
		mlan_debug(mlan, 1, ("Read scratchpad failed.\n"));
		return(0);
	}

	mlan->DOWCRC=0;
	for(i=3; i<send_cnt; i++) {
		mlan->dowcrc(mlan, send_block[i]);
	}

	if(mlan->DOWCRC != 0) {
		mlan_debug(mlan, 1, ("CRC failed\n"));
		/* return(0); */
	}

	/* Return the status byte (8th offset, plus the three crap vars) */
	return(send_block[10]);
}

int setDS2406Switch(MLan *mlan, uchar *serial, int switchPort, int onoff)
{
	int status=0;
	int rv=FALSE;

	/* Make sure we got good shit */
	assert(mlan);
	assert(serial);

	/* Make sure a valid value was passed for onoff */
	assert(onoff == DS2406_ON || onoff == DS2406_OFF);

	/* Make sure a valid switchPort was passed in */
	assert(switchPort == DS2406SwitchA || switchPort == DS2406SwitchB);

	status=getDS2406Status(mlan, serial);

	/* Are we turning the thing on, or off? */
	/* This may look odd, but internally 1 = off, 0 = on */
	if(onoff==DS2406_ON) {
		status&=(~switchPort);
	} else {
		status|=switchPort;
	}

	/* Set it back */
	rv=setDS2406Status(mlan, serial, status);
	return(rv);
}
