/*
 * Copyright (c) 2001  Dustin Sallings <dustin@spy.net>
 *
 * $Id: mlanscm.c,v 1.1 2001/12/08 11:26:58 dustin Exp $
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

#include <libguile.h>
#include <guile/gh.h>

#include <mlan.h>

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
	SCM_ASSERT((mlan!=NULL), dev, "init failed", "mlan-init");

	/* Check to see if there's a debug level passed in */
	if(debuglevel != SCM_EOL) {
		SCM car=SCM_CAR(debuglevel);
		SCM_ASSERT(SCM_INUMP(car), debuglevel, SCM_ARG2, "mlan-init");
		mlan->debug=SCM_INUM(car);
	}

	init_rv=mlan->ds2480detect(mlan);
	assert(init_rv==TRUE);

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
	scm_puts("#<mlan ", port);
	scm_puts(fdstring, port);
	scm_puts(">", port);

	return(1);
}

/* Get the MLan object from a smob. */
static MLan *mlan_getmlan(SCM mlan_smob, char *where)
{
	MLan *mlan=NULL;

	SCM_ASSERT((SCM_NIMP(mlan_smob) && SCM_CAR(mlan_smob) == mlan_tag),
		mlan_smob, SCM_ARG1, where);

	mlan=(MLan *)SCM_CDR(mlan_smob);
	assert(mlan);

	return(mlan);
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

	serial_str=gh_scm2newstr(serial, NULL);
	mlan->parseSerial(mlan, serial_str, ser);
	free(serial_str);

	rv=mlan->access(mlan, ser);

	SCM_ASSERT(rv, serial, "Access failed", "mlan-access");

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
	SCM_ASSERT(rv==value_int, value, "unable to set level", "mlan-setlevel");
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
	int send_cnt=0;
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
		SCM_ASSERT(SCM_INUM(car), car, "int expected", "mlan-block");
		bv=SCM_INUM(car);
		SCM_ASSERT( (bv>=0 && bv<256), car,
			"byte value must be between 0 and 255 (inclusive)", "mlan-block");
		send_block[send_cnt++]=bv;
	}

	tmp=mlan->block(mlan, do_reset_v, send_block, send_cnt);
	SCM_ASSERT(tmp, mlan_smob, "block failed", "mlan-block");

	/* CRC */
	mlan->DOWCRC=0;
	for(tmp=send_cnt-9; tmp<send_cnt; tmp++)
		mlan->dowcrc(mlan, send_block[tmp]);

	SCM_ASSERT(mlan->DOWCRC==0, mlan_smob, "CRC failed", "mlan-block");

	for(; send_cnt>=0; send_cnt--) {
		rv=gh_cons(gh_int2scm(send_block[send_cnt]), rv);
	}

	return(rv);
}

void init_mlan_type()
{
	mlan_tag=scm_make_smob_type("mlan", sizeof(MLan));
	assert(mlan_tag > 0);

	scm_set_smob_print(mlan_tag, print_mlan);
	scm_set_smob_free(mlan_tag, free_mlan);

	scm_make_gsubr("mlan-init", 1, 0, 1, make_mlan);
	scm_make_gsubr("mlan-search", 1, 0, 0, mlan_search);
	scm_make_gsubr("mlan-access", 2, 0, 0, mlan_access);
	scm_make_gsubr("mlan-touchbyte", 2, 0, 0, mlan_touchbyte);
	scm_make_gsubr("mlan-setlevel", 2, 0, 0, mlan_setlevel);
	scm_make_gsubr("mlan-msdelay", 2, 0, 0, mlan_msdelay);
	scm_make_gsubr("mlan-block", 3, 0, 0, mlan_block);
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
