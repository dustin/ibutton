/*
 * Copyright (c) 2002  Dustin Sallings <dustin@spy.net>
 *
 * $Id: collection.h,v 1.3 2002/01/30 09:52:07 dustin Exp $
 */

#ifndef COLLECTION_H
#define COLLECTION_H 1

/* Necessary booleans */
#define YES 1
#define NO 0

/* When not using threads, flush after every CALLS_PER_FLUSH calls. */
#ifndef CALLS_PER_FLUSH
# define CALLS_PER_FLUSH 10
#endif /* no CALLS_PER_FLUSH */

/* Defaults for listening (may be overridden at compile time) */
#ifndef MLAN_PORT
# define MLAN_PORT 6789
#endif /* MLAN_PORT */
#ifndef MLAN_GROUP
# define MLAN_GROUP "225.0.0.37"
#endif /* MLAN_GROUP */

#define COLLECTOR_VERSION "1.1"

extern int col_verbose;

/* Verbose printing */
#define verboseprint(a, b) if(col_verbose>=a) { printf b; }

#endif /* COLLECTION_H */
