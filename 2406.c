#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mlan.h>
#include <commands.h>
#include <ds2406.h>

static void usage(char *name) {
	fprintf(stderr, "Usage:  %s serial_number [on|off]\n"
		"\tPrint switch status report.  Optional argument "
		"manipulates switch A.\n", name);
	exit(1);
}

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL, *dev=NULL;
	int status=0;
	int onoff=-1;

	if(argc<2) {
		usage(argv[0]);
	}
	if(argc>2) {
		if(strcasecmp(argv[2], "on") == 0) {
			onoff=DS2406_ON;
		} else if(strcasecmp(argv[2], "off") == 0) {
			onoff=DS2406_OFF;
		} else {
			usage(argv[0]);
		}
	}
	serial_in=argv[1];

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

	status=getDS2406Status(mlan, serial);
	printf("Status=%x\n", status);
	printf("Switch A is %s\n", (status&DS2406SwitchA)?"off":"on");
	printf("Switch B is %s\n", (status&DS2406SwitchB)?"off":"on");

	if(onoff == DS2406_OFF || onoff == DS2406_ON) {
		printf("Turning %s %s\n", serial_in, (onoff==DS2406_ON?"on":"off"));
		setDS2406Switch(mlan, serial, DS2406SwitchA, onoff);
	}

	mlan->destroy(mlan);

	exit(0);
}

/*
 * arch-tag: 200018B3-13E5-11D8-9272-000393CFE6B8
 */
