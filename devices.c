/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: devices.c,v 1.1 1999/12/07 10:09:02 dustin Exp $
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

/* Stuff for a DS1920 */

/* Get a temperature reading from a DS1920 */
static int
ds1920Sample(MLan *mlan, uchar *serial, float *temp)
{
	uchar send_block[30], tmpbyte;
	int send_cnt=0, tsht, i;
	float tmp,cr,cpc;

	assert(mlan);
	assert(serial);

	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS1920\n"));
		return(FALSE);
	}

	tmpbyte=mlan->touchbyte(mlan, 0x44);
	mlan_debug(mlan, 3, ("Got %02x back from touchbyte\n", tmpbyte));

	if(mlan->setlevel(mlan, MODE_STRONG5) != MODE_STRONG5) {
		mlan_debug(mlan, 1, ("Strong pull-up failed.\n") );
		return(FALSE);
	}

	/* Wait a second */
	sleep(1);

	if(mlan->setlevel(mlan, MODE_NORMAL) != MODE_NORMAL) {
		mlan_debug(mlan, 1, ("Disabling strong pull-up failed.\n") );
		return(FALSE);
	}

	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS1920\n"));
		return(FALSE);
	}

	/* Command to read the temperature */
	send_block[send_cnt++] = 0xBE;
	for(i=0; i<9; i++) {
		send_block[send_cnt++]=0xff;
	}

	if(! (mlan->block(mlan, FALSE, send_block, send_cnt)) ) {
		mlan_debug(mlan, 1, ("Read scratchpad failed.\n"));
		return(FALSE);
	}

	mlan->DOWCRC=0;
	for(i=send_cnt-9; i<send_cnt; i++)
		mlan->dowcrc(mlan, send_block[i]);

	if(mlan->DOWCRC != 0) {
		mlan_debug(mlan, 1, ("CRC failed\n"));
		return(FALSE);
	}

	/* Calculate the temperature from the scratchpad */
	tsht = send_block[1];
	if (send_block[2] & 0x01)
		tsht |= -256;
	tmp = (float)(tsht/2);
	cr = send_block[7];
	cpc = send_block[8];
	if(cpc == 0) {
		mlan_debug(mlan, 1, ("CPC is zero, that sucks\n"));
		return(FALSE);
	} else {
		tmp = tmp - (float)0.25 + (cpc - cr)/cpc;
	}
	/* Convert to Farenheit */
	*temp=tmp*9/5 + 32;
	return(TRUE);
}

int
sample(MLan *mlan, uchar *serial, void *data)
{
	int ret=FALSE;

	assert(mlan);
	assert(serial);

	switch(serial[0]) {
		case 0x10:
			ret=ds1920Sample(mlan, serial, data);
			break;
	}
	return(ret);
}
