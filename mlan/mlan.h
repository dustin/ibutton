/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 */

#ifndef MLAN_H
#define MLAN_H 1

#include <sys/types.h>

#ifdef MYMALLOC
# include <mymalloc.h>
#endif

#include "utility.h"
#include "mlan-defines.h"

struct __mlan;
typedef struct __mlan MLan;

struct __mlan {
    int   debug;                /* Debug Level */
    char *port;                 /* The device we opened */
    int   fd;                   /* File descriptor holding the 1-wire protocol. */

    /* Status stuff */
    int mode;                   /* DS2480 Mode */
    int baud;                   /* DS2480 Baud rate */
    int speed;                  /* DS2480 speed */
    int level;                  /* DS2480 level */

    /* config stuff */
    int usec_per_byte;
    int slew_rate;

    /* Search stuff */
    int LastDiscrepancy;
    int LastDevice;
    int LastFamilyDiscrepancy;

    /* Timeouts */
    int readTimeout;
    int writeTimeout;

    unsigned short CRC16;       /* Storage for CRC 16 */
    uchar          SerialNum[MLAN_SERIAL_SIZE]; /* Serial numbers */
    uchar          DOWCRC;      /* more CRC stuff */
    int            ProgramAvailable; /* Whether programming is available. */

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
    uchar *(*parseSerial)(MLan *mlan, char *serial, uchar *output);
    int (*getBlock)(MLan *mlan, uchar *serial, int page, int pages, uchar *);
    int (*writeScratchpad)(MLan *mlan, uchar *serial, int page, int, uchar *);
    int (*copyScratchpad)(MLan *mlan, uchar *serial, int page, int size);
    int (*clearMemory)(MLan *mlan, uchar *serial);

    /* misc crap */
    uchar (*dowcrc)(MLan *mlan, uchar x);
    ushort (*docrc16)(MLan *mlan, ushort x);
};

/* Get the default mlan port */
char *mlan_get_port();

/* Initialize an mlan device */
MLan *mlan_init(char *port, int baud_rate);

/* Devices */
char *get_sample(MLan *mlan, uchar *serial);

void binDumpBlock(uchar *buffer, int size, int start_addr);
void dumpBlock(uchar *buffer, int size);
uchar *parseSerial(char *in, uchar *out);

/* Common functions */
int recall(MLan *mlan, uchar *serial);

/* GMT offset (in seconds) */
int findGMTOffset(void);


#endif /* MLAN_H */
