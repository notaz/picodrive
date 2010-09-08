// Some mp3 related code for Sega/Mega CD.
// Uses the Helix Fixed-point MP3 decoder

// (c) Copyright 2007, Grazvydas "notaz" Ignotas

#include <stdio.h>
#include <string.h>

#include <pico/pico_int.h>
#include <pico/sound/mix.h>
#include "helix/pub/mp3dec.h"
#include "mp3.h"
#include "lprintf.h"

static HMP3Decoder mp3dec = 0;
static FILE *mp3_current_file = NULL;
static int mp3_file_len = 0, mp3_file_pos = 0;
static int mp3_buffer_offs = 0;
static unsigned char mp3_input_buffer[2*1024];

#ifdef __GP2X__
#define mp3_update mp3_update_local
#define mp3_start_play mp3_start_play_local
#endif

static int try_get_header(unsigned char *buff, MP3FrameInfo *fi)
{
	int ret, offs1, offs = 0;

	while (1)
	{
		offs1 = mp3_find_sync_word(buff + offs, 2048 - offs);
		if (offs1 < 0) return -2;
		offs += offs1;
		if (2048 - offs < 4) return -3;

		// printf("trying header %08x\n", *(int *)(buff + offs));

		ret = MP3GetNextFrameInfo(mp3dec, fi, buff + offs);
		if (ret == 0 && fi->bitrate != 0) break;
		offs++;
	}

	return ret;
}

int mp3_get_bitrate(void *f_, int len)
{
	unsigned char buff[2048];
	MP3FrameInfo fi;
	FILE *f = f_;
	int ret;

	memset(buff, 0, sizeof(buff));

	if (mp3dec)
		MP3FreeDecoder(mp3dec);
	mp3dec = MP3InitDecoder();

	fseek(f, 0, SEEK_SET);
	ret = fread(buff, 1, sizeof(buff), f);
	fseek(f, 0, SEEK_SET);
	if (ret <= 0)
		return -1;

	ret = try_get_header(buff, &fi);
	if (ret != 0 || fi.bitrate == 0) {
		// try to read somewhere around the middle
		fseek(f, len>>1, SEEK_SET);
		fread(buff, 1, 2048, f);
		fseek(f, 0, SEEK_SET);
		ret = try_get_header(buff, &fi);
	}
	if (ret != 0)
		return ret;

	// printf("bitrate: %i\n", fi.bitrate / 1000);

	return fi.bitrate / 1000;
}

static int mp3_decode(void)
{
	unsigned char *readPtr;
	int bytesLeft;
	int offset; // mp3 frame offset from readPtr
	int had_err;
	int err = 0;

	do
	{
		if (mp3_file_pos >= mp3_file_len)
			return 1; /* EOF, nothing to do */

		fseek(mp3_current_file, mp3_file_pos, SEEK_SET);
		bytesLeft = fread(mp3_input_buffer, 1, sizeof(mp3_input_buffer), mp3_current_file);

		offset = mp3_find_sync_word(mp3_input_buffer, bytesLeft);
		if (offset < 0) {
			lprintf("find_sync_word (%i/%i) err %i\n", mp3_file_pos, mp3_file_len, offset);
			mp3_file_pos = mp3_file_len;
			return 1; // EOF
		}
		readPtr = mp3_input_buffer + offset;
		bytesLeft -= offset;

		had_err = err;
		err = MP3Decode(mp3dec, &readPtr, &bytesLeft, cdda_out_buffer, 0);
		if (err) {
			if (err == ERR_MP3_MAINDATA_UNDERFLOW && !had_err) {
				// just need another frame
				mp3_file_pos += readPtr - mp3_input_buffer;
				continue;
			}
			if (err == ERR_MP3_INDATA_UNDERFLOW && !had_err) {
				if (offset == 0)
					// something's really wrong here, frame had to fit
					mp3_file_pos = mp3_file_len;
				else
					mp3_file_pos += offset;
				continue;
			}
			if (-12 <= err && err <= -6) {
				// ERR_MP3_INVALID_FRAMEHEADER, ERR_MP3_INVALID_*
				// just try to skip the offending frame..
				mp3_file_pos += offset + 1;
				continue;
			}
			lprintf("MP3Decode err (%i/%i) %i\n", mp3_file_pos, mp3_file_len, err);
			mp3_file_pos = mp3_file_len;
			return 1;
		}
		mp3_file_pos += readPtr - mp3_input_buffer;
	}
	while (0);

	return 0;
}

void mp3_start_play(void *f_, int pos)
{
	FILE *f = f_;

	mp3_file_len = mp3_file_pos = 0;
	mp3_current_file = NULL;
	mp3_buffer_offs = 0;

	if (!(PicoOpt & POPT_EN_MCD_CDDA) || f == NULL) // cdda disabled or no file?
		return;

	// must re-init decoder for new track
	if (mp3dec)
		MP3FreeDecoder(mp3dec);
	mp3dec = MP3InitDecoder();

	mp3_current_file = f;
	fseek(f, 0, SEEK_END);
	mp3_file_len = ftell(f);

	// search for first sync word, skipping stuff like ID3 tags
	while (mp3_file_pos < 128*1024) {
		int offs, bytes;

		fseek(f, mp3_file_pos, SEEK_SET);
		bytes = fread(mp3_input_buffer, 1, sizeof(mp3_input_buffer), f);
		if (bytes < 4)
			break;
		offs = mp3_find_sync_word(mp3_input_buffer, bytes);
		if (offs >= 0) {
			mp3_file_pos += offs;
			break;
		}
		mp3_file_pos += bytes - 2;
	}

	// seek..
	if (pos) {
		unsigned long long pos64 = mp3_file_len - mp3_file_pos;
		pos64 *= pos;
		mp3_file_pos += pos64 >> 10;
	}

	mp3_decode();
}

void mp3_update(int *buffer, int length, int stereo)
{
	int length_mp3, shr = 0;
	void (*mix_samples)(int *dest_buf, short *mp3_buf, int count) = mix_16h_to_32;

	if (mp3_current_file == NULL || mp3_file_pos >= mp3_file_len)
		return; /* no file / EOF */

	length_mp3 = length;
	if (PsndRate <= 11025 + 100) {
		mix_samples = mix_16h_to_32_s2;
		length_mp3 <<= 2; shr = 2;
	}
	else if (PsndRate <= 22050 + 100) {
		mix_samples = mix_16h_to_32_s1;
		length_mp3 <<= 1; shr = 1;
	}

	if (1152 - mp3_buffer_offs >= length_mp3) {
		mix_samples(buffer, cdda_out_buffer + mp3_buffer_offs*2, length<<1);

		mp3_buffer_offs += length_mp3;
	} else {
		int ret, left = 1152 - mp3_buffer_offs;

		mix_samples(buffer, cdda_out_buffer + mp3_buffer_offs*2, (left>>shr)<<1);
		ret = mp3_decode();
		if (ret == 0) {
			mp3_buffer_offs = length_mp3 - left;
			mix_samples(buffer + ((left>>shr)<<1), cdda_out_buffer, (mp3_buffer_offs>>shr)<<1);
		} else
			mp3_buffer_offs = 0;
	}
}

