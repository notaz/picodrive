/*
 * Some mp3 related code for Sega/Mega CD.
 * Uses Libav/FFmpeg libavcodec
 * (C) notaz, 2013
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <libavcodec/avcodec.h>

#include <pico/pico_int.h>
#include "../libpicofe/lprintf.h"
#include "mp3.h"

static AVCodecContext *ctx;

int mp3dec_decode(FILE *f, int *file_pos, int file_len)
{
	unsigned char input_buf[2 * 1024];
	int frame_size;
	AVPacket avpkt;
	int bytes_in;
	int bytes_out;
	int offset;
	int len;

	av_init_packet(&avpkt);

	do
	{
		if (*file_pos >= file_len)
			return 1; // EOF, nothing to do

		fseek(f, *file_pos, SEEK_SET);
		bytes_in = fread(input_buf, 1, sizeof(input_buf), f);

		offset = mp3_find_sync_word(input_buf, bytes_in);
		if (offset < 0) {
			lprintf("find_sync_word (%i/%i) err %i\n",
				*file_pos, file_len, offset);
			*file_pos = file_len;
			return 1; // EOF
		}

		// to avoid being flooded with "incorrect frame size" errors,
		// we must calculate and pass exact frame size - lame
		frame_size = mpeg1_l3_bitrates[input_buf[offset + 2] >> 4];
		frame_size = frame_size * 144000 / 44100;
		frame_size += (input_buf[offset + 2] >> 1) & 1;

		if (offset > 0 && bytes_in - offset < frame_size) {
			// underflow
			*file_pos += offset;
			continue;
		}

		avpkt.data = input_buf + offset;
		avpkt.size = frame_size;
		bytes_out = sizeof(cdda_out_buffer);

		len = avcodec_decode_audio3(ctx, cdda_out_buffer,
			&bytes_out, &avpkt);
		if (len <= 0) {
			lprintf("mp3 decode err (%i/%i) %i\n",
				*file_pos, file_len, len);

			// attempt to skip the offending frame..
			*file_pos += offset + 1;
			continue;
		}

		*file_pos += offset + len;
	}
	while (0);

	return 0;
}

int mp3dec_start(void)
{
	AVCodec *codec;
	int ret;

	if (ctx != NULL)
		return 0;

	// init decoder

	//avcodec_init();
	avcodec_register_all();

	// AV_CODEC_ID_MP3 ?
	codec = avcodec_find_decoder(CODEC_ID_MP3);
	if (codec == NULL) {
		lprintf("mp3dec: codec missing\n");
		return -1;
	}

	ctx = avcodec_alloc_context();
	if (ctx == NULL) {
		lprintf("mp3dec: avcodec_alloc_context failed\n");
		return -1;
	}

	ret = avcodec_open(ctx, codec);
	if (ret < 0) {
		lprintf("mp3dec: avcodec_open failed: %d\n", ret);
		av_free(ctx);
		ctx = NULL;
		return -1;
	}

	return 0;
}
