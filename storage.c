#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mlan.h>
#include <commands.h>
#include <ds1921.h>

#define DATA_SIZE 512
#define START_DATA 3

#define NO_FLAG -1
#define READ_FLAG 1
#define WRITE_FLAG 2
#define DUMP_FLAG 3
#define ERASE_FLAG 4

void putBlock(MLan *mlan, uchar *serial, uchar *buffer,
	int startpage, int pages, int size)
{
	int page=0, j=0, i=0;

	assert(mlan);
	assert(serial);
	assert(buffer);

	/* Make sure the sizes and positions are OK */
	assert(startpage>=0);
	assert(startpage<256);
	assert(size>=0);
	assert(pages>=0);
	assert(pages<256);
	assert(size<=pages*32);

	for(page=startpage; page<pages && j<size+START_DATA; page++) {
		uchar pagedata[32];
		memset(pagedata, 0x00, sizeof(pagedata));
		/* j continues from last time */
		for(i=0;i<sizeof(pagedata);i++) {
			pagedata[i]=buffer[j++];
		}
		if(mlan->writeScratchpad(mlan, serial, page, 32, pagedata)!=TRUE) {
			fprintf(stderr, "Write scratchpad failed.\n");
			exit(-1);
		}
		if(mlan->copyScratchpad(mlan, serial, page, 32) != TRUE) {
			fprintf(stderr, "Copy scratchpad failed.\n");
			exit(-1);
		}
	}
}

void erase(MLan *mlan, uchar *serial)
{
	uchar buffer[DATA_SIZE];

	assert(mlan);
	assert(serial);

	memset(buffer, 0x00, sizeof(buffer));
	putBlock(mlan, serial, buffer, 0, 16, 512);
}

void putFile(MLan *mlan, uchar *serial, char *file)
{
	int fd=-1;
	struct stat st;
	uchar buffer[DATA_SIZE];
	int size;
	int i=0;

	assert(mlan);
	assert(serial);
	assert(file);

	memset(buffer, 0x00, sizeof(buffer));

	if(stat(file, &st)<0) {
		perror(file);
		exit(-1);
	}
	size=st.st_size;
	if(size>(DATA_SIZE-START_DATA)) {
		fprintf(stderr, "You're too big for me (%d bytes)\n", size);
		exit(-1);
	}

	/* LSB first */
	buffer[0]=(size&0xFF);
	buffer[1]=(size>>8);

	fd=open(file, O_RDONLY, 0);
	if(fd<0) {
		perror(file);
		exit(-1);
	}
	/* Read starting at the fourth byte */
	if( read(fd, buffer+START_DATA, size) != size) {
		fprintf(stderr, "Did not read enough data.\n");
		exit(-1);
	}

	/* Calculate the CRC */
	mlan->DOWCRC=buffer[0];
	mlan->dowcrc(mlan, buffer[1]);
	for(i=0; i<size; i++) {
		mlan->dowcrc(mlan, buffer[i+START_DATA]);
	}

	buffer[2]=mlan->DOWCRC;

	putBlock(mlan, serial, buffer, 0, 16, size);
}

void getFile(MLan *mlan, uchar *serial, char *file)
{
	uchar data[DATA_SIZE];
	uchar buffer[33];
	int i=0, j=0, k=0, size=512;
	int fd=-1;
	int crc;

	assert(mlan);
	assert(serial);
	assert(file);

	for(i=0; i<16 && j<size; i++) {

		memset(buffer, 0x00, sizeof(buffer));
		/* Read the page */
		mlan->getBlock(mlan, serial, i, 1, buffer);

		/* Set the size */
		if(i==0) {
			size=(buffer[0])|(buffer[1]<<8);
			crc=buffer[2];
		}

		/* Do the actual copy */
		for(k=0; k<32; k++) {
			data[j++]=buffer[k];
		}
	}

	/* Figure out the size */
	/*
	printf("Read %d bytes (really %d):\n", size, j);
	dumpBlock(data, j);
	*/

	/* Calculate the CRC */
	mlan->DOWCRC=data[0];
	mlan->dowcrc(mlan, data[1]);
	for(i=0; i<size; i++) {
		mlan->dowcrc(mlan, data[i+START_DATA]);
	}

	/* Check out the CRC */
	if(crc!=mlan->DOWCRC) {
		fprintf(stderr, "WARNING: CRCs do not match:  %04X != %04X\n",
			mlan->DOWCRC, crc);
	}

	/* Now write the file out */
	fd=open(file, O_WRONLY|O_CREAT|O_EXCL, 0644);
	if(fd<0) {
		perror(file);
		exit(-1);
	}
	/* Read starting at the START_DATAth byte */
	if( write(fd, data+START_DATA, size) != size) {
		fprintf(stderr, "Did not write enough data.\n");
		exit(-1);
	}
}

void dumpData(MLan *mlan, uchar *serial)
{
	uchar data[DATA_SIZE];
	uchar buffer[33];
	int i=0, j=0, k=0;

	assert(mlan);
	assert(serial);

	for(i=0; i<16; i++) {

		memset(buffer, 0x00, sizeof(buffer));
		/* Read the page */
		mlan->getBlock(mlan, serial, i, 1, buffer);

		/* Do the actual copy */
		for(k=0; k<32; k++) {
			assert(j<DATA_SIZE);
			data[j++]=buffer[k];
		}
	}

	dumpBlock(data, DATA_SIZE);
}

void usage(char *cmd)
{
	fprintf(stderr, "Usage:  %s -w serial_number in_filename\n", cmd);
	fprintf(stderr, "  -or-  %s -r serial_number out_filename\n", cmd);
	fprintf(stderr, "  -or-  %s -d serial_number\n", cmd);
	fprintf(stderr, "  -or-  %s -e serial_number\n", cmd);
	fprintf(stderr, "Please note that this does *not* store or read files"
		" in a standard format.\n");
	exit(-1);
}

int main(int argc, char **argv)
{
	MLan *mlan=NULL;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL, *dev=NULL;
	int ch;
	int flag=NO_FLAG;
	char *cmd=NULL, *filename=NULL;

	/* save the command */
	cmd=argv[0];

	while( (ch=getopt(argc, argv, "rwde")) != -1) {
		switch(ch) {
			case 'r':
				if(flag!=NO_FLAG) {
					fprintf(stderr, "Flag already given.\n");
					usage(cmd);
				}
				flag=READ_FLAG;
				break;
			case 'w':
				if(flag!=NO_FLAG) {
					fprintf(stderr, "Flag already given.\n");
					usage(cmd);
				}
				flag=WRITE_FLAG;
				break;
			case 'd':
				if(flag!=NO_FLAG) {
					fprintf(stderr, "Flag already given.\n");
					usage(cmd);
				}
				flag=DUMP_FLAG;
				break;
			case 'e':
				if(flag!=NO_FLAG) {
					fprintf(stderr, "Flag already given.\n");
					usage(cmd);
				}
				flag=ERASE_FLAG;
				break;
			case '?':
				usage(cmd);
		}
	}

	/* Adjust the argv and argc */
	argc-=optind;
	argv+=optind;

	if(flag==NO_FLAG) {
		fprintf(stderr, "No flag given given.\n");
		usage(cmd);
	}

	if(argc<1) {
		fprintf(stderr, "Need a serial number.\n");
		exit(1);
	}
	serial_in=argv[0];

	/* Get the right stuff if we're doing a read or write operation */
	if(flag == READ_FLAG || flag == WRITE_FLAG) {
		if(argc<2) {
			fprintf(stderr, "Need a filename.\n");
			usage(cmd);
		}
		filename=argv[1];
	}

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
		fprintf(stderr, "Found no DS2480\n");
		exit(-1);
	}

	/* Was there a file argument? */
	if(flag==WRITE_FLAG) {
		printf("Writing...");
		fflush(stdout);
		putFile(mlan, serial, filename);
		printf("done.\n");
	} else if(flag==READ_FLAG) {
		printf("Reading...");
		fflush(stdout);
		getFile(mlan, serial, filename);
		printf("done.\n");
	} else if(flag==DUMP_FLAG) {
		dumpData(mlan, serial);
	} else if(flag==ERASE_FLAG) {
		printf("Erasing...");
		fflush(stdout);
		erase(mlan, serial);
		printf("done.\n");
	} else {
		printf("How did you get here?\n");
	}

	mlan->destroy(mlan);

	exit(0);
}
