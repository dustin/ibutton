/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds1920.c,v 1.2 2000/07/14 06:18:19 dustin Exp $
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
#include <ds1920.h>

static float
ds1920temp_convert_out(int in)
{
	return( ((float)in/2) - 0.25);
}

/* Get a temperature reading from a DS1920 */
struct ds1920_data
ds1920Sample(MLan *mlan, uchar *serial)
{
	uchar send_block[30], tmpbyte;
	int send_cnt=0, tsht, i;
	float tmp,cr,cpc;
	struct ds1920_data data;

	assert(mlan);
	assert(serial);

	/* Make sure this is a DS1920 */
	assert(serial[0] == 0x10);

	memset(&data, 0x00, sizeof(data));

	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS1920\n"));
		return(data);
	}

	tmpbyte=mlan->touchbyte(mlan, CONVERT_TEMPERATURE);
	mlan_debug(mlan, 3, ("Got %02x back from touchbyte\n", tmpbyte));

	if(mlan->setlevel(mlan, MODE_STRONG5) != MODE_STRONG5) {
		mlan_debug(mlan, 1, ("Strong pull-up failed.\n") );
		return(data);
	}

	/* Wait a second */
	sleep(1);

	if(mlan->setlevel(mlan, MODE_NORMAL) != MODE_NORMAL) {
		mlan_debug(mlan, 1, ("Disabling strong pull-up failed.\n") );
		return(data);
	}

	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS1920\n"));
		return(data);
	}

	/* Command to read the temperature */
	send_block[send_cnt++] = 0xBE;
	for(i=0; i<9; i++) {
		send_block[send_cnt++]=0xff;
	}

	if(! (mlan->block(mlan, FALSE, send_block, send_cnt)) ) {
		mlan_debug(mlan, 1, ("Read scratchpad failed.\n"));
		return(data);
	}

	mlan->DOWCRC=0;
	for(i=send_cnt-9; i<send_cnt; i++)
		mlan->dowcrc(mlan, send_block[i]);

	if(mlan->DOWCRC != 0) {
		mlan_debug(mlan, 1, ("CRC failed\n"));
		return(data);
	}

	mlan_debug(mlan, 2, ("TH=%f\n", ds1920temp_convert_out(send_block[3])) );
	mlan_debug(mlan, 2, ("T=%f\n", ds1920temp_convert_out(send_block[1])) );
	mlan_debug(mlan, 2, ("TL=%f\n", ds1920temp_convert_out(send_block[4])) );

	/* Calculate the temperature from the scratchpad */
	tsht = send_block[1];
	if (send_block[2] & 0x01)
		tsht |= -256;
	/* tmp = (float)(tsht/2); */
	tmp=ds1920temp_convert_out(tsht);
	cr = send_block[7];
	cpc = send_block[8];
	if(cpc == 0) {
		mlan_debug(mlan, 1, ("CPC is zero, that sucks\n"));
		return(data);
	} else {
		/* tmp = tmp - (float)0.25 + (cpc - cr)/cpc; */
		tmp = tmp + (cpc - cr)/cpc;
	}
	data.temp=tmp;
	/* The string readings */
	sprintf(data.reading_c, "%.2f", data.temp);
	sprintf(data.reading_f, "%.2f", ctof(data.temp));
	data.valid=TRUE;
	return(data);
}
