/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: mrecv.c,v 1.9 2002/01/25 23:16:18 dustin Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#ifdef WORKING_PTHREAD
#include <pthread.h>
#endif /* WORKING_PTHREAD */

#ifdef HAVE_RRD_H
/* Workaround for a namespace collision */
#define time_value rrd_time_value
#include <rrd.h>
#undef time_value
#endif /* HAVE_RRD_H */

#include "data.h"

/* This is how we listen */
#define MLAN_PORT 6789
#define MLAN_GROUP "225.0.0.37"
#define MSGBUFSIZE 256

/* Necessary booleans */
#define YES 1
#define NO 0

/* When not using threads, flush after every CALLS_PER_FLUSH calls. */
#define CALLS_PER_FLUSH 10

static struct rrd_queue *queue=NULL;

#ifdef WORKING_PTHREAD
static pthread_mutex_t queue_mutex;
#endif /* WORKING_PTHREAD */

/* Portable locking */
#ifdef WORKING_PTHREAD
# define LOCK_QUEUE pthread_mutex_lock(&queue_mutex);
# define UNLOCK_QUEUE pthread_mutex_unlock(&queue_mutex);
#else
# define LOCK_QUEUE
# define UNLOCK_QUEUE
#endif

#define RRD_CREATE_STRING "create %s -s 60 -b now-5minutes " \
	"DS:temp:GAUGE:900:-10:50 " \
	"RRA:AVERAGE:0.5:5:210240 " \
	"RRA:MIN:0.5:5:210240 " \
	"RRA:MAX:0.5:5:210240 " \
	"RRA:AVERAGE:0.5:120:8760 " \
	"RRA:MIN:0.5:120:8760 " \
	"RRA:MAX:0.5:120:8760 " \
	"RRA:AVERAGE:0.5:1440:3650 " \
	"RRA:MIN:0.5:1440:3650 " \
	"RRA:MAX:0.5:1440:3650 "

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
newFile(struct data_list *p)
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

static void saveDataRRD(struct data_list *p)
{
#ifdef HAVE_RRD_H
	char buf[8192];
	char **args=NULL;
	int rv=0;
	extern int optind;

	snprintf(buf, sizeof(buf), "update %s.rrd %d:%f", p->serial,
		p->timestamp, p->reading);

	/* Do something with it */
	args=split(buf, " ");
	optind=0;
	rv=rrd_update(listLength(args), args);
	if(rv<0 || rrd_test_error()) {
		rrdErrorPrint(buf, args);
		/* Figure out if I need to create a new file, and if so, do it */
		if(newFile(p)==YES) {
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
#else /* HAVE_RRD_H */
	printf("ERROR:  RRD not found at build time.\n");
#endif /* HAVE_RRD_H */
}

static void saveData(struct data_list *p) {
	/* Do the RRD save */
	assert(p!=NULL);
	saveDataRRD(p);
	/*
	printf("FLUSH:  %s was %.2f at %d\n", p->serial, p->reading,
		(int)p->timestamp);
	*/
}

static void
doFlush()
{
	struct data_list *p=NULL;
	struct rrd_queue *tmpqueue=NULL;
	int i=0;

	/* Only do this if there's anything in the queue */
	if(queue!=NULL) {
		/* First, lock and take the current queue away. */
		LOCK_QUEUE
		tmpqueue=queue;
		queue=NULL;
		UNLOCK_QUEUE

		/* Now, work on it */
		for(i=0, p=tmpqueue->list; p!=NULL; i++, p=p->next) {
			saveData(p);
		}

		disposeOfRRDQueue(tmpqueue);
	}
}

#ifdef WORKING_PTHREAD
static void
*flusher(void *nothing)
{   
	int rv=0;
	/* Daemonize */
	rv=pthread_detach(pthread_self());
	assert(rv>=0);
	for(;;) {
		sleep(60);
		doFlush();
	}
}
#endif /* WORKING_PTHREAD */

static void process(const char *msg)
{
	struct log_datum *d=NULL;
	static int calls=0;

	assert(msg);
	calls++;

	d=parseLogEntry(msg);
	assert(d);

	LOCK_QUEUE
	if(queue==NULL) {
		queue=newRRDQueue();
	}
	appendToRRDQueue(queue, d);
	UNLOCK_QUEUE

	/* If there's not working pthreads, manually flush every once in a while */
#ifndef WORKING_PTHREAD
	if(calls > 0 && calls%CALLS_PER_FLUSH) {
		doFlush();
	}
#endif /* WORKING_PTHREAD */

	/* Clean up */
	disposeOfLogEntry(d);
}

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	int             fd=0, nbytes=0, addrlen=0, reuse=1;
	struct ip_mreq  mreq;
	char            msgbuf[MSGBUFSIZE];

#ifdef WORKING_PTHREAD
	pthread_t		flusher_thread;
#endif /* WORKING_PTHREAD */

	fprintf(stderr, "Initializing multicast receiver with"
#ifndef WORKING_PTHREAD
		"out"
#endif /* WORKING_PTHREAD */
		" thread support at %s:%d.\n", MLAN_GROUP, MLAN_PORT);

	/* create what looks like an ordinary UDP socket */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	/* set up destination address */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	/* N.B.: differs from * sender */
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(MLAN_PORT);

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(int))<0){
		perror("setsockopt");
	}

	/* bind to receive address */
	if (bind(fd, (struct sockaddr *) & addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(1);
	}
	/* use setsockopt() to request that the kernel join a multicast group */
	mreq.imr_multiaddr.s_addr = inet_addr(MLAN_GROUP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if(setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0){
		perror("setsockopt");
		exit(1);
	}

#ifdef WORKING_PTHREAD
	pthread_mutex_init(&queue_mutex, NULL);

	pthread_create(&flusher_thread, NULL, flusher, NULL);
#endif /* WORKING_PTHREAD */

	/* now just enter a read-print loop */
	addrlen = sizeof(addr);
	for(;;) {
		if ((nbytes = recvfrom(fd, msgbuf, MSGBUFSIZE, 0,
			       (struct sockaddr *) & addr, &addrlen)) < 0) {
			perror("recvfrom");
			exit(1);
		}
		process(msgbuf);
	}
}
