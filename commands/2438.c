#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mlan.h>
#include <commands.h>
#include <ds2438.h>

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL;
	struct ds2438_data data;

	if(argc<2) {
		fprintf(stderr, "Usage:  %s serial_number\n", argv[0]);
		exit(1);
	}
	serial_in=argv[1];

	mlan=mlan_init(mlan_get_port(), PARMSET_9600);

	assert(mlan);
	mlan->debug=0;

	mlan->parseSerial(mlan, serial_in, serial);

	if(mlan->ds2480detect(mlan)!=TRUE) {
		printf("Found no DS2480\n");
		exit(-1);
	}

	data=getDS2438Data(mlan, serial);

	printf("Vdd=%f, Vad=%f, Temp=%f, Humidity=%f%%\n", data.Vdd, data.Vad,
		data.temp, data.humidity);

	mlan->destroy(mlan);

	exit(0);
}

/*
 * arch-tag: 2006DC74-13E5-11D8-9B02-000393CFE6B8
 */
