#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <mlan.h>

static float
do1921temp_convert_out(int in)
{
	return( ((float)in/2) - 40);
}

/* Dump a block */
void dumpBlock(uchar *buffer, int size)
{
	int i=0;
	for(i=0; i<size; i++) {
		printf("%02X ", buffer[size]);
		if(i>0 && i%25==0) {
			puts("");
		}
	}
	puts("");
}

/* return the number of bytes read */
int
getBlock(MLan *mlan, uchar *serial, int page, uchar *buffer)
{
	uchar send_block[1024];
	int header=0;
	int length=0, send_cnt=0, atmp=0;
	int i=0, j=0;

	assert(mlan);
	assert(serial);
	assert(buffer);

	/* Clear out the send set */
	for(i=0; i<sizeof(send_block); i++) {
		send_block[i]=0x00;
	}

	/* Access the bus */
	if(!mlan->access(mlan, serial)) {
		printf("Error reading from DS1920\n");
		exit(-1);
	}
	/* OK, define the command (read from the beginning ) */
	send_block[send_cnt++] = 0x55;
	for(i=0; i<8; i++)
		send_block[send_cnt++] = serial[i];
	send_block[send_cnt++] = 0xF0;
	/* Calculate the position of the page...each page is 0x20 bytes, and
	 * the second byte goes first. */
	atmp=page*0x20;
	send_block[send_cnt++] = atmp & 0xff;
	send_block[send_cnt++] = atmp>>8;
	printf("Fetching from page %d (0x%04x) (address %02x%02x)\n",
		page, atmp, send_block[send_cnt-2], send_block[send_cnt-1]);
	header=send_cnt;
	for (i = 0; i < 32; i++)
		send_block[send_cnt++]=0xff;
	/* send the block */
	if(! (mlan->block(mlan, TRUE, send_block, send_cnt)) ) {
		printf("Error sending request for block!\n");
		exit(-1);
	}

	/* Find the length */
	length=send_block[header];

	/* Copy it into the return buffer */
	for(j=0, i=header; i<header+length; i++) {
		buffer[j++]=send_block[i];
	}

	return(length);
}

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	uchar buffer[1024];
	char *serial_in=NULL, *dev=NULL;
	int i=0, length=0;

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
	length=getBlock(mlan, serial, 16, buffer);
	printf("Register data:\n");
	dumpBlock(buffer, length);

	/* Histogram is at 64 */
	length=getBlock(mlan, serial, 64, buffer);
	printf("Histogram data:\n");
	dumpBlock(buffer, length);

	/* 128 is the temperature samples */
	length=getBlock(mlan, serial, 128, buffer);
	printf("Data read:\n");
	for(i=0; i<length; i++) {
		float temp=ctof(do1921temp_convert_out(buffer[i]));
		if(temp>-40.00) {
			printf("Temperature:  %.2f\n", temp);
		}
	}

	printf("Done!\n");

	mlan->destroy(mlan);
	return(0);
}
