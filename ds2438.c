/*
 * Copyright (c) 2001  Dustin Sallings <dustin@spy.net>
 *
 * $Id: ds2438.c,v 1.2 2001/10/04 05:54:08 dustin Exp $
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mlan.h>
#include <commands.h>
#include <ds2438.h>


static int _ds2438_volt_ad(MLan *mlan, uchar *serial, int vdd) {
	uchar tmpbyte=0;
	uchar send_block[80];
	int send_cnt=0;
	int i;

	assert(mlan);
	assert(serial);

	memset(&send_block, 0x00, sizeof(send_block));

	/* Find it */
	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS2438\n"));
		return(FALSE);
	}
	/* Tell it to convert */
	tmpbyte=mlan->touchbyte(mlan, DS2438CONVERTV);

	/* Attempt up to three resets */
	for(i=0; i<3; i++) {
		if(mlan->reset(mlan) != 0) {
			i=7; /* if the reset was successful, jump out of the loop */
		}
	}

	/* Find it again */
	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS2438\n"));
		return(FALSE);
	}
	send_block[send_cnt++]=DS2438RECALL;
	send_block[send_cnt++]=0x00; /* Read from the start */
	/* We want to read this */
	for(i=0; i<9; i++) {
		send_block[send_cnt++]=0xFF;
	}
	/* Send it, get response */
	if(!(mlan->block(mlan, FALSE, send_block, send_cnt)) ) {
		mlan_debug(mlan, 1, ("Error recalling data\n"));
		return(FALSE);
	}

	/* Calculate the CRC */
	mlan->DOWCRC=0;
	for(i=send_cnt-9; i<send_cnt; i++) {
		mlan->dowcrc(mlan, send_block[i]);
	}
	if(mlan->DOWCRC != 0) {
		mlan_debug(mlan, 1, ("CRC failed\n"));
		return(FALSE);
	}

	tmpbyte=send_block[2]&0x08;
	if(((tmpbyte == 0x08) && vdd) || ((tmpbyte == 0x00) && !(vdd))) {
		mlan_debug(mlan, 1, ("Invalid data while getting voltage data\n"));
	}

	/* Access *again* */
	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS2438\n"));
		return(FALSE);
	}

	/* Write stuff back */
	send_cnt=0;
	send_block[send_cnt++]=DS2438WRITE_SCRATCHPAD;
	send_block[send_cnt++]=0x00;

	if(vdd) {
		send_block[send_cnt++] = send_block[2] | 0x08;
	} else {
		send_block[send_cnt++] = send_block[2] & 0xF7;
	}

	/* Copy the stuff back, and get us ready to read again */
	for(i=0; i<7; i++) {
		send_block[send_cnt++] = send_block[i+4];
	}

	/* Write 'em and read 'em */
	if(!(mlan->block(mlan, FALSE, send_block, send_cnt)) ) {
		mlan_debug(mlan, 1, ("Error writing scratchpad data\n"));
		return(FALSE);
	}

	/* Attempt up to three resets */
	for(i=0; i<3; i++) {
		if(mlan->reset(mlan) != 0) {
			i=7; /* if the reset was successful, jump out of the loop */
		}
	}

	return(TRUE);
}

static float _ds2438_reading(MLan *mlan, uchar *serial, int vdd)
{
	uchar send_block[80];
	uchar tmpbyte;
	int send_cnt=0;
	int i=0;
	float rv=-1.0;

	assert(mlan);
	assert(serial);

	/* Make sure we can set up the AD stuff right */
	if(!_ds2438_volt_ad(mlan, serial, vdd)) {
		return(rv);
	}

	/* Access *again* */
	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS2438\n"));
		return(rv);
	}

	/* Tell it to convert */
	tmpbyte=mlan->touchbyte(mlan, DS2438CONVERTV);

	/* Attempt up to three resets */
	for(i=0; i<3; i++) {
		if(mlan->reset(mlan) != 0) {
			i=7; /* if the reset was successful, jump out of the loop */
		}
	}

	/* Yet another access */
	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS2438\n"));
		return(rv);
	}

	/* Not sure why this is happening */
	send_block[send_cnt++]=DS2438RECALL;
	send_block[send_cnt++]=0x00;
	/* Write 'em and read 'em */
	if(!(mlan->block(mlan, FALSE, send_block, send_cnt)) ) {
		mlan_debug(mlan, 1, ("Error recalling data\n"));
		return(rv);
	}

	/* Yet another access */
	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS2438\n"));
		return(rv);
	}

	/* Read the scratchpad */
	send_cnt=0;
	send_block[send_cnt++]=DS2438READ_SCRATCHPAD;
	send_block[send_cnt++]=0x00;
	for(i=0;i<9;i++) {
		send_block[send_cnt++]=0xff;
	}
	if(!(mlan->block(mlan, FALSE, send_block, send_cnt)) ) {
		mlan_debug(mlan, 1, ("Error reading scratchpad data\n"));
		return(rv);
	}

	/* Calculate the CRC */
	mlan->DOWCRC=0;
	for(i=2; i<send_cnt; i++) {
		mlan->dowcrc(mlan, send_block[i]);
	}
	if(mlan->DOWCRC != 0) {
		mlan_debug(mlan, 1, ("CRC failed\n"));
		return(rv);
	}

	i=(send_block[6] << 8) | send_block[5];

	rv=((float)i / 100.0);

	return(rv);
}

static double _ds2438_gettemp(MLan *mlan, uchar *serial)
{
	uchar tmpbyte=0;
	uchar send_block[80];
	int send_cnt=0;
	int i=0;
	double rv=0.00;

	assert(mlan);
	assert(serial);

	/* find it */
	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS2438\n"));
		return(rv);
	}

	/* Tell it to convert */
	tmpbyte=mlan->touchbyte(mlan, DS2438CONVERTT);

	/* access again */
	if(!mlan->access(mlan, serial)) {
		mlan_debug(mlan, 1, ("Error reading from DS2438\n"));
		return(rv);
	}

	send_block[send_cnt++]=DS2438READ_SCRATCHPAD;
	send_block[send_cnt++]=0x00;

	for(i=0; i<9; i++) {
		send_block[send_cnt++]=0xFF;
	}

	if(!(mlan->block(mlan, FALSE, send_block, send_cnt)) ) {
		mlan_debug(mlan, 1, ("Error reading scratchpad data\n"));
		return(rv);
	}

	/* Calculate the CRC */
	mlan->DOWCRC=0;
	for(i=2; i<send_cnt; i++) {
		mlan->dowcrc(mlan, send_block[i]);
	}
	if(mlan->DOWCRC != 0) {
		mlan_debug(mlan, 1, ("CRC failed\n"));
		return(rv);
	}

	/* Calculate the temperature */
	rv = (double)(((send_block[4] << 8) | send_block[3]) >> 3) * 0.03125;

	return(rv);
}

/*
 * Get the data for a 2438
 */
struct ds2438_data getDS2438Data(MLan *mlan, uchar *serial)
{
	struct ds2438_data data;

	assert(mlan);
	assert(serial);
	assert(serial[0] == 0x26);

	/* Zero out the data */
	memset(&data, 0x00, sizeof(data));

	data.Vdd=_ds2438_reading(mlan, serial, TRUE);
	data.Vad=_ds2438_reading(mlan, serial, FALSE);
	data.temp=_ds2438_gettemp(mlan, serial);
	data.humidity=
		(((data.Vad/data.Vdd) - 0.16)/0.0062)/(1.0546 - 0.00216 * data.temp);

	data.valid=TRUE;

	return(data);
}
