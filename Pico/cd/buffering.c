// Buffering handling
// (c) Copyright 2007, Grazvydas "notaz" Ignotas

#include "../PicoInt.h"

int PicoCDBuffers = 0;
static unsigned char *cd_buffer = NULL;
static int prev_lba = 0x80000000;

static int hits, reads;

//#define THREADED_CD_IO

/* threaded reader */
#ifdef THREADED_CD_IO
#include <pthread.h>
#define tioprintf printf

static pthread_t thr_thread = 0;
static pthread_cond_t  thr_cond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t thr_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned char *thr_buffer[2][2048 + 304] __attribute__((aligned(4)));
static int thr_lba_need;
static int thr_lba_have[2];

static void thr_read_lba(int slot, int lba)
{
	int is_bin = Pico_mcd->TOC.Tracks[0].ftype == TYPE_BIN;
	int where_seek = is_bin ? (lba * 2352 + 16) : (lba << 11);

	pm_seek(Pico_mcd->TOC.Tracks[0].F, where_seek, SEEK_SET);
	pm_read(thr_buffer[slot], 2048, Pico_mcd->TOC.Tracks[0].F);
	thr_lba_have[slot] = lba;
}

static void *buffering_thread(void *arg)
{
	int free_slot, lba;

	elprintf(EL_STATUS, "CD I/O thread started.");

	pthread_mutex_lock(&thr_mutex);

	while (1)
	{
		if (thr_lba_need < 0) goto wait;

		free_slot = -1;
		if (thr_lba_have[0] == -1) free_slot = 0;
		if (thr_lba_have[1] == -1) free_slot = 1;
		if (free_slot == -1) goto wait;

		lba = thr_lba_need;
		if (lba != thr_lba_have[free_slot^1]) {
			thr_read_lba(free_slot, lba);
			tioprintf("t done %i %i\n", lba, free_slot);
			continue;
		}
		lba++;
		if (lba != thr_lba_have[free_slot^1]) {
			thr_read_lba(free_slot, lba);
			tioprintf("t done %i %i\n", lba, free_slot);
			continue;
		}

wait:
		pthread_cond_wait(&thr_cond, &thr_mutex);
		tioprintf("t wake\n");
	}

	pthread_mutex_unlock(&thr_mutex);

	return NULL;
}

static void threaded_read(void *dest, int lba)
{
	int i, have = -1;
	tioprintf("\n");

	if (lba == thr_lba_have[0]) have = 0;
	if (lba == thr_lba_have[1]) have = 1;
	if (have != -1)
	{
		tioprintf("r hit  %i %i\n", lba, have);
		memcpy32(dest, (int *)thr_buffer[have], 2048/4);
		if (lba != prev_lba) {
			thr_lba_have[have] = -1; // make free slot
			thr_lba_need = lba + 1;  // guess a sequential read..
			pthread_cond_signal(&thr_cond);
			sched_yield();
			prev_lba = lba;
		}
		return;
	}

	tioprintf("r miss %i\n", lba);
	thr_lba_need = lba;
	pthread_mutex_lock(&thr_mutex);
	pthread_mutex_unlock(&thr_mutex);
	if (lba == thr_lba_have[0]) have = 0;
	if (lba == thr_lba_have[1]) have = 1;
	if (have == -1)
	{
		// try again..
		thr_lba_have[0] = thr_lba_have[1] = -1;
		for (i = 0; have == -1 && i < 10; i++)
		{
			tioprintf("r hard %i\n", lba);
			pthread_cond_signal(&thr_cond);
			sched_yield();
			pthread_mutex_lock(&thr_mutex);
			pthread_mutex_unlock(&thr_mutex);
			if (lba == thr_lba_have[0]) have = 0;
			if (lba == thr_lba_have[1]) have = 1;
		}
	}

	// we MUST have the data at this point..
	if (have == -1) { tioprintf("BUG!\n"); exit(1); }
	tioprintf("r reco %i %i\n", lba, have);
	memcpy32(dest, (int *)thr_buffer[have], 2048/4);
	thr_lba_have[have] = -1;
	pthread_cond_signal(&thr_cond);

	prev_lba = lba;
	return;
}
#endif


void PicoCDBufferInit(void)
{
	void *tmp;

	prev_lba = 0x80000000;
	hits = reads = 0;
	cd_buffer = NULL;

	if (PicoCDBuffers <= 1) {
		PicoCDBuffers = 0;
		goto no_buffering; /* buffering off */
	}

	/* try alloc'ing until we succeed */
	while (PicoCDBuffers > 0)
	{
		tmp = realloc(cd_buffer, PicoCDBuffers * 2048 + 304);
		if (tmp != NULL) break;
		PicoCDBuffers >>= 1;
	}

	if (PicoCDBuffers > 0) {
		cd_buffer = tmp;
		return;
	}

no_buffering:;
#ifdef THREADED_CD_IO
	thr_lba_need = thr_lba_have[0] = thr_lba_have[1] = -1;
	if (thr_thread == 0)
	{
		pthread_create(&thr_thread, NULL, buffering_thread, NULL);
	}
#endif
}


void PicoCDBufferFree(void)
{
#ifdef THREADED_CD_IO
	pthread_mutex_lock(&thr_mutex);
	pthread_mutex_unlock(&thr_mutex);
#endif
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
#ifdef THREADED_CD_IO
		threaded_read(dest, lba);
		return;
#else
		/* no buffering */
		int where_seek = is_bin ? (lba * 2352 + 16) : (lba << 11);
		pm_seek(Pico_mcd->TOC.Tracks[0].F, where_seek, SEEK_SET);
		pm_read(dest, 2048, Pico_mcd->TOC.Tracks[0].F);
		return;
#endif
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

