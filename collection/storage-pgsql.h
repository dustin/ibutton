/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: storage-pgsql.h,v 1.1 2002/01/26 08:24:42 dustin Exp $
 */

#ifndef STORAGE_PGSQL_H
#define STORAGE_PGSQL_H 1

struct pg_config_t {
	char *dbhost;
	char *dbuser;
	char *dbpass;
	char *dboptions;
	char *dbname;
	char *dbport;
};

extern struct pg_config_t pg_config;

void saveDataPostgres(struct data_list *p);

#endif /* STORAGE_PGSQL_H */
