/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: mrecv.c,v 1.11 2002/01/26 07:40:02 dustin Exp $
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

#ifdef HAVE_RRD_H
/* Workaround for a namespace collision */
#define time_value rrd_time_value
#include <rrd.h>
#undef time_value
#endif /* HAVE_RRD_H */

#ifdef HAVE_LIBPQ_FE_H
#include <libpq-fe.h>
#endif /* HAVE_LIBPQ_FE_H */

/* Local includes */
#include "data.h"
#include "../utility.h"

/* Defaults for listening */
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

#ifdef HAVE_LIBPQ_FE_H

/* Definition of how to connect to the database. */
static struct {
	char *dbhost;
	char *dbuser;
	char *dbpass;
	char *dboptions;
	char *dbname;
	char *dbport;
} pg_config;

/* The actual database connection */
static PGconn *dbConn=NULL;

#endif /* HAVE_LIBPQ_FE_H */

/* This holds the multicast socket */
static int msocket=0;

static int verbose=0;

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

#ifdef HAVE_RRD_H
static void saveDataRRD(struct data_list *p)
{
	char buf[8192];
	char **args=NULL;
	int rv=0;
	extern int optind;

	snprintf(buf, sizeof(buf), "update %s.rrd %d:%f", p->serial,
		p->timestamp, p->reading);

	if(verbose>1) {
		printf("RRD update query:  %s\n", buf);
	}

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
}
#endif /* HAVE_RRD_H */

#ifdef HAVE_LIBPQ_FE_H

static void
abandonPostgresConnection()
{
	if(verbose>0) {
		printf("Abandoning postgres connection (if there is one).\n");
	}
	if(dbConn!=NULL) {
		PQfinish(dbConn);
		dbConn=NULL;
	}
}

static int
getPostgresConnection()
{
	int proceed=NO;

	/* Check to see if we have a valid connection */
	if(dbConn==NULL || (PQstatus(dbConn) == CONNECTION_BAD)) {
		/* If there's an invalid connection, close it */
		abandonPostgresConnection();

		if(verbose>0) {
			printf("Connecting to postgres.\n");
		}

		dbConn=PQsetdbLogin(pg_config.dbhost, pg_config.dbport,
			pg_config.dboptions, NULL, pg_config.dbname, pg_config.dbuser,
			pg_config.dbpass);
		if (PQstatus(dbConn) == CONNECTION_BAD) {
			fprintf(stderr, "Error connecting to postgres:  %s\n",
				PQerrorMessage(dbConn));
		} else {
			proceed=YES;
		}
	} else {
		proceed=YES;
	}
	return(proceed);
}

static void saveDataPostgres(struct data_list *p)
{
	static int queries=0;

	if(getPostgresConnection()==YES) {
		char query[8192];
		struct tm *t;
		time_t tt=0;
		PGresult *res=NULL;
		char *tuples=NULL;
		int ntuples=0;

		tt=p->timestamp;
		t=localtime(&tt);
		assert(t);
		snprintf(query, sizeof(query),
			"insert into samples(ts, sensor_id, sample)\n"
			"\tselect '%d/%d/%d %d:%d:%d', sensor_id, %f from sensors\n"
			"\twhere serial='%s'",
			(1900+t->tm_year), (1+t->tm_mon), t->tm_mday,
			t->tm_hour, t->tm_min, t->tm_sec, p->reading,
			p->serial);
		if(verbose>1) {
			printf("Query:  %s\n", query);
		}

		/* Issue the query */
		res=PQexec(dbConn, query);
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			fprintf(stderr, "Postgres error:  %s\n\tDuring query:  %s\n",
				PQerrorMessage(dbConn), query);
		}
		/* Figure out how many tuples we updated */
		tuples=PQcmdTuples(res);
		assert(tuples);
		ntuples=atoi(tuples);
		assert(ntuples < 2);
		if(ntuples==0) {
			fprintf(stderr, "Postgres error:  Insert failed.  Query: %s\n",
				query);
		}
		/* Get rid of the result */
		PQclear(res);

		/* Reconnect after every 100 connections, just because */
		if(queries>0 && (queries%100==0)) {
			abandonPostgresConnection();
		}
		queries++;
	}
}
#endif /* HAVE_LIBPQ_FE_H */

static void saveData(struct data_list *p)
{
	/* Do the RRD save */
	assert(p!=NULL);

	if(verbose>0) {
		printf("FLUSH:  %s was %.2f at %d\n", p->serial, p->reading,
			(int)p->timestamp);
	}

#ifdef HAVE_RRD_H
	saveDataRRD(p);
#endif /* HAVE_RRD_H */

#ifdef HAVE_LIBPQ_FE_H
	saveDataPostgres(p);
#endif /* HAVE_LIBPQ_FE_H */

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

	if(verbose>1) {
		printf("MSG:  %s\n", msg);
	}

	d=parseLogEntry(msg);
	assert(d);

	if(verbose>0) {
		printf("RECV:  %f from %s at %lu\n", d->reading, d->serial,
			(unsigned long)d->tv.tv_sec);
	}

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
	char   msgbuf[MSGBUFSIZE];
	struct sockaddr_in addr;
	int    nbytes=0, addrlen=0;

	/* now just enter a read-print loop */
	addrlen = sizeof(addr);
	for(;;) {
		if ((nbytes = recvfrom(msocket, msgbuf, MSGBUFSIZE, 0,
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
	memset(&pg_config, 0x00, sizeof(pg_config));

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
				verbose++;
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
