#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <mlan.h>

int main(int argc, char **argv)
{
	MLan *mlan;
	uchar serial[MLAN_SERIAL_SIZE];
	char *serial_in=NULL, *s=NULL;
	int i;

	if(argc<2) {
		fprintf(stderr, "Need a serial number.\n");
		exit(1);
	}
	serial_in=argv[1];

	mlan=mlan_init("/dev/tty00", PARMSET_9600);
	assert(mlan);
	mlan->debug=0;

	mlan->parseSerial(mlan, serial_in, serial);

	if(mlan->ds2480detect(mlan)!=TRUE) {
		printf("Found no DS2480\n");
	}

	for(i=0; i<MLAN_SERIAL_SIZE; i++) {
		printf("%02x ", serial[i]);
	}
	printf("\n");

	if( (s=get_sample(mlan, serial)) != NULL) {
		printf("\tTemperature reading:  %s\n", s);
	}

	mlan->destroy(mlan);
	return(0);
}
