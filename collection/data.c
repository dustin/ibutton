/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: data.c,v 1.9 2002/01/29 22:42:21 dustin Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "mlan.h"
#include "data.h"

#define SPLITSIZE 64

char **split(const char *input, const char *delim)
{
	char **rv=NULL, *p=NULL, *in=NULL;
	int pos=0;

	assert(input);
	assert(delim);

	/* Copy the string so we can be more destructive */
	in=strdup(input);
	assert(in);

	/* Get the initial size */
	rv=calloc(sizeof(char *), SPLITSIZE);
	assert(rv);

	/* setup strtok */
	p=strtok(in, delim);
	assert(p);

	/* Get the tokens */
	for(; p!=NULL; p=strtok(NULL, delim)) {
		rv[pos++]=strdup(p);
		assert(pos<(SPLITSIZE-1));
	}
	rv[pos]=NULL;

	free(in);

	return(rv);
}

void freeList(char **list)
{
	int i=0;
	if(list!=NULL) {
		for(i=0; list[i]!=NULL; i++) {
			free(list[i]);
		}
		free(list);
	}
}

int listLength(const char **list)
{
	int rv=0;
	for(rv=0; list[rv]!=NULL; rv++) {
		assert(rv<SPLITSIZE);
	}
	return(rv);
}

struct log_datum *parseLogEntry(const char *line)
{
	struct log_datum *rv=NULL;
	struct tm t;
	char **fields=NULL;
	char **ta=NULL;
	char **extra=NULL;
	int i=0;
	uchar serial[MLAN_SERIAL_SIZE];

	assert(line);

	/* place to go */
	rv=calloc(sizeof(struct log_datum), 1);
	assert(rv);

	/* Split the fields */
	fields=split(line, "\t;");
	if(listLength(fields)<3) {
		fprintf(stderr, "Not enough data fields in multicast packet:\n\t%s\n",
			line);
		goto finished;
	}
	/* Split the time */
	ta=split(fields[0], ": /.");
	if(listLength(ta)<6) {
		fprintf(stderr, "Not enough data fields in time field:  %s\n",
			fields[0]);
		goto finished;
	}

	t.tm_wday=0;
	t.tm_yday=0;
	t.tm_isdst=0;
	t.tm_year=atoi(ta[0])-1900;
	t.tm_mon=atoi(ta[1])-1;
	t.tm_mday=atoi(ta[2]);
	t.tm_hour=atoi(ta[3]);
	t.tm_min=atoi(ta[4]);
	t.tm_sec=atoi(ta[5]);

	rv->tv.tv_sec=mktime(&t);
	if(rv->tv.tv_sec<0) {
		fprintf(stderr, "Time data made no sense:  %d/%d/%d %d:%d:%d\n",
			t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		goto finished;
	}
	rv->tv.tv_usec=(float)(atoi(ta[6]))/1000000.0;

	rv->serial=strdup(fields[1]);
	assert(rv->serial);
	rv->reading=atof(fields[2]);

	parseSerial(rv->serial, serial);
	rv->type=(short)serial[0];

	/* Grab extra arguments and split them */
	if(listLength(ta)>6) {
		extra=split(ta[7], "=,");
	}

	/* Do device-specific-stuff here */

	switch(rv->type) {
		case DEVICE_1920:
			for(i=0; (extra!=NULL) && i<listLength(extra); i+=2) {
				assert(listLength(extra)>=(i+1));

				if(strcmp(extra[i], "l")==0) {
					rv->dev.dev_1920.low=atof(extra[i+1]);
				} else if(strcmp(extra[i], "h")==0) {
					rv->dev.dev_1920.high=atof(extra[i+1]);
				}

			}
			break;
		case DEVICE_1921:
			for(i=0; (extra!=NULL) && i<listLength(extra); i+=2) {
				assert(listLength(extra)>=(i+1));

				if(strcmp(extra[i], "r")==0) {
					rv->dev.dev_1921.sample_rate=atoi(extra[i+1]);
				} else if(strcmp(extra[i], "s")==0) {
					rv->dev.dev_1921.mission_ts=atoi(extra[i+1]);
				}

			}
			break;
	}

	/* If we made it this far, it's because the line was a valid datum */
	rv->isValid=1;

	/* Do final cleanup stuff before returning */
	finished:
	freeList(fields);
	freeList(ta);
	freeList(extra);
	return(rv);
}

void disposeOfLogEntry(struct log_datum *entry)
{
	assert(entry);
	assert(entry->isValid == 1);

	/* Mark it as invalid */
	entry->isValid=0;

	free(entry->serial);
	free(entry);
}

void
logDatumPrint(struct log_datum *p)
{
	assert(p);

	printf("Datum type of %s is %x.  Reading is %f\n",
		p->serial, p->type, p->reading);
	printf("Device specific:\n");
	switch(p->type) {
		case DEVICE_1920:
			printf("\tLow temperature:  %f\n\tHigh Temperature:  %f\n",
				p->dev.dev_1920.low, p->dev.dev_1920.high);
			break;
		case DEVICE_1921:
			printf("\tStart time:  %d\n\tSample rate:  %d\n",
				p->dev.dev_1921.mission_ts, p->dev.dev_1921.sample_rate);
			break;
		default:
			printf("\tUnknown device type:  %x\n", p->type);
			break;
	}
}

/*!
 * Append the given string to the end of the data queue.  Return the new
 * data queue (may change if it was originally null). */
struct data_queue *
appendToRRDQueue(struct data_queue *dl, char *datum)
{
	struct data_queue *p=NULL;
	struct data_queue *newe=NULL;

	assert(datum);

	/* Seek to the end of the list */
	for(p=dl; p!=NULL && p->next!=NULL; p=p->next);

	newe=calloc(sizeof(struct data_queue), 1);
	assert(newe);

	newe->line=strdup(datum);
	assert(newe->line);

	if(p==NULL) {
		dl=newe;
	} else {
		p->next=newe;
	}

	return(dl);
}

void disposeOfRRDQueue(struct data_queue *dl)
{
	assert(dl);
	if(dl->next != NULL) {
		disposeOfRRDQueue(dl->next);
	}
	free(dl->line);
	free(dl);
}

#ifdef DATA_MAIN
int main(int argc, char **argv)
{
	struct log_datum *entry=NULL;
	time_t t;

	entry=parseLogEntry(argv[1]);
	printf("%d.%d - %s - %f\n", entry->tv.tv_sec, entry->tv.tv_usec,
		entry->serial, entry->reading);

	t=entry->tv.tv_sec;
	printf("%s\n", ctime(&t));

	disposeOfLogEntry(entry);
}
#endif
