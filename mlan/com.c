/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 */

#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <time.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <mlan.h>

#define SHOW_FLAG(i, flag) ( ((i&flag) == 0) ? "" : #flag)
#define PRINT_FLAG(i, flag) printf("%s ", ( ((i&flag) == 0) ? "" : #flag))

static void _com_debug_attr(MLan *mlan, char *message) {
    struct termios com;

    /* Grab the current termios values */
    if(tcgetattr(mlan->fd, &com) != 0) {
        perror("tcgetattr");
    }

    printf("termios flags (%s)\n", message);

    /* Print them out */
    printf("\tciflags:  ");
    PRINT_FLAG(com.c_iflag, BRKINT);
    PRINT_FLAG(com.c_iflag, ICRNL);
    PRINT_FLAG(com.c_iflag, IGNBRK);
    PRINT_FLAG(com.c_iflag, IGNCR);
    PRINT_FLAG(com.c_iflag, IGNPAR);
    PRINT_FLAG(com.c_iflag, IMAXBEL);
    PRINT_FLAG(com.c_iflag, INLCR);
    PRINT_FLAG(com.c_iflag, INPCK);
    PRINT_FLAG(com.c_iflag, ISTRIP);
    PRINT_FLAG(com.c_iflag, IXANY);
    PRINT_FLAG(com.c_iflag, IXOFF);
    PRINT_FLAG(com.c_iflag, IXON);
    PRINT_FLAG(com.c_iflag, PARMRK);
    printf("\n");

    printf("\tcoflags:  ");
    /*
      PRINT_FLAG(com.c_oflag, BSDLY);
      PRINT_FLAG(com.c_oflag, CRDLY);
      PRINT_FLAG(com.c_oflag, FFDLY);
      PRINT_FLAG(com.c_oflag, NLDLY);
      PRINT_FLAG(com.c_oflag, OCRNL);
      PRINT_FLAG(com.c_oflag, OFDEL);
      PRINT_FLAG(com.c_oflag, OFILL);
      PRINT_FLAG(com.c_oflag, OLCUC);
    */
    PRINT_FLAG(com.c_oflag, ONLCR);
    /*
      PRINT_FLAG(com.c_oflag, ONLRET);
      PRINT_FLAG(com.c_oflag, ONOCR);
    */
    /*
      PRINT_FLAG(com.c_oflag, ONOEOT);
      PRINT_FLAG(com.c_oflag, OPOST);
      PRINT_FLAG(com.c_oflag, OXTABS);
    */
    /*
      PRINT_FLAG(com.c_oflag, TABDLY);
      PRINT_FLAG(com.c_oflag, VTDLY);
    */
    printf("\n");

    printf("\tcflags:  ");
    /*
      PRINT_FLAG(com.c_cflag, CIGNORE);
      PRINT_FLAG(com.c_cflag, CLOCAL);
    */
    PRINT_FLAG(com.c_cflag, CS5);
    PRINT_FLAG(com.c_cflag, CS6);
    PRINT_FLAG(com.c_cflag, CS7);
    PRINT_FLAG(com.c_cflag, CS8);
    PRINT_FLAG(com.c_cflag, CREAD);
#ifdef CRTS_IFLOW
    PRINT_FLAG(com.c_cflag, CRTS_IFLOW);
#endif
    PRINT_FLAG(com.c_cflag, CRTSCTS);
    PRINT_FLAG(com.c_cflag, CSIZE);
    PRINT_FLAG(com.c_cflag, CSTOPB);
    PRINT_FLAG(com.c_cflag, HUPCL);
    /*
      PRINT_FLAG(com.c_cflag, MDMBUF);
    */
    PRINT_FLAG(com.c_cflag, PARENB);
    PRINT_FLAG(com.c_cflag, PARODD);
    printf("\n");

    printf("\tlflags:  ");
    /*
      PRINT_FLAG(com.c_lflag, ALTWERASE);
    */
    PRINT_FLAG(com.c_lflag, ECHO);
    PRINT_FLAG(com.c_lflag, ECHOCTL);
    PRINT_FLAG(com.c_lflag, ECHOE);
    PRINT_FLAG(com.c_lflag, ECHOK);
    PRINT_FLAG(com.c_lflag, ECHOKE);
    PRINT_FLAG(com.c_lflag, ECHONL);
    PRINT_FLAG(com.c_lflag, ECHOPRT);
    PRINT_FLAG(com.c_lflag, FLUSHO);
    PRINT_FLAG(com.c_lflag, ICANON);
    PRINT_FLAG(com.c_lflag, IEXTEN);
    PRINT_FLAG(com.c_lflag, ISIG);
    PRINT_FLAG(com.c_lflag, NOFLSH);
    /*
      PRINT_FLAG(com.c_lflag, NOKERNINFO);
    */
    PRINT_FLAG(com.c_lflag, PENDIN);
    PRINT_FLAG(com.c_lflag, TOSTOP);
    /*
      PRINT_FLAG(com.c_lflag, XCASE);
    */
    printf("\n");

    printf("ispeed:  %d\n", (int)cfgetispeed(&com));
    printf("ospeed:  %d\n", (int)cfgetospeed(&com));
}

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
    if(cfsetispeed(&com, speed) != 0) {
        perror("cfsetispeed");
    }
    if(cfsetospeed(&com, speed) != 0) {
        perror("cfsetospeed");
    }

    /* Save it */
    mlan->flush(mlan);
    if(mlan->debug > 2) {
        _com_debug_attr(mlan, "before");
    }
    if(tcsetattr(mlan->fd, TCSANOW, &com) != 0) {
        perror("tcsetattr");
    }
    if(mlan->debug > 2) {
        _com_debug_attr(mlan, "after");
    }

    mlan->baud = new_baud;

    /* mlan_debug(mlan, 2, ("Returning from setbaud\n")); */
}

void _com_flush(MLan *mlan)
{
    assert(mlan);
    mlan_debug(mlan, 2, ("Calling flush\n"));
    if(tcflush(mlan->fd, TCIOFLUSH) != 0) {
        perror("tcflush");
    }
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
            fprintf(stderr, "Timed out on write.\n");
            break;
        }
    }
    mlan_debug(mlan, 2, ("Returning from write (%d==%d - %d)\n",
                         rv, outlen, errno));
    return(rv==outlen);
}

int _com_read(MLan *mlan, int inlen, uchar *inbuf)
{
    int rv=0, i=0;
    time_t start=0, current=0;
    fd_set rfdset;

    assert(mlan);

    mlan_debug(mlan, 2, ("Calling read(%d)\n", inlen));

    /* Give it some time... */
    usleep(mlan->usec_per_byte * (inlen+5) + 1000);

    FD_ZERO(&rfdset);

    start=time(NULL);
    while(rv<inlen) {
        int tmp;
        struct timeval tv;

        tv.tv_sec=mlan->readTimeout;
        tv.tv_usec=0;

        FD_SET(mlan->fd, &rfdset);
        if(select(mlan->fd+1, &rfdset, NULL, NULL, &tv) == 1) {
            tmp=read(mlan->fd, inbuf+rv, inlen-rv);
            if(tmp>=0) {
                rv += tmp;
                mlan_debug(mlan, 3, ("Read %d bytes\n", rv) );
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
        } /* select */
        current=time(NULL);
        if( (current-start) > mlan->readTimeout) {
            fprintf(stderr, "Timed out on read.\n");
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
    if(tcsendbreak(mlan->fd, 1) != 0) {
        perror("tcsendbreak");
    }
    /* mlan_debug(mlan, 2, ("Returning from break\n")); */
}
