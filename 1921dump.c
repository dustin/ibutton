#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mlan.h>
#include <commands.h>
#include <ds1921.h>

void rawStatus(int status)
{
	printf("status_temp_core_busy=%d\n",
		0!=(status&STATUS_TEMP_CORE_BUSY));
	printf("status_memory_cleared=%d\n",
		0!=(status&STATUS_MEMORY_CLEARED));
	printf("status_mission_in_progress=%d\n",
		0!=(status&STATUS_MISSION_IN_PROGRESS));
	printf("status_sample_in_progress=%d\n",
		0!=(status&STATUS_SAMPLE_IN_PROGRESS));
	printf("status_low_alarm=%d\n",
		0!=(status&STATUS_LOW_ALARM));
	printf("status_high_alarm=%d\n",
		0!=(status&STATUS_HI_ALARM));
	printf("status_realtime_alarm=%d\n",
		0!=(status&STATUS_REALTIME_ALARM));
}

void rawControl(int control)
{
	printf("control_clock_disabled=%d\n",
		0!=(control&CONTROL_RTC_OSC_DISABLED));
	printf("control_memory_clear_enabled=%d\n",
		0!=(control&CONTROL_MEMORY_CLR_ENABLED));
	printf("control_mission_enabled=%d\n",
		0!=(control&CONTROL_MISSION_ENABLED));
	printf("control_rollover_enabled=%d\n",
		0!=(control&CONTROL_ROLLOVER_ENABLED));
	printf("control_low_alarm_enabled=%d\n",
		0!=(control&CONTROL_LOW_ALARM_ENABLED));
	printf("control_high_alarm_enabled=%d\n",
		0!=(control&CONTROL_HI_ALARM_ENABLED));
	printf("control_timer_alarm_enabled=%d\n",
		0!=(control&CONTROL_TIMER_ALARM_ENABLED));
}

void rawHistogram(int h[])
{
	int i;

	for(i=0; i<HISTOGRAM_SIZE; i++) {
		float temp, temp2;
		temp=ds1921temp_convert_out(i<<2);
		temp2=ds1921temp_convert_out((i+1)<<2) - 0.01;
		printf("histogram_%02d=%.02f, %.02f, %d\n", i, temp, temp2, h[i]);
	}
}

void dumpRaw(struct ds1921_data d)
{
	int i;
	time_t t;

	t=time(NULL);

	printf("# System time:  %s", ctime(&t));
	printf("# DS1921 time:  %s", ctime(&d.status.clock.clock));
	printf("current_time=%d\n", (int)d.status.clock.clock);
	printf("mission_delay=%d\n", d.status.mission_delay);
	printf("mission_start=%d\n", (int)d.status.mission_ts.clock);
	printf("min_sample_time=%d\n",
		(int)d.status.mission_ts.clock+
			(d.status.sample_rate*60*d.samples[0].offset));
	printf("sample_rate=%d\n", d.status.sample_rate);
	rawStatus(d.status.status);
	rawControl(d.status.control);
	printf("low_alarm=%.2f\n", d.status.low_alarm);
	printf("high_alarm=%.2f\n", d.status.high_alarm);
	printf("mission_samples=%d\n", d.status.mission_s_counter);
	printf("device_samples=%d\n", d.status.device_s_counter);

	/* Low alarms */
	for(i=0; i<ALARMSIZE; i++) {
		if(d.low_alarms[i].duration>0) {
			printf("low_alarm_%02d=%d %d\n", i,
				d.low_alarms[i].sample_offset, d.low_alarms[i].duration);
		}
	}
	/* High alarms */
	for(i=0; i<ALARMSIZE; i++) {
		if(d.hi_alarms[i].duration>0) {
			printf("high_alarm_%02d=%d %d\n", i,
				d.hi_alarms[i].sample_offset, d.hi_alarms[i].duration);
		}
	}
	rawHistogram(d.histogram);

	for(i=0; i<d.n_samples; i++) {
		printf("sample_data_%04d=%d, %.02f\n", i, d.samples[i].offset,
			d.samples[i].sample);
	}
}

void usage(char *cmd)
{
	fprintf(stderr, "Usage:  %s [-r] serial_number\n", cmd);
	exit(-1);
}

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL, *dev=NULL;
	struct ds1921_data data;
	int dump_raw=0;
	char *cmd=NULL;
	int ch=-1;

	extern int optind;

	cmd=argv[0];

	/* Go ahead and initialize the timezone stuff */
	tzset();

	/* Parse the arguments */
	while( (ch=getopt(argc, argv, "raw")) != -1) {
		switch(ch) {
			case 'r':
				dump_raw=1;
				break;
			case '?':
				usage(cmd);
				break;
		}
	}

	argc-=optind;
	argv+=optind;

	if(argc<1) {
		usage(cmd);
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

	data=getDS1921Data(mlan, serial);
	if(data.valid) {
		if(dump_raw) {
			dumpRaw(data);
		} else {
			printDS1921(data);
		}
	} else {
		printf("Received no valid data for %s\n", serial_in);
	}

	mlan->destroy(mlan);

#ifdef MYMALLOC
	_mdebug_dump();
#endif /* MYMALLOC */

	exit(0);
}
