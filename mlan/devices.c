/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: devices.c,v 1.19 2002/01/30 09:22:07 dustin Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>

#include <mlan.h>
#include <commands.h>
#include <ds1920.h>
#include <ds1921.h>
#include <ds2406.h>

int
findGMTOffset(void)
{
	struct tm *tm;
	int i;
	time_t t, tmp;

	t = time(NULL);
	tmp = t;
	tm=gmtime(&t);
	t = mktime(tm);

	i=(int)tmp - (int)t;
	tm=localtime(&t);

	if(tm->tm_isdst > 0) {
		i+=3600;
	}
	return(i);
}

/* Dump a block for debugging */
void dumpBlock(uchar *buffer, int size)
{
	int i=0;
	int line=0;

	assert(buffer);
	/* This thing would look really funny if a line had fewer than 16 bytes */
	/*
	assert( (size%16)==0 );
	*/

	printf("Dumping %d bytes\n", size);
	for(line=0; line*16<size; line++) {
		printf("%04X  ", line*16);
		for(i=0; i<16; i++) {
			printf("%02X ", buffer[(line*16)+i]);
		}
		putchar(' ');
		for(i=0; i<16; i++) {
			int c=buffer[(line*16)+i];
			if(isgraph(c) || c==' ') {
				putchar(c);
			} else {
				putchar('.');
			}
		}
		printf("\n");
	}
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

/* abstracted sampler */
char *
get_sample(MLan *mlan, uchar *serial)
{
	static char buffer[80];
	char *ret=NULL;

	assert(mlan);
	assert(serial);

	memset(buffer, 0x00, sizeof(ret));

	switch(serial[0]) {
		case 0x10: {
				struct ds1920_data d;
				d=getDS1920Data(mlan, serial);
				if(d.valid) {
					assert(strlen(d.reading_c)<sizeof(buffer));
					strcpy(buffer, d.reading_c);
					ret=buffer;
				}
			}
			break;
		case 0x12: {
				int status=0;
				status=getDS2406Status(mlan, serial);
				if(status!=0) {
					sprintf(buffer, "Value:  %x", status);
					ret=buffer;
				}
			}
			break;
		case 0x21: {
				struct ds1921_data d;
				d=getDS1921Data(mlan, serial);
				assert(strlen(d.summary)<sizeof(buffer));
				strcpy(buffer, d.summary);
				ret=buffer;
		}
	}
	return(ret);
}

/*
 * arch-tag: 200E12F8-13E5-11D8-AFE6-000393CFE6B8
 */
