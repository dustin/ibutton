/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: sample_devices.c,v 1.14 2001/08/02 21:33:28 dustin Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#define _BSD_SIGNALS
#include <signal.h>

/* For multicast */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Local */
#include <mlan.h>
#include <ds1920.h>

/* Globals because I have to move it out of the way in a signal handler */
FILE		*logfile=NULL;
char		*logfilename=NULL;
int			need_to_reinit=1;
char		*busdev=NULL;
char		*curdir=NULL;

/* The multicast stuff */
int msocket=-1;
struct sockaddr_in maddr;

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
record_cur(const char *serial, const char *value)
{
	char	fn[8192];
	FILE	*f;

	strcpy(fn, curdir);
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

/* Initialize the multicast socket and addr.  */
static void
initMulti(const char *group, int port, int mttl)
{
	char ttl='\0';

	assert(mttl>=0);
	assert(mttl<256);

	msocket=socket(AF_INET, SOCK_DGRAM, 0);
	if(msocket<0) {
		perror("socket");
		exit(1);
	}

	/* If a TTL was given, use it */
	if(mttl>0) {
		ttl=(char)mttl;
		if( setsockopt(msocket, IPPROTO_IP, IP_MULTICAST_TTL,
			&ttl, sizeof(ttl) )<0) {

			perror("set multicast ttl");
			exit(1);
		}
	}

	memset(&maddr, 0, sizeof(maddr));
	maddr.sin_family = AF_INET;
	maddr.sin_addr.s_addr = inet_addr(group);
	maddr.sin_port = htons(port);
}

/* Send a multicast message */
static void
msend(const char *msg)
{
	if(msocket>=0) {
		/* length of the string +1 to send the NULL */
		if(sendto(msocket, msg, (strlen(msg)+1), 0,
			(struct sockaddr *)&maddr, sizeof(maddr)) < 0) {
			perror("sendto");
		}
	}
}

static void
usage(const char *name)
{
	fprintf(stderr, "Usage:  %s -b busdev -l logfile -c logdir "
		"[-m multigroup] [-p multiport] [-t multittl]\n", name);
	fprintf(stderr, "  busdev - serial device containing the bus to poll\n");
	fprintf(stderr, "  logfile - file to write log entries\n");
	fprintf(stderr, "  logdir - directory to write indivual snapshots\n");
	fprintf(stderr, "  multigroup - multicast group to announce readings\n");
	fprintf(stderr, "  multiport - port for sending multicast [1313]\n");
	fprintf(stderr, "  multittl - multicast TTL [0]\n");
	fprintf(stderr,
		"By default, if no multicast group is defined, there will be\n"
		"no multicast announcements.\n");
	exit(1);
}

static void
getoptions(int argc, char **argv)
{
	int c;
	extern char *optarg;
	char *multigroup=NULL;
	int multiport=1313;
	int multittl=0;

	while( (c=getopt(argc, argv, "b:l:c:m:p:t:")) != -1) {
		switch(c) {
			case 'b':
				busdev=optarg;
				break;
			case 'l':
				logfilename=optarg;
				break;
			case 'c':
				curdir=optarg;
				break;
			case 'm':
				multigroup=optarg;
				break;
			case 'p':
				multiport=atoi(optarg);
				break;
			case 't':
				multittl=atoi(optarg);
				break;
			case '?':
				usage(argv[0]);
				break;
		}
	}

	/* Make sure the required options are supplied */
	if(busdev==NULL || logfilename==NULL || curdir==NULL) {
		usage(argv[0]);
	}

	/* If we got multicast config, initialize it */
	if(multigroup!=NULL) {
		initMulti(multigroup, multiport, multittl);
	}
}

/* Main */
int
main(int argc, char **argv)
{
	uchar		list[MAX_SERIAL_NUMS][MLAN_SERIAL_SIZE];
	int			list_count=0, i=0, failures=0;
	int			rslt=0;
	MLan		*mlan=NULL;

	/* process the arguments */
	getoptions(argc, argv);

	/* Deal with the arguments. */
	logfile = fopen(logfilename, "a");
	assert(logfile);
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
						char data_str[8192];
						snprintf(data_str, sizeof(data_str),
							"%s\t%s\t%.2f\tl=%.2f,h=%.2f",
							get_time_str(), get_serial(list[i]),
							ctof(data.temp), ctof(data.temp_low),
							ctof(data.temp_hi));
						/* Log it */
						fprintf(logfile, "%s\n", data_str);
						/* Multicast it */
						msend(data_str);
						/* Now record the current */
						snprintf(data_str, sizeof(data_str),
							"%.2f", ctof(data.temp));
						record_cur(get_serial(list[i]),data_str);
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
