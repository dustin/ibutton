#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mlan.h>
#include <commands.h>
#include <ds2406.h>

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL, *dev=NULL;
	int status=0;
	int onoff=0;

	if(argc<2) {
		fprintf(stderr, "Usage:  %s serial_number\n", argv[0]);
		exit(1);
	}
	if(argc>2) {
		onoff=atoi(argv[2]);
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

	setDS2406Switch(mlan, serial, DS2406SwitchA, onoff);

	mlan->destroy(mlan);

	exit(0);
}
