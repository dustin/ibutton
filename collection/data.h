/*
 * Copyright (c) 2002  Dustin Sallings
 *
 * $Id: data.h,v 1.11 2002/01/30 21:09:17 dustin Exp $
 */

#ifndef DATA_H
#define DATA_H 1

#include <sys/time.h>

#include <mlan.h>

#ifdef MYMALLOC
#include <mymalloc.h>
#endif /* MYMALLOC */

/* 1920 specific attributes */
struct dev_1920_specific {
	float low;
	float high;
};

/* 1921 specific attributes */
struct dev_1921_specific {
	time_t mission_ts;
	int sample_rate;
};

/* A union for recording device specific attributes in the log */
union dev_specific {
	struct dev_1920_specific dev_1920;
	struct dev_1921_specific dev_1921;
};

/* Represents a log entry */
struct log_datum {
	struct timeval tv;
	char *serial;
	float reading;
	short type;
	union dev_specific dev;

	int isValid:2;
	/* If isValid is false, there'll be an error here describing why. */
	char *errorMsg;
};

struct data_queue {
	char *line;
	struct data_queue *next;
};

/* log parsing and disposal */
struct log_datum *parseLogEntry(const char *);
void disposeOfLogEntry(struct log_datum *);
/* debug */
void logDatumPrint(struct log_datum *);

/* Array stuff */
char **split(const char *input, const char *delim);
void freeList(char **list);
int listLength(char **list);

/* Linked list stuff */
struct data_queue *appendToRRDQueue(struct data_queue *dl, const char *datum);
void disposeOfRRDQueue(struct data_queue *dl);

#endif
