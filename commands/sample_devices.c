/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 */

/*{{{ Includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
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
/*}}}*/

/* Globals and defines {{{ */

/* Globals because I have to move it out of the way in a signal handler */
static FILE     *logfile=NULL;
static char     *logfilename=NULL;
static int       need_to_reinit=1, need_to_rotate=0;
static char     *busdev=NULL;
static char     *curdir=NULL;

/* The multicast stuff */
static int msocket=-1;
static struct sockaddr_in maddr;

/* Server socket */
static int serverSocket=-1;

static struct watch_list *watching=NULL;

/* The maximum amount of time we'll keep a device on the watch list */
#define MAX_AGE 900
/* Number of seconds between samples */
#define SAMPLE_FREQUENCY 60
/* Maximum number of failures before a device is removed from the watch list */
#define MAX_FAILURES 10

/*}}}*/

/* Prototypes {{{ */
static void setsignals();
/*}}}*/

/* Data structures {{{*/
/* This is the list of things being watched */
struct watch_list {
    uchar serial[MLAN_SERIAL_SIZE];
    time_t last_read; /* Time of last successful read */
    time_t next_read; /* Time of next read attempt */
    int fail_count;   /* number of concurrent failures */
    struct watch_list *next;
};

/*}}}*/

/* Logging and reporting {{{ */
static time_t
getSerialFileTimestamp(uchar *serial)
{
    char    fn[8192];
    struct stat sb;
    time_t rv=0;
    char *string_ser=NULL;

    assert(serial);

    string_ser=get_serial(serial);
    assert(serial);

    strcpy(fn, curdir);
    strcat(fn, "/");
    strcat(fn, string_ser);

    assert(strlen(fn)<sizeof(fn));

    /* If we can stat it, use the mtime */
    if(stat(fn, &sb)==0) {
        rv=sb.st_mtime;
    }

    return(rv);
}

/* Get a watch_list entry for the given serial number */
static struct watch_list *
getWatch(uchar *serial)
{
    struct watch_list *p=NULL;

    assert(serial);

    /* Search for a match */
    for(p=watching; p!=NULL
            && (memcmp(serial, p->serial, MLAN_SERIAL_SIZE)!=0); p=p->next);
    assert(p==NULL || memcmp(serial, p->serial, MLAN_SERIAL_SIZE)==0);

    if(p==NULL) {
        log_info("Adding %s to the watch list.\n", get_serial(serial));
        p=calloc(sizeof(struct watch_list), 1);
        assert(p);
        memcpy(p->serial, serial, MLAN_SERIAL_SIZE);
        p->next=watching;
        watching=p;
        p->last_read=getSerialFileTimestamp(serial);
    }

    assert(p!=NULL && memcmp(serial, p->serial, MLAN_SERIAL_SIZE)==0);

    return(p);
}

static int
watchIsGarbage(struct watch_list *w)
{
    time_t now = time(NULL);
    int rv=0;

    assert(w != NULL);
    assert(w->serial != NULL);

    rv=(w->fail_count > MAX_FAILURES || w->last_read < now - MAX_AGE);

    /*
      log_info("watchIsGarbage(%s) age=%d fail_count=%d -- rv=%d\n",
      get_serial(w->serial), time(NULL) - w->last_read, w->fail_count, rv);
    */

    return(rv);
}

static void
cleanWatchList()
{
    struct watch_list *p=NULL;

    /* Deal with stuff at the front of the list */
    while(watching != NULL && watchIsGarbage(watching)) {
        p=watching;
        assert(p != NULL);
        assert(p->serial != NULL);
        log_error("Removing %s from the watch list (age=%d, fail_count=%d)\n",
                  get_serial(p->serial), time(NULL) - p->last_read, p->fail_count);
        watching = p->next;
        free(p);
    }

    /* Remove anything else in the list */
    if(watching != NULL) {
        struct watch_list *tmp=NULL;
        for(p=watching; p != NULL && p->next != NULL; p=p->next) {
            if(watchIsGarbage(p->next)) {
                tmp=p->next;
                assert(tmp != NULL);
                assert(tmp->serial != NULL);
                log_error(
                          "Removing %s from the watch list (age=%d, fail_count=%d)\n",
                          get_serial(tmp->serial), time(NULL) - tmp->last_read,
                          tmp->fail_count);
                p->next=tmp->next;
                free(tmp);
            }
        }
    }
}

static void
updateSuccess(uchar *serial, time_t when)
{
    struct watch_list *p=getWatch(serial);

    p->last_read=when;
    p->next_read=when + SAMPLE_FREQUENCY;
    p->fail_count=0;
}

static void
updateFailure(uchar *serial, time_t when)
{
    struct watch_list *p=getWatch(serial);

    /* calculate the next read as a function of the current failure count */

    p->next_read=when + (5 + pow(2,p->fail_count));
    p->fail_count++;
}

/* Record the current sample into a file unique to the serial number */
static void
record_cur(const char *serial, const char *value, time_t update_time)
{
    char    fn[8192];
    struct utimbuf tvp;
    FILE    *f;

    strcpy(fn, curdir);
    strcat(fn, "/");
    strcat(fn, serial);

    assert(strlen(fn)<sizeof(fn));

    f=fopen(fn, "w");
    if(f==NULL) {
        perror(fn);
        return;
    }
    fprintf(f, "%s\n", value);
    fclose(f);

    tvp.actime=time(NULL);
    tvp.modtime=update_time;

    utime(fn, &tvp);
}

/* Initialize the watch list from the stamps left in the curdir */
static void
init_from_cur()
{
    DIR *dir=NULL;
    struct dirent *d=NULL;
    uchar serial[MLAN_SERIAL_SIZE];

    dir=opendir(curdir);
    while( (d = readdir(dir)) != NULL) {
        sdParseSerial(d->d_name, serial);
        if(serial[0] != 0x00) {
            getWatch(serial);
        }
    }
}

/* Get the time for the record. */
static char*
get_time_str_now()
{
    struct timeval tv;
    struct tm *tp;
    struct timezone tz;
    static char str[50];
    double seconds;

    /* Get some better precisioned time */
    gettimeofday(&tv, &tz);

    tp = localtime((time_t *)&tv.tv_sec);
    seconds = tp->tm_sec+(double)tv.tv_usec/1000000;
    sprintf(str, "%.4d/%.2d/%.2d %.2d:%.2d:%s%.6f",
            tp->tm_year + 1900,
            tp->tm_mon + 1,
            tp->tm_mday,
            tp->tm_hour,
            tp->tm_min,
            (seconds <10 ? "0" : ""),
            seconds);
    return(str);
}

/* Get the time for the record. */
static char*
get_time_str(time_t t)
{
    struct tm *tp;
    static char str[50];
    time_t sec=0;

    sec=t;
    tp = localtime(&sec);

    sprintf(str, "%.4d/%.2d/%.2d %.2d:%.2d:%s%d.0",
            tp->tm_year + 1900,
            tp->tm_mon + 1,
            tp->tm_mday,
            tp->tm_hour,
            tp->tm_min,
            (tp->tm_sec <10 ? "0" : ""), tp->tm_sec);
    return(str);
}

static void
log_msg(const char *level, const char *fmt, va_list ap)
{
    fprintf(stderr, "%s:  %s:  ", get_time_str_now(), level);
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
}

void
log_error(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg("ERROR", fmt, ap);
    va_end(ap);
}

void
log_info(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    log_msg("INFO", fmt, ap);
    va_end(ap);
}
/* }}} */

/* Signal handling {{{ */
void
_sighup(int sig)
{
    need_to_rotate=1;
}

static void rotateLogs()
{
    char logfiletmp[1024];
    log_info("Rotating logs.\n");
    if(logfile != NULL) {
        fclose(logfile);
    }
    assert(strlen(logfilename) < 1000);
    sprintf(logfiletmp, "%s.old", logfilename);
    rename(logfilename, logfiletmp);
    logfile=fopen(logfilename, "w");
    assert(logfile);
    setsignals();
    need_to_rotate=0;
}

void
_sigalarm(int sig)
{
    log_error("Got SIGALRM\n");
    need_to_reinit=1;
    setsignals();
}

static void
setsignals()
{
    signal(SIGHUP, _sighup);
    signal(SIGALRM, _sigalarm);
}
/* }}} */

/* Multicast handling {{{ */
/* Initialize the multicast socket and addr.  */
static void
initMulti(const char *group, int port, int mttl)
{
    char ttl='\0';

    assert(mttl>=0);
    assert(mttl<256);

    msocket=socket(AF_INET, SOCK_DGRAM, 0);
    if(msocket<0) {
        perror("socket");
        exit(1);
    }

    /* If a TTL was given, use it */
    if(mttl>0) {
        ttl=(char)mttl;
        if( setsockopt(msocket, IPPROTO_IP, IP_MULTICAST_TTL,
                       &ttl, sizeof(ttl) )<0) {

            perror("set multicast ttl");
            exit(1);
        }
    }

    memset(&maddr, 0, sizeof(maddr));
    maddr.sin_family = AF_INET;
    maddr.sin_addr.s_addr = inet_addr(group);
    maddr.sin_port = htons(port);
}

/* Send a multicast message */
static void
msend(const char *msg)
{
    static int sent=0;
    int i=0, cont=1;
    extern int errno;

    if(msocket>=0) {
        /* Try up to three times to send the packet */
        for(i=0; i<3 && cont==1; i++) {
            /* length of the string +1 to send the NULL */
            if(sendto(msocket, msg, (strlen(msg)+1), 0,
                      (struct sockaddr *)&maddr, sizeof(maddr)) < 0) {
                switch(errno) {
                case ENOBUFS:
                    /* Slow it down a bit, try again */
                    usleep(10);
                    break;
                default:
                    /* Any other error is fatal */
                    cont=0;
                    break;
                }
                perror("sendto");
            } else {
                /* Don't continue the loop, we have success! */
                cont=0;
            }
        }

        /* Sleep a bit after a few packets, just to be courteous */
        if( (++sent%100) ) {
            usleep(10);
        }
    }
}
/* }}} */

/* Device handling {{{ */

/* Parse the serial number if it looks like a serial number, else return a
 * serial number beginning with 0x00 */
uchar *
sdParseSerial(char *in, uchar *out)
{
    uchar *rv=NULL;

    /* Validate the size */
    if(strlen(in) == (MLAN_SERIAL_SIZE*2)) {
        rv=parseSerial(in, out);
    } else {
        out[0]=0x00;
    }
    return(rv);
}

static int dealWith1920(MLan *mlan, uchar *serial)
{
    struct ds1920_data data;
    int rv=0;

    data=getDS1920Data(mlan, serial);
    if(data.valid!=TRUE) {
        rv=-1;
        /* Log the failure */
        log_error("Did not return valid data from %s.\n", get_serial(serial));
        updateFailure(serial, time(NULL));
    } else {
        char data_str[8192];
        snprintf(data_str, sizeof(data_str),
                 "%s\t%s\t%.2f\tl=%.2f,h=%.2f",
                 get_time_str_now(), get_serial(serial),
                 data.temp, data.temp_low,
                 data.temp_hi);
        /* Log it */
        fprintf(logfile, "%s\n", data_str);
        /* Multicast it */
        msend(data_str);
        /* Now record the current */
        snprintf(data_str, sizeof(data_str), "%.2f", data.temp);
        record_cur(get_serial(serial), data_str, time(NULL));
        /* Mark it as updated */
        updateSuccess(serial, time(NULL));
    }

    return(rv);
}

static int
dealWith1921(MLan *mlan, uchar *serial)
{
    struct ds1921_data data;
    int i=0;
    /* Error until we determine otherwise */
    int rv=-1;

    data=getDS1921Data(mlan, serial);
    if(data.valid==TRUE) {
        struct watch_list *watch=getWatch(serial);
        time_t last_record=0;
        float last_reading=0;
        char data_str[8192];
        /* Disable alarms for recording the data.  Nothing should
         * block here, and if it doesn't finish properly, we're
         * going to lose our timestamp thingy.  It can take a while
         * when there's a lot of data, but ooooh well.  */
        alarm(0);
        for(i=0; i<data.n_samples; i++) {
            /* Only send data we haven't seen */
            if(data.samples[i].timestamp > watch->last_read) {
                snprintf(data_str, sizeof(data_str),
                         "%s\t%s\t%.2f\ts=%d,r=%d",
                         get_time_str(data.samples[i].timestamp),
                         get_serial(serial), data.samples[i].sample,
                         (int)data.status.mission_ts.clock,
                         data.status.sample_rate);
                /* Log it */
                fprintf(logfile, "%s\n", data_str);
                /* Multicast it */
                msend(data_str);
            }
            last_record=data.samples[i].timestamp;
            last_reading=data.samples[i].sample;
        }
        /* Mark it as updated */
        updateSuccess(serial, last_record);
        snprintf(data_str, sizeof(data_str), "%.2f", last_reading);
        record_cur(get_serial(serial), data_str, last_record);

        /* OK, if we got this far, it wasn't an error */
        rv=0;
    } else {
        log_error("Did not return valid data from %s.\n", get_serial(serial));
        updateFailure(serial, time(NULL));
    }

    return(rv);
}

static int
dealWith(MLan *mlan, uchar *serial)
{
    int rv=0;

    assert(mlan);
    assert(serial);

    switch(serial[0]) {
    case DEVICE_1920:
        rv=dealWith1920(mlan, serial);
        break;
    case DEVICE_1921:
        rv=dealWith1921(mlan, serial);
        break;
    default:
        /* Mark it as updated */
        updateSuccess(serial, time(NULL));
        break;
    }

    return(rv);
}

static int
busLoop(MLan *mlan)
{
    uchar list[MAX_SERIAL_NUMS][MLAN_SERIAL_SIZE];
    struct watch_list *wtmp=NULL;
    int failures=0, successes=0;
    int list_count=0;
    int i=0;
    int rslt=0;
    /* Devices are eligible for sampling if the last time they were seen is
     * more than SAMPLE_FREQUENCY seconds ago */
    time_t now=time(NULL);

    /* Set the timer for the first */
    alarm(5);
    rslt=mlan->first(mlan, TRUE, FALSE);
    while(rslt) {
        /* Copy the serial number into our list */
        mlan->copySerial(mlan, list[list_count++]);
        /* Don't go too far */
        assert(list_count<sizeof(list)-1);
        /* Grab the next device, reset the timer */
        alarm(5);
        rslt = mlan->next(mlan, TRUE, FALSE);
    }

    /* Loop through the list and make sure all serials are in the watch list */
    for(i=0; i<list_count; i++) {
        /* This will make sure the device is in the watch list */
        getWatch(list[i]);
    }

    /* Loop through the watch list and gather samples */
    for(wtmp=watching; wtmp != NULL; wtmp=wtmp->next) {
        if(wtmp->next_read <= now) {
            alarm(15);
            if(dealWith(mlan, wtmp->serial) < 0) {
                failures++;
            } else {
                successes++;
            }
        }
    }

    /* Disable alarms */
    alarm(0);

    if(list_count==0 && successes==0) {
        log_error("Didn't find any devices, indicating failure.\n");
        failures++;
    }

    /* Clean the watch list after a loop */
    cleanWatchList();

    return(failures);
}
/* }}} */

/* Bus handling {{{ */
/* NOTE:  The initialization may be called more than once */
static MLan *
init(char *port)
{
    MLan *m=NULL;

    m=mlan_init(port, PARMSET_9600);

    if(m!=NULL) {
        if(m->ds2480detect(m)!=TRUE) {
            log_error("Did not detect DS2480.\n");
            m->destroy(m);
            m=NULL;
        }
    } else {
        log_error("Failed to open device.\n");
    }

    return(m);
}

static MLan*
mlanReinit(MLan *mlan)
{
    if(mlan) {
        mlan->destroy(mlan);
    }
    mlan=NULL;

#ifdef MYMALLOC
    _mdebug_dump();
#endif /* MYMALLOC */

    while(mlan==NULL) {
        log_info("(re)initializing the 1wire bus.\n");
        mlan=init(busdev);
        if(mlan==NULL) {
            log_error("Init failed, sleeping...\n");
            sleep(15);
        } else {
            log_info("Initialization successful.\n");
            need_to_reinit=0;
        }
    }
    /* Wait a second */
    sleep(1);

    return(mlan);
}
/* }}} */

/* Main loop {{{*/
static void
mainLoop()
{
    int         failures=0;
    int         selected=0, i=0;
    MLan        *mlan=NULL;
    time_t      thisTime=0, lastTime=0;
    struct client *clients[MAX_CONNECTIONS];

    memset(&clients, 0x00, sizeof(clients));

    /* Do the initial initialization of the bus */
    mlan=mlanReinit(mlan);

    /* Loop forever */
    for(;;) {
        fd_set rfdset, wfdset, efdset;
        struct timeval tv;
        int highestFd=serverSocket;

        FD_ZERO(&rfdset);
        FD_ZERO(&wfdset);
        FD_ZERO(&efdset);
        /* Listen to the server socket */
        if(serverSocket > 0) {
            FD_SET(serverSocket, &rfdset);
        }
        /* Check for any client sockets that need to be added */
        for(i=0; i<MAX_CONNECTIONS; i++) {
            if(clients[i] != NULL) {
                if(clients[i]->s > highestFd) {
                    highestFd=clients[i]->s;
                }
                /* Always queue for reading and exception */
                if(clients[i]->connState == FINISHED) {
                    closeConnection(clients[i]);
                    free(clients[i]);
                    clients[i]=NULL;
                } else {
                    if(clients[i]->connState == CONNECTED) {
                        FD_SET(clients[i]->s, &rfdset);
                        FD_SET(clients[i]->s, &efdset);
                    }
                    /* If this client has nothing to write, let it go, else
                       queue the write */
                    if(clients[i]->msgs == NULL) {
                        if(clients[i]->connState == QUITTING) {
                            /* Nothing to say, nothing to hear...shut down */
                            clients[i]->connState=FINISHED;
                        }
                    } else {
                        /* We're interested in writing to this fd */
                        FD_SET(clients[i]->s, &wfdset);
                    }
                } /* Not in the finished state */
            } /* Examining a valid connection */
        } /* Examining all known connections */

        tv.tv_sec=1;
        tv.tv_usec=0;

        selected=select(highestFd+1, &rfdset, &wfdset, &efdset, &tv);
        thisTime=time(NULL);

        if(selected > 0) {
            if(FD_ISSET(serverSocket, &rfdset)) {
                int client=0;
                struct sockaddr_in fsin;
                socklen_t fromlen=sizeof(fsin);

                client=accept(serverSocket, (struct sockaddr *)&fsin, &fromlen);
                if(client < 0) {
                    perror("accept");
                } else {
                    clients[client]=calloc(1, sizeof(struct client));
                    assert(clients[client]);
                    clients[client]->s=client;
                    handleNewConnection(clients[client], fsin);
                    log_info("Accepted a connection on %d from %s\n",
                             client, clients[client]->ip);
                }
            }

            /* OK, look for reads */
            for(i=0; i<highestFd+1; i++) {
                if(i != serverSocket && FD_ISSET(i, &rfdset)) {
                    assert(clients[i]);
                    if(handleRead(mlan, clients[i]) == CLIENT_DONE) {
                        clients[i]->connState=QUITTING;
                    }
                } if(FD_ISSET(i, &wfdset)) {
                    handleWrite(clients[i]);
                } if(FD_ISSET(i, &efdset)) {
                    clients[i]->connState=QUITTING;
                }
            }
        }

        /* If it's been at least a second, check the bus */
        if(thisTime != lastTime) {
            /* Look over the bus, see if there's any news there */
            if(mlan != NULL && need_to_reinit == 0) {
                failures=busLoop(mlan);
            }

            /* Write the log */
            fflush(logfile);

            if(failures>0 || need_to_reinit != 0) {
                /* Wait five seconds before reopening the device. */
                log_error("There were failures, reinitializing.\n");
                mlan=mlanReinit(mlan);
            }
            lastTime=thisTime;
        }
        if(need_to_rotate) {
            rotateLogs();
        }
    }
    /* NOT REACHED */
}
/*}}}*/

/* Main/usage/etc... {{{*/
static void
usage(const char *name)
{
    fprintf(stderr, "Usage:  %s -b busdev -l logfile -c logdir "
            "[-m multigroup] [-p multiport] [-t multittl] [-s serverPort]\n", name);
    fprintf(stderr, "  busdev - serial device containing the bus to poll\n");
    fprintf(stderr, "  logfile - file to write log entries\n");
    fprintf(stderr, "  logdir - directory to write indivual snapshots\n");
    fprintf(stderr, "  multigroup - multicast group to announce readings\n");
    fprintf(stderr, "  multiport - port for sending multicast [1313]\n");
    fprintf(stderr, "  multittl - multicast TTL [0]\n");
    fprintf(stderr, "  serverPort - port for TCP server [-1]\n"
            "By default, if no serverPort is specified, there will be no "
            "server listening.\n"
            "Likewise, if no multicast group is defined, there will be "
            "no multicast\n"
            "announcements.\n");
    exit(1);
}

static void
getoptions(int argc, char **argv)
{
    int c;
    extern char *optarg;
    char *multigroup=NULL;
    int multiport=1313;
    int multittl=0;
    int serverPort=0;

    while( (c=getopt(argc, argv, "b:l:c:m:p:t:s:")) != -1) {
        switch(c) {
        case 'b':
            busdev=optarg;
            break;
        case 'l':
            logfilename=optarg;
            break;
        case 'c':
            curdir=optarg;
            break;
        case 'm':
            multigroup=optarg;
            break;
        case 'p':
            multiport=atoi(optarg);
            break;
        case 't':
            multittl=atoi(optarg);
            break;
        case 's':
            serverPort=atoi(optarg);
            break;
        case '?':
            usage(argv[0]);
            break;
        }
    }

    /* Make sure the required options are supplied */
    if(busdev==NULL || logfilename==NULL || curdir==NULL) {
        usage(argv[0]);
    }

    /* If we got multicast config, initialize it */
    if(multigroup!=NULL) {
        initMulti(multigroup, multiport, multittl);
    }

    if(serverPort > 0) {
        serverSocket=getServerSocket(serverPort);
    }
}

/* Main */
int
main(int argc, char **argv)
{
    /* process the arguments */
    getoptions(argc, argv);

    /* Deal with the arguments. */
    logfile = fopen(logfilename, "a");
    assert(logfile);
    /* Set signal handlers */
    setsignals();

    /* initialize the watch list */
    init_from_cur();
    mainLoop();

    /* NOT REACHED */
    return(1);
}
/*}}}*/

/*
 * vim: foldmethod=marker
 */
