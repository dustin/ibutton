/*
 * Copyright (c) 2002  Dustin Sallings
 *
 * $Id: data.h,v 1.2 2002/01/24 10:10:25 dustin Exp $
 */

#ifndef DATA_H
#define DATA_H 1

#include <sys/time.h>

#ifdef MYMALLOC
#include "../mymalloc.h"
#endif /* MYMALLOC */

/* Represents a log entry */
struct log_datum {
	struct timeval tv;
	char *serial;
	float reading;
	float low;
	float high;

	int isValid:2;
};

struct data_list {
	char *serial;
	float reading;
	struct data_list *next;
};

struct rrd_queue {
	time_t timestamp;
	struct data_list *list;
};

/* log parsing and disposal */
struct log_datum *parseLogEntry(const char *);
void disposeOfLogEntry(struct log_datum *);

/* Array stuff */
char **split(const char *input, const char *delim);
void freeList(char **list);
int listLength(const char **list);

/* Linked list stuff */
void appendToRRDQueue(struct rrd_queue *dl, struct log_datum *datum);
void disposeOfRRDQueue(struct rrd_queue *dl);
struct rrd_queue *newRRDQueue();

/* Get the keys and values as Strings */
char **getRRDQueueKeys(struct rrd_queue *dl);
char **getRRDQueueValues(struct rrd_queue *dl);

#endif
