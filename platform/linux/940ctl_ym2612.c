/* faked 940 code just uses local copy of ym2612 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include "../../Pico/sound/ym2612.h"
#include "../gp2x/gp2x.h"
#include "../gp2x/emu.h"
#include "../gp2x/menu.h"


static YM2612 ym2612;

YM2612 *ym2612_940 = &ym2612;
int  mix_buffer_[44100/50*2];	/* this is where the YM2612 samples will be mixed to */
int *mix_buffer = mix_buffer_;


/***********************************************************/

#define MAXOUT		(+32767)
#define MINOUT		(-32768)

/* limitter */
#define Limit(val, max,min) { \
	if ( val > max )      val = max; \
	else if ( val < min ) val = min; \
}


int YM2612Write_940(unsigned int a, unsigned int v)
{
	YM2612Write_(a, v);

	return 0; // cause the engine to do updates once per frame only
}

UINT8 YM2612Read_940(void)
{
	return YM2612Read_();
}


int YM2612PicoTick_940(int n)
{
	YM2612PicoTick_(n);

	return 0;
}


void YM2612PicoStateLoad_940(void)
{
	int i;

	YM2612PicoStateLoad_();

	for(i = 0; i < 0x100; i++) {
		YM2612Write_(0, i);
		YM2612Write_(1, ym2612.REGS[i]);
	}
	for(i = 0; i < 0x100; i++) {
		YM2612Write_(2, i);
		YM2612Write_(3, ym2612.REGS[i|0x100]);
	}
}


void YM2612Init_940(int baseclock, int rate)
{
	YM2612Init_(baseclock, rate);
}


void YM2612ResetChip_940(void)
{
	YM2612ResetChip_();
}


void YM2612UpdateOne_940(short *buffer, int length, int stereo)
{
	int i;

	YM2612UpdateOne_(buffer, length, stereo);

	/* mix data */
	if (stereo) {
		int *mb = mix_buffer;
		for (i = length; i > 0; i--) {
			int l, r;
			l = r = *buffer;
			l += *mb++, r += *mb++;
			Limit( l, MAXOUT, MINOUT );
			Limit( r, MAXOUT, MINOUT );
			*buffer++ = l; *buffer++ = r;
		}
	} else {
		for (i = 0; i < length; i++) {
			int l = mix_buffer[i];
			l += buffer[i];
			Limit( l, MAXOUT, MINOUT );
			buffer[i] = l;
		}
	}
}

