/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
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
