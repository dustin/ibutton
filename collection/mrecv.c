/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: mrecv.c,v 1.4 2002/01/24 10:33:26 dustin Exp $
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <rrd.h>

#include "data.h"

#define HELLO_PORT 6789
#define HELLO_GROUP "225.0.0.37"
#define MSGBUFSIZE 256

static struct rrd_queue *queue=NULL;

#define TIMETRUNC(t) (((int)t/60)*60)

#ifndef RRDFILE
# define RRDFILE "file.rrd"
#endif /* RRDFILE */

static void saveDataRRD()
{
	char **keys=NULL;
	char **vals=NULL;
	char **args=NULL;
	int i=0, rv=0;
	char buf[8192];
	char timebuf[32];
	extern int optind;

	assert(queue);

	memset(&buf, 0x00, sizeof(buf));
	keys=getRRDQueueKeys(queue);
	assert(keys);
	vals=getRRDQueueValues(queue);
	assert(vals);

	snprintf(timebuf, sizeof(timebuf), "%d", queue->timestamp);

	strcpy(buf, "update " RRDFILE " -t ");
	for(i=0; keys[i]; i++) {
		strcat(buf, keys[i]);
		if(keys[i+1]!=NULL) {
			strcat(buf, ":");
		}
		assert(strlen(buf)<sizeof(buf));
	}
	strcat(buf, " ");
	strcat(buf, timebuf);

	for(i=0; vals[i]; i++) {
		strcat(buf, ":");
		strcat(buf, vals[i]);
		assert(strlen(buf)<sizeof(buf));
	}

	/* Do something with it */
	fflush(stdout);
	args=split(buf, " ");
	optind=0;
	rv=rrd_update(listLength(args), args);
	if(rv<0 || rrd_test_error()) {
		int i=0;
		puts(buf);
		printf("ERROR:  %s\n", rrd_get_error());
		for(i=0; i<listLength(args); i++) {
			printf("\targs[%d]=``%s''\n", i, args[i]);
		}
		rrd_clear_error();
	}

	/* Clean up */
	freeList(keys);
	freeList(vals);
	freeList(args);
}

static void saveData() {
	/* Do the RRD save */
	saveDataRRD();
}

static void process(const char *msg)
{
	struct log_datum *d=NULL;
	time_t t=0;

	assert(msg);

	d=parseLogEntry(msg);
	assert(d);

	t=TIMETRUNC(d->tv.tv_sec);

	if(queue==NULL) {
		queue=newRRDQueue();
		queue->timestamp=t;
		appendToRRDQueue(queue, d);
	} else {
		/* Figure out if the timestamp changed */
		if(queue->timestamp == t) {
			/* Timestamp's the same, just append the new entry */
			appendToRRDQueue(queue, d);
		} else {
			/* Process the old */
			saveData();

			/* Get rid of the old and add the new */
			disposeOfRRDQueue(queue);
#ifdef MYMALLOC
			_mdebug_dump();
#endif /* MYMALLOC */
			queue=newRRDQueue();
			queue->timestamp=t;
			appendToRRDQueue(queue, d);
		}
	}

	/* Clean up */
	disposeOfLogEntry(d);
}

int main(int argc, char *argv[])
{
	struct sockaddr_in addr;
	int             fd=0, nbytes=0, addrlen=0, reuse=1;
	struct ip_mreq  mreq;
	char            msgbuf[MSGBUFSIZE];

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
	addr.sin_port = htons(HELLO_PORT);

	if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(int))<0){
		perror("setsockopt");
	}

	/* bind to receive address */
	if (bind(fd, (struct sockaddr *) & addr, sizeof(addr)) < 0) {
		perror("bind");
		exit(1);
	}
	/* use setsockopt() to request that the kernel join a multicast group */
	mreq.imr_multiaddr.s_addr = inet_addr(HELLO_GROUP);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	if(setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0){
		perror("setsockopt");
		exit(1);
	}
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
