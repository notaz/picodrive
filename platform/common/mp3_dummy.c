/*
 * dummy/none mp3 code
 * (C) notaz, 2013
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */

#include "mp3.h"
#include <pico/pico.h>

int mp3_get_bitrate(void *f_, int len)
{
	return -1;
}

void mp3_start_play(void *f_, int pos)
{
}

void mp3_update(int *buffer, int length, int stereo)
{
}
