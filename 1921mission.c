#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mlan.h>
#include <commands.h>
#include <ds1921.h>

static void showUsage(char *cmd)
{
	fprintf(stderr, "Usage:  %s [-r] [-s sample_rate] [-d mission_delay]\n"
		"\t\t[-l low_alert] [-h high_alert] serial_number\n",
			cmd);
	fprintf(stderr, "Temperature is given in degrees celsius.\n");
	exit(-1);
}

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL;
	struct ds1921_data data;
	char *tmp, temp;
	char *cmd;

	extern char *optarg;
	extern int optind;
	int ch=0;

	cmd=argv[0];

	/* Prepare data */
	memset(&data, 0x00, sizeof(data));

	/* Defaults */
	data.status.low_alarm=-40; /* Damned cold */
	data.status.high_alarm=85; /* Damned hot */
	data.status.sample_rate=15;

	/* Parse the arguments */
	while( (ch=getopt(argc, argv, "s:d:l:h:r")) != -1) {
		switch(ch) {
			case 'r':
				/* Enable rollover */
				data.status.control|=CONTROL_ROLLOVER_ENABLED;
				break;
			case 's':
				/* Set the sample rate */
				data.status.sample_rate=atoi(optarg);
				if(data.status.sample_rate<1) {
					fprintf(stderr, "Invalid sample rate:  %s\n", optarg);
					showUsage(cmd);
				}
				break;
			case 'd':
				/* Set the delay */
				data.status.mission_delay=atoi(optarg);
				if(data.status.mission_delay<0) {
					fprintf(stderr, "Invalid mission delay:  %s\n", optarg);
					showUsage(cmd);
				}
				break;
			case 'l':
				/* Set the low temperature */
				temp=strtod(optarg, &tmp);
				if(tmp==optarg) {
					fprintf(stderr, "Invalid low temperature:  %s\n", optarg);
					showUsage(cmd);
				}
				data.status.low_alarm=temp;
				data.status.control|=CONTROL_HI_ALARM_ENABLED;
				break;
			case 'h':
				/* Set the low temperature */
				temp=strtod(optarg, &tmp);
				if(tmp==optarg) {
					fprintf(stderr, "Invalid high temperature:  %s\n", optarg);
					showUsage(cmd);
				}
				data.status.high_alarm=temp;
				data.status.control|=CONTROL_LOW_ALARM_ENABLED;
				break;
			case '?':
				showUsage(cmd);
				break;
		}
	}

	/* Adjust */
	argc-=optind;
	argv+=optind;

	/* The serial number should be the first thing in argv now. */
	if(argc<1) {
		fprintf(stderr, "Need a serial number.\n");
		showUsage(cmd);
	}
	serial_in=argv[0];

	mlan=mlan_init(mlan_get_port(), PARMSET_9600);
	assert(mlan);
	mlan->debug=0;

	mlan->parseSerial(mlan, serial_in, serial);

	if(mlan->ds2480detect(mlan)!=TRUE) {
		printf("Found no DS2480\n");
		exit(-1);
	}

	if(ds1921_mission(mlan, serial, data)==TRUE) {
		printf("Missioned!\n");
	} else {
		printf("Error missioning.\n");
	}

	mlan->destroy(mlan);

	exit(0);
}

/*
 * arch-tag: 1FFB7570-13E5-11D8-B9F6-000393CFE6B8
 */
