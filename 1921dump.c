#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <mlan.h>
#include <commands.h>
#include <ds1921.h>

static float
do1921temp_convert_out(int in)
{
	return( ((float)in/2) - 40);
}

/* Dump a block */
void dumpBlock(uchar *buffer, int size)
{
	int i=0;
	printf("Dumping %d bytes\n", size);
	for(i=0; i<size; i++) {
		printf("%02X ", buffer[i]);
		if(i>0 && i%25==0) {
			puts("");
		}
	}
	puts("");
}

/* Dump a block in binary */
void binDumpBlock(uchar *buffer, int size, int start_addr)
{
	int i=0;
	printf("Dumping %d bytes in binary\n", size);
	for(i=0; i<size; i++) {
		printf("%04X (%02d) ", start_addr+i, i);
		printf("%s", buffer[i]&0x80?"1":"0");
		printf("%s", buffer[i]&0x40?"1":"0");
		printf("%s", buffer[i]&0x20?"1":"0");
		printf("%s", buffer[i]&0x10?"1":"0");
		printf("%s", buffer[i]&0x08?"1":"0");
		printf("%s", buffer[i]&0x04?"1":"0");
		printf("%s", buffer[i]&0x02?"1":"0");
		printf("%s", buffer[i]&0x01?"1":"0");
		puts("");
	}
}

/* This is for the realtime clock */
void getTime1(uchar *buffer, struct ds1921_data *d)
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
void getTime2(uchar *buffer, struct ds1921_data *d)
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

void showStatus(int status)
{
	if(status&0x80) {
		printf("\tTemperature core is busy.\n");
	}
	if(status&0x40) {
		printf("\tMemory has been cleared.\n");
	}
	if(status&0x20) {
		printf("\tMission is in progress.\n");
	}
	if(status&0x10) {
		printf("\tSample is in progress.\n");
	}
	/* Skip, unused
	if(status&0x08) {
		printf("\t.\n");
	}
	*/
	if(status&0x04) {
		printf("\tLow temperature alarm has occurred.\n");
	}
	if(status&0x02) {
		printf("\tHigh temperature alarm has occurred.\n");
	}
	if(status&0x01) {
		printf("\tRealtime alarm has occurred\n");
	}
}

void showControl(int control)
{
	if(control&0x80) {
		printf("\tReal-time clock oscillator is disabled.\n");
	}
	if(control&0x40) {
		printf("\tMemory clear is enabled.\n");
	}
	/* unused
	if(control&0x20) {
		printf("\tMission is in progress.\n");
	}
	*/
	if(control&0x10) {
		printf("\tMission enabled.\n");
	}
	if(control&0x08) {
		printf("\tRollover enabled\n");
	}
	if(control&0x04) {
		printf("\tTemperature low alarm search enabled.\n");
	}
	if(control&0x02) {
		printf("\tTemperature high alarm search enabled.\n");
	}
	if(control&0x01) {
		printf("\tTimer alarm enabled.\n");
	}
}

/* Decode the register stuff from the DS1921 */
struct ds1921_data decodeRegister(uchar *buffer)
{
	struct ds1921_data d;
	memset(&d, 0x00, sizeof(d));

	getTime1(buffer, &d);
	getTime2(buffer+21, &d);

	d.status.low_alarm=do1921temp_convert_out(buffer[11]);
	d.status.high_alarm=do1921temp_convert_out(buffer[12]);
	d.status.control=buffer[14];
	d.status.sample_rate=buffer[13];
	d.status.status=buffer[20];

	d.status.mission_delay=(buffer[19]<<8)+(buffer[18]);

	d.status.mission_s_counter=
		(buffer[28]<<16)|(buffer[27]<<8)|(buffer[26]);
	d.status.device_s_counter=
		(buffer[31]<<16)|(buffer[30]<<8)|(buffer[29]);
	return(d);
}

void showHistogram(int h[])
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
	printf("Mission start time:  %04d/%02d/%02d %02d:%02d:00\n",
		d.status.mission_ts.year, d.status.mission_ts.month,
		d.status.mission_ts.date,
		d.status.mission_ts.hours, d.status.mission_ts.minutes);
	printf("Mission was delayed %d minutes.\n", d.status.mission_delay);
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
			printf("\tSample %d is %.2ff (%.2fc)\n", i, temp, d.samples[i]);
		}
	}
}

/* return the number of bytes read */
int
getBlock(MLan *mlan, uchar *serial, int page, int pages, uchar *buffer)
{
	uchar send_block[128];
	int header=0;
	int send_cnt=0, atmp=0;
	int offset=0;
	int i=0, j=0;
	int ret=-1;

	assert(mlan);
	assert(serial);
	assert(buffer);

	do {
		/* Start back at zero */
		send_cnt=0;

		/* Clear out the send set */
		for(i=0; i<sizeof(send_block); i++) {
			send_block[i]=0x00;
		}

		mlan->DOWCRC=0;
		send_block[send_cnt++] = MATCH_ROM;
		for(i=0; i<8; i++)
			send_block[send_cnt++] = serial[i];
		/* Now our command */
		send_block[send_cnt++] = READ_MEMORY;
		/* Calculate the position of the page...each page is 0x20 bytes, and
		 * the second byte goes first. */
		atmp=page*0x20;
		send_block[send_cnt++] = atmp & 0xff;
		send_block[send_cnt++] = atmp>>8;
		mlan->dowcrc(mlan, send_block[send_cnt-2]);
		mlan->dowcrc(mlan, send_block[send_cnt-1]);
		header=send_cnt;

		for (i = 0; i < 34; i++)
			send_block[send_cnt++]=0xff;
		/* send the block */
		if(! (mlan->block(mlan, TRUE, send_block, send_cnt)) ) {
			printf("Error sending request for block!\n");
			return(-1);
		}

		for(i=0; i<34; i++) {
			mlan->dowcrc(mlan, send_block[header+i+1]);
		}

		/* Copy it into the return buffer */
		for(j=0, i=header; i<header+32; i++) {
			buffer[offset+j++]=send_block[i];
		}

		offset+=32;
		pages--;
		page++;
	} while(pages>0);
	return(ret);
}

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	uchar buffer[8192];
	char *serial_in=NULL, *dev=NULL;
	int i=0, j=0, pages=0;
	struct ds1921_data data;

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
	}

	/* Register page is at 16 */
	getBlock(mlan, serial, 16, 1, buffer);
	data=decodeRegister(buffer);

	/* Histogram is at 64 */
	getBlock(mlan, serial, 64, 4, buffer);
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
	getBlock(mlan, serial, 128, pages, buffer);
	for(i=0; i<data.status.mission_s_counter; i++) {
		float temp=do1921temp_convert_out(buffer[i]);

		assert(data.n_samples<SAMPLE_SIZE);
		data.samples[data.n_samples++]=temp;
	}

	printDS1921(data);

	printf("Done!\n");

	mlan->destroy(mlan);
	return(0);
}
