/*
 * Protection handlers for MNIB
 * Copyright 2004Pete Rittwage <peter(at)rittwage(dot)com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gcr.h"
#include "prot.h"

BYTE density_map_rapidlok[MAX_TRACKS_1541] = {
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	/*  1 - 10 */
	3, 3, 3, 3, 3, 3, 3, 2, 2, 2,	/* 11 - 20 */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	/* 21 - 30 */
	2, 2, 2, 2, 2,					/* 31 - 35 */
	0, 2, 2, 2, 2, 2, 2				/* 36 - 42  */
};

void
shift_buffer(BYTE * buffer, int length, int n)
{
	int i;
	BYTE tempbuf[NIB_TRACK_LENGTH];
	BYTE carry;
	int carryshift;

	carryshift = 8 - n;
	memcpy(tempbuf, buffer, length);

	// shift buffer right by n bits
	carry = tempbuf[length - 1];
	for (i = 0; i < length; i++)
	{
		buffer[i] = (tempbuf[i] >> n) | (carry << carryshift);
		carry = tempbuf[i];
	}
}

BYTE *
align_vmax(BYTE * work_buffer, int tracklen)
{
	BYTE *pos, *buffer_end, *start_pos;
	int run;

	/* Try to find V-MAX track marker bytes	*/

	run = 0;
	pos = work_buffer;
	start_pos = work_buffer;
	buffer_end = work_buffer + tracklen;

	while (pos < buffer_end)
	{
		// duplicator's markers
		if ( (*pos == 0x4b) || (*pos == 0x5a) || (*pos == 0x49)  || (*pos == 0xa5))
		{
			if(!run) start_pos = pos;  // mark first byte
			if (run > 5) return (start_pos); // assume this is it
			run++;
		}
		else
			run = 0;

		pos++;
	}
	return (0);
}

BYTE *
align_vmax_new(BYTE * work_buffer, int tracklen)
{
	BYTE *pos, *buffer_end, *key_temp, *key;
	int run, longest;

	run = 0;
	longest = 0;
	pos = work_buffer;
	buffer_end = work_buffer + tracklen;
	key = key_temp = NULL;

	/* try to find longest good gcr run */
	while (pos < buffer_end - 2)
	{
		if ( (*pos == 0x4b) || (*pos == 0x5a) || (*pos == 0x49)  || (*pos == 0xa5) )
		{
			if(run > 5) key_temp = pos - run;
			run++;
		}
		else
		{
			if (run > longest)
			{
				key = key_temp;
				longest = run;
				//gapbyte = *pos;
			}
			run = 0;
		}
		pos++;
	}

	return(key);
}

BYTE *
align_vmax_cw(BYTE * work_buffer, int tracklen)
{
	BYTE *pos, *buffer_end, *start_pos;
	int run;

	run = 0;
	pos = work_buffer;
	start_pos = work_buffer;
	buffer_end = work_buffer + tracklen;

	/* Cinemaware titles have a marker $64 $a5 $a5 $a5 */

	while (pos < buffer_end - 3)
	{
		// duplicator's markers
		if ( (*pos == 0x64) && (*(pos+1) == 0xa5) && (*(pos+2) == 0xa5) && (*(pos+3) == 0xa5) )
			return (pos); // assume this is it
		else
			pos++;
	}

	return (0);
}

// Line up the track cycle to the start of the longest gap mark
// this helps some custom protection tracks master properly
BYTE *
auto_gap(BYTE * work_buffer, int tracklen)
{
	BYTE *pos, *buffer_end, *key_temp, *key;
	int run, longest;

	run = 0;
	longest = 0;
	pos = work_buffer;
	buffer_end = work_buffer + tracklen;
	key = key_temp = NULL;

	/* try to find longest good gcr run */
	while (pos < buffer_end - 2)
	{
		if (*pos == *(pos + 1))	// && (*pos != 0x00 ))
		{
			key_temp = pos + 2;
			run++;
		}
		else
		{
			if (run > longest)
			{
				key = key_temp;
				longest = run;
				//gapbyte = *pos;
			}
			run = 0;
		}
		pos++;
	}

	/* last 5 bytes of gap */
	// printf("gapbyte: %x, len: %d\n",gapbyte,longest);
	if(key >= work_buffer + 5)
		return(key - 5);
	else
		return(key);
}

// The idea behind this is that weak bits commonly occur
// at the ends of tracks when they were mastered.
// we can line up the track cycle to this
// in lieu of no other hints
BYTE *
find_weak_gap(BYTE * work_buffer, int tracklen)
{
	BYTE *pos, *buffer_end, *key_temp, *key;
	int run, longest;

	run = 0;
	longest = 0;
	pos = work_buffer;
	buffer_end = work_buffer + tracklen;
	key = key_temp = NULL;

	/* try to find longest bad gcr run */
	while (pos < buffer_end)
	{
		if (is_bad_gcr(work_buffer, buffer_end - work_buffer,
		  pos - work_buffer))
		{
			// mark next GCR byte
			key_temp = pos + 1;
			run++;
		}
		else
		{
			if (run > longest)
			{
				key = key_temp;
				longest = run;
			}
			run = 0;
		}
		pos++;
	}

	/* first byte after bad run */
	return (key);
}

// Line up the track cycle to the start of the longest sync mark
// this helps some custom protection tracks master properly
BYTE *
find_long_sync(BYTE * work_buffer, int tracklen)
{
	BYTE *pos, *buffer_end, *key_temp, *key;
	int run, longest;

	run = 0;
	longest = 0;
	pos = work_buffer;
	buffer_end = work_buffer + tracklen;
	key = key_temp = NULL;

	/* try to find longest sync run */
	while (pos < buffer_end)
	{
		if (*pos == 0xff)
		{
			if (run == 0)
				key_temp = pos;
			run++;
		}
		else
		{
			if (run > longest)
			{
				key = key_temp;
				longest = run;
			}
			run = 0;
		}
		pos++;
	}

	/* first byte of longest sync run */
	return (key);
}

#include <assert.h>

void fix_first_gcr(BYTE *gcrdata, int length, int pos)
{
    // fix first bad byte in a row
    unsigned int lastbyte, mask, data;
    BYTE dstmask;

    lastbyte = (pos == 0) ? gcrdata[length-1] : gcrdata[pos-1];
    data = ((lastbyte & 0x03) << 8) | gcrdata[pos];

    dstmask = 0x80;
    for (mask = (7 << 7); mask >= 7; mask >>= 1)
    {
        if ((data & mask) == 0)
            break;
        else
            dstmask = (dstmask >> 1) | 0x80;
    }
    assert(mask >= 7);
    gcrdata[pos] &= dstmask;
}


void fix_last_gcr(BYTE *gcrdata, int length, int pos)
{
    // fix last bad byte in a row
    unsigned int lastbyte, mask, data;
    BYTE dstmask;

    lastbyte = (pos == 0) ? gcrdata[length-1] : gcrdata[pos-1];
    data = ((lastbyte & 0x03) << 8) | gcrdata[pos];

    dstmask = 0x00;
    for (mask = 7; mask <= (7 << 7); mask = mask << 1)
    {
        if ((data & mask) == 0)
            break;
        else
            dstmask = (dstmask << 1) | 0x01;
    }
    assert(mask <= (7 << 7));
    gcrdata[pos] &= dstmask;
}
