#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <mlan.h>

int main(int argc, char **argv)
{
	MLan *mlan;
	int i, j, rslt, current=0;
	uchar list[MAX_SERIAL_NUMS][MLAN_SERIAL_SIZE];
	char *dev=NULL;
	int only_alarming=FALSE;
	int do_sample=FALSE;
	int ch;

	extern int optind;

	/* Parse the arguments. */
	while( (ch=getopt(argc, argv, "as")) != -1) {
		switch(ch) {
			case 'a':
				only_alarming=TRUE;
				break;
			case 's':
				do_sample=TRUE;
			case '?':
				break;
		}
	}

	argc -= optind;
	argv += optind;

	if(getenv("MLAN_DEVICE")) {
		dev=getenv("MLAN_DEVICE");
	} else {
		dev="/dev/tty00";
	}
	mlan=mlan_init(dev, PARMSET_9600);
	assert(mlan);
	mlan->debug=0;

	if(mlan->ds2480detect(mlan)!=TRUE) {
		printf("Found no DS2480\n");
	}

	rslt=mlan->first(mlan, TRUE, only_alarming);
	while(rslt) {
		mlan->copySerial(mlan, list[current++]);
		rslt = mlan->next(mlan, TRUE, only_alarming);
	}

	for(i=0; i<current; i++) {
		char *what=NULL;
		printf("Serial[%d]:  ", i);
		for(j=0; j<MLAN_SERIAL_SIZE; j++) {
			printf("%02X", list[i][j]);
		}
		what=mlan->serialLookup(mlan, (int)list[i][0]);
		if(what!=NULL) {
			printf(" -- %s", what);
		}
		printf("\n");

		if(do_sample) {
			char *s=NULL;
			if( (s=get_sample(mlan, list[i])) != NULL) {
				printf("\tStatus reading:  %s\n", s);
			}
		}
	}

	return(0);
}
