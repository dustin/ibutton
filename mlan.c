/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: mlan.c,v 1.3 1999/12/07 05:37:28 dustin Exp $
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

extern void     _com_setbaud(MLan * mlan, int new_baud);
extern void     _com_flush(MLan * mlan);
extern int      _com_write(MLan * mlan, int outlen, uchar * outbuf);
extern int      _com_read(MLan * mlan, int inlen, uchar * inbuf);
extern void     _com_cbreak(MLan * mlan);
extern int      _ds2480_detect(MLan * mlan);
extern int      _ds2480_changebaud(MLan * mlan, uchar newbaud);

static          uchar
dowcrc(MLan * mlan, uchar x)
{
	static uchar    dscrc_table[] = {
		0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126,
		32, 163, 253, 31, 65, 157, 195, 33, 127, 252, 162,
		64, 30, 95, 1, 227, 189, 62, 96, 130, 220, 35, 125,
		159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128,
		222, 60, 98, 190, 224, 2, 92, 223, 129, 99, 61,
		124, 34, 192, 158, 29, 67, 161, 255, 70, 24, 250,
		164, 39, 121, 155, 197, 132, 218, 56, 102, 229,
		187, 89, 7, 219, 133, 103, 57, 186, 228, 6, 88,
		25, 71, 165, 251, 120, 38, 196, 154, 101, 59, 217,
		135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152,
		122, 36, 248, 166, 68, 26, 153, 199, 37, 123, 58,
		100, 134, 216, 91, 5, 231, 185, 140, 210, 48, 110,
		237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147,
		205, 17, 79, 173, 243, 112, 46, 204, 146, 211, 141,
		111, 49, 178, 236, 14, 80, 175, 241, 19, 77, 206,
		144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
		50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76,
		18, 145, 207, 45, 115, 202, 148, 118, 40, 171, 245,
		23, 73, 8, 86, 180, 234, 105, 55, 213, 139, 87, 9,
		235, 181, 54, 104, 138, 212, 149, 203, 41, 119,
		244, 170, 72, 22, 233, 183, 85, 11, 136, 214, 52,
		106, 43, 117, 151, 201, 74, 20, 246, 168, 116, 42,
		200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215,
		137, 107, 53
	};
	mlan->DOWCRC = dscrc_table[mlan->DOWCRC ^ x];
	return (mlan->DOWCRC);
}

/* Millisecond sleep */
static void
msDelay(MLan *mlan, int t)
{
	mlan_debug(mlan, 3, ("Sleeping %dms\n", t) );
	usleep(t * 1000);
}

/* Bit utility to read and write a bit in a buffer */
static int
bitacc(int op, int state, int loc, uchar * buf)
{
	int             nbyt, nbit;

	nbyt = (loc / 8);
	nbit = loc - (nbyt * 8);

	if (op == WRITE_FUNCTION) {
		if (state)
			buf[nbyt] |= (0x01 << nbit);
		else
			buf[nbyt] &= ~(0x01 << nbit);
		return (1);
	} else {
		return ((buf[nbyt] >> nbit) & 0x01);
	}
}

static int
_mlan_setlevel(MLan *mlan, int newlevel)
{
	uchar sendpacket[10],readbuffer[10];
	int sendlen=0;
	int rt=FALSE;

	if(newlevel != mlan->level) {
		/* OK, we need to do this either way. */
		if(mlan->mode != MODSEL_COMMAND) {
			mlan->mode=MODSEL_COMMAND;
			sendpacket[sendlen++] = MODE_COMMAND;
		}
		if(newlevel == MODE_NORMAL) {
			/* Stop pulse command */
			sendpacket[sendlen++] = MODE_STOP_PULSE;

			mlan->flush(mlan);

			if(mlan->write(mlan, sendlen, sendpacket)) {
				if(mlan->read(mlan, 1, readbuffer)==1) {
					if((readbuffer[0] & 0xE0) == 0xE0) {
						rt=TRUE;
						mlan->level=MODE_NORMAL;
					}
				}
			}
		} else {
			if(newlevel == MODE_STRONG5) {
				sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_5VPULSE |
						PARMSET_infinite;
				sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_CHMOD |
						SPEEDSEL_PULSE | BITPOL_5V;
			} else if(newlevel == MODE_PROGRAM) {
				if(!mlan->ProgramAvailable) {
					return(MODE_NORMAL);
				}
				sendpacket[sendlen++] = CMD_CONFIG | PARMSEL_12VPULSE |
						PARMSET_infinite;
				sendpacket[sendlen++] = CMD_COMM | FUNCTSEL_CHMOD |
						SPEEDSEL_PULSE | BITPOL_12V;
			}

			mlan->flush(mlan);
			if(mlan->write(mlan, sendlen, sendpacket)) {
				if(mlan->read(mlan, 1, readbuffer) == 1) {
					if((readbuffer[0] & 0x81) == 0) {
						mlan->level = newlevel;
						rt = TRUE;
					}
				}
			}
		}
		if(rt != TRUE)
			mlan->ds2480detect(mlan);
	} /* Needed a new level */

	return(mlan->level);
}

int
_mlan_touchreset(MLan *mlan)
{
	uchar readbuffer[10],sendpacket[10];
	int sendlen=0;

	mlan->setlevel(mlan, MODE_NORMAL);

	if(mlan->mode != MODSEL_COMMAND) {
		mlan->mode = MODSEL_COMMAND;
		sendpacket[sendlen++] = MODE_COMMAND;
	}

	sendpacket[sendlen++] = (uchar)(CMD_COMM | FUNCTSEL_RESET | mlan->speed);

	mlan->flush(mlan);

	if( mlan->write(mlan, sendlen, sendpacket)) {
		if( mlan->read(mlan, 1, readbuffer) == 1) {
			if (((readbuffer[0] & RB_RESET_MASK) == RB_PRESENCE) ||
				((readbuffer[0] & RB_RESET_MASK) == RB_ALARMPRESENCE)) {
				mlan->ProgramAvailable = ((readbuffer[0] & 0x20) == 0x20);
				return TRUE;
			} else {
				return(FALSE);
			}
		}
	}

	/* Something bad happened */
	mlan->ds2480detect(mlan);
	return(FALSE);
}

static int
_mlan_first(MLan * mlan, int DoReset, int OnlyAlarmingDevices)
{
	mlan->LastDiscrepancy = 0;
	mlan->LastDevice = FALSE;
	mlan->LastFamilyDiscrepancy = 0;

	return (mlan->next(mlan, DoReset, OnlyAlarmingDevices));
}

static int
_mlan_next(MLan * mlan, int DoReset, int OnlyAlarmingDevices)
{
	int             i, TempLastDiscrepancy, pos;
	uchar           TempSerialNum[8];
	uchar           readbuffer[20], sendpacket[40];
	int             sendlen = 0;

	/* If the last was the last, reset the search */
	if (mlan->LastDevice) {
		mlan->LastDiscrepancy = 0;
		mlan->LastDevice = FALSE;
		mlan->LastFamilyDiscrepancy = 0;
	}
	/* Reset the wire if requested */
	if (DoReset) {
		if (!(mlan->reset(mlan))) {
			mlan->LastDiscrepancy = 0;
			mlan->LastFamilyDiscrepancy = 0;
			return (FALSE);
		}
	}
	/* Make sure the command is alright */
	if (mlan->mode != MODSEL_DATA) {
		mlan->mode = MODSEL_DATA;
		sendpacket[sendlen++] = MODE_DATA;
	}
	/* Search command */
	if (OnlyAlarmingDevices) {
		sendpacket[sendlen++] = 0xEC;	/* Alarm search */
	} else {
		sendpacket[sendlen++] = 0xF0;	/* regular search command */
	}

	/* Back to command mode */
	mlan->mode = MODSEL_COMMAND;
	sendpacket[sendlen++] = MODE_COMMAND;

	/* Search mode on */
	sendpacket[sendlen++] = (uchar) (CMD_COMM | FUNCTSEL_SEARCHON | mlan->speed);

	/* Data mode again */
	mlan->mode = MODSEL_DATA;
	sendpacket[sendlen++] = MODE_DATA;

	/* Temperary last descrepancy */
	TempLastDiscrepancy = 0xFF;

	/* Add 16 bytes of the search. */
	pos = sendlen;
	for (i = 0; i < 16; i++)
		sendpacket[sendlen++] = 0;

	/* Only modify bits if not the first search */
	if (mlan->LastDiscrepancy != 0xff) {
		/* Set the bits in the added buffer */
		for (i = 0; i < 64; i++) {
			if (i < (mlan->LastDiscrepancy - 1)) {
				bitacc(WRITE_FUNCTION,
				       bitacc(READ_FUNCTION, 0, i, &mlan->SerialNum[0]),
				     (short) (i * 2 + 1), &sendpacket[pos]);
			} else if (i == (mlan->LastDiscrepancy - 1)) {
				bitacc(WRITE_FUNCTION, 1,
				     (short) (i * 2 + 1), &sendpacket[pos]);
			}
		}
	}
	/* command mode again */
	mlan->mode = MODSEL_COMMAND;
	sendpacket[sendlen++] = MODE_COMMAND;

	/* Search off */
	sendpacket[sendlen++] = (uchar) (CMD_COMM | FUNCTSEL_SEARCHOFF | mlan->speed);

	/* flush the buffers */
	mlan->flush(mlan);

	/* Send the packet */
	if (mlan->write(mlan, sendlen, sendpacket)) {
		/* Read back the response */
		if (mlan->read(mlan, 17, readbuffer) == 17) {
			/* See what up */
			for (i = 0; i < 64; i++) {
				/* Get the serial number */
				bitacc(WRITE_FUNCTION,
				bitacc(READ_FUNCTION, 0, (short) (i * 2 + 1),
				     &readbuffer[1]), i, &TempSerialNum[0]);
				if ((bitacc(READ_FUNCTION, 0, (short) (i * 2),
					    &readbuffer[1]) == 1) && (bitacc(READ_FUNCTION, 0,
				(short) (i * 2 + 1), &readbuffer[1]) == 0)) {
					TempLastDiscrepancy = i + 1;
					if (i < 8) {
						mlan->LastFamilyDiscrepancy = i + 1;
					}	/* if */
				}	/* if */
			}	/* for */
			mlan->DOWCRC = 0;
			for (i = 0; i < 8; i++)
				mlan->dowcrc(mlan, TempSerialNum[i]);
			if ((mlan->DOWCRC != 0) || (mlan->LastDiscrepancy == 63)
			    || (TempSerialNum[0] == 0)) {
				mlan->LastDiscrepancy = 0;
				mlan->LastDevice = FALSE;
				mlan->LastFamilyDiscrepancy = 0;
				return (FALSE);
			} else {
				if ((TempLastDiscrepancy == mlan->LastDiscrepancy)
				    || (TempLastDiscrepancy == 0xFF)) {
					mlan->LastDevice = TRUE;
				}
				/* Copy the serial number */
				for (i = 0; i < 8; i++)
					mlan->SerialNum[i] = TempSerialNum[i];

				mlan->LastDiscrepancy = TempLastDiscrepancy;
				return (TRUE);
			}
		}		/* if */
	}			/* if */
	/* Shouldn't get here unless something bad happened, reset. */
	mlan->ds2480detect(mlan);
	mlan->LastDiscrepancy = 0;
	mlan->LastDevice = FALSE;
	mlan->LastFamilyDiscrepancy = 0;

	return (FALSE);
}

MLan           *
mlan_init(char *port, int baud_rate)
{
	MLan           *mlan;

	assert(port);

	mlan = calloc(1, sizeof(MLan));
	assert(mlan);

	/* Serial functions */
	mlan->setbaud = _com_setbaud;
	mlan->flush = _com_flush;
	mlan->write = _com_write;
	mlan->read = _com_read;
	mlan->cbreak = _com_cbreak;

	/* DS2480 functions */
	mlan->ds2480detect = _ds2480_detect;
	mlan->ds2480changebaud = _ds2480_changebaud;

	/* Search functions */
	mlan->first = _mlan_first;
	mlan->next = _mlan_next;

	/* Control functions */
	mlan->setlevel=_mlan_setlevel;

	/* Misc functions */
	mlan->dowcrc = dowcrc;
	mlan->msDelay = msDelay;

	mlan->debug = 0;
	mlan->mode = MODSEL_COMMAND;
	mlan->baud = PARMSET_9600;
	mlan->speed = SPEEDSEL_FLEX;
	mlan->level = MODE_NORMAL;

	mlan->fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (mlan->fd < 0) {
		free(mlan);
		/* Short-circuit on failure */
		return (NULL);
	}
	/* Set the baud rate */
	mlan->setbaud(mlan, baud_rate);

	return (mlan);
}
