#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mlan.h>
#include <commands.h>
#include <ds1920.h>

static void recall(MLan *mlan, uchar *serial)
{
	uchar send_buffer[16];
	int send_cnt=0;
	assert(mlan);
	assert(serial);

	if(!mlan->access(mlan, serial)) {
		fprintf(stderr, "Error accessing device for recall command.\n");
		exit(-1);
	}

	send_buffer[send_cnt++]=DS1920RECALL;
	if(! (mlan->block(mlan, FALSE, send_buffer, send_cnt))) {
		fprintf(stderr, "Error issuing recall command!\n");
		exit(-1);
	}
}

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL, *dev=NULL;
	struct ds1920_data data;

	if(argc<2 || argc==3 || argc>4) {
		fprintf(stderr, "Usage:  %s serial_number [low_alarm high_alarm]\n",
			argv[0]);
		fprintf(stderr, "Temperatures given in celsius.\n");
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

	/* If we have a high and a low, use them. */
	if(argc==4) {
		memset(&data, 0x00, sizeof(data));
		data.temp_low=atof(argv[2]);
		data.temp_hi=atof(argv[3]);
		printf("Setting parameters...");
		fflush(stdout);
		setDS1920Params(mlan, serial, data);
		printf("done.\n");
	}

	/* OK, now let's look it up again */
	recall(mlan, serial);
	data=getDS1920Data(mlan, serial);
	printDS1920(data);

	mlan->destroy(mlan);

	exit(0);
}

/*
 * arch-tag: 1FF35BA8-13E5-11D8-A40F-000393CFE6B8
 */
