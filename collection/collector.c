/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: collector.c,v 1.2 2002/01/26 08:28:37 dustin Exp $
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

#ifdef GETOPT_H
#include <getopt.h>
#endif /* GETOPT_H */
#ifdef GETOPTS_H
#include <getopts.h>
#endif /* GETOPTS_H */

#ifdef WORKING_PTHREAD
#include <pthread.h>
#endif /* WORKING_PTHREAD */

/* Local includes */
#include "../utility.h"
#include "data.h"
#include "collection.h"
#include "storage-pgsql.h"
#include "storage-rrd.h"

static struct data_queue *queue=NULL;

#ifdef WORKING_PTHREAD
static pthread_mutex_t queue_mutex;
#endif /* WORKING_PTHREAD */

/* This holds the multicast socket */
static int msocket=0;

int col_verbose=0;

/* Portable locking */
#ifdef WORKING_PTHREAD
# define LOCK_QUEUE pthread_mutex_lock(&queue_mutex);
# define UNLOCK_QUEUE pthread_mutex_unlock(&queue_mutex);
#else
# define LOCK_QUEUE
# define UNLOCK_QUEUE
#endif

static void saveData(struct data_list *p)
{
	/* Do the RRD save */
	assert(p!=NULL);

	verboseprint(1, ("FLUSH:  %s was %.2f at %d\n", p->serial, p->reading,
			(int)p->timestamp));

#ifdef HAVE_RRD_H
	verboseprint(2, ("Saving to RRD.\n"));
	saveDataRRD(p);
#endif /* HAVE_RRD_H */

#ifdef HAVE_LIBPQ_FE_H
	if((pg_config.dbhost!=NULL) || (pg_config.dbname!=NULL)) {
		verboseprint(2, ("Saving to postgres.\n"));
		saveDataPostgres(p);
	} else {
		verboseprint(2, ("Not saving to postgres, not configured.\n"));
	}
#endif /* HAVE_LIBPQ_FE_H */

}

static void
doFlush()
{
	struct data_list *p=NULL;
	struct data_queue *tmpqueue=NULL;
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

	verboseprint(2, ("MSG:  %s\n", msg));

	d=parseLogEntry(msg);
	assert(d);

	verboseprint(1, ("RECV:  %f from %s at %lu\n", d->reading, d->serial,
			(unsigned long)d->tv.tv_sec));

	LOCK_QUEUE
	if(queue==NULL) {
		queue=newRRDQueue();
	}
	appendToRRDQueue(queue, d);
	UNLOCK_QUEUE

	/* If there's not working pthreads, manually flush every once in a while */
#ifndef WORKING_PTHREAD
	if(calls > 0 && (calls%CALLS_PER_FLUSH == 0) ) {
		doFlush();
	}
#endif /* WORKING_PTHREAD */

	/* Clean up */
	disposeOfLogEntry(d);
}

static void
init(const char *multigroup, int multiport)
{
	int    reuse=1;
	struct sockaddr_in addr;
	struct ip_mreq  mreq;

#ifdef WORKING_PTHREAD
	pthread_t		flusher_thread;
#endif /* WORKING_PTHREAD */

	fprintf(stderr, "Initializing multicast receiver with"
#ifndef WORKING_PTHREAD
		"out"
#endif /* WORKING_PTHREAD */
		" thread support at %s:%d.\n", multigroup, multiport);

	/* create what looks like an ordinary UDP socket */
	if ((msocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	/* set up destination address */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	/* N.B.: differs from * sender */
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(multiport);

	if(setsockopt(msocket, SOL_SOCKET, SO_REUSEADDR,
		(char*)&reuse, sizeof(int))<0){
		perror("setsockopt");
	}

	/* bind to receive address */
	if (bind(msocket, (struct sockaddr *) & addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(1);
	}
	/* use setsockopt() to request that the kernel join a multicast group */
	mreq.imr_multiaddr.s_addr = inet_addr(multigroup);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if(setsockopt(msocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
		sizeof(mreq)) < 0) {

		perror("setsockopt");
		exit(1);
	}

#ifdef WORKING_PTHREAD
	pthread_mutex_init(&queue_mutex, NULL);

	pthread_create(&flusher_thread, NULL, flusher, NULL);
#endif /* WORKING_PTHREAD */

}

static void
mainloop()
{
	char   msgbuf[256];
	struct sockaddr_in addr;
	int    nbytes=0, addrlen=0;

	/* now just enter a read-print loop */
	addrlen = sizeof(addr);
	for(;;) {
		if ((nbytes = recvfrom(msocket, msgbuf, sizeof(msgbuf), 0,
			       (struct sockaddr *) & addr, &addrlen)) < 0) {
			perror("recvfrom");
			exit(1);
		}
		process(msgbuf);
	}
}

static void
usage(char *prog)
{
	fprintf(stderr, "Usage:  %s [-m multigroup] [-p multiport] [-v] ", prog);
#ifdef HAVE_LIBPQ_FE_H
	fprintf(stderr, "[-H db_host] [-P db_port]\n\t[-D db_name] [-U db_user] "
					"[-A db_pass] [-O db_options]\n");
#endif /* HAVE_LIBPQ_FE_H */

	exit(1);
}

/* This defines the optargs dynamically */
#define BASEOPTIONS "m:p:v"
#ifdef HAVE_LIBPQ_FE_H
# define PGOPTIONS "H:P:D:U:A:O:"
#else
# define PGOPTIONS ""
#endif /* HAVE_LIBPQ_FE_H */

#define OPTIONS BASEOPTIONS PGOPTIONS

int main(int argc, char *argv[])
{
	char *multigroup=MLAN_GROUP;
	int multiport=MLAN_PORT;
	int c=0;
	extern char *optarg;

	/* Clear out the config */
#ifdef HAVE_LIBPQ_FE_H
	memset(&pg_config, 0x00, sizeof(pg_config));
#endif /* HAVE_LIBPQ_FE_H */

	/* Deal with the arguments here */
	while( (c=getopt(argc, argv, OPTIONS)) != -1) {
		switch(c) {
			case 'm':
				multigroup=optarg;
				break;
			case 'p':
				multiport=atoi(optarg);
				break;
			case 'v':
				col_verbose++;
				break;
#ifdef HAVE_LIBPQ_FE_H
			case 'H':
				pg_config.dbhost=optarg;
				break;
			case 'P':
				pg_config.dbport=optarg;
				break;
			case 'D':
				pg_config.dbname=optarg;
				break;
			case 'U':
				pg_config.dbuser=optarg;
				break;
			case 'A':
				pg_config.dbpass=optarg;
				break;
			case 'O':
				pg_config.dboptions=optarg;
				break;
#endif /* HAVE_LIBPQ_FE_H */
			case '?':
				usage(argv[0]);
				break;
		};
	}

	init(multigroup, multiport);
	mainloop();
	return(0);
}
