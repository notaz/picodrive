// Buffering handling
// (c) Copyright 2007, Grazvydas "notaz" Ignotas

#include "../PicoInt.h"

int PicoCDBuffers = 0;
static unsigned char *cd_buffer = NULL;
static int prev_lba = 0x80000000;

static int hits, reads;


void PicoCDBufferInit(void)
{
	void *tmp;

	prev_lba = 0x80000000;
	hits = reads = 0;

	if (PicoCDBuffers <= 1) {
		PicoCDBuffers = 0;
		return; /* buffering off */
	}

	/* try alloc'ing until we succeed */
	while (PicoCDBuffers > 0)
	{
		tmp = realloc(cd_buffer, PicoCDBuffers * 2048 + 304);
		if (tmp != NULL) break;
		PicoCDBuffers >>= 1;
	}

	if (PicoCDBuffers <= 0) return; /* buffering became off */

	cd_buffer = tmp;
}


void PicoCDBufferFree(void)
{
	if (cd_buffer) {
		free(cd_buffer);
		cd_buffer = NULL;
	}
	if (reads)
		elprintf(EL_STATUS, "CD buffer hits: %i/%i (%i%%)\n", hits, reads, hits * 100 / reads);
}


/* this is a try to fight slow SD access of GP2X */
PICO_INTERNAL void PicoCDBufferRead(void *dest, int lba)
{
	int is_bin, offs, read_len, moved = 0;
	reads++;

	is_bin = Pico_mcd->TOC.Tracks[0].ftype == TYPE_BIN;

	if (PicoCDBuffers <= 0)
	{
		/* no buffering */
		int where_seek = is_bin ? (lba * 2352 + 16) : (lba << 11);
		pm_seek(Pico_mcd->TOC.Tracks[0].F, where_seek, SEEK_SET);
		pm_read(dest, 2048, Pico_mcd->TOC.Tracks[0].F);
		return;
	}

	/* hit? */
	offs = lba - prev_lba;
	if (offs >= 0 && offs < PicoCDBuffers)
	{
		hits++;
		if (offs == 0) dprintf("CD buffer seek to old %i -> %i\n", prev_lba, lba);
		memcpy32(dest, (int *)(cd_buffer + offs*2048), 2048/4);
		return;
	}

	if (prev_lba + PicoCDBuffers != lba)
	{
		int where_seek = is_bin ? (lba * 2352 + 16) : (lba << 11);
		dprintf("CD buffer seek %i -> %i\n", prev_lba, lba);
		pm_seek(Pico_mcd->TOC.Tracks[0].F, where_seek, SEEK_SET);
	}

	dprintf("CD buffer miss %i -> %i\n", prev_lba, lba);

	if (lba < prev_lba && prev_lba - lba < PicoCDBuffers)
	{
		read_len = prev_lba - lba;
		dprintf("CD buffer move=%i, read_len=%i", PicoCDBuffers - read_len, read_len);
		memmove(cd_buffer + read_len*2048, cd_buffer, (PicoCDBuffers - read_len)*2048);
		moved = 1;
	}
	else
	{
		read_len = PicoCDBuffers;
	}

	if (PicoMessage != NULL && read_len >= 512)
	{
		PicoMessage("Buffering data...");
	}

	if (is_bin)
	{
		int i = 0;
#if REDUCE_IO_CALLS
		int bufs = (read_len*2048) / (2048+304);
		pm_read(cd_buffer, bufs*(2048+304), Pico_mcd->TOC.Tracks[0].F);
		for (i = 1; i < bufs; i++)
			// should really use memmove here, but my memcpy32 implementation is also suitable here
			memcpy32((int *)(cd_buffer + i*2048), (int *)(cd_buffer + i*(2048+304)), 2048/4);
#endif
		for (; i < read_len - 1; i++)
		{
			pm_read(cd_buffer + i*2048, 2048 + 304, Pico_mcd->TOC.Tracks[0].F);
			// pm_seek(Pico_mcd->TOC.Tracks[0].F, 304, SEEK_CUR); // seeking is slower, in PSP case even more
		}
		// further data might be moved, do not overwrite
		pm_read(cd_buffer + i*2048, 2048, Pico_mcd->TOC.Tracks[0].F);
		pm_seek(Pico_mcd->TOC.Tracks[0].F, 304, SEEK_CUR);
	}
	else
	{
		pm_read(cd_buffer, read_len*2048, Pico_mcd->TOC.Tracks[0].F);
	}
	memcpy32(dest, (int *) cd_buffer, 2048/4);
	prev_lba = lba;

	if (moved)
	{
		/* file pointer must point to the same data in file, as would-be data after our buffer */
		int where_seek;
		lba += PicoCDBuffers;
		where_seek = is_bin ? (lba * 2352 + 16) : (lba << 11);
		pm_seek(Pico_mcd->TOC.Tracks[0].F, where_seek, SEEK_SET);
	}
}

