/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: mlan.c,v 1.25 2002/01/30 06:31:02 dustin Exp $
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

extern void     _com_setbaud(MLan * mlan, int new_baud);
extern void     _com_flush(MLan * mlan);
extern int      _com_write(MLan * mlan, int outlen, uchar * outbuf);
extern int      _com_read(MLan * mlan, int inlen, uchar * inbuf);
extern void     _com_cbreak(MLan * mlan);
extern int      _ds2480_detect(MLan * mlan);
extern int      _ds2480_changebaud(MLan * mlan, uchar newbaud);

static int _table_initialized=0;
static char **serial_table=NULL;

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
	assert(mlan);
	mlan->DOWCRC = dscrc_table[mlan->DOWCRC ^ x];
	return (mlan->DOWCRC);
}

/* Convert a hex string to binary thingy */
static uchar *
h2b(int size, char *buf, uchar *outbuf)
{
	int i=0, j=0;
	int map[256];

	assert(buf);
	assert(outbuf);
	assert(strlen(buf)==(size*2));

	for(i=0; i<256; i++) {
		if (i >= '0' && i <= '9') {
			map[i] = i - '0';
		} else if (i >= 'a' && i <= 'f') {
			map[i] = (i - 'a') + 10;
		} else if (i >= 'A' && i <= 'F') {
			map[i] = (i - 'A') + 10;
		} else {
			map[i] = -1;
		}
	}

	for (i = 0; i < size * 2; i += 2) {
		assert(map[(int) buf[i]] >= 0 && map[(int) buf[i + 1]] >= 0);
		outbuf[j++]=(map[(int) buf[i]] << 4 | map[(int) buf[i + 1]]);
	}
	return(outbuf);
}

/* External API for parsing serial numbers */
char *
parseSerial(char *in, uchar *out)
{
	assert(in);
	assert(out);
	assert(strlen(in)==(MLAN_SERIAL_SIZE*2));
	return(h2b(MLAN_SERIAL_SIZE, in, out));
}

/* Parse a serial number */
static char *
_mlan_parseSerial(MLan *mlan, char *in, uchar *out)
{
	return(parseSerial(in, out));
}

/* Millisecond sleep */
static void
msDelay(MLan *mlan, int t)
{
	assert(mlan);
	mlan_debug(mlan, 2, ("Sleeping %dms\n", t) );
	usleep(t * 1000);
}

/* Bit utility to read and write a bit in a buffer */
static int
bitacc(int op, int state, int loc, uchar * buf)
{
	int             nbyt, nbit;

	assert(buf);

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

	assert(mlan);

	if(newlevel != mlan->level) {
		/* OK, we need to do this either way. */
		if(mlan->mode != MODSEL_COMMAND) {
			mlan->mode=MODSEL_COMMAND;
			sendpacket[sendlen++] = MODE_COMMAND;
		}
		if(newlevel == MODE_NORMAL) {
			/* Stop pulse command */
			sendpacket[sendlen++] = MODE_STOP_PULSE;

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

static int
_mlan_touchreset(MLan *mlan)
{
	uchar readbuffer[10],sendpacket[10];
	int sendlen=0;

	assert(mlan);

	mlan->setlevel(mlan, MODE_NORMAL);

	if(mlan->mode != MODSEL_COMMAND) {
		mlan->mode = MODSEL_COMMAND;
		sendpacket[sendlen++] = MODE_COMMAND;
	}

	sendpacket[sendlen++] = (uchar)(CMD_COMM | FUNCTSEL_RESET | mlan->speed);

	mlan_debug(mlan, 3, ("Reset sending %x (%x | %x | %x)\n",
		sendpacket[0], CMD_COMM, FUNCTSEL_RESET, mlan->speed) );

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
	assert(mlan);

	mlan->LastDiscrepancy = 0;
	mlan->LastDevice = FALSE;
	mlan->LastFamilyDiscrepancy = 0;

	return (mlan->next(mlan, DoReset, OnlyAlarmingDevices));
}

static int
_mlan_next(MLan * mlan, int DoReset, int OnlyAlarmingDevices)
{
	int             i, TempLastDiscrepancy, pos;
	uchar           TempSerialNum[MLAN_SERIAL_SIZE];
	uchar           readbuffer[20], sendpacket[40];
	int             sendlen = 0;

	assert(mlan);

	/* If the last was the last, reset the search */
	if (mlan->LastDevice) {
		mlan->LastDiscrepancy = 0;
		mlan->LastDevice = FALSE;
		mlan->LastFamilyDiscrepancy = 0;
		return(FALSE);
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
		sendpacket[sendlen++] = CONDITIONAL_SEARCH_ROM;
	} else {
		sendpacket[sendlen++] = SEARCH_ROM;
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
			mlan_debug(mlan, 3, ("Serial:  "));
			for (i = 0; i < MLAN_SERIAL_SIZE; i++) {
				mlan_debug(mlan, 3, ("%02x ", TempSerialNum[i]) );
				mlan->dowcrc(mlan, TempSerialNum[i]);
			}
			mlan_debug(mlan, 3, ("\n"));
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
				for (i = 0; i < MLAN_SERIAL_SIZE; i++)
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

static void
_copy_serial(MLan *mlan, uchar *dest)
{
	int i;

	assert(mlan);
	assert(dest);

	for(i=0; i<MLAN_SERIAL_SIZE; i++) {
		dest[i]=mlan->SerialNum[i];
	}
}

static void
_mlan_init_table(void)
{
	assert(serial_table==NULL);

	serial_table = (char **)calloc(256, sizeof(char *));
	assert(serial_table);

	/* These are things we know */
	assert(serial_table[0x01] == NULL);
	serial_table[0x01]=strdup("DS1990 Serial Number iButton");
	assert(serial_table[0x02] == NULL);
	serial_table[0x02]=strdup("DS1991 Multi-key iButton");
	assert(serial_table[0x04] == NULL);
	serial_table[0x04]=strdup("DS1992/1993/1994 Memory iButton");
	assert(serial_table[0x09] == NULL);
	serial_table[0x09]=strdup("DS9097u 1-wire to RS232 converter");
	assert(serial_table[0x0a] == NULL);
	serial_table[0x0a]=strdup("DS1995 64kbit Memory iButton");
	assert(serial_table[0x0b] == NULL);
	serial_table[0x0b]=strdup("DS1985 16kbit Add-only iButton");
	assert(serial_table[0x0c] == NULL);
	serial_table[0x0c]=strdup("DS1986 64kbit Memory iButton");
	assert(serial_table[0x0f] == NULL);
	serial_table[0x0f]=strdup("DS1986 64kbit Add-only iButton");
	assert(serial_table[0x10] == NULL);
	serial_table[0x10]=strdup("DS1820/1920 Temperature Sensor");
	assert(serial_table[0x12] == NULL);
	serial_table[0x12]=strdup("DS2407 2 Channel Switch");
	assert(serial_table[0x14] == NULL);
	serial_table[0x14]=strdup("DS1971 256-bit EEPROM iButton");
	assert(serial_table[0x18] == NULL);
	serial_table[0x18]=strdup("DS1962/1963 Monetary iButton");
	assert(serial_table[0x1a] == NULL);
	serial_table[0x1a]=strdup("DS1963L Monetary iButton");
	assert(serial_table[0x21] == NULL);
	serial_table[0x21]=strdup("DS1921 Thermochron");
	assert(serial_table[0x26] == NULL);
	serial_table[0x26]=strdup("DS2438 Battery Monitor/Humidity Sensor");
	assert(serial_table[0x23] == NULL);
	serial_table[0x23]=strdup("DS1973 4kbit EEPROM iButton");
	assert(serial_table[0x96] == NULL);
	serial_table[0x96]=strdup("DS199550-400 Java Button");

	_table_initialized=1;
}

static void
_mlan_register_serial(MLan *mlan, int id, char *str)
{
	assert(id < 256);
	assert(id >= 0);
	assert(str != NULL);

	if(serial_table[id]!=NULL) {
		free(serial_table[id]);
	}

	serial_table[id]=strdup(str);
}

static char *
_mlan_serial_lookup(MLan *mlan, int id)
{
	assert(id < 256);
	assert(id >= 0);
	return(serial_table[id]);
}

static int
_mlan_block(MLan *mlan, int doreset, uchar *buf, int len)
{
	uchar sendpacket[80];
	int sendlen=0, pos=0, i=0, crcblocks=0;

	assert(mlan);
	assert(buf);

	assert(len<64);

	if(doreset) {
		if(! (mlan->reset(mlan))) {
			mlan_debug(mlan, 2, ("mlan_block: reset failed\n"));
			return(FALSE);
		}
	}

	if(mlan->mode != MODSEL_DATA) {
		mlan->mode=MODSEL_DATA;
		sendpacket[sendlen++] = MODE_DATA;
	}

	pos=sendlen;
	for(i=0; i<len; i++) {
		sendpacket[sendlen++]=buf[i];
		if(buf[i]==MODE_COMMAND) {
			sendpacket[sendlen++] = buf[i];
		} else if(buf[i]==0xff) {
			crcblocks++;
		}
	}

	if(mlan->write(mlan, sendlen, sendpacket)) {
		if(mlan->read(mlan, len, buf) == len) {
			mlan->DOWCRC=0;
			for(i=sendlen-crcblocks; i<sendlen; i++) {
				mlan->dowcrc(mlan, sendpacket[i]);
			}
			if(mlan->DOWCRC==0) {
				return(TRUE);
			} else {
				mlan_debug(mlan, 2, ("mlan_block: CRC failed\n"));
			}
		}
	}

	/* If the above failed, resync */
	mlan->ds2480detect(mlan);
	mlan_debug(mlan, 2, ("mlan_block: everything failed\n"));
	return(FALSE);
}

static int
_mlan_access(MLan *mlan, uchar *serial)
{
	uchar send_block[MLAN_SERIAL_SIZE + 5];
	int send_cnt=0;
	int i;

	assert(mlan);
	assert(serial);

	if(mlan->reset(mlan)) {
		send_block[send_cnt++]=MATCH_ROM;
		for(i=0; i<MLAN_SERIAL_SIZE; i++) {
			send_block[send_cnt++]=serial[i];
		}

		if(mlan->block(mlan, FALSE, send_block, send_cnt)) {
			for(i=0; i<MLAN_SERIAL_SIZE ; i++) {
				if(send_block[i+1] != serial[i]) {
					return(FALSE);
				}
			}
			if(send_block[0]!=MATCH_ROM)
				return(FALSE);
			else
				return(TRUE);
		}
	}
	/* If the reset failed, return false */
	return(FALSE);
}

static int
_mlan_touchbyte(MLan *mlan, int byte)
{
	uchar readbuffer[10], sendpacket[10];
	int sendlen=0;

	assert(mlan);

	mlan->level=MODE_NORMAL;

	if(mlan->mode != MODSEL_DATA) {
		mlan->mode=MODSEL_DATA;
		sendpacket[sendlen++] = MODE_DATA;
	}

	sendpacket[sendlen++] = (uchar)byte;

	if (byte == MODE_COMMAND)
		sendpacket[sendlen++] = (uchar)byte;

	if(mlan->write(mlan, sendlen, sendpacket)) {
		if(mlan->read(mlan, 1, readbuffer)==1) {
			return((int)readbuffer[0]);
		}
	}
	mlan->ds2480detect(mlan);
	return(0);
}

static int
_mlan_writescratchpad(MLan *mlan, uchar *serial, int page,int size,uchar *data)
{
	uchar send_block[128];
	int send_cnt=0;
	int i=0, atmp=0;

	assert(mlan);
	assert(serial);
	assert(data);

	mlan_debug(mlan, 2, ("-- writeScratchpad, %d bytes at page %d --\n",
		size, page));

	if(mlan->reset(mlan)==FALSE) {
		return(FALSE);
	}

	send_block[send_cnt++] = MATCH_ROM;
	for(i=0; i<8; i++)
		send_block[send_cnt++] = serial[i];
	/* Our command */
	send_block[send_cnt++] = WRITE_SCRATCHPAD;
	/* Where will we be writing? */
	atmp=page*0x20;
	send_block[send_cnt++] = atmp & 0xff;
	send_block[send_cnt++] = atmp>>8;
	mlan_debug(mlan, 3, ("writeScratchpad using address %02X%02X\n",
		send_block[send_cnt-2], send_block[send_cnt-1]));
	/* Give it our data */
	for(i=0; i<size; i++) {
		send_block[send_cnt++]=data[i];
	}
	if(! (mlan->block(mlan, TRUE, send_block, send_cnt))) {
		printf("Error writing to scratchpad!\n");
		return(FALSE);
	}

	/* Read it back */
	send_cnt=0;
	send_block[send_cnt++] = MATCH_ROM;
	for(i=0; i<8; i++)
		send_block[send_cnt++] = serial[i];
	send_block[send_cnt++] = READ_SCRATCHPAD;
	for (i = 0; i < (size + 3); i++)
		send_block[send_cnt++] = 0xFF;
	if(! (mlan->block(mlan, TRUE, send_block, send_cnt))) {
		printf("Error reading from scratchpad!\n");
		return(FALSE);
	}

	/* Verify it */
	if(send_block[10] != (atmp&0xff)) {
		printf("writescratchpad: First half of start page was incorrect.\n");
		return(FALSE);
	}
	if(send_block[11] != (atmp>>8)) {
		printf("writescratchpad: Second half of start page was incorrect.\n");
		return(FALSE);
	}
	if(send_block[12] != (size-1)) {
		printf("writescratchpad: Size was incorrect.\n");
		return(FALSE);
	}

	for(i=0; i<size; i++) {
		if(send_block[13+i] != data[i]) {
			printf("writescratchpad: Byte %d was incorrect during read.\n", i);
			return(FALSE);
		}
	}

	return(TRUE);
}

static int
_mlan_copyscratchpad(MLan *mlan, uchar *serial, int page, int length)
{
	uchar send_block[128];
	int send_cnt=0;
	int i=0, atmp=0;

	assert(mlan);
	assert(serial);
	assert(length>0);

	i=0;

	mlan_debug(mlan, 2, ("-- copyScratchpad (%d %d) --\n", page, length));

	send_block[send_cnt++] = MATCH_ROM;
	for(i=0; i<8; i++)
		send_block[send_cnt++] = serial[i];
	send_block[send_cnt++] = COPY_SCRATCHPAD;
	atmp=page*0x20;
	send_block[send_cnt++] = atmp & 0xff;
	send_block[send_cnt++] = atmp>>8;
	send_block[send_cnt++] = length-1;
	send_block[send_cnt++] = 0xff;

	if(! (mlan->block(mlan, TRUE, send_block, send_cnt)) ) {
		printf("Error copying scratchpad!\n");
		return(FALSE);
	}

	if(send_block[10] != (atmp & 0xff)) {
		printf("copyscratchpad: First position byte was incorrect.\n");
		return(FALSE);
	}
	if(send_block[11] != (atmp>>8)) {
		printf("copyscratchpad: Second position byte was incorrect.\n");
		return(FALSE);
	}
	if(send_block[12] != (length-1)) {
		printf("copyscratchpad: length was incorrect.\n");
		return(FALSE);
	}

	return(TRUE);
}

static int
_mlan_clearmemory(MLan *mlan, uchar *serial)
{
	uchar send_block[80];
	int send_cnt=0, i=0;

	assert(mlan);
	assert(serial);

	send_block[send_cnt++] = MATCH_ROM;
	for(i=0; i<8; i++)
		send_block[send_cnt++] = serial[i];
	send_block[send_cnt++] = CLEAR_MEMORY;

	if(mlan->block(mlan, TRUE, send_block, send_cnt)==FALSE) {
		return(FALSE);
	}

	return(TRUE);
}

static int
_mlan_getblock(MLan *mlan, uchar *serial, int page, int pages, uchar *buffer)
{
	uchar send_block[128];
	int header=0;
	int send_cnt=0, atmp=0;
	int offset=0;
	int i=0, j=0;

	assert(mlan);
	assert(serial);
	assert(buffer);

	do {
		/* Start back at zero */
		send_cnt=0;

		/* Clear out the send set */
		for(i=0; i<sizeof(send_block); i++) {
			send_block[i]=0x00;
		}

		mlan->DOWCRC=0;
		send_block[send_cnt++] = MATCH_ROM;
		for(i=0; i<8; i++)
			send_block[send_cnt++] = serial[i];
		/* Now our command */
		send_block[send_cnt++] = READ_MEMORY;
		/* Calculate the position of the page...each page is 0x20 bytes,
		 * and the second byte goes first. */
		atmp=page*0x20;
		send_block[send_cnt++] = atmp & 0xff;
		send_block[send_cnt++] = atmp>>8;
		mlan->dowcrc(mlan, send_block[send_cnt-2]);
		mlan->dowcrc(mlan, send_block[send_cnt-1]);
		header=send_cnt;

		for (i = 0; i < 34; i++)
			send_block[send_cnt++]=0xff;
		/* send the block */
		if(! (mlan->block(mlan, TRUE, send_block, send_cnt)) ) {
			printf("Error sending request for block!\n");
			return(FALSE);
		}

		for(i=0; i<34; i++) {
			mlan->dowcrc(mlan, send_block[header+i+1]);
		}

		/* Copy it into the return buffer */
		for(j=offset, i=header; i<header+32; i++) {
			buffer[j++]=send_block[i];
		}

		offset+=32;
		pages--;
		page++;
	} while(pages>0);
	return(TRUE);
}

static void
_mlan_destroy(MLan *mlan)
{
	assert(mlan);
	if(mlan->fd>0) {
		if(close(mlan->fd) < 0) {
			perror("close");
		}
		/* Free our copy of the port */
		if(mlan->port!=NULL) {
			free(mlan->port);
		}
	}
	free(mlan);
}

MLan           *
mlan_init(char *port, int baud_rate)
{
	MLan           *mlan;

	assert(port);

	if(_table_initialized==0) {
		_mlan_init_table();
	}

	mlan = calloc(1, sizeof(MLan));
	assert(mlan);

	/* Destroy */
	mlan->destroy=_mlan_destroy;

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

	/* MLan functions */
	mlan->reset = _mlan_touchreset;
	mlan->touchbyte = _mlan_touchbyte;
	mlan->access = _mlan_access;
	mlan->block = _mlan_block;

	/* Control functions */
	mlan->setlevel=_mlan_setlevel;

	/* Misc functions */
	mlan->dowcrc = dowcrc;
	mlan->msDelay = msDelay;
	mlan->copySerial = _copy_serial;
	mlan->serialLookup = _mlan_serial_lookup;
	mlan->registerSerial = _mlan_register_serial;
	mlan->parseSerial = _mlan_parseSerial;
	mlan->getBlock = _mlan_getblock;
	mlan->writeScratchpad = _mlan_writescratchpad;
	mlan->copyScratchpad = _mlan_copyscratchpad;
	mlan->clearMemory = _mlan_clearmemory;

	mlan->debug = 0;
	mlan->mode = MODSEL_COMMAND;
	mlan->baud = PARMSET_9600;
	mlan->speed = SPEEDSEL_FLEX;
	mlan->level = MODE_NORMAL;
	mlan->slew_rate = PARMSET_Slew1p65Vus;
	mlan->usec_per_byte = 860;

	mlan->readTimeout = MLAN_DEFAULT_TIMEOUT;
	mlan->writeTimeout = MLAN_DEFAULT_TIMEOUT;

	mlan->fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
	if (mlan->fd < 0) {
		perror(port);
		free(mlan);
		/* Short-circuit on failure */
		return (NULL);
	}
	/* Save the port */
	mlan->port=strdup(port);
	/* Set the baud rate */
	mlan->setbaud(mlan, baud_rate);

	return (mlan);
}
