/*
 * Copyright (c) 2002  Dustin Sallings
 *
 * $Id: data.h,v 1.1 2002/01/23 22:36:14 dustin Exp $
 */

#ifndef DATA_H
#define DATA_H 1

#include <sys/time.h>

/* Represents a log entry */
struct log_datum {
	struct timeval tv;
	char *serial;
	float reading;
	float low;
	float high;

	int isValid:2;
};

struct log_datum *parseLogEntry(const char *);
void disposeOfLogEntry(struct log_datum *);

char **split(const char *input, const char *delim);
void freeList(char **list);
int listLength(const char **list);

#endif
