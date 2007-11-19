/*
 * Copyright (c) 2004  Dustin Sallings <dustin@spy.net>
 */

#ifndef SAMPLE_DEVICES_H
#define SAMPLE_DEVICES_H 1

#define POS_MIN(a, b) ((a != NULL && a < b) ? a : b)
#define POS_MAX(a, b) ((a > b) ? a : b)

#define MAX_CONNECTIONS 64
#define CLIENT_BUF_SIZE 1024

#define CLIENT_CONTINUE 0 
#define CLIENT_DONE 1

/* Possible TCP client states */
enum client_state {
	CONNECTED, /* Connected, ready to talk. */
	QUITTING,  /* Disconnecting, close after write buffer is empty */
	FINISHED   /* Close this connection */
};

/* Queued up client messages */
struct client_msg {
	char *data;
	int offset;
	int toWrite;
	struct client_msg *next;
};

/* Client connection information */
struct client {
	int s;
	short connState;
	int bytesRead;
	char rbuf[CLIENT_BUF_SIZE];
	/* Client messages */
	struct client_msg *msgs;
	/* String version of the IP address of the remote end */
	char ip[16];
};

/* Command storage structure */
struct cmdPointer {
	char *name;
	int (*cmd)(MLan *mlan, struct client *client, int argc, char **argv);
};

/* Command pointers */
extern struct cmdPointer cmdPointers[];

/* Prototypes */
void log_error(char *str, ...);
void log_info(char *str, ...);
void handleNewConnection(struct client*, struct sockaddr_in);
int getServerSocket(int port);
int handleRead(MLan *mlan, struct client *client);
void handleWrite(struct client *client);
uchar *sdParseSerial(char *in, uchar *out);

/* Free a client message */
void freeClientMessage(struct client_msg *msg);
/* Free a list of client messages */
void freeClientMessages(struct client_msg *msg);
/* Close a connection */
void closeConnection(struct client *client);

#endif /* SAMPLE_DEVICES_H */
