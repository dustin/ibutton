/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: storage-rrd.c,v 1.3 2002/01/26 10:56:31 dustin Exp $
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

#define RRD_CREATE_STRING "create %s -s 60 -b now-5minutes " \
	"DS:temp:GAUGE:900:-20:75 " \
	"RRA:AVERAGE:0.5:5:210240 " \
	"RRA:MIN:0.5:5:210240 " \
	"RRA:MAX:0.5:5:210240 " \
	"RRA:AVERAGE:0.5:120:8760 " \
	"RRA:MIN:0.5:120:8760 " \
	"RRA:MAX:0.5:120:8760 " \
	"RRA:AVERAGE:0.5:1440:3650 " \
	"RRA:MIN:0.5:1440:3650 " \
	"RRA:MAX:0.5:1440:3650 "

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
rrdNewFile(struct data_list *p)
{
	char buf[8192];
	char file[1024];
	char **args=NULL;
	int rv=0;
	extern int optind;

	snprintf(file, sizeof(file), "%s.rrd", p->serial);

	/* Figure out if the file exists */
	if(access(file, F_OK)==0) {
		return(NO);
	}

	/* If not, create it  */
	snprintf(buf, sizeof(buf), RRD_CREATE_STRING, file);
	args=split(buf, " ");
	optind=0;
	rv=rrd_create(listLength(args), args);
	if(rv<0 || rrd_test_error()) {
		rrdErrorPrint(buf, args);
		rv=NO;
	} else {
		rv=YES;
	}
	return(rv);
}

void saveDataRRD(struct data_list *p)
{
	char buf[8192];
	char **args=NULL;
	int rv=0;
	extern int optind;

	snprintf(buf, sizeof(buf), "update %s.rrd %lu:%f", p->serial,
		(unsigned long)p->timestamp, p->reading);

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
