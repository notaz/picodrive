#include <stdio.h>
#include <string.h>

#include "../../Pico/sound/mix.h"
#include "code940/940shared.h"
#include "helix/pub/mp3dec.h"

static short mp3_out_buffer[2*1152];
static HMP3Decoder mp3dec = 0;
static int mp3_buffer_offs = 0;

extern _940_ctl_t *shared_ctl;
extern unsigned char *mp3_mem;
extern int PsndRate;


static int try_get_header(unsigned char *buff, MP3FrameInfo *fi)
{
	int ret, offs1, offs = 0;

	while (1)
	{
		offs1 = MP3FindSyncWord(buff + offs, 2048 - offs);
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

	if (!mp3dec) mp3dec = MP3InitDecoder();

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


static void mp3_decode(void)
{
	// tried copying this to cached mem, no improvement noticed
	int mp3_offs = shared_ctl->mp3_offs;
	unsigned char *readPtr = mp3_mem + mp3_offs;
	int bytesLeft = shared_ctl->mp3_len - mp3_offs;
	int offset; // frame offset from readPtr
	int err;

	if (bytesLeft <= 0) return; // EOF, nothing to do

	offset = MP3FindSyncWord(readPtr, bytesLeft);
	if (offset < 0) {
		shared_ctl->mp3_offs = shared_ctl->mp3_len;
		return; // EOF
	}
	readPtr += offset;
	bytesLeft -= offset;

	err = MP3Decode(mp3dec, &readPtr, &bytesLeft, mp3_out_buffer, 0);
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


void mp3_update_local(int *buffer, int length, int stereo)
{
	int length_mp3, shr = 0;
	void (*mix_samples)(int *dest_buf, short *mp3_buf, int count) = mix_16h_to_32;

	length_mp3 = length;
	if (PsndRate == 22050) { mix_samples = mix_16h_to_32_s1; length_mp3 <<= 1; shr = 1; }
	else if (PsndRate == 11025) { mix_samples = mix_16h_to_32_s2; length_mp3 <<= 2; shr = 2; }

	if (1152 - mp3_buffer_offs >= length_mp3) {
		mix_samples(buffer, mp3_out_buffer + mp3_buffer_offs*2, length<<1);

		mp3_buffer_offs += length_mp3;
	} else {
		int left = 1152 - mp3_buffer_offs;

		mix_samples(buffer, mp3_out_buffer + mp3_buffer_offs*2, (left>>shr)<<1);
		mp3_decode();
		mp3_buffer_offs = length_mp3 - left;
		mix_samples(buffer + ((left>>shr)<<1), mp3_out_buffer, (mp3_buffer_offs>>shr)<<1);
	}
}


void mp3_start_local(void)
{
	mp3_buffer_offs = 0;
	mp3_decode();
}

