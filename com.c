/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: com.c,v 1.1 1999/09/20 02:52:51 dustin Exp $
 */

#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <mlan.h>

void _com_setbaud(MLan *mlan, int new_baud)
{
	struct termios com;
	int speed;

	memset(&com,0x00,sizeof(com));

	/* Configure the com port */
	com.c_cflag =   CS8 | CREAD | CLOCAL;
	com.c_iflag= IGNPAR;
	com.c_cc[VTIME]=5;
	com.c_cc[VMIN]=0;

	/* Set up the baud rate */
	switch(new_baud) {
		case PARMSET_9600:
			mlan->speed = B9600;
			mlan->usec_per_byte = 833;
			break;
		case PARMSET_19200:
			mlan->speed = B19200;
			mlan->usec_per_byte = 416;
			break;
		case PARMSET_57600:
			mlan->speed = B57600;
			mlan->usec_per_byte = 139;
			break;
		case PARMSET_115200:
			mlan->speed = B115200;
			mlan->usec_per_byte = 69;
			break;
		default:
			mlan->speed = B9600;
			mlan->usec_per_byte = 833;
	}

	/* Set both in and out */
	cfsetispeed(&com, speed);
	cfsetospeed(&com, speed);

	/* Save it */
	tcflush(mlan->fd, TCIOFLUSH);
	tcsetattr(mlan->fd, TCSANOW, &com);

	mlan->baud = new_baud;
}

void _com_flush(MLan *mlan)
{
	tcdrain(mlan->fd);
	tcflush(mlan->fd, TCIOFLUSH);
}

int _com_write(MLan *mlan, int outlen, uchar *outbuf)
{
	int rv;
	
	rv = write(mlan->fd, outbuf, outlen);
	if(rv>0) {
		while(rv<outlen) {
			int tmp;
			tmp=write(mlan->fd, outbuf+rv, outlen-rv);
			if(tmp>0) {
				rv += tmp;
			} else {
				if(errno==EAGAIN) {
					msDelay(5);
					errno=0;
				} else {
					perror("write");
					break;
				}
			}
		}
	}
	mlan->flush(mlan);
	return(rv==outlen);
}

int _com_read(MLan *mlan, int inlen, uchar *inbuf)
{
	int rv;

	/* Give it some time... */
	usleep(mlan->usec_per_byte * (inlen+5) + 800);

	rv = read(mlan->fd, inbuf, inlen);
	if(rv>0) {
		while(rv<inlen) {
			int tmp;
			tmp=read(mlan->fd, inbuf+rv, inlen-rv);
			if(tmp>0) {
				rv += tmp;
			} else {
				if(errno==EAGAIN) {
					msDelay(5);
					errno=0;
				} else {
					perror("read");
					break;
				}
			}
		}
	}

	return(rv);
}

void _com_cbreak(MLan *mlan)
{
	tcsendbreak(mlan->fd, 1);
}
