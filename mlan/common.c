/*
 * Copyright (c) 2004  Dustin Sallings <dustin@spy.net>
 *
 * arch-tag: 119160-40-118-82-0030187026
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

int
recall(MLan *mlan, uchar *serial)
{
	uchar send_buffer[16];
	int send_cnt=0;
	assert(mlan);
	assert(serial);

	if(!mlan->access(mlan, serial)) {
		fprintf(stderr, "Error accessing device for recall command.\n");
		return(FALSE);
	}

	send_buffer[send_cnt++]=RECALL;
	if(! (mlan->block(mlan, FALSE, send_buffer, send_cnt))) {
		fprintf(stderr, "Error issuing recall command!\n");
		return(FALSE);
	}

	return(TRUE);
}

