/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: sample_devices.c,v 1.9 2000/11/08 05:01:33 dustin Exp $
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#define _BSD_SIGNALS
#include <signal.h>
#include <mlan.h>
#include <ds1920.h>

/* Globals because I have to move it out of the way in a signal handler */
FILE		*logfile=NULL;
char		*logfilename=NULL;
int			need_to_reinit=1;

/* Prototype */
static void setsignals();

/* NOTE:  The initialization may be called more than once */
static MLan *
init(char *port)
{
	return(mlan_init(port, PARMSET_9600));
}

/* Record the current sample into a file unique to the serial number */
static void
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

/* Get the time for the record. */
static char*
get_time_str()
{
	struct timeval tv;
	struct tm *tp;
	struct timezone tz;
	static char str[50];
	double seconds;

	/* Get some better precisioned time */
	gettimeofday(&tv, &tz);

	tp = localtime(&tv.tv_sec);
	seconds = tp->tm_sec+(double)tv.tv_usec/1000000;
	sprintf(str, "%.4d/%.2d/%.2d %.2d:%.2d:%s%.6f",
		tp->tm_year + 1900,
		tp->tm_mon + 1,
		tp->tm_mday,
		tp->tm_hour,
		tp->tm_min,
		(seconds <10 ? "0" : ""),
		seconds);
	return(str);
}

static void
log_error(char *str)
{
	fprintf(stderr, "%s:  ERROR:  %s", get_time_str(), str);
	fflush(stderr);
}

/* Get the serial number as a string */
static char*
get_serial(uchar *serial)
{
	static char r[(MLAN_SERIAL_SIZE * 2) + 1];
	char *map="0123456789ABCDEF";
	int i=0, j=0;
	assert(serial);
	for(i=0; i<MLAN_SERIAL_SIZE; i++) {
		r[j++]=map[((serial[i] & 0xf0) >> 4)];
		r[j++]=map[(serial[i] & 0x0f)];
	}
	r[j]=0x00;
	return(r);
}

void
_sighup(int sig)
{
	char logfiletmp[1024];
	log_error("Got SIGHUP\n");
	if(logfile != NULL) {
		fclose(logfile);
	}
	need_to_reinit=1;
	assert(strlen(logfilename) < 1000);
	sprintf(logfiletmp, "%s.old", logfilename);
	rename(logfilename, logfiletmp);
	logfile=fopen(logfilename, "w");
	assert(logfile);
	setsignals();
}

void
_sigalarm(int sig)
{
	log_error("Got SIGALRM\n");
	need_to_reinit=1;
	setsignals();
}

static void
setsignals()
{
	signal(SIGHUP, _sighup);
	signal(SIGALRM, _sigalarm);
}

/* Main */
int
main(int argc, char **argv)
{
	uchar		list[MAX_SERIAL_NUMS][MLAN_SERIAL_SIZE];
	int			list_count=0, i=0, failures=0;
	int			rslt=0;
	char		*busdev=NULL, *curdir=NULL;
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
	logfilename = argv[2];
	logfile = fopen(logfilename, "a");
	assert(logfile);
	curdir = argv[3];
	/* Set signal handlers */
	setsignals();

	/* Loop forever */
	for(;;) {
		/* If we need to reinitialize the bus, do so */
		if(need_to_reinit) {
			if(mlan) {
				mlan->destroy(mlan);
			}
			mlan=NULL;

			while(mlan==NULL) {
				log_error("(re)initializing the 1wire bus.\n");
				mlan=init(busdev);
				if(mlan==NULL) {
					log_error("Init failed, sleeping...\n");
					sleep(15);
				} else {
					log_error("Initialization successful.\n");
					need_to_reinit=0;
				}
			}
			/* Wait a second */
			sleep(1);
		}
		alarm(5);
		rslt=mlan->first(mlan, TRUE, FALSE);
		list_count=0;
		while(rslt) {
			/* Copy the serial number into our list */
			mlan->copySerial(mlan, list[list_count++]);
			/* Grab the next device */
			alarm(5);
			rslt = mlan->next(mlan, TRUE, FALSE);
		}
		failures=0;
		/* Loop through the list and gather samples */
		for(i=0; i<list_count; i++) {
			switch(list[i][0]) {
				case 0x10: {
					struct ds1920_data data;
					alarm(5);
					data=getDS1920Data(mlan, list[i]);
					if(data.valid!=TRUE) {
						failures++;
						/* Log the failure */
						fprintf(logfile, "%s\t%s\t%s\n",
							get_time_str(), get_serial(list[i]),
								"Error getting sample");
					} else {
						char current_sample[80];
						fprintf(logfile, "%s\t%s\t%.2f\tl=%.2f,h=%.2f\n",
							get_time_str(), get_serial(list[i]),
							ctof(data.temp),
							ctof(data.temp_low), ctof(data.temp_hi));
						snprintf(current_sample, sizeof(current_sample),
							"%.2f", ctof(data.temp));
						record_cur(curdir, get_serial(list[i]),current_sample);
					}
				} break;
				default:
					/* Nothing */
					break;
			}
		}
		/* Write the log */
		fflush(logfile);

		alarm(0);

		if(failures>0) {
			/* Wait five seconds before reopening the device. */
			log_error("There were failures, requesting reinitialization.\n");
			need_to_reinit=1;
		}

		if(list_count==0) {
			log_error("Didn't find any devices, reinitializing.\n");
			need_to_reinit=1;
		}

		/* Wait a while before the next sample */
		sleep(59);
	}
	/* NOT REACHED */
}
