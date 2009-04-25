/*
 * Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include <mlan.h>
#include <commands.h>
#include <ds1920.h>

void
printDS1920(struct ds1920_data d)
{
    printf("Temp:  %.2fc (%.2ff)\n", d.temp, CTOF(d.temp));
    printf("Low alarm:  %.2fc (%.2ff)\n", d.temp_low, CTOF(d.temp_low));
    printf("High alarm:  %.2fc (%.2ff)\n", d.temp_hi, CTOF(d.temp_hi));
}

int
setDS1920Params(MLan *mlan, uchar *serial, struct ds1920_data d)
{
    uchar send_buffer[16];
    int send_cnt=0;
    int i=0;

    assert(mlan);
    assert(serial);

    /* Make sure it's a ds1920 */
    assert(serial[0] == 0x10);

    /* Zero */
    memset(&send_buffer, 0x00, sizeof(send_buffer));

    /* We're going to do this send manually since we can't send to a page. */

    /* Access the device */
    if(!mlan->access(mlan, serial)) {
        mlan_debug(mlan, 1, ("Error reading from DS1920\n"));
        return(FALSE);
    }

    send_buffer[send_cnt++]=DS1920WRITE_SCRACTHPAD;
    if(d.temp_hi>0) {
        send_buffer[send_cnt++]=d.temp_hi;
    } else {
        send_buffer[send_cnt++]=abs(d.temp_hi) | 0x80;
    }
    if(d.temp_low>0) {
        send_buffer[send_cnt++]=d.temp_low;
    } else {
        send_buffer[send_cnt++]=abs(d.temp_low) | 0x80;
    }
    mlan_debug(mlan, 3, ("Writing DS1920 scratchpad.\n"));
    if(! (mlan->block(mlan, FALSE, send_buffer, send_cnt))) {
        printf("Error writing to scratchpad!\n");
        return(FALSE);
    }

    /* OK, request the copy, I do a MATCH_ROM on this one, because it works
     * now.*/
    send_cnt=0;
    send_buffer[send_cnt++] = MATCH_ROM;
    for(i=0; i<MLAN_SERIAL_SIZE; i++) {
        send_buffer[send_cnt++]=serial[i];
    }

    send_buffer[send_cnt++] = DS1920COPY_SCRATCHPAD;
    mlan_debug(mlan, 3, ("Copying DS1920 scratchpad.\n"));
    if(! (mlan->block(mlan, TRUE, send_buffer, send_cnt))) {
        printf("Error copying scratchpad.\n");
        return(FALSE);
    }

    /* Give it some power */
    mlan_debug(mlan, 3, ("Strong pull-up.\n"));
    if(mlan->setlevel(mlan, MODE_STRONG5) != MODE_STRONG5) {
        printf("Strong pull-up failed.\n");
        return(FALSE);
    }

    /* Give it some time to think */
    mlan->msDelay(mlan, 500);

    /* Turn off the hose */
    mlan_debug(mlan, 3, ("Disabling strong pull-up.\n"));
    if(mlan->setlevel(mlan, MODE_NORMAL) != MODE_NORMAL) {
        printf("Disabling strong pull-up failed.\n");
        return(FALSE);
    }
    return(TRUE);
}

/* Get a temperature reading from a DS1920 */
struct ds1920_data
getDS1920Data(MLan *mlan, uchar *serial)
{
    uchar send_block[30], tmpbyte;
    int send_cnt=0, tsht, i;
    float tmp,cr,cpc;
    struct ds1920_data data;

    assert(mlan);
    assert(serial);

    /* Make sure this is a DS1920 */
    assert(serial[0] == 0x10);

    memset(&data, 0x00, sizeof(data));

    if(!mlan->access(mlan, serial)) {
        mlan_debug(mlan, 1, ("Error reading from DS1920\n"));
        return(data);
    }

    tmpbyte=mlan->touchbyte(mlan, CONVERT_TEMPERATURE);
    mlan_debug(mlan, 3, ("Got %02x back from touchbyte\n", tmpbyte));

    if(mlan->setlevel(mlan, MODE_STRONG5) != MODE_STRONG5) {
        mlan_debug(mlan, 1, ("Strong pull-up failed.\n") );
        return(data);
    }

    /* Wait a half a second */
    mlan->msDelay(mlan, 500);

    if(mlan->setlevel(mlan, MODE_NORMAL) != MODE_NORMAL) {
        mlan_debug(mlan, 1, ("Disabling strong pull-up failed.\n") );
        return(data);
    }

    if(!mlan->access(mlan, serial)) {
        mlan_debug(mlan, 1, ("Error reading from DS1920\n"));
        return(data);
    }

    /* Command to read the temperature */
    send_block[send_cnt++] = DS1920READ_SCRATCHPAD;
    for(i=0; i<9; i++) {
        send_block[send_cnt++]=0xff;
    }

    if(! (mlan->block(mlan, FALSE, send_block, send_cnt)) ) {
        mlan_debug(mlan, 1, ("Read scratchpad failed.\n"));
        return(data);
    }

    mlan->DOWCRC=0;
    for(i=send_cnt-9; i<send_cnt; i++)
        mlan->dowcrc(mlan, send_block[i]);

    if(mlan->DOWCRC != 0) {
        mlan_debug(mlan, 1, ("CRC failed\n"));
        return(data);
    }

    /* Store the high and low values */
    data.temp_hi=send_block[3]&0x7F; /* Drop the sign */
    if(send_block[3]&0x80) {
        /* If there was a sign, negate the value */
        data.temp_hi=-data.temp_hi;
    }
    data.temp_low=send_block[4]&0x7F; /* Drop the sign */
    if(send_block[4]&0x80) {
        /* If there was a sign, negate the value */
        data.temp_low=-data.temp_low;
    }

    mlan_debug(mlan, 2, ("TH=%x (%f)\n", send_block[3], data.temp_hi));
    mlan_debug(mlan, 2, ("T=%f\n",  ds1920temp_convert_out(send_block[1])) );
    mlan_debug(mlan, 2, ("TL=%x (%f)\n", send_block[4], data.temp_low));

    /* Calculate the temperature from the scratchpad, get a more accurate
     * reading. */
    tsht = send_block[1];
    if (send_block[2] & 0x01)
        tsht |= -256;
    /* tmp = (float)(tsht/2); */
    tmp=ds1920temp_convert_out(tsht);
    cr = send_block[7];
    cpc = send_block[8];
    if(cpc == 0) {
        mlan_debug(mlan, 1, ("CPC is zero, that sucks\n"));
        return(data);
    } else {
        /* tmp = tmp - (float)0.25 + (cpc - cr)/cpc; */
        tmp = tmp + (cpc - cr)/cpc;
    }
    data.temp=tmp;
    /* The string readings */
    sprintf(data.reading_c, "%.2f", data.temp);
    sprintf(data.reading_f, "%.2f", CTOF(data.temp));
    data.valid=TRUE;
    return(data);
}
