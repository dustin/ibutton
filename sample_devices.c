/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: sample_devices.c,v 1.1 1999/12/08 09:54:37 dustin Exp $
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <mlan.h>

/* NOTE:  The initialization may be called more than once */
MLan *
init(char *port)
{
	return(mlan_init(port, PARMSET_9600));
}

/* Uninitializer */
void
teardown(MLan *mlan)
{
	mlan->destroy(mlan);
}

/* Record the current sample into a file unique to the serial number */
void
record_cur(const char *dir, const char *serial, const char *value)
{
	char	fn[8192];
	FILE	*f;

	strcpy(fn, dir);
	strcat(fn, "/");
	strcat(fn, serial);

	f=fopen(fn, "w");
	if(f==NULL) {
		perror(fn);
		return;
	}
	fprintf(f, "%s\n", value);
	fclose(f);
}

static char*
get_time_str()
{
	/* Clock generation code goes here */
}

/* Main */
int
main(int argc, char **argv)
{
	uchar		sn[MLAN_SERIAL_SIZE];
	uchar		list[MAX_SERIAL_NUMS][MLAN_SERIAL_SIZE];
	int			list_count=0, i=0, status=0, failures=0;
	int			rslt=0;
	const char	*sample_str=NULL;
	char		*busdev=NULL, *curdir=NULL:
	FILE		*logfile=NULL;
	MLan		*mlan=NULL;

	/* Make sure we have enough arguments */
	if(argc < 4) {
		fprintf(stderr, "Too few arguments.  Usage:\n"
			"%s busdev logfile curdirectory\n"
			"\tbusdev:  Serial device containing the bus to poll\n"
			"\tlogfile: Logfile to record history\n"
			"\tcurdirectory: Directory to store current data\n",
			argv[0]);
		exit(1);
	}

	/* Deal with the arguments. */
	busdev = argv[1];
	logfile = fopen(argv[2], "a");
	assert(logfile);
	curdir = argv[3];

	if( mlan=init(busdev) == NULL ) {
		fprintf(stderr, "Unable to initialize 9097, not a good start.\n");
		exit(1);
	}

	/* Loop forever */
	for(;;) {
		rslt=mlan->first(mlan, TRUE, FALSE);
		list_count=0;
		while(rslt) {
			/* Copy the serial number into our list */
			mlan->copySerial(mlan, list[list_count++]);
			/* Grab the next device */
			rslt = mlan->next(mlan, TRUE, FALSE);
		}
		failures=0;
		/* Loop through the list and gather samples */
		for(i=0; i<list_count; i++) {
			switch(list[i][0]) {
				case 0x10:
					sample_str=get_sample(mlan, list[i]);
					if(sample_str=NULL) {
						failures++;
						sample_str="Error getting sample";
					}
					break;
				default:
					sample_str=NULL;
					break;
			}
			if(sample_str!=NULL) {
				fprintf(logfile, "%s\t%s\t%s\n", get_time_str(),
					get_serial(list[i]), sample_str);
			}
		}
	}
}
