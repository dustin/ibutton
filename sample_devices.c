/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 *
 * $Id: sample_devices.c,v 1.22 2002/01/29 08:40:41 dustin Exp $
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

/* Prototypes */
static void setsignals();
static void log_error(char *str);

/* This is the list of things being watched */
struct watch_list {
	uchar serial[MLAN_SERIAL_SIZE];
	time_t last_read;
	struct watch_list *next;
};

static struct watch_list *watching=NULL;

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

static int
secondsSinceLastUpdate(uchar *serial)
{
	int rv=86400;
	struct watch_list *p=NULL;
	time_t now=0;

	assert(serial);

	/* Search for a match */
	for(p=watching; p!=NULL
		&& (memcmp(serial, p->serial, MLAN_SERIAL_SIZE)!=0); p=p->next);

	assert(p==NULL || memcmp(serial, p->serial, MLAN_SERIAL_SIZE)==0);

	now=time(NULL);
	if(p==NULL) {
		p=calloc(sizeof(struct watch_list), 1);
		assert(p);
		memcpy(p->serial, serial, MLAN_SERIAL_SIZE);
		p->next=watching;
		watching=p;
	} else {
		rv=now-p->last_read;
	}

	/* Timestamp it */
	assert(p);
	p->last_read=now;

	/*
	printf("secondsSinceLastUpdate(%s) => %d\n", get_serial(serial), rv);
	*/

	return(rv);
}

/* NOTE:  The initialization may be called more than once */
static MLan *
init(char *port)
{
	MLan *m=NULL;

	m=mlan_init(port, PARMSET_9600);

	if(m!=NULL) {
		if(m->ds2480detect(m)!=TRUE) {
			log_error("Did not detect DS2480.\n");
			m->destroy(m);
			m=NULL;
		}
	} else {
		log_error("Failed to open device.\n");
	}

	return(m);
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

	tp = localtime((time_t *)&tv.tv_sec);
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

static int
dealWith(MLan *mlan, uchar *serial)
{
	int age=0;
	int rv=0;

	assert(mlan);
	assert(serial);

	age=secondsSinceLastUpdate(serial);

	/* Short-circuit if the age is less than sixty seconds */
	if(age<60) {
		return(0);
	}

	switch(serial[0]) {
		case 0x10: {
			struct ds1920_data data;
			alarm(5);
			data=getDS1920Data(mlan, serial);
			if(data.valid!=TRUE) {
				rv=-1;
				/* Log the failure */
				fprintf(logfile, "%s\t%s\t%s\n",
					get_time_str(), get_serial(serial),
						"Error getting sample");
			} else {
				char data_str[8192];
				snprintf(data_str, sizeof(data_str),
					"%s\t%s\t%.2f\tl=%.2f,h=%.2f",
					get_time_str(), get_serial(serial),
					data.temp, data.temp_low,
					data.temp_hi);
				/* Log it */
				fprintf(logfile, "%s\n", data_str);
				/* Multicast it */
				msend(data_str);
				/* Now record the current */
				snprintf(data_str, sizeof(data_str),
					"%.2f", data.temp);
				record_cur(get_serial(serial),data_str);
			}
		} break;
		default:
			/* Nothing */
			break;
	}

	return(rv);
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

#ifdef MYMALLOC
			_mdebug_dump();
#endif /* MYMALLOC */

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
		/* Try three times to get a list of devices.  I don't know why this
		 * doesn't work the first time, but whatever.  */
		list_count=0;
		alarm(5);
		rslt=mlan->first(mlan, TRUE, FALSE);
		while(rslt) {
			/* Copy the serial number into our list */
			mlan->copySerial(mlan, list[list_count++]);
			/* Don't go too far */
			assert(list_count<sizeof(list)-1);
			/* Grab the next device */
			alarm(5);
			rslt = mlan->next(mlan, TRUE, FALSE);
		}
		failures=0;
		/* Loop through the list and gather samples */
		for(i=0; i<list_count; i++) {
			if(dealWith(mlan, list[i])<0) {
				failures++;
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
		sleep(1);
	}
	/* NOT REACHED */
}
