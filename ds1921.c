#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <mlan.h>
#include <commands.h>
#include <ds1921.h>

static float
do1921temp_convert_out(int in)
{
	return( ((float)in/2) - 40);
}

static int
do1921temp_convert_in(float in)
{
	return( (in*2) + 80);
}

/* This is for the realtime clock */
static void getTime1(uchar *buffer, struct ds1921_data *d)
{
	int seconds=0, minutes=0, hours=0, day=0, date=0, month=0, year=0;

	assert(buffer);
	assert(d);

	/* Minutes and seconds are calculated the same way, and are the second
	 * and first bytes respectively. */
	seconds= (buffer[0]&0x0f) + (10* ((buffer[0]&0x70) >> 4) );
	minutes=(buffer[1]&0x0f) + (10* ((buffer[1]&0x70) >> 4) );
	/* Hours is in the third byte */
	hours=(buffer[2]&0x0f) + (10* ( (buffer[2]&0x10) >>4));
	/* Day of the week is the last three bits of the fourth byte */
	day=buffer[3]&0x07;
	/* Date is the fifth byte */
	date= (buffer[4]&0x0f) + (10* ((buffer[4]&0x50) >> 4) );
	/* Month is the sixth byte */
	month=(buffer[5]&0x0f) + (10* ((buffer[5]&0x10) >> 4) );

	/* Figure out whether we're in 2000 or not */
	year=((buffer[5]&0x80)>>7) * 100;
	/* 1900 + century + the stuff in the seventh byte = year */
	year+=1900+(buffer[6]&0x0f) + (10* ( (buffer[6]&0xf0) >> 4) );

	/* Stick it in the structure */
	d->status.clock.year=year;
	d->status.clock.month=month;
	d->status.clock.date=date;
	d->status.clock.day=day;
	d->status.clock.hours=hours;
	d->status.clock.minutes=minutes;
	d->status.clock.seconds=seconds;
}

/* this is for the mission timestamp */
static void getTime2(uchar *buffer, struct ds1921_data *d)
{
	int minutes=0, hours=0, date=0, month=0, year=0;

	assert(buffer);
	assert(d);

	minutes=(buffer[0]&0x0f) + (10* ((buffer[0]&0x70) >> 4) );
	hours=(buffer[1]&0x0f) + (10* ( (buffer[1]&0x10) >>4));
	date= (buffer[2]&0x0f) + (10* ((buffer[2]&0x50) >> 4) );
	month=(buffer[3]&0x0f) + (10* ((buffer[3]&0x10) >> 4) );
	year+=1900+(buffer[4]&0x0f) + (10* ( (buffer[4]&0xf0) >> 4) );
	/* yes to kia */
	if(year<1970) {
		year+=100;
	}

	/* Stick it in the structure */
	d->status.mission_ts.year=year;
	d->status.mission_ts.month=month;
	d->status.mission_ts.date=date;
	d->status.mission_ts.hours=hours;
	d->status.mission_ts.minutes=minutes;
}

static void showStatus(int status)
{
	if(status&STATUS_TEMP_CORE_BUSY) {
		printf("\tTemperature core is busy.\n");
	}
	if(status&STATUS_MEMORY_CLEARED) {
		printf("\tMemory has been cleared.\n");
	}
	if(status&STATUS_MISSION_IN_PROGRESS) {
		printf("\tMission is in progress.\n");
	}
	if(status&STATUS_SAMPLE_IN_PROGRESS) {
		printf("\tSample is in progress.\n");
	}
	/* Skip, unused
	if(status&STATUS_UNUSED) {
		printf("\t.\n");
	}
	*/
	if(status&STATUS_LOW_ALARM) {
		printf("\tLow temperature alarm has occurred.\n");
	}
	if(status&STATUS_HI_ALARM) {
		printf("\tHigh temperature alarm has occurred.\n");
	}
	if(status&STATUS_REALTIME_ALARM) {
		printf("\tRealtime alarm has occurred\n");
	}
}

static void showControl(int control)
{
	if(control&CONTROL_RTC_OSC_DISABLED) {
		printf("\tReal-time clock oscillator is disabled.\n");
	}
	if(control&CONTROL_MEMORY_CLR_ENABLED) {
		printf("\tMemory clear is enabled.\n");
	}
	/* unused
	if(control&CONTROL_UNUSED) {
		printf("\tMission is in progress.\n");
	}
	*/
	if(control&CONTROL_MISSION_ENABLED) {
		printf("\tMission enabled.\n");
	}
	if(control&CONTROL_ROLLOVER_ENABLED) {
		printf("\tRollover enabled\n");
	}
	if(control&CONTROL_LOW_ALARM_ENABLED) {
		printf("\tTemperature low alarm search enabled.\n");
	}
	if(control&CONTROL_HI_ALARM_ENABLED) {
		printf("\tTemperature high alarm search enabled.\n");
	}
	if(control&CONTROL_TIMER_ALARM_ENABLED) {
		printf("\tTimer alarm enabled.\n");
	}
}

/* Decode the register stuff from the DS1921 */
static void decodeRegister(uchar *buffer, struct ds1921_data *d)
{
	getTime1(buffer, d);
	getTime2(buffer+21, d);

	d->status.low_alarm=do1921temp_convert_out(buffer[11]);
	d->status.high_alarm=do1921temp_convert_out(buffer[12]);
	d->status.control=buffer[14];
	d->status.sample_rate=buffer[13];
	d->status.status=buffer[20];

	d->status.mission_delay=(buffer[19]<<8)|(buffer[18]);

	d->status.mission_s_counter=
		(buffer[28]<<16)|(buffer[27]<<8)|(buffer[26]);
	d->status.device_s_counter=
		(buffer[31]<<16)|(buffer[30]<<8)|(buffer[29]);
}

static void showHistogram(int h[])
{
	int i;

	for(i=0; i<HISTOGRAM_SIZE; i++) {

		float temp, temp2;
		
		temp=do1921temp_convert_out(i<<2);
		temp2=do1921temp_convert_out((i+1)<<2);

		if(h[i]>0) {
			printf("%.2f to %.2f:  %d\n", ctof(temp), ctof(temp2), h[i]);
		}
	}
}

static char *ds1921_sample_time(int i, struct ds1921_data d)
{
	static char result[80];
	time_t t=0;
	struct tm tm;

	memset(&tm, 0x00, sizeof(tm));

	tm.tm_min=d.status.mission_ts.minutes;
	tm.tm_hour=d.status.mission_ts.hours;
	tm.tm_mday=d.status.mission_ts.date;
	tm.tm_mon=d.status.mission_ts.month-1;
	tm.tm_year=d.status.mission_ts.year-1900;

	t=mktime(&tm);

	/* Add the sample_rate times the sample we're on) */
	t+=60*(i*d.status.sample_rate);

	localtime_r(&t, &tm);

	strftime(result, 80, "%F %T", &tm);

	return(result);
}

void printDS1921(struct ds1921_data d)
{
	int i;
	char *days[]={
		NULL, "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday",
			"Friday", "Saturday", NULL
	};
	/* Display what we've got */
	printf("Current time:  %s, %04d/%02d/%02d %02d:%02d:%02d\n",
		days[d.status.clock.day],
		d.status.clock.year, d.status.clock.month, d.status.clock.date,
		d.status.clock.hours, d.status.clock.minutes, d.status.clock.seconds);

	/* If the mission is delayed, display the time to mission, else display
	 * when it started. */
	if(d.status.mission_delay) {
		printf("Mission begins in %d minutes.\n", d.status.mission_delay);
	} else {
		printf("Mission start time:  %04d/%02d/%02d %02d:%02d:00\n",
			d.status.mission_ts.year, d.status.mission_ts.month,
			d.status.mission_ts.date,
			d.status.mission_ts.hours, d.status.mission_ts.minutes);
	}
	printf("Current sample rate is one sample per %d minutes\n",
		d.status.sample_rate);
	printf("Mission status:\n");
	showStatus(d.status.status);
	printf("Mission control info:\n");
	showControl(d.status.control);

	printf("Low alarm:  %.2f\n", ctof(d.status.low_alarm));
	printf("High alarm:  %.2f\n", ctof(d.status.high_alarm));

	printf("Mission samples counter:  %d\n", d.status.mission_s_counter);
	printf("Device samples counter:  %d\n", d.status.device_s_counter);

	printf("Histogram:\n");
	showHistogram(d.histogram);

	printf("Temperature samples:\n");
	for(i=0; i<d.n_samples; i++) {
		float temp=ctof(d.samples[i]);
		if(temp>-40.0) {
			printf("\tSample %03d from %s is %.2ff (%.2fc)\n",
				i, ds1921_sample_time(i, d), temp, d.samples[i]);
		}
	}
}

static void getSummary(struct ds1921_data *d)
{
	if(d->status.status&STATUS_MISSION_IN_PROGRESS) {
		sprintf(d->summary, "Mission in progress since "
							"%04d/%02d/%02d %02d:%02d:00",
			d->status.mission_ts.year, d->status.mission_ts.month,
			d->status.mission_ts.date,
			d->status.mission_ts.hours, d->status.mission_ts.minutes);
	} else {
		sprintf(d->summary, "%s", "No mission in progress.");
	}
}

int ds1921_mission(MLan *mlan, uchar *serial, struct ds1921_data data)
{
	uchar buffer[64];
	struct ds1921_data data2;
	int year=0;
	int control=0;

	assert(mlan);
	assert(serial);

	memset(&data2, 0x00, sizeof(data2));
	memset(&buffer, 0x00, sizeof(buffer));

	buffer[0]= ((data.status.clock.seconds/10)<<4)
		| (data.status.clock.seconds%10);
	buffer[1]= ((data.status.clock.minutes/10)<<4)
		| (data.status.clock.minutes%10);
	buffer[2]= ((data.status.clock.hours/10)<<4)
		| (data.status.clock.hours%10);
	buffer[3]=data.status.clock.day;
	buffer[4]= ((data.status.clock.date/10)<<4)
		| (data.status.clock.date%10);
	buffer[5]=((data.status.clock.month/10)<<4)
		| (data.status.clock.month%10);
	/* The year is slightly more tricky */
	year=data.status.clock.year-1900;
	if(year>=100) {
		buffer[5]|=0x80;
		year-=100;
	}
	buffer[6]=((year/10)<<4) | (year%10);

	/* Control data */
	control=data.status.control;
	buffer[14]=control|CONTROL_MISSION_ENABLED
		|CONTROL_MEMORY_CLR_ENABLED;

	buffer[19]=(data.status.mission_delay>>8);
	buffer[18]=(data.status.mission_delay & 0xFF);

	/* Set the sample rate */
	buffer[13]=data.status.sample_rate;

	/* Low threshold */
	buffer[11]=do1921temp_convert_in(data.status.low_alarm);
	buffer[12]=do1921temp_convert_in(data.status.high_alarm);

	/*
	printf("This is the mission data:\n");
	binDumpBlock(buffer, 16, 0x200);
	decodeRegister(buffer, &data2);
	printDS1921(data2);
	*/

	/* Send it on */
	if(mlan->writeScratchpad(mlan, serial, 16, 32, buffer)!=TRUE) {
		printf("Write scratchpad failed.\n");
		return(FALSE);
	}
	if(mlan->copyScratchpad(mlan, serial, 16, 32) != TRUE) {
		printf("Write scratchpad failed.\n");
		return(FALSE);
	}

	/* Clear memory */
	if(mlan->clearMemory(mlan, serial)!=TRUE) {
		printf("Clear memory failed.\n");
		return(FALSE);
	}

	buffer[14]=control|CONTROL_MISSION_ENABLED;

	/* OK, turn it on! */
	if(mlan->writeScratchpad(mlan, serial, 16, 32, buffer)!=TRUE) {
		printf("Write scratchpad failed.\n");
		return(FALSE);
	}
	if(mlan->copyScratchpad(mlan, serial, 16, 32) != TRUE) {
		printf("Write scratchpad failed.\n");
		return(FALSE);
	}

	return(TRUE);
}

struct ds1921_data getDS1921Data(MLan *mlan, uchar *serial)
{
	struct ds1921_data data;
	uchar buffer[8192];
	int i=0, j=0, pages=0;

	/* Zero it out */
	memset(&data, 0x00, sizeof(data));
	memset(&buffer, 0x00, sizeof(buffer));

	/* Make sure it's a 1921 */
	assert(serial[0] == 0x21);

	/* Register data is at 16 */
	mlan->getBlock(mlan, serial, 16, 1, buffer);
	decodeRegister(buffer, &data);

	/* Histogram is at 64 */
	mlan->getBlock(mlan, serial, 64, 4, buffer);
	for(j=0, i=0; j<HISTOGRAM_SIZE; i+=2) {
		int n;
		n=buffer[i+1];
		n<<=0;
		n|=buffer[i];
		data.histogram[j++]=buffer[i];
	}

	/* 128 is the temperature samples.  The number of pages my vary based
	 * on the amount of samples this mission has seen.  Let's figure out
	 * how many we should grab... */
	pages=(data.status.mission_s_counter/32)+1;
	if(pages>64)
		pages=64;
	mlan->getBlock(mlan, serial, 128, pages, buffer);
	for(i=0; i<data.status.mission_s_counter; i++) {
		float temp=do1921temp_convert_out(buffer[i]);
		assert(data.n_samples<SAMPLE_SIZE);
		data.samples[data.n_samples++]=temp;
	}

	getSummary(&data);

	return(data);
}
