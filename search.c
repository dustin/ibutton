#include <stdio.h>
#include <assert.h>
#include <mlan.h>

int main(int argc, char **argv)
{
	MLan *mlan;
	int i, j, rslt, current=0;
	uchar list[MAX_SERIAL_NUMS][MLAN_SERIAL_SIZE];

	mlan=mlan_init("/dev/tty00", PARMSET_9600);
	assert(mlan);
	/* mlan->debug=5; */

	if(mlan->ds2480detect(mlan)!=TRUE) {
		printf("Found no DS2480\n");
	}

	rslt=mlan->first(mlan, TRUE, FALSE);
	while(rslt) {
		mlan->copySerial(mlan, list[current++]);
		rslt = mlan->next(mlan, TRUE, FALSE);
	}

	for(i=0; i<current; i++) {
		char *what;
		printf("Serial[%d]:  ", i);
		for(j=0; j<MLAN_SERIAL_SIZE; j++) {
			printf("%02x ", list[i][j]);
		}
		what=mlan->serialLookup(mlan, (int)list[i][0]);
		if(what!=NULL) {
			printf("-- %s", what);
		}
		printf("\n");

		/* Sample is abstracted, but the application needs to know what to
		 * do with it, we'll just look at DS1920's here.  */
		if((int)list[i][0]==0x10) {
			float temp;
			if(sample(mlan, list[i], (void *)&temp)) {
				printf("\tTemperature reading:  %2f\n", temp);
			}
		}
	}

	return(0);
}
