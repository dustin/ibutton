#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mlan.h>
#include <commands.h>
#include <ds1921.h>

struct ds1921_data getMissionData()
{
	time_t t;
	struct tm tm;
	struct ds1921_data data;

	memset(&data, 0x00, sizeof(data));

	/* Set the time */
	t=time(NULL);
	gmtime_r(&t, &tm);
	data.status.clock.seconds=tm.tm_sec;
	data.status.clock.minutes=tm.tm_min;
	data.status.clock.hours=tm.tm_hour;
	data.status.clock.date=tm.tm_mday;
	data.status.clock.month=tm.tm_mon+1;
	data.status.clock.year=tm.tm_year+1900;
	data.status.clock.day=tm.tm_wday+1;

	/* One minute sample rate */
	data.status.sample_rate=15;

	data.status.mission_delay=5;

	data.status.low_alarm=ftoc(60);
	data.status.high_alarm=ftoc(86);

	/* Various control stuff we want to use in this mission */
	data.status.control=CONTROL_HI_ALARM_ENABLED|CONTROL_LOW_ALARM_ENABLED|
		CONTROL_ROLLOVER_ENABLED;

	return(data);
}

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL, *dev=NULL;

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

	ds1921_mission(mlan, serial, getMissionData());

	printf("Done!\n");

	mlan->destroy(mlan);

	exit(0);
}
