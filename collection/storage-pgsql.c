/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: storage-pgsql.c,v 1.3 2002/01/26 10:08:43 dustin Exp $
 */

#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#ifdef HAVE_LIBPQ_FE_H
#include <libpq-fe.h>
#endif /* HAVE_LIBPQ_FE_H */

/* Local includes */
#include "data.h"
#include "collection.h"
#include "storage-pgsql.h"
#include "../utility.h"

#ifdef HAVE_LIBPQ_FE_H

/* Definition of how to connect to the database. */
struct pg_config_t pg_config;

/* The actual database connection */
static PGconn *dbConn=NULL;

#endif /* HAVE_LIBPQ_FE_H */

#ifdef HAVE_LIBPQ_FE_H

static void
abandonPostgresConnection()
{
	verboseprint(1, ("Abandoning postgres connection (if there is one).\n"));
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

		verboseprint(1, ("Connecting to postgres.\n"));

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

void
initPostgresStore()
{
	memset(&pg_config, 0x00, sizeof(pg_config));
	pg_config.dbport="5432";
}

void
saveDataPostgres(struct data_list *p)
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
		verboseprint(2, ("Query:  %s\n", query));

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
