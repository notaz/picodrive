#include "../PicoInt.h"

//#include <stdlib.h>

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
		tmp = realloc(cd_buffer, PicoCDBuffers * 2048);
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
		printf("CD buffer hits: %i/%i (%i%%)\n", hits, reads, hits * 100 / reads);
}


/* this is a try to fight slow SD access of GP2X */
void PicoCDBufferRead(void *dest, int lba)
{
	int is_bin, offs, read_len;
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
		dprintf("CD buffer move");
		read_len = prev_lba - lba;
		memmove(cd_buffer + read_len*2048, cd_buffer, (PicoCDBuffers - read_len)*2048);
	}
	else
	{
		read_len = PicoCDBuffers;
	}

	if (is_bin)
	{
		int i;
		for (i = 0; i < read_len; i++)
		{
			pm_read(cd_buffer + i*2048, 2048, Pico_mcd->TOC.Tracks[0].F);
			pm_seek(Pico_mcd->TOC.Tracks[0].F, 304, SEEK_CUR);
		}
	}
	else
	{
		pm_read(cd_buffer, read_len*2048, Pico_mcd->TOC.Tracks[0].F);
	}
	memcpy32(dest, (int *) cd_buffer, 2048/4);
	prev_lba = lba;
}

