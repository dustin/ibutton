/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: mlan.h,v 1.15 2001/12/10 08:23:12 dustin Exp $
 */

#ifndef MLAN_H
#define MLAN_H 1

/* Need a uchar */
typedef unsigned char uchar;

#define WRITE_FUNCTION 1
#define READ_FUNCTION  0   

#define READ_ERROR    -1
#define INVALID_DIR   -2       
#define NO_FILE       -3    
#define WRITE_ERROR   -4   
#define WRONG_TYPE    -5
#define FILE_TOO_BIG  -6

#ifndef TRUE
#define FALSE          0
#define TRUE           1
#endif

/* DS2480 modes */
#define MODE_DATA                      0xE1
#define MODE_COMMAND                   0xE3
#define MODE_STOP_PULSE                0xF1

/* Return byte values */
#define RB_CHIPID_MASK                 0x1C
#define RB_RESET_MASK                  0x03
#define RB_1WIRESHORT                  0x00
#define RB_PRESENCE                    0x01
#define RB_ALARMPRESENCE               0x02 
#define RB_NOPRESENCE                  0x03

#define RB_BIT_MASK                    0x03
#define RB_BIT_ONE                     0x03
#define RB_BIT_ZERO                    0x00

/* Bit range masks */
#define CMD_MASK                       0x80
#define FUNCTSEL_MASK                  0x60
#define BITPOL_MASK                    0x10
#define SPEEDSEL_MASK                  0x0C
#define MODSEL_MASK                    0x02
#define PARMSEL_MASK                   0x70
#define PARMSET_MASK                   0x0E

/* Command or config */
#define CMD_COMM                       0x81
#define CMD_CONFIG                     0x01

/* Function Select bits */
#define FUNCTSEL_BIT                   0x00
#define FUNCTSEL_SEARCHON              0x30
#define FUNCTSEL_SEARCHOFF             0x20
#define FUNCTSEL_RESET                 0x40
#define FUNCTSEL_CHMOD                 0x60

/* Bit polarity/pulse voltage bits */
#define BITPOL_ONE                     0x10
#define BITPOL_ZERO                    0x00
#define BITPOL_5V                      0x00
#define BITPOL_12V                     0x10

/* One wire speed bits */
#define SPEEDSEL_STD                   0x00
#define SPEEDSEL_FLEX                  0x04
#define SPEEDSEL_OD                    0x08
#define SPEEDSEL_PULSE                 0x0C

/* Data/command mode select bits */
#define MODSEL_DATA                    0x00
#define MODSEL_COMMAND                 0x02

/* 5V follow pulse select bits) */
#define PRIME5V_TRUE                   0x02
#define PRIME5V_FALSE                  0x00

/* Parameter select bits */
#define PARMSEL_PARMREAD               0x00
#define PARMSEL_SLEW                   0x10
#define PARMSEL_12VPULSE               0x20
#define PARMSEL_5VPULSE                0x30
#define PARMSEL_WRITE1LOW              0x40
#define PARMSEL_SAMPLEOFFSET           0x50
#define PARMSEL_ACTIVEPULLUPTIME       0x60
#define PARMSEL_BAUDRATE               0x70

/* Pull down slew rates */
#define PARMSET_Slew15Vus              0x00
#define PARMSET_Slew2p2Vus             0x02
#define PARMSET_Slew1p65Vus            0x04
#define PARMSET_Slew1p37Vus            0x06
#define PARMSET_Slew1p1Vus             0x08
#define PARMSET_Slew0p83Vus            0x0A
#define PARMSET_Slew0p7Vus             0x0C
#define PARMSET_Slew0p55Vus            0x0E

/* 12V programming pulse time tables */
#define PARMSET_32us                   0x00
#define PARMSET_64us                   0x02
#define PARMSET_128us                  0x04
#define PARMSET_256us                  0x06
#define PARMSET_512us                  0x08
#define PARMSET_1024us                 0x0A
#define PARMSET_2048us                 0x0C
#define PARMSET_infinite               0x0E

/* 5V strong pul up pulse rate time table */
#define PARMSET_16p4ms                 0x00
#define PARMSET_65p5ms                 0x02
#define PARMSET_131ms                  0x04
#define PARMSET_262ms                  0x06
#define PARMSET_524ms                  0x08
#define PARMSET_1p05s                  0x0A
#define PARMSET_2p10s                  0x0C
#define PARMSET_infinite               0x0E

/* Write 1 low time */
#define PARMSET_Write8us               0x00
#define PARMSET_Write9us               0x02
#define PARMSET_Write10us              0x04
#define PARMSET_Write11us              0x06
#define PARMSET_Write12us              0x08
#define PARMSET_Write13us              0x0A
#define PARMSET_Write14us              0x0C
#define PARMSET_Write15us              0x0E

/* Data sample offset and write 0 recovery time */
#define PARMSET_SampOff3us             0x00
#define PARMSET_SampOff4us             0x02
#define PARMSET_SampOff5us             0x04
#define PARMSET_SampOff6us             0x06
#define PARMSET_SampOff7us             0x08
#define PARMSET_SampOff8us             0x0A
#define PARMSET_SampOff9us             0x0C
#define PARMSET_SampOff10us            0x0E

/* Active pull up on time */
#define PARMSET_PullUp0p0us            0x00
#define PARMSET_PullUp0p5us            0x02
#define PARMSET_PullUp1p0us            0x04
#define PARMSET_PullUp1p5us            0x06
#define PARMSET_PullUp2p0us            0x08
#define PARMSET_PullUp2p5us            0x0A
#define PARMSET_PullUp3p0us            0x0C
#define PARMSET_PullUp3p5us            0x0E

/* Baud rate bits */
#define PARMSET_9600                   0x00
#define PARMSET_19200                  0x02
#define PARMSET_57600                  0x04
#define PARMSET_115200                 0x06

/* DS2480 program voltage available */
#define DS2480PROG_MASK                0x20

/* Mode bit flags */
#define MODE_NORMAL                    0x00
#define MODE_OVERDRIVE                 0x01
#define MODE_STRONG5                   0x02
#define MODE_PROGRAM                   0x04
#define MODE_BREAK                     0x08

/* Data sizes */

#define MLAN_SERIAL_SIZE				8
#define MAX_SERIAL_NUMS					200

/* Tweaking stuff */

/* Default amount of time before we give up on a read or write */
#define MLAN_DEFAULT_TIMEOUT			5

#ifndef mlan_debug
#define mlan_debug(a, b, c) if(a->debug > b) { printf c; }
#endif

struct __mlan;
typedef struct __mlan MLan;

struct __mlan {
	int debug;		/* Debug Level */
	char *port;		/* The device we opened */
	int fd;			/* File descriptor holding the 1-wire protocol. */

	/* Status stuff */
	int mode;		/* DS2480 Mode */
	int baud;		/* DS2480 Baud rate */
	int speed;	/* DS2480 speed */
	int level;	/* DS2480 level */

	/* config stuff */
	int	usec_per_byte;
	int slew_rate;

	/* Search stuff */
	int LastDiscrepancy;
	int LastDevice;
	int LastFamilyDiscrepancy;

	/* Timeouts */
	int readTimeout;
	int writeTimeout;

	unsigned short CRC16;	/* Storage for CRC 16 */
	uchar SerialNum[MLAN_SERIAL_SIZE];		/* Serial numbers */
	uchar DOWCRC;			/* more CRC stuff */
	int ProgramAvailable;	/* Whether programming is available. */

	/* Tear the thing down */
	void (*destroy)(MLan *mlan);

	/* Serial Functions */
	void (*setbaud)(MLan *mlan, int newbaud); /* Set the baud rate */
	void (*flush)(MLan *mlan);
	int (*write)(MLan *mlan, int outlen, uchar *outbuf);
	int (*read)(MLan *mlan, int inlen, uchar *inbuf);
	void (*cbreak)(MLan *mlan);

	/* DS2480 functions */
	int (*ds2480detect)(MLan *mlan);
	int (*ds2480changebaud)(MLan *mlan, uchar new_baud);

	/* Search functions */
	int (*first)(MLan *mlan, int DoReset, int OnlyAlarmingDevices);
	int (*next)(MLan *mlan, int DoReset, int OnlyAlarmingDevices);

	/* MLAN functions */
	int (*access)(MLan *mlan, uchar *serial);
	int (*block)(MLan *mlan, int doreset, uchar *buf, int len);
	int (*reset)(MLan *mlan);

	/* Misc commands and stuff */
	int (*setlevel)(MLan *mlan,int newlevel);
	int (*touchbit)(MLan *mlan, int sendbit);
	int (*writebyte)(MLan *mlan, int sendbyte);
	int (*readbyte)(MLan *mlan);
	int (*touchbyte)(MLan *mlan, int sendbyte);
	int (*setspeed)(MLan *mlan, int newspeed);
	int (*programpulse)(MLan *mlan);
	void (*msDelay)(MLan *mlan, int t);
	void (*copySerial)(MLan *mlan, uchar *in);
	char *(*serialLookup)(MLan *mlan, int which);
	void (*registerSerial)(MLan *mlan, int id, char *value);
	char *(*parseSerial)(MLan *mlan, char *serial, uchar *output);
	int (*getBlock)(MLan *mlan, uchar *serial, int page, int pages, uchar *);
	int (*writeScratchpad)(MLan *mlan, uchar *serial, int page, int, uchar *);
	int (*copyScratchpad)(MLan *mlan, uchar *serial, int page, int size);
	int (*clearMemory)(MLan *mlan, uchar *serial);

	/* misc crap */
	uchar (*dowcrc)(MLan *mlan, uchar x);
};

MLan *mlan_init(char *port, int baud_rate);

/* Devices */
char *get_sample(MLan *mlan, uchar *serial);

float ctof(float);
float ftoc(float);
void binDumpBlock(uchar *buffer, int size, int start_addr);
void dumpBlock(uchar *buffer, int size);

/* GMT offset (in seconds) */
int findGMTOffset(void); 


#endif /* MLAN_H */
