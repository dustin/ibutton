/*
 * Copyright (c) 2001  Dustin Sallings <dustin@spy.net>
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

#include <libguile.h>
#include <guile/gh.h>

#include <mlan.h>

/* This allows me to do a throw with two string arguments (symbol and
 * error) and does everything properly. */
#define THROW(a, b) scm_throw(gh_symbol2scm(a), SCM_LIST1(gh_str02scm(b)))

/* This is deprecated in newer versions, but doesn't exist in older
 * versions.  */
#ifdef SCM_MAJOR_VERSION
# if ((SCM_MAJOR_VERSION>=1) && (SCM_MINOR_VERSION>=5))
#  define MAKE_SUB scm_c_define_gsubr
# endif
#endif

#ifndef MAKE_SUB
# define MAKE_SUB scm_make_gsubr
#endif

static long mlan_tag=-1;

static SCM make_mlan(SCM dev, SCM debuglevel)
{
    MLan *mlan=NULL;
    char *dev_string=NULL;
    SCM mlan_smob;
    int init_rv=0;

    /* Make sure the thing got initialized */
    assert(mlan_tag>0);

    /* Check the argument */
    SCM_ASSERT(SCM_NIMP(dev) && SCM_STRINGP(dev), dev, SCM_ARG1, "mlan-init");
    dev_string=gh_scm2newstr(dev, NULL);
    mlan=mlan_init(dev_string, PARMSET_9600);
    free(dev_string);
    if(mlan==NULL) {
        THROW("init-failed", "Failed to open and init port");
    }

    /* Check to see if there's a debug level passed in */
    if(debuglevel != SCM_EOL) {
        SCM car=SCM_CAR(debuglevel);
        SCM_ASSERT(SCM_INUMP(car), debuglevel, SCM_ARG2, "mlan-init");
        mlan->debug=SCM_INUM(car);
    }

    init_rv=mlan->ds2480detect(mlan);
    if(init_rv!=TRUE) {
        THROW("init-failed", "Failed to detect DS2480");
    }

    SCM_NEWCELL(mlan_smob);
    SCM_SETCDR(mlan_smob, (int)mlan);
    SCM_SETCAR(mlan_smob, mlan_tag);

    return(mlan_smob);
}

static scm_sizet free_mlan(SCM mlan_smob)
{
    MLan *mlan=(MLan *)SCM_CDR(mlan_smob);
    mlan->destroy(mlan);
    return(0);
}

static int print_mlan(SCM mlan_smob, SCM port, scm_print_state *pstate)
{
    char fdstring[16];
    MLan *mlan=(MLan *)SCM_CDR(mlan_smob);

    sprintf(fdstring, "%d", mlan->fd);
    scm_puts("#<mlan dev=", port);
    scm_puts(mlan->port, port);
    scm_puts(", fd=", port);
    scm_puts(fdstring, port);
    scm_puts(">", port);

    return(1);
}

/* Get the MLan object from a smob. */
static MLan *mlan_getmlan(SCM mlan_smob, char *where)
{
    MLan *mlan=NULL;

    SCM_ASSERT((SCM_NIMP(mlan_smob) && (long)SCM_CAR(mlan_smob) == mlan_tag),
               mlan_smob, SCM_ARG1, where);

    mlan=(MLan *)SCM_CDR(mlan_smob);
    assert(mlan);

    return(mlan);
}

/* Is this what we think it is? */
static SCM mlan_p(SCM mlan_smob)
{
    SCM rv=SCM_BOOL_F;

    if(SCM_NIMP(mlan_smob) && (long)SCM_CAR(mlan_smob) == mlan_tag) {
        rv=SCM_BOOL_T;
    }

    return(rv);
}

static SCM mlan_search(SCM mlan_smob)
{
    MLan *mlan=NULL;
    SCM rv=SCM_BOOL_F;
    uchar serial[MLAN_SERIAL_SIZE];
    int i=0, rslt=0;

    mlan=mlan_getmlan(mlan_smob, "mlan-search");

    rv=SCM_EOL;
    rslt=mlan->first(mlan, TRUE, 0);
    while(rslt) {
        uchar ser[(MLAN_SERIAL_SIZE*2)+1];
        memset(&ser, 0x00, sizeof(ser));

        /* Get the serial number */
        mlan->copySerial(mlan, serial);

        /* Convert to hex */
        for(i=0; i<MLAN_SERIAL_SIZE; i++) {
            sprintf(ser+(i*2), "%02X", serial[i]);
        }

        /* Append */
        rv=gh_cons(gh_str2scm(ser, strlen(ser)) ,rv);

        /* Next */
        rslt=mlan->next(mlan, TRUE, 0);
    }

    return(rv);
}

static SCM mlan_access(SCM mlan_smob, SCM serial)
{
    MLan *mlan=NULL;
    uchar ser[MLAN_SERIAL_SIZE];
    char *serial_str=NULL;
    int rv=0;

    mlan=mlan_getmlan(mlan_smob, "mlan-access");

    SCM_ASSERT(SCM_NIMP(serial) && SCM_STRINGP(serial), serial,
               SCM_ARG1, "mlan-access");

    serial_str=gh_scm2newstr(serial, NULL);
    mlan->parseSerial(mlan, serial_str, ser);
    free(serial_str);

    rv=mlan->access(mlan, ser);

    if(rv!=TRUE) {
        THROW("access-failed", "Access call failed");
    }

    return(SCM_BOOL_T);
}

static SCM mlan_touchbyte(SCM mlan_smob, SCM value)
{
    MLan *mlan=NULL;
    int rv=0;
    int value_int=0;
    /* Check type */
    SCM_ASSERT(SCM_INUMP(value), value, SCM_ARG2, "mlan-touchbyte");
    /* parse type */
    value_int=SCM_INUM(value);
    /* Get the MLan thing */
    mlan=mlan_getmlan(mlan_smob, "mlan-touchbyte");
    /* Do the touch */
    rv=mlan->touchbyte(mlan, value_int);
    /* Return the value of the touch */
    return(gh_int2scm(rv));
}

static SCM mlan_setlevel(SCM mlan_smob, SCM value)
{
    MLan *mlan=NULL;
    int rv=0;
    int value_int=0;
    /* Check type */
    SCM_ASSERT(SCM_INUMP(value), value, SCM_ARG2, "mlan-setlevel");
    /* parse type */
    value_int=SCM_INUM(value);
    /* Get the MLan thing */
    mlan=mlan_getmlan(mlan_smob, "mlan-setlevel");
    /* Set the level */
    rv=mlan->setlevel(mlan, value_int);
    /* Return the value of the level */
    if(rv!=value_int) {
        THROW("setlevel-failed", "Failed to set level on MLan bus");
    }
    return(gh_int2scm(rv));
}

static SCM mlan_msdelay(SCM mlan_smob, SCM value)
{
    MLan *mlan=NULL;
    int value_int=0;
    /* Check type */
    SCM_ASSERT(SCM_INUMP(value), value, SCM_ARG2, "mlan-setlevel");
    /* parse type */
    value_int=SCM_INUM(value);
    /* Get the MLan thing */
    mlan=mlan_getmlan(mlan_smob, "mlan-msdelay");
    /* Do the delay */
    mlan->msDelay(mlan, value_int);
    return(SCM_UNSPECIFIED);
}

static SCM mlan_block(SCM mlan_smob, SCM do_reset, SCM bytes)
{
    MLan *mlan=NULL;
    int do_reset_v=0;
    SCM rv=SCM_EOL;
    SCM lit=SCM_EOL;
    uchar send_block[30];
    int send_cnt=0, crcblocks=0;
    int tmp=0;

    /* Check types */
    SCM_ASSERT(SCM_BOOLP(do_reset), do_reset, SCM_ARG2, "mlan-block");
    SCM_ASSERT(SCM_CONSP(bytes), bytes, SCM_ARG3, "mlan-block");
    /* parse type */
    do_reset_v=SCM_NFALSEP(do_reset);
    /* Just to be safe, because I can't remember true from false */
    if(do_reset_v) { do_reset_v=TRUE; }

    /* Get the MLan thing */
    mlan=mlan_getmlan(mlan_smob, "mlan-block");

    /* OK, grab the bytes */
    for(lit=bytes; SCM_CONSP(lit); lit=SCM_CDR(lit)) {
        SCM car=SCM_CAR(lit);
        int bv=0;
        if(!SCM_INUMP(car)) {
            THROW("invalid-argument", "MLan block must contain only ints");
        }
        bv=SCM_INUM(car);
        if(bv>255 || bv<0) {
            THROW("invalid-argument",
                  "byte value must be between 0 and 255 (inclusive)");
        }
        if(bv==0xff) {
            crcblocks++;
        }
        send_block[send_cnt++]=bv;
    }

    tmp=mlan->block(mlan, do_reset_v, send_block, send_cnt);
    if(tmp!=TRUE) {
        THROW("block-failed", "MLan block failed");
    }

    /* CRC */
    mlan->DOWCRC=0;
    for(tmp=send_cnt-crcblocks; tmp<send_cnt; tmp++)
        mlan->dowcrc(mlan, send_block[tmp]);

    if(mlan->DOWCRC!=0) {
        THROW("crc-failed", "MLan block CRC failed");
    }

    for(; send_cnt>=0; send_cnt--) {
        rv=gh_cons(gh_int2scm(send_block[send_cnt]), rv);
    }

    return(rv);
}

static SCM mlan_getblock(SCM mlan_smob, SCM serial, SCM page, SCM pages)
{
    MLan *mlan=NULL;
    char *serial_str;
    SCM rv=SCM_EOL;
    uchar ser[MLAN_SERIAL_SIZE];
    uchar buffer[256*32];
    int i=0;

    /* Check types */
    SCM_ASSERT(SCM_INUMP(page), page, SCM_ARG3, "mlan-getblock");
    SCM_ASSERT(SCM_INUMP(pages), pages, SCM_ARG4, "mlan-getblock");

    /* Check the size */
    if( (32*SCM_INUM(pages)) >= sizeof(buffer) ) {
        THROW("invalid-argument", "pages must be less than 256");
    }

    /* Get the MLan thing */
    mlan=mlan_getmlan(mlan_smob, "mlan-getblock");

    /* Parse the serial */
    SCM_ASSERT(SCM_NIMP(serial) && SCM_STRINGP(serial), serial,
               SCM_ARG2, "mlan-access");
    serial_str=gh_scm2newstr(serial, NULL);
    mlan->parseSerial(mlan, serial_str, ser);
    free(serial_str);

    mlan->getBlock(mlan, ser, SCM_INUM(page), SCM_INUM(pages), buffer);

    for(i=32*SCM_INUM(pages); i>=0; i--) {
        rv=gh_cons(gh_int2scm(buffer[i]), rv);
    }

    return(rv);
}

SCM mlan_parse_serial(SCM mlan_smob, SCM serial)
{
    MLan *mlan=NULL;
    uchar ser[MLAN_SERIAL_SIZE];
    char *serial_str=NULL;
    SCM rv=SCM_EOL;
    int i=0;

    mlan=mlan_getmlan(mlan_smob, "mlan-parse-serial");

    SCM_ASSERT(SCM_NIMP(serial) && SCM_STRINGP(serial), serial,
               SCM_ARG1, "mlan-parse-serial");

    serial_str=gh_scm2newstr(serial, NULL);
    mlan->parseSerial(mlan, serial_str, ser);
    free(serial_str);

    for(i=(MLAN_SERIAL_SIZE-1); i>=0; i--) {
        rv=gh_cons(gh_int2scm(ser[i]), rv);
    }

    return(rv);
}

static SCM mlan_reset(SCM mlan_smob)
{
    MLan *mlan=NULL;
    int rv=0;
    /* Get the MLan thing */
    mlan=mlan_getmlan(mlan_smob, "mlan-reset");
    /* Do the touch */
    rv=mlan->reset(mlan);
    /* Return the value of the touch */
    return(gh_int2scm(rv));
}

void init_mlan_type()
{
    mlan_tag=scm_make_smob_type("mlan", sizeof(MLan));
    assert(mlan_tag > 0);

    scm_set_smob_print(mlan_tag, print_mlan);
    scm_set_smob_free(mlan_tag, free_mlan);

    /* Subs */
    MAKE_SUB("mlan-init", 1, 0, 1, make_mlan);
    MAKE_SUB("mlanp", 1, 0, 0, mlan_p);
    MAKE_SUB("mlan-search", 1, 0, 0, mlan_search);
    MAKE_SUB("mlan-access", 2, 0, 0, mlan_access);
    MAKE_SUB("mlan-touchbyte", 2, 0, 0, mlan_touchbyte);
    MAKE_SUB("mlan-setlevel", 2, 0, 0, mlan_setlevel);
    MAKE_SUB("mlan-msdelay", 2, 0, 0, mlan_msdelay);
    MAKE_SUB("mlan-block", 3, 0, 0, mlan_block);
    MAKE_SUB("mlan-getblock", 4, 0, 0, mlan_getblock);
    MAKE_SUB("mlan-parse-serial", 2, 0, 0, mlan_parse_serial);
    MAKE_SUB("mlan-reset", 1, 0, 0, mlan_reset);

    /* Constants */
    gh_define("mlan-mode-normal", gh_int2scm(0x00));
    gh_define("mlan-mode-overdrive", gh_int2scm(0x01));
    gh_define("mlan-mode-strong5", gh_int2scm(0x02));
    gh_define("mlan-mode-program", gh_int2scm(0x04));
    gh_define("mlan-mode-break", gh_int2scm(0x08));
}

static void inner_main(int argc, char **argv)
{
    init_mlan_type();
    gh_repl(argc, argv);
}

int main(int argc, char **argv)
{
    gh_enter(argc, argv, inner_main);
    return(0);
}
