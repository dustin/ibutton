/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: com.c,v 1.10 1999/12/09 08:48:03 dustin Exp $
 */

#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <mlan.h>

void _com_setbaud(MLan *mlan, int new_baud)
{
	struct termios com;
	int speed;

	assert(mlan);

	mlan_debug(mlan, 2, ("Calling setbaud (%d)\n", new_baud));

	memset(&com,0x00,sizeof(com));

	/* Configure the com port */
	com.c_cflag =   CS8 | CREAD | CLOCAL;
	com.c_iflag= IGNPAR;
	com.c_cc[VTIME]=5;
	com.c_cc[VMIN]=0;

	/* Set up the baud rate */
	switch(new_baud) {
		case PARMSET_9600:
			mlan_debug(mlan, 3, ("Setting baud to 9600\n") );
			speed = B9600;
			mlan->usec_per_byte = 860;
			break;
#ifdef B19200
		case PARMSET_19200:
			mlan_debug(mlan, 3, ("Setting baud to 19200\n") );
			speed = B19200;
			mlan->usec_per_byte = 416;
			break;
#endif
#ifdef B57600
		case PARMSET_57600:
			mlan_debug(mlan, 3, ("Setting baud to 57600\n") );
			speed = B57600;
			mlan->usec_per_byte = 139;
			break;
#endif
#ifdef B115200
		case PARMSET_115200:
			mlan_debug(mlan, 3, ("Setting baud to 115200\n") );
			speed = B115200;
			mlan->usec_per_byte = 69;
			break;
#endif
		default:
			mlan_debug(mlan, 3, ("Setting baud to 9600\n") );
			speed = B9600;
			mlan->usec_per_byte = 860;
	}

	/* Set both in and out */
	cfsetispeed(&com, speed);
	cfsetospeed(&com, speed);

	/* Save it */
	mlan->flush(mlan);
	tcsetattr(mlan->fd, TCSANOW, &com);

	mlan->baud = new_baud;

	/* mlan_debug(mlan, 2, ("Returning from setbaud\n")); */
}

void _com_flush(MLan *mlan)
{
	assert(mlan);
	mlan_debug(mlan, 2, ("Calling flush\n"));
	tcflush(mlan->fd, TCIOFLUSH);
	/* mlan_debug(mlan, 2, ("Returning from flush\n")); */
}

int _com_write(MLan *mlan, int outlen, uchar *outbuf)
{
	int rv=0, i;
	time_t start, current;

	assert(mlan);
	assert(outbuf);
	
	mlan_debug(mlan, 2, ("Calling write(%d)\n", outlen));

	if(mlan->debug>3) {
		printf("Sending:  ");
		for(i=0; i<outlen; i++) {
			printf("%02x ", outbuf[i]);
		}
		printf("\n");
	}
	
	start=time(NULL);
	while(rv<outlen) {
		int tmp;
		tmp=write(mlan->fd, outbuf+rv, outlen-rv);
		if(tmp>0) {
			rv += tmp;
		} else {
			if(errno==EAGAIN) {
				mlan->msDelay(mlan, 10);
				errno=0;
			} else {
				perror("mlan->write");
				break;
			}
		}
		current=time(NULL);
		if( (current-start) > mlan->writeTimeout ) {
			mlan_debug(mlan, 1, ("Timed out on write.\n") );
			break;
		}
	}
	mlan_debug(mlan, 2, ("Returning from write (%d - %d)\n", rv, errno));
	return(rv==outlen);
}

int _com_read(MLan *mlan, int inlen, uchar *inbuf)
{
	int rv=0, i=0;
	time_t start=0, current=0;

	assert(mlan);

	mlan_debug(mlan, 2, ("Calling read(%d)\n", inlen));

	/* Give it some time... */
	usleep(mlan->usec_per_byte * (inlen+5) + 1000);

	start=time(NULL);
	while(rv<inlen) {
		int tmp;
		mlan_debug(mlan, 3, ("Read %d bytes\n", rv) );
		tmp=read(mlan->fd, inbuf+rv, inlen-rv);
		if(tmp>0) {
			rv += tmp;
		} else {
			if(errno==EAGAIN) {
				mlan_debug(mlan, 4, ("EAGAIN while doing read\n"));
				mlan->msDelay(mlan, 10);
				errno=0;
			} else {
				fprintf(stderr, "On byte %d of %d\n", rv, inlen);
				if(rv<inlen && errno==0) {
					/* This is a strange condition */
					perror("mlan->read short");
				} else {
					perror("mlan->read");
				}
				break;
			}
		}
		current=time(NULL);
		if( (current-start) > mlan->readTimeout) {
			mlan_debug(mlan, 1, ("Timed out on read.") );
			break;
		}
	}

	if(mlan->debug>3) {
		printf("Read:  ");
		for(i=0; i<rv; i++) {
			printf("%02x ", inbuf[i]);
		}
		printf("\n");
	}

	mlan_debug(mlan, 2, ("Returning from read (%d)\n", rv));
	return(rv);
}

void _com_cbreak(MLan *mlan)
{
	assert(mlan);
	mlan_debug(mlan, 2, ("Calling break\n"));
	tcsendbreak(mlan->fd, 1);
	/* mlan_debug(mlan, 2, ("Returning from break\n")); */
}
