/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: devices.c,v 1.8 2000/07/14 18:17:54 dustin Exp $
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
#include <ds1921.h>

float
ctof(float in)
{
	float ret;
	ret=(in*9/5) + 32;
	return(ret);
}

float
ftoc(float in)
{
	float ret;
	ret=(in-32) * 5/9;
	return(ret);
}


/* Dump a block for debugging */
void dumpBlock(uchar *buffer, int size)
{
	int i=0;
	printf("Dumping %d bytes\n", size);
	for(i=0; i<size; i++) {
		printf("%02X ", buffer[i]);
		if(i>0 && i%25==0) {
			puts("");
		}
	}
	puts("");
}

/* Dump a block in binary */
void binDumpBlock(uchar *buffer, int size, int start_addr)
{
	int i=0;
	printf("Dumping %d bytes in binary\n", size);
	for(i=0; i<size; i++) {
		printf("%04X (%02d) ", start_addr+i, i);
		printf("%s", buffer[i]&0x80?"1":"0");
		printf("%s", buffer[i]&0x40?"1":"0");
		printf("%s", buffer[i]&0x20?"1":"0");
		printf("%s", buffer[i]&0x10?"1":"0");
		printf("%s", buffer[i]&0x08?"1":"0");
		printf("%s", buffer[i]&0x04?"1":"0");
		printf("%s", buffer[i]&0x02?"1":"0");
		printf("%s", buffer[i]&0x01?"1":"0");
		puts("");
	}
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

/* abstracted sampler */
char *
get_sample(MLan *mlan, uchar *serial)
{
	char *ret=NULL;

	assert(mlan);
	assert(serial);

	switch(serial[0]) {
		case 0x10: {
				struct ds1920_data d;
				d=ds1920Sample(mlan, serial);
				ret=d.reading_f;
			}
			break;
		case 0x21: {
				struct ds1921_data d;
				d=getDS1921Data(mlan, serial);
				ret=d.summary;
		}
	}
	return(ret);
}
