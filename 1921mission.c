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
	fprintf(stderr, "Temperature is given in degrees farenheit.\n");
}

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL, *dev=NULL;
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
	data.status.low_alarm=ftoc(-40);
	data.status.high_alarm=ftoc(185);
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
				data.status.low_alarm=ftoc(temp);
				data.status.control|=CONTROL_HI_ALARM_ENABLED;
				break;
			case 'h':
				/* Set the low temperature */
				temp=strtod(optarg, &tmp);
				if(tmp==optarg) {
					fprintf(stderr, "Invalid high temperature:  %s\n", optarg);
					showUsage(cmd);
				}
				data.status.high_alarm=ftoc(temp);
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
		exit(1);
	}
	serial_in=argv[0];

	if(getenv("MLAN_DEVICE")) {
		dev=getenv("MLAN_DEVICE");
	} else {
		dev="/dev/tty00";
	}
	mlan=mlan_init(dev, PARMSET_9600);
	assert(mlan);
	mlan->debug=0;

	mlan->parseSerial(mlan, serial_in, serial);

	if(mlan->ds2480detect(mlan)!=TRUE) {
		printf("Found no DS2480\n");
		exit(-1);
	}

	ds1921_mission(mlan, serial, data);

	printf("Missioned!\n");

	mlan->destroy(mlan);

	exit(0);
}
