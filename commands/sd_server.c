/*
 * Copyright (c) 2004  Dustin Sallings <dustin@spy.net>
 */

/*{{{1 Includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_ALLOCA_H
# include <alloca.h>
#endif
#define _BSD_SIGNALS
#include <signal.h>

#ifdef HAVE_UTIME_H
#include <utime.h>
#endif /* HAVE_UTIME_H */

/* For multicast */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* My extra clibs */
#include <stringlib.h>

/* Local */
#include <mlan.h>
#include <ds1920.h>
#include <ds1921.h>
#include <ds2406.h>
#include "sample_devices.h"
/*}}}1*/

static int writeToClient(struct client *client, const char *fmt, ...);
static char *sdParseSerial(char *in, uchar *out);

/* {{{1 Commands */
/* 2406 command {{{2*/
static int
run2406Command(MLan *mlan, struct client *client, int argc, char **args)
{
	int rv=CLIENT_CONTINUE;
	int status=0;
	uchar serial[MLAN_SERIAL_SIZE];
	char usage[80];

	/* Let's go ahead and assume the usage is incorrect */
	snprintf(usage, sizeof(usage), ":( incorrect arguments.  "
				"usage:  %s serial_number [channel on/off]\n", args[0]);

	/* Validate the serial number */
	if(argc > 1) {
		sdParseSerial(args[1], serial);
		if(serial[0] != 0x12) {
			writeToClient(client,
				":( serial number must belong to a 2406, %s does not\n",
					args[1]);
			return(rv);
		}
	}

	if(argc == 2) {
		status=getDS2406Status(mlan, serial);

		writeToClient(client, ":) a=%s b=%s\n",
				(status&DS2406SwitchA)?"off":"on",
				(status&DS2406SwitchB)?"off":"on");
		
	} else if(argc == 4) {
		int which=0;
		int state=0;
		/* Parse the switch port */
		if(strcasecmp(args[2], "a") == 0) {
			which=DS2406SwitchA;
		} else if(strcasecmp(args[2], "b") == 0) {
			which=DS2406SwitchB;
		} else {
			writeToClient(client, usage);
			return(rv);
		}
		/* Parse the new state */
		if(strcasecmp(args[3], "on") == 0) {
			state=DS2406_ON;
		} else if(strcasecmp(args[3], "off") == 0) {
			state=DS2406_OFF;
		} else {
			writeToClient(client, usage);
			return(rv);
		}
		/* Do it! */
		if(setDS2406Switch(mlan, serial, which, state) == TRUE) {
			log_info("Changed switch %s/%s to %s for client %d (%s)\n",
				args[1], args[2], args[3], client->s, client->ip);
			writeToClient(client, ":) switch flipped\n");
		} else {
			writeToClient(client, ":( switch failed to flip\n");
		}
	} else {
		/* Let the error message fall through */
		writeToClient(client, usage);
	}

	return(rv);
} /*}}}2*/

/* {{{2 1920 command */
static int
run1920Command(MLan *mlan, struct client *client, int argc, char **args)
{
	int rv=CLIENT_CONTINUE;
	uchar serial[MLAN_SERIAL_SIZE];
	struct ds1920_data data;
	char usage[80];

	/* Let's go ahead and assume the usage is incorrect */
	snprintf(usage, sizeof(usage), ":( incorrect arguments.  "
				"usage:  %s serial_number [low_alarm high_alarm]\n", args[0]);

	/* Validate the arguments */
	if(argc<2 || argc==3 || argc>4) {
		writeToClient(client, usage);
		return(rv);
	} else {
		sdParseSerial(args[1], serial);
		if(serial[0] != DEVICE_1920) {
			writeToClient(client,
				":( serial number must belong to a 1920, %s does not\n",
					args[1]);
			return(rv);
		}
	}

	if(argc == 4) {
		memset(&data, 0x00, sizeof(data));
		data.temp_low=atof(args[2]);
		data.temp_hi=atof(args[3]);
		log_info(
			"Setting new 1920 parameters (%s %.2f %.2f) from client %d (%s)\n",
			args[1], data.temp_low, data.temp_hi, client->s, client->ip);
		if(setDS1920Params(mlan, serial, data) != TRUE) {
			writeToClient(client, ":( problem setting parameters\n");
			return(rv);
		}
	}

	recall(mlan, serial);
	data=getDS1920Data(mlan, serial);
	if(data.valid == TRUE) {
		writeToClient(client, ":) %.2f lo=%.2f hi=%.2f\n",
			data.temp, data.temp_low, data.temp_hi);
	} else {
		writeToClient(client, ":( problem reading 1920 data\n");
	}

	return(rv);
} /* }}}2 */

/* {{{2 1921 mission command */
static int
run1921MissionCommand(MLan *mlan, struct client *client, int argc, char **args)
{
	int rv=CLIENT_CONTINUE;
	uchar serial[MLAN_SERIAL_SIZE];
	struct ds1921_data data;
	char usage[160];
	extern char *optarg;
	extern int optind;
	char temp, *tmp;
	int ch=0;

	optind=1;

	/* Let's go ahead and assume the usage is incorrect */
	snprintf(usage, sizeof(usage), ":( incorrect arguments.  "
				"usage:  %s [-r] [-s sample_rate] [-d mission_delay] "
				"[-l low_alert] [-h high_alert] serial_number\n", args[0]);

	while( (ch=getopt(argc, args, "s:d:l:h:r")) != -1) {
		switch(ch) {
			case 'r':
				/* Enable rollover */
				data.status.control|=CONTROL_ROLLOVER_ENABLED;
				break;
			case 's':
				/* Set the sample rate */
				data.status.sample_rate=atoi(optarg);
				if(data.status.sample_rate<1) {
					writeToClient(client, usage);
					return(rv);
				}
				break;
			case 'd':
				/* Set the delay */
				data.status.mission_delay=atoi(optarg);
				if(data.status.mission_delay<0) {
					writeToClient(client, usage);
					return(rv);
				}
				break;
			case 'l':
				/* Set the low temperature */
				temp=strtod(optarg, &tmp);
				if(tmp==optarg) {
					writeToClient(client, usage);
					return(rv);
				}
				break;
			case 'h':
				/* Set the high temperature */
				temp=strtod(optarg, &tmp);
				if(tmp == optarg) {
					writeToClient(client, usage);
					return(rv);
				}
				break;
			case '?':
				writeToClient(client, usage);
				return(rv);
		}
	}

	argc-=optind;
	args+=optind;

	/* Validate the arguments */
	if(argc<1) {
		writeToClient(client, usage);
		return(rv);
	} else {
		sdParseSerial(args[0], serial);
		if(serial[0] != 0x21) {
			writeToClient(client,
				":( serial number must belong to a 1921, %s does not\n",
					args[0]);
			return(rv);
		}
	}

	if(ds1921_mission(mlan, serial, data)==TRUE) {
		writeToClient(client, ":) Missioning successful\n");
	} else {
		writeToClient(client, ":( problem missioning 1921\n");
	}

	return(rv);
} /* }}}2 */

/* {{{2 1921 info command */
static int
run1921InfoCommand(MLan *mlan, struct client *client, int argc, char **args)
{
	int rv=CLIENT_CONTINUE;
	uchar serial[MLAN_SERIAL_SIZE];
	struct ds1921_data data;
	char usage[80];

	/* Let's go ahead and assume the usage is incorrect */
	snprintf(usage, sizeof(usage), ":( incorrect arguments.  "
				"usage:  %s serial_number\n", args[0]);

	/* Validate the arguments */
	if(argc != 2) {
		writeToClient(client, usage);
		return(rv);
	} else {
		sdParseSerial(args[1], serial);
		if(serial[0] != DEVICE_1921) {
			writeToClient(client,
				":( serial number must belong to a 1921, %s does not\n",
					args[1]);
			return(rv);
		}
	}

	data=getDS1921Data(mlan, serial);
	if(data.valid == TRUE) {
		if(data.status.status & STATUS_MISSION_IN_PROGRESS) {
			writeToClient(client, ":) missioned %d missioncounter: %d "
				"devcounter: %d samplerate: %d\n",
				(int)data.status.mission_ts.clock,
				data.status.mission_s_counter, data.status.device_s_counter,
				data.status.sample_rate);
		} else {
			writeToClient(client, ":) nomission devcounter:  %d samples\n",
				data.status.device_s_counter);
		}
	} else {
		writeToClient(client, ":( problem reading 1921 data\n");
	}

	return(rv);
} /* }}}2 */

/* search command {{{2 */
static int
runSearchCommand(MLan *mlan, struct client *client, int argc, char **args)
{
	int rv=CLIENT_CONTINUE;
	int rslt=0, current=0;
	char *textResult=NULL;
	char **list=NULL;

	/* Allocate room for the list */
	list=calloc(MAX_SERIAL_NUMS, sizeof(char *));

	rslt=mlan->first(mlan, TRUE, 0);
	while(rslt) {
		uchar serial[MLAN_SERIAL_SIZE];
		/* get the serial number into the current storage */
		mlan->copySerial(mlan, serial);
		list[current++]=strdup(get_serial(serial));
		assert(current < MAX_SERIAL_NUMS);
		/* Find the next one */
		rslt = mlan->next(mlan, TRUE, 0);
	}
	textResult=strnJoin(" ", list, current);

	writeToClient(client, ":) %s\n", textResult);

	free(textResult);
	freePtrList(list);

	return(rv);
}
/* }}}2 */

/* Quit command {{{2 */
static int
runQuitCommand(MLan *mlan, struct client *client, int argc, char **args)
{
	writeToClient(client, ":) good bye\n");
	return(CLIENT_DONE);
}
/* }}}2 */

struct cmdPointer cmdPointers[]={
	{"quit", runQuitCommand},
	{"search", runSearchCommand},
	{"2406", run2406Command},
	{"1920", run1920Command},
	{"1921mission", run1921MissionCommand},
	{"1921info", run1921InfoCommand},
	{NULL, NULL}
};
/* }}}1 */

/* command infrastructure {{{1 */
static int
runCommand(MLan *mlan, struct client *client, char *command)
{
	int rv=CLIENT_CONTINUE;
	int i=0;
	char **argv=NULL;
	struct cmdPointer *cmd=NULL;

	log_info("Client %d from %s requested %s\n",
		client->s, client->ip, command);

	argv=strSplit(' ', command, 0);

	/* Figure out what command to run */
	for(i=0; (cmd == NULL) && (cmdPointers[i].name != NULL); i++) {
		if(strcmp(cmdPointers[i].name, argv[0]) == 0) {
			cmd=&cmdPointers[i];
		}
	}
	if(cmd == NULL) {
		/* Invalid command */
		int numCmds=(sizeof(cmdPointers)/sizeof(struct cmdPointer));
		char **cmds=calloc(numCmds, sizeof(char *));
		char *cmdsString=NULL;
		assert(cmds);

		/* Copy the command names */
		for(i=0; i<numCmds; i++) {
			cmds[i]=cmdPointers[i].name;
		}

		cmdsString=strJoin(" ", cmds);

		writeToClient(client, ":( invalid command:  %s (valid:  %s)\n",
			argv[0], cmdsString);

		free(cmds);
		free(cmdsString);
	} else {
		/* User is running a command */
		rv=cmd->cmd(mlan, client, ptrListLength(argv), argv);
	}

	freePtrList(argv);

	return(rv);
}

/* Parse the serial number if it looks like a serial number, else return a
 * serial number beginning with 0x00 */
static char *
sdParseSerial(char *in, uchar *out)
{
	char *rv=NULL;

	/* Validate the size */
	if(strlen(in) == (MLAN_SERIAL_SIZE*2)) {
		rv=parseSerial(in, out);
	} else {
		out[0]=0x00;
	}
	return(rv);
}

static int
processClientData(MLan *mlan, struct client *client)
{
	int rv=CLIENT_CONTINUE;
	char *p=client->rbuf;
	char *cr=NULL;
	char *lf=NULL;
	char *soonest=NULL;
	char *latest=NULL;

	/* Skip over leading whitespace */
	while(*p != NULL && isspace(*p)) {
		p++;
	}

	cr=strchr(p, '\r');
	lf=strchr(p, '\n');

	soonest=POS_MIN(cr, lf);
	latest=POS_MAX(cr, lf);

	if(soonest > 0) {
		int len=((int)soonest - (int)(p));
		/* We move the rest of the data in */
		int toMove=strlen(latest);

		char *stuff=(char *)alloca(len + 1);

		strncpy(stuff, p, len);
		stuff[len]=0x00;

		/* Move the remaining data up the buffer */
		memmove(client->rbuf, latest, toMove);
		/* Terminate the string */
		client->rbuf[toMove]=0x00;
		/* Set bytes read to the end of this string */
		client->bytesRead = strlen(client->rbuf);

		/* Run this command */
		rv=runCommand(mlan, client, stuff);

		/* Recurse in case there were more commands */
		if(rv == CLIENT_CONTINUE) {
			rv=processClientData(mlan, client);
		}
	}

	return(rv);
}

void
closeConnection(struct client *client)
{
	log_info("Closing connection %d from %s.\n", client->s, client->ip);
	close(client->s);
	freeClientMessages(client->msgs);
}   

int
handleRead(MLan *mlan, struct client *client)
{
	int rv=CLIENT_CONTINUE;
	int toRead=(CLIENT_BUF_SIZE - client->bytesRead - 1);
	int bytesRead=0;

	if(toRead > 0) {
		/* See if we can read stuff */
		bytesRead=read(client->s, client->rbuf+client->bytesRead, toRead);
		if(bytesRead > 0) {
			client->bytesRead += bytesRead;
			client->rbuf[client->bytesRead]=0x00;
			assert(client->bytesRead < CLIENT_BUF_SIZE);
			rv=processClientData(mlan, client);
		} else {
			rv=CLIENT_DONE;
		}
	} else {
		/* Not sure what's going on here, but disconnecting */
		rv=CLIENT_DONE;
	}

	return(rv);
}

static int
writeToClient(struct client *client, const char *fmt, ...)
{
	va_list ap;
#ifdef HAVE_VASPRINTF
	char *aptr;
#else
	char buf[CLIENT_BUF_SIZE];
#endif
	int toWrite=0;
	struct client_msg *newmsg, *lastmsg;

	va_start(ap, fmt);
#ifdef HAVE_VASPRINTF
	toWrite=vasprintf(&aptr, fmt, ap);
#else
	toWrite=vsnprintf(buf, CLIENT_BUF_SIZE, fmt, ap);
	assert(toWrite < CLIENT_BUF_SIZE);
#endif
	va_end(ap);

	/* OK, now append that to the messages list */
	newmsg=calloc(1, sizeof(struct client_msg));
#ifdef HAVE_VASPRINTF
	newmsg->data=aptr;
#else
	newmsg->data=strdup(buf);
#endif
	newmsg->offset=0;
	newmsg->toWrite=toWrite;

	if(client->msgs == NULL) {
		client->msgs=newmsg;
	} else {
		/* Flip through all of the current messages */
		for(lastmsg=client->msgs; lastmsg->next != NULL; lastmsg=lastmsg->next);
		lastmsg->next=newmsg;
	}
	return(toWrite);
}

void
freeClientMessage(struct client_msg *msg)
{
	if(msg != NULL) {
		free(msg->data);
		free(msg);
	}
}

void
freeClientMessages(struct client_msg *msg)
{
	if(msg != NULL) {
		freeClientMessages(msg->next);
		freeClientMessage(msg);
	}
}

void
handleWrite(struct client *client)
{
	int written=0;

	/* Try to write as much as we have to write from wherever we are */
	written=write(client->s, client->msgs->data + client->msgs->offset,
		client->msgs->toWrite);

	if(written > 0) {
		client->msgs->toWrite -= written;
		client->msgs->offset += written;
		/* If we've written enough of this message, pop it off the list and get
		 * the next one ready (or the end, whichever it is) */
		if(client->msgs->toWrite <= 0) {
			struct client_msg *next=client->msgs->next;
			freeClientMessage(client->msgs);
			client->msgs=next;
		}
	} else {
		log_error("Error writing to client %d (%s):  %s, closing\n",
			client->s, client->ip, strerror(errno));
		/* Mark this connection as finished, it'll be automatically closed */
		client->connState=FINISHED;
	}
}

void
handleNewConnection(struct client *client, struct sockaddr_in fsin)
{
	/* IP address as a string */
	char *iptmp=NULL;
	/* set up non blocking */
	int fflags = fcntl(client->s, F_GETFL);
	if(fcntl(client->s, F_SETFL, fflags | O_NONBLOCK) < 0) {
		perror("fcntl for nonblock");
	}

	client->connState=CONNECTED;
	iptmp=inet_ntoa(fsin.sin_addr);
	strncat(client->ip, iptmp, sizeof(client->ip));
	client->ip[sizeof(client->ip)-1]=0x00;

	/* Hook up the welcome message */
	writeToClient(client, ":) Hello!\n");
}

int
getServerSocket(int port)
{
	int reuse=1, s=0;
	struct sockaddr_in sin;

	if((s=socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("server: socket");
		exit(1);
	}

	memset((char *) &sin, 0x00, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(int));

	if( bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		perror("server: bind");
		exit(1);
	}

	if(listen(s, 5) < 0) {
		perror("server: listen");
		exit(1);
	}

	log_info("Server port initialized.\n");
	
	return(s);
}
/* }}}1 */

/*
 * arch-tag: 623333-49-118-82-0030187026
 * vim: foldmethod=marker
 */
