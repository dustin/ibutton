/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: devices.c,v 1.3 1999/12/08 09:54:31 dustin Exp $
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

static float
temp_convert_out(int in)
{
	return( ((float)in/2) - 0.25);
}

/*
I'll uncomment this next time it's used.
static int
temp_convert_in(float in)
{
	float ret;
	ret=in;
	ret+=.25;
	ret*=2;
	return( (int)ret );
}
*/

static float
ctof(float in)
{
	float ret;
	ret=(in*9/5) + 32;
	return(ret);
}

/*
static float
ftoc(float in)
{
	float ret;
	ret=(in-32) * 5/9;
	return(ret);
}
*/

/* Get a temperature reading from a DS1920 */
static char*
ds1920Sample(MLan *mlan, uchar *serial)
{
	uchar send_block[30], tmpbyte;
	int send_cnt=0, tsht, i;
	float temp, tmp,cr,cpc;

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

	mlan_debug(mlan, 2, ("TH=%f\n", temp_convert_out(send_block[3])) );
	mlan_debug(mlan, 2, ("T=%f\n", temp_convert_out(send_block[1])) );
	mlan_debug(mlan, 2, ("TL=%f\n", temp_convert_out(send_block[4])) );

	/* Calculate the temperature from the scratchpad */
	tsht = send_block[1];
	if (send_block[2] & 0x01)
		tsht |= -256;
	/* tmp = (float)(tsht/2); */
	tmp=temp_convert_out(tsht);
	cr = send_block[7];
	cpc = send_block[8];
	if(cpc == 0) {
		mlan_debug(mlan, 1, ("CPC is zero, that sucks\n"));
		return(FALSE);
	} else {
		/* tmp = tmp - (float)0.25 + (cpc - cr)/cpc; */
		tmp = tmp + (cpc - cr)/cpc;
	}
	mlan_debug(mlan, 2, ("Celsius:  %f\n", tmp) );
	/* Convert to Farenheit */
	temp=ctof(tmp);
	sprintf(mlan->sample_buffer, "%f", temp);
	return(mlan->sample_buffer);
}

/* abstracted sampler */
char *
get_sample(MLan *mlan, uchar *serial)
{
	char *ret=NULL;

	assert(mlan);
	assert(serial);

	switch(serial[0]) {
		case 0x10:
			ret=ds1920Sample(mlan, serial);
			break;
	}
	return(ret);
}
