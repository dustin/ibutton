/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: storage-rrd.c,v 1.8 2002/01/30 21:10:30 dustin Exp $
 */

#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#ifdef HAVE_RRD_H
/* Workaround for a namespace collision */
#define time_value rrd_time_value
#include <rrd.h>
#undef time_value
#endif /* HAVE_RRD_H */

/* Local includes */
#include "data.h"
#include "collection.h"
#include "storage-rrd.h"
#include "../utility.h"

#define RRD_TEMPLATE_STRING "create %s -s %d -b %d " \
	"DS:temp:GAUGE:%d:-20:75 " \
	"RRA:AVERAGE:0.5:%d:%d " \
	"RRA:MIN:0.5:%d:%d " \
	"RRA:MAX:0.5:%d:%d " \
	"RRA:AVERAGE:0.5:%d:%d " \
	"RRA:MIN:0.5:%d:%d " \
	"RRA:MAX:0.5:%d:%d " \
	"RRA:AVERAGE:0.5:%d:%d " \
	"RRA:MIN:0.5:%d:%d " \
	"RRA:MAX:0.5:%d:%d "

#define THREE_PAIRS(a1, b1, a2, b2, a3, b3) \
	a1, b1, a1, b1, a1, b1, \
	a2, b2, a2, b2, a2, b2, \
	a3, b3, a3, b3, a3, b3

#define RRA_EXPAND(r) \
	THREE_PAIRS(r[0][0],r[1][0],r[0][1],r[1][1],r[0][2],r[1][2])

#ifdef HAVE_RRD_H
static void
rrdErrorPrint(char *buf, char **args)
{
	int i=0;

	fprintf(stderr, "%s\n", buf);
	fprintf(stderr, "ERROR:  %s\n", rrd_get_error());
	for(i=0; i<listLength(args); i++) {
		fprintf(stderr, "\targs[%d]=``%s''\n", i, args[i]);
	}
	rrd_clear_error();
}

static int
rrdNewFile(struct log_datum *p)
{
	char buf[8192];
	char file[1024];
	char **args=NULL;
	int rv=0;
	int sample_rate=1;
	int heartbeat=0;
	int rra[2][3];
	time_t start_time=0;
	extern int optind;

	snprintf(file, sizeof(file), "%s.rrd", p->serial);

	/* Figure out if the file exists */
	if(access(file, F_OK)==0) {
		return(NO);
	}

	if(p->type==DEVICE_1921) {
		sample_rate=p->dev.dev_1921.sample_rate;
		start_time=p->dev.dev_1921.mission_ts;
	} else {
		/* Set the start time to now */
		start_time=time(NULL);
	}
	/* Roll the time back an hour, just in case */
	start_time-=3600;

	/* I don't like this, but I'm going with it because I'm tired of
	 * looking at this line of code. */
	heartbeat=sample_rate*60*10;

	rra[0][0]=1;
	if(sample_rate<5) {
		rra[0][0]=5;
	}
	rra[1][0]=(1440*(365/2))/sample_rate;
	rra[0][1]=120/sample_rate;
	if(rra[0][1]<2) {
		rra[0][1]=sample_rate*10;
	}
	rra[1][1]=8760/sample_rate;

	rra[0][2]=1440/sample_rate;
	if(rra[0][2]<2) {
		rra[0][2]=sample_rate*100;
	}
	rra[1][2]=(2*3650)/sample_rate;

	/* If not, create it  */
	snprintf(buf, sizeof(buf), RRD_TEMPLATE_STRING,
		file, (60*sample_rate), (int)start_time, heartbeat,
		RRA_EXPAND(rra));

	verboseprint(2, ("RRD create query:  %s\n", buf));

	args=split(buf, " ");
	optind=0;
	rv=rrd_create(listLength(args), args);
	if(rv<0 || rrd_test_error()) {
		rrdErrorPrint(buf, args);
		rv=NO;
	} else {
		rv=YES;
	}
	freeList(args);
	return(rv);
}

void saveDataRRD(struct log_datum *p)
{
	char buf[8192];
	char **args=NULL;
	int rv=0;
	extern int optind;

	snprintf(buf, sizeof(buf), "update %s.rrd %lu:%f", p->serial,
		(unsigned long)p->tv.tv_sec, p->reading);

	verboseprint(2, ("RRD update query:  %s\n", buf));

	/* Do something with it */
	args=split(buf, " ");
	optind=0;
	rv=rrd_update(listLength(args), args);
	if(rv<0 || rrd_test_error()) {
		rrdErrorPrint(buf, args);
		/* Figure out if I need to create a new file, and if so, do it */
		if(rrdNewFile(p)==YES) {
			fprintf(stderr, "NEW FILE!\n");
			optind=0;
			rv=rrd_update(listLength(args), args);
			if(rv<0 || rrd_test_error()) {
				rrdErrorPrint(buf, args);
			}
		}
	}

	/* Clean up */
	freeList(args);
}
#endif /* HAVE_RRD_H */

/*
 * arch-tag: BBEA7658-13E5-11D8-900C-000393CFE6B8
 */
