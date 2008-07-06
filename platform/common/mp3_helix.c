// Some mp3 related code for Sega/Mega CD.
// Uses the Helix Fixed-point MP3 decoder

// (c) Copyright 2007, Grazvydas "notaz" Ignotas

#include <stdio.h>
#include <string.h>

#include "../../Pico/PicoInt.h"
#include "../../Pico/sound/mix.h"
#include "helix/pub/mp3dec.h"
#include "lprintf.h"

static HMP3Decoder mp3dec = 0;
static int mp3_buffer_offs = 0;


static int find_sync_word(unsigned char *buf, int nBytes)
{
	unsigned char *p, *pe;

	/* find byte-aligned syncword - need 12 (MPEG 1,2) or 11 (MPEG 2.5) matching bits */
	for (p = buf, pe = buf + nBytes - 4; p < pe; p++)
	{
		int pn;
		if (p[0] != 0xff) continue;
		pn = p[1];
		if ((pn & 0xf8) != 0xf8 || // currently must be MPEG1
		    (pn & 6) == 0) {       // invalid layer
			p++; continue;
		}
		pn = p[2];
		if ((pn & 0xf0) < 0x20 || (pn & 0xf0) == 0xf0 || // bitrates
		    (pn & 0x0c) != 0) { // not 44kHz
			continue;
		}

		return p - buf;
	}

	return -1;
}


static int try_get_header(unsigned char *buff, MP3FrameInfo *fi)
{
	int ret, offs1, offs = 0;

	while (1)
	{
		offs1 = find_sync_word(buff + offs, 2048 - offs);
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

int mp3_get_bitrate(FILE *f, int len)
{
	unsigned char buff[2048];
	MP3FrameInfo fi;
	int ret;

	memset(buff, 0, 2048);

	if (mp3dec) MP3FreeDecoder(mp3dec);
	mp3dec = MP3InitDecoder();

	fseek(f, 0, SEEK_SET);
	ret = fread(buff, 1, 2048, f);
	fseek(f, 0, SEEK_SET);
	if (ret <= 0) return -1;

	ret = try_get_header(buff, &fi);
	if (ret != 0 || fi.bitrate == 0) {
		// try to read somewhere around the middle
		fseek(f, len>>1, SEEK_SET);
		fread(buff, 1, 2048, f);
		fseek(f, 0, SEEK_SET);
		ret = try_get_header(buff, &fi);
	}
	if (ret != 0) return ret;

	// printf("bitrate: %i\n", fi.bitrate / 1000);

	return fi.bitrate / 1000;
}


#ifdef __GP2X__

#include "../gp2x/code940/940shared.h"

extern _940_ctl_t *shared_ctl;
extern unsigned char *mp3_mem;

static int mp3_decode(void)
{
	// tried copying this to cached mem, no improvement noticed
	int mp3_offs = shared_ctl->mp3_offs;
	unsigned char *readPtr = mp3_mem + mp3_offs;
	int bytesLeft = shared_ctl->mp3_len - mp3_offs;
	int offset; // frame offset from readPtr
	int retries = 0, err;

	if (bytesLeft <= 0) return 1; // EOF, nothing to do

retry:
	offset = find_sync_word(readPtr, bytesLeft);
	if (offset < 0) {
		shared_ctl->mp3_offs = shared_ctl->mp3_len;
		return 1; // EOF
	}
	readPtr += offset;
	bytesLeft -= offset;

	err = MP3Decode(mp3dec, &readPtr, &bytesLeft, cdda_out_buffer, 0);
	if (err) {
		if (err == ERR_MP3_INDATA_UNDERFLOW) {
			shared_ctl->mp3_offs = shared_ctl->mp3_len; // EOF
			return 1;
		} else if (err <= -6 && err >= -12) {
			// ERR_MP3_INVALID_FRAMEHEADER, ERR_MP3_INVALID_*
			// just try to skip the offending frame..
			readPtr++;
			bytesLeft--;
			if (retries++ < 2) goto retry;
			else lprintf("mp3 decode failed with %i after %i retries\n", err, retries);
		}
		shared_ctl->mp3_errors++;
		shared_ctl->mp3_lasterr = err;
	}
	shared_ctl->mp3_offs = readPtr - mp3_mem;
	return 0;
}

void mp3_start_local(void)
{
	// must re-init decoder for new track
	if (mp3dec) MP3FreeDecoder(mp3dec);
	mp3dec = MP3InitDecoder();

	mp3_buffer_offs = 0;
	mp3_decode();
}

#define mp3_update mp3_update_local

#else // !__GP2X__

static FILE *mp3_current_file = NULL;
static int mp3_file_len = 0, mp3_file_pos = 0;
static unsigned char mp3_input_buffer[2*1024];

static int mp3_decode(void)
{
	unsigned char *readPtr;
	int bytesLeft;
	int offset; // mp3 frame offset from readPtr
	int err;

	do
	{
		if (mp3_file_pos >= mp3_file_len) return 1; // EOF, nothing to do

		fseek(mp3_current_file, mp3_file_pos, SEEK_SET);
		bytesLeft = fread(mp3_input_buffer, 1, sizeof(mp3_input_buffer), mp3_current_file);

		offset = find_sync_word(mp3_input_buffer, bytesLeft);
		if (offset < 0) {
			//lprintf("find_sync_word (%i/%i) err %i\n", mp3_file_pos, mp3_file_len, offset);
			mp3_file_pos = mp3_file_len;
			return 1; // EOF
		}
		readPtr = mp3_input_buffer + offset;
		bytesLeft -= offset;

		err = MP3Decode(mp3dec, &readPtr, &bytesLeft, cdda_out_buffer, 0);
		if (err) {
			//lprintf("MP3Decode err (%i/%i) %i\n", mp3_file_pos, mp3_file_len, err);
			if (err == ERR_MP3_INDATA_UNDERFLOW) {
				if (offset == 0)
					// something's really wrong here, frame had to fit
					mp3_file_pos = mp3_file_len;
				else
					mp3_file_pos += offset;
				continue;
			} else if (err <= -6 && err >= -12) {
				// ERR_MP3_INVALID_FRAMEHEADER, ERR_MP3_INVALID_*
				// just try to skip the offending frame..
				mp3_file_pos += offset + 1;
				continue;
			}
			mp3_file_pos = mp3_file_len;
			return 1;
		}
		mp3_file_pos += readPtr - mp3_input_buffer;
	}
	while (0);

	return 0;
}

void mp3_start_play(FILE *f, int pos)
{
	mp3_file_len = mp3_file_pos = 0;
	mp3_current_file = NULL;
	mp3_buffer_offs = 0;

	// must re-init decoder for new track
	if (mp3dec) MP3FreeDecoder(mp3dec);
	mp3dec = MP3InitDecoder();

	if (!(PicoOpt&POPT_EN_MCD_CDDA) || f == NULL) // cdda disabled or no file?
		return;

	//lprintf("mp3_start_play %p %i\n", f, pos);

	mp3_current_file = f;
	fseek(f, 0, SEEK_END);
	mp3_file_len = ftell(f);

	// seek..
	if (pos) {
		mp3_file_pos = (mp3_file_len << 6) >> 10;
		mp3_file_pos *= pos;
		mp3_file_pos >>= 6;
	}

	mp3_decode();
}

int mp3_get_offset(void)
{
	unsigned int offs1024 = 0;
	int cdda_on;

	cdda_on = (PicoAHW & PAHW_MCD) && (PicoOpt&POPT_EN_MCD_CDDA) && !(Pico_mcd->s68k_regs[0x36] & 1) &&
			(Pico_mcd->scd.Status_CDC & 1) && mp3_current_file != NULL;

	if (cdda_on) {
		offs1024  = mp3_file_pos << 7;
		offs1024 /= mp3_file_len >> 3;
	}
	//lprintf("mp3_get_offset offs1024=%u (%i/%i)\n", offs1024, mp3_file_pos, mp3_file_len);

	return offs1024;
}

#endif // ifndef __GP2X__

void mp3_update(int *buffer, int length, int stereo)
{
	int length_mp3, shr = 0;
	void (*mix_samples)(int *dest_buf, short *mp3_buf, int count) = mix_16h_to_32;

#ifndef __GP2X__
	if (mp3_current_file == NULL || mp3_file_pos >= mp3_file_len) return; // no file / EOF
#endif

	length_mp3 = length;
	if (PsndRate == 22050) { mix_samples = mix_16h_to_32_s1; length_mp3 <<= 1; shr = 1; }
	else if (PsndRate == 11025) { mix_samples = mix_16h_to_32_s2; length_mp3 <<= 2; shr = 2; }

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


