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
#include "../gp2x/code940/940shared.h"
#include "../gp2x/helix/pub/mp3dec.h"
#include "../../Pico/PicoInt.h"


static YM2612 ym2612;

YM2612 *ym2612_940 = &ym2612;

// static _940_data_t  shared_data_;
static _940_ctl_t   shared_ctl_;
// static _940_data_t *shared_data = &shared_data_;
_940_ctl_t  *shared_ctl = &shared_ctl_;

unsigned char *mp3_mem = 0;

#define MP3_SIZE_MAX (0x1000000 - 4*640*480)

/***********************************************************/


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


void sharedmem_init(void)
{
	mp3_mem = malloc(MP3_SIZE_MAX);
}

void sharedmem_deinit(void)
{
	free(mp3_mem);
}

void YM2612Init_940(int baseclock, int rate)
{
	YM2612Init_(baseclock, rate);
}


void YM2612ResetChip_940(void)
{
	YM2612ResetChip_();
}


#if 0
static void local_decode(void)
{
	int mp3_offs = shared_ctl->mp3_offs;
	unsigned char *readPtr = mp3_mem + mp3_offs;
	int bytesLeft = shared_ctl->mp3_len - mp3_offs;
	int offset; // frame offset from readPtr
	int err = 0;

	if (bytesLeft <= 0) return; // EOF, nothing to do

	offset = MP3FindSyncWord(readPtr, bytesLeft);
	if (offset < 0) {
		shared_ctl->mp3_offs = shared_ctl->mp3_len;
		return; // EOF
	}
	readPtr += offset;
	bytesLeft -= offset;

	err = MP3Decode(shared_data->mp3dec, &readPtr, &bytesLeft,
			shared_data->mp3_buffer[shared_ctl->mp3_buffsel], 0);
	if (err) {
		if (err == ERR_MP3_INDATA_UNDERFLOW) {
			shared_ctl->mp3_offs = shared_ctl->mp3_len; // EOF
			return;
		} else if (err <= -6 && err >= -12) {
			// ERR_MP3_INVALID_FRAMEHEADER, ERR_MP3_INVALID_*
			// just try to skip the offending frame..
			readPtr++;
		}
		shared_ctl->mp3_errors++;
		shared_ctl->mp3_lasterr = err;
	}
	shared_ctl->mp3_offs = readPtr - mp3_mem;
}
#endif




static FILE *loaded_mp3 = 0;

int YM2612UpdateOne_940(int *buffer, int length, int stereo, int is_buf_empty)
{
#if 0
	int cdda_on, *ym_buffer = mix_buffer;
	static int mp3_samples_ready = 0, mp3_buffer_offs = 0;
	static int mp3_play_bufsel = 1;


	YM2612UpdateOne_(buffer, length, stereo); // really writes to mix_buffer

	// emulatind MCD, not data track, CDC is reading, playback was started, track not ended
	cdda_on = (PicoMCD & 1) && !(Pico_mcd->s68k_regs[0x36] & 1) && (Pico_mcd->scd.Status_CDC & 1)
			&& loaded_mp3 && shared_ctl->mp3_offs < shared_ctl->mp3_len;

	/* mix data from previous go */
	if (cdda_on && mp3_samples_ready >= length)
	{
		if (1152 - mp3_buffer_offs >= length) {
			mix_samples(buffer, ym_buffer, shared_data->mp3_buffer[mp3_play_bufsel] + mp3_buffer_offs*2, length, stereo);

			mp3_buffer_offs += length;
		} else {
			// collect from both buffers..
			int left = 1152 - mp3_buffer_offs;
			mix_samples(buffer, ym_buffer, shared_data->mp3_buffer[mp3_play_bufsel] + mp3_buffer_offs*2, left, stereo);
			mp3_play_bufsel ^= 1;
			mp3_buffer_offs = length - left;
			mix_samples(buffer + left * 2, ym_buffer + left * 2,
				shared_data->mp3_buffer[mp3_play_bufsel], mp3_buffer_offs, stereo);
		}
		mp3_samples_ready -= length;
	} else {
		mix_samples(buffer, ym_buffer, 0, length, stereo);
	}

	// make sure we will have enough mp3 samples next frame
	if (cdda_on && mp3_samples_ready < length)
	{
		shared_ctl->mp3_buffsel ^= 1;
		local_decode();
		mp3_samples_ready += 1152;
	}
#else
	return YM2612UpdateOne_(buffer, length, stereo, is_buf_empty);
#endif
}


void mp3_update(int *buffer, int length, int stereo)
{
	// nothing..
}


/***********************************************************/

void mp3_start_play(FILE *f, int pos) // pos is 0-1023
{
	int byte_offs = 0;

	if (loaded_mp3 != f)
	{
		printf("loading mp3... "); fflush(stdout);
		fseek(f, 0, SEEK_SET);
		fread(mp3_mem, 1, MP3_SIZE_MAX, f);
		if (feof(f)) printf("done.\n");
		else printf("done. mp3 too large, not all data loaded.\n");
		shared_ctl->mp3_len = ftell(f);
		loaded_mp3 = f;
	}

	// seek..
	if (pos) {
		byte_offs  = (shared_ctl->mp3_len << 6) >> 10;
		byte_offs *= pos;
		byte_offs >>= 6;
	}
	printf("mp3 pos1024: %i, byte_offs %i/%i\n", pos, byte_offs, shared_ctl->mp3_len);

	shared_ctl->mp3_offs = byte_offs;
}


int mp3_get_offset(void)
{
	return 0;
}


