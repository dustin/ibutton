#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <mlan.h>

int main(int argc, char **argv)
{
	MLan *mlan;
	uchar sn[MLAN_SERIAL_SIZE];
	int slews[]={
		PARMSET_Slew0p55Vus, PARMSET_Slew0p7Vus, PARMSET_Slew0p83Vus,
		PARMSET_Slew1p1Vus, PARMSET_Slew1p37Vus, PARMSET_Slew1p65Vus,
		PARMSET_Slew2p2Vus, PARMSET_Slew15Vus
	};
	char *slew_names[]={
		"PARMSET_Slew0p55Vus", "PARMSET_Slew0p7Vus", "PARMSET_Slew0p83Vus",
		"PARMSET_Slew1p1Vus", "PARMSET_Slew1p37Vus", "PARMSET_Slew1p65Vus",
		"PARMSET_Slew2p2Vus", "PARMSET_Slew2p2Vus"
	};
	int i=0, rslt=0;
	char *s;

	mlan=mlan_init("/dev/tty00", PARMSET_9600);
	assert(mlan);
	mlan->debug=0;

	for(i=0; i<sizeof(slews); i++) {
		if(mlan->ds2480detect(mlan)==TRUE) {
			printf("Found DS2480 at %s\n", slew_names[i]);
		}
		rslt=mlan->first(mlan, TRUE, FALSE);
		while(rslt) {
			mlan->copySerial(mlan, sn);
			/* See if there's a DS1920 on the bus */
			if(sn[0] == 0x10) {
				break;
			}
		}
		if( (s=get_sample(mlan, sn)) != NULL) {
			printf("Sample was %s\n", s);
			break;
		}
	}

	mlan->destroy(mlan);
	return(0);
}
