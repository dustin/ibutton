/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: storage-pgsql.h,v 1.3 2002/01/29 21:36:12 dustin Exp $
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

void saveDataPostgres(struct log_datum *p);
void initPostgresStore();

#endif /* STORAGE_PGSQL_H */

/*
 * arch-tag: BBFBC21E-13E5-11D8-969A-000393CFE6B8
 */
