#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mlan.h>
#include <commands.h>
#include <ds1920.h>

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL, *dev=NULL;
	struct ds1920_data data;

	if(argc<2) {
		fprintf(stderr, "Need a serial number.\n");
		exit(1);
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

	memset(&data, 0x00, sizeof(data));
	data.temp_low=ftoc(55);
	data.temp_hi=ftoc(88);

	setDS1920Params(mlan, serial, data);

	printf("Done!\n");

	mlan->destroy(mlan);

	exit(0);
}
