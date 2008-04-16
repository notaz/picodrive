// Code for communication with ARM940 and control of it.
// (c) Copyright 2007, Grazvydas "notaz" Ignotas

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include "code940/940shared.h"
#include "gp2x.h"
#include "emu.h"
#include "menu.h"
#include "../common/mp3.h"
#include "../common/arm_utils.h"
#include "../common/menu.h"
#include "../common/emu.h"
#include "../../Pico/PicoInt.h"
#include "../../Pico/sound/ym2612.h"
#include "../../Pico/sound/mix.h"

/* we will need some gp2x internals here */
extern volatile unsigned short *gp2x_memregs; /* from minimal library rlyeh */
extern volatile unsigned long  *gp2x_memregl;

extern int reset_timing;
static unsigned char *shared_mem = 0;
static _940_data_t *shared_data = 0;
_940_ctl_t *shared_ctl = 0;
unsigned char *mp3_mem = 0;

#define MP3_SIZE_MAX (0x400000 + 0x800000) // 12M
#define CODE940_FILE "pico940_v2.bin"

int crashed_940 = 0;

static FILE *loaded_mp3 = 0;

/***********************************************************/

#define MAXOUT		(+32767)
#define MINOUT		(-32768)

/* limitter */
#define Limit(val, max,min) { \
	if ( val > max )      val = max; \
	else if ( val < min ) val = min; \
}

/* these will be managed locally on our side */
static UINT8 *REGS = 0;		/* we will also keep local copy of regs for savestates and such */
static INT32 *addr_A1;		/* address line A1      */

static int   dacen;
static INT32 dacout;
static UINT8 ST_address;	/* address register     */

static int   writebuff_ptr = 0;


/* OPN Mode Register Write */
static int set_timers( int v )
{
	int change;

	/* b7 = CSM MODE */
	/* b6 = 3 slot mode */
	/* b5 = reset b */
	/* b4 = reset a */
	/* b3 = timer enable b */
	/* b2 = timer enable a */
	/* b1 = load b */
	/* b0 = load a */
	change = (ym2612_st->mode ^ v) & 0xc0;
	ym2612_st->mode = v;

	/* reset Timer b flag */
	if( v & 0x20 )
		ym2612_st->status &= ~2;

	/* reset Timer a flag */
	if( v & 0x10 )
		ym2612_st->status &= ~1;

	return change;
}

/* YM2612 write */
/* a = address */
/* v = value   */
/* returns 1 if sample affecting state changed */
int YM2612Write_940(unsigned int a, unsigned int v)
{
	int addr;
	int upd = 1;	/* the write affects sample generation */

	v &= 0xff;	/* adjust to 8 bit bus */
	a &= 3;

	//printf("%05i:%03i: ym w ([%i] %02x)\n", Pico.m.frame_count, Pico.m.scanline, a, v);

	switch( a ) {
	case 0:	/* address port 0 */
		if (!*addr_A1 && ST_address == v)
			return 0;	/* address already selected, don't send this command to 940 */
		ST_address = v;
		/* don't send DAC or timer related address changes to 940 */
		if (!*addr_A1 && (v & 0xf0) == 0x20 &&
			(v == 0x24 || v == 0x25 || v == 0x26 || v == 0x2a))
				return 0;
		*addr_A1 = 0;
		upd = 0;
		break;

	case 1:	/* data port 0    */
		if (*addr_A1 != 0) {
			return 0;	/* verified on real YM2608 */
		}

		addr = ST_address;
		REGS[addr] = v;

		switch( addr & 0xf0 )
		{
		case 0x20:	/* 0x20-0x2f Mode */
			switch( addr )
			{
			case 0x24: { // timer A High 8
					int TAnew = (ym2612_st->TA & 0x03)|(((int)v)<<2);
					if (ym2612_st->TA != TAnew) {
						// we should reset ticker only if new value is written. Outrun requires this.
						ym2612_st->TA = TAnew;
						ym2612_st->TAC = (1024-TAnew)*18;
						ym2612_st->TAT = 0;
					}
					return 0;
				}
			case 0x25: { // timer A Low 2
					int TAnew = (ym2612_st->TA & 0x3fc)|(v&3);
					if (ym2612_st->TA != TAnew) {
						ym2612_st->TA = TAnew;
						ym2612_st->TAC = (1024-TAnew)*18;
						ym2612_st->TAT = 0;
					}
					return 0;
				}
			case 0x26: // timer B
				if (ym2612_st->TB != v) {
					ym2612_st->TB = v;
					ym2612_st->TBC  = (256-v)<<4;
					ym2612_st->TBC *= 18;
					ym2612_st->TBT  = 0;
				}
				return 0;
			case 0x27:	/* mode, timer control */
				if (set_timers( v ))
					break; // other side needs ST.mode for 3slot mode
				return 0;
			case 0x2a:	/* DAC data (YM2612) */
				dacout = ((int)v - 0x80) << 6;	/* level unknown (notaz: 8 seems to be too much) */
				return 0;
			case 0x2b:	/* DAC Sel  (YM2612) */
				/* b7 = dac enable */
				dacen = v & 0x80;
				upd = 0;
				break; // other side has to know this
			default:
				break;
			}
			break;
		}
		break;

	case 2:	/* address port 1 */
		if (*addr_A1 && ST_address == v)
			return 0;
		ST_address = v;
		*addr_A1 = 1;
		upd = 0;
		break;

	case 3:	/* data port 1    */
		if (*addr_A1 != 1) {
			return 0;	/* verified on real YM2608 */
		}

		addr = ST_address | 0x100;
		REGS[addr] = v;
		break;
	}

	//printf("ym pass\n");

	if(currentConfig.EmuOpt & 4) {
		UINT16 *writebuff = shared_ctl->writebuffsel ? shared_ctl->writebuff0 : shared_ctl->writebuff1;

		/* detect rapid ym updates */
		if (upd && !(writebuff_ptr & 0x80000000) && Pico.m.scanline < 224) {
			int mid = Pico.m.pal ? 68 : 93;
			if (Pico.m.scanline > mid) {
				//printf("%05i:%03i: rapid ym\n", Pico.m.frame_count, Pico.m.scanline);
				writebuff[writebuff_ptr++ & 0xffff] = 0xfffe;
				writebuff_ptr |= 0x80000000;
				//printf("%05i:%03i: ym w ([%02x] %02x, upd=%i)\n", Pico.m.frame_count, Pico.m.scanline, addr, v, upd);
			}
		}

		/* queue this write for 940 */
		if ((writebuff_ptr&0xffff) < 2047) {
			writebuff[writebuff_ptr++ & 0xffff] = (a<<8)|v;
		} else {
			printf("warning: writebuff_ptr > 2047 ([%i] %02x)\n", a, v);
		}
	}

	return 0; // cause the engine to do updates once per frame only
}


#define CHECK_BUSY(job) \
	(gp2x_memregs[0x3b46>>1] & (1<<(job-1)))

static void wait_busy_940(int job)
{
	int i;

	job--;
	for (i = 0; (gp2x_memregs[0x3b46>>1] & (1<<job)) && i < 0x10000; i++)
		spend_cycles(8*1024); // tested to be best for mp3 dec
	if (i < 0x10000) return;

	/* 940 crashed */
	printf("940 crashed (cnt: %i, ve: ", shared_ctl->loopc);
	for (i = 0; i < 8; i++)
		printf("%i ", shared_ctl->vstarts[i]);
	printf(")\n");
	printf("irq pending flags: DUALCPU %04x, SRCPND %08lx (see 26), INTPND %08lx\n",
		gp2x_memregs[0x3b46>>1], gp2x_memregl[0x4500>>2], gp2x_memregl[0x4510>>2]);
	printf("last lr: %08x, lastjob: %i\n", shared_ctl->last_lr, shared_ctl->lastjob);
	printf("trying to interrupt..\n");
	gp2x_memregs[0x3B3E>>1] = 0xffff;
	for (i = 0; gp2x_memregs[0x3b46>>1] && i < 0x10000; i++)
		spend_cycles(8*1024);
	printf("i = 0x%x\n", i);
	printf("irq pending flags: DUALCPU %04x, SRCPND %08lx (see 26), INTPND %08lx\n",
		gp2x_memregs[0x3b46>>1], gp2x_memregl[0x4500>>2], gp2x_memregl[0x4510>>2]);
	printf("last lr: %08x, lastjob: %i\n", shared_ctl->last_lr, shared_ctl->lastjob);

	strcpy(menuErrorMsg, "940 crashed, too much overclock?");
	engineState = PGS_Menu;
	crashed_940 = 1;
}


static void add_job_940(int job)
{
	if (job <= 0 || job > 16) {
		printf("add_job_940: bad job: %i\n", job);
		return;
	}

	// generate interrupt for this job
	job--;
	gp2x_memregs[(0x3B20+job*2)>>1] = 1;

//	printf("added %i, pending %04x\n", job+1, gp2x_memregs[0x3b46>>1]);
}


void YM2612PicoStateLoad_940(void)
{
	int i, old_A1 = *addr_A1;

	/* make sure JOB940_PICOSTATELOAD gets done before next JOB940_YM2612UPDATEONE */
	add_job_940(JOB940_PICOSTATELOAD);
	if (CHECK_BUSY(JOB940_PICOSTATELOAD)) wait_busy_940(JOB940_PICOSTATELOAD);

	writebuff_ptr = 0;

	// feed all the registers and update internal state
	for(i = 0; i < 0x100; i++) {
		YM2612Write_940(0, i);
		YM2612Write_940(1, REGS[i]);
	}
	for(i = 0; i < 0x100; i++) {
		YM2612Write_940(2, i);
		YM2612Write_940(3, REGS[i|0x100]);
	}

	*addr_A1 = old_A1;
}


static void internal_reset(void)
{
	writebuff_ptr = 0;
	ym2612_st->mode   = 0;
	ym2612_st->status = 0;	/* normal mode */
	ym2612_st->TA     = 0;
	ym2612_st->TAC    = 0;
	ym2612_st->TAT    = 0;
	ym2612_st->TB     = 0;
	ym2612_st->TBC    = 0;
	ym2612_st->TBT    = 0;
	dacen     = 0;
	dacout    = 0;
	ST_address= 0;
}


/* this must be called after mmu hack, the allocated regions must not get cached */
void sharedmem_init(void)
{
	if (shared_mem != NULL) return;

	shared_mem = (unsigned char *) mmap(0, 0x210000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0x2000000);
	if(shared_mem == MAP_FAILED)
	{
		printf("mmap(shared_data) failed with %i\n", errno);
		exit(1);
	}
	shared_data = (_940_data_t *) (shared_mem+0x100000);
	/* this area must not get buffered on either side */
	shared_ctl =  (_940_ctl_t *)  (shared_mem+0x200000);
	mp3_mem = (unsigned char *) mmap(0, MP3_SIZE_MAX, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0x2400000);
	if (mp3_mem == MAP_FAILED)
	{
		printf("mmap(mp3_mem) failed with %i\n", errno);
		exit(1);
	}
	crashed_940 = 1;
}


void sharedmem_deinit(void)
{
	munmap(shared_mem, 0x210000);
	munmap(mp3_mem, MP3_SIZE_MAX);
	shared_mem = mp3_mem = NULL;
	shared_data = NULL;
	shared_ctl = NULL;
}


extern char **g_argv;

/* none of the functions in this file should be called before this one */
void YM2612Init_940(int baseclock, int rate)
{
	printf("YM2612Init_940()\n");
	printf("Mem usage: shared_data: %i, shared_ctl: %i\n", sizeof(*shared_data), sizeof(*shared_ctl));

	Reset940(1, 2);
	Pause940(1);

	gp2x_memregs[0x3B40>>1] = 0;      // disable DUALCPU interrupts for 920
	gp2x_memregs[0x3B42>>1] = 1;      // enable  DUALCPU interrupts for 940

	gp2x_memregl[0x4504>>2] = 0;        // make sure no FIQs will be generated
	gp2x_memregl[0x4508>>2] = ~(1<<26); // unmask DUALCPU ints in the undocumented 940's interrupt controller


	if (crashed_940)
	{
		unsigned char ucData[1024];
		int nRead, i, nLen = 0;
		char binpath[1024];
		FILE *fp;

		strncpy(binpath, g_argv[0], 1023);
		binpath[1023] = 0;
		for (i = strlen(binpath); i > 0; i--)
			if (binpath[i] == '/') { binpath[i] = 0; break; }
		strcat(binpath, "/" CODE940_FILE);

		fp = fopen(binpath, "rb");
		if(!fp)
		{
			memset(gp2x_screen, 0, 320*240*2);
			text_out16(10, 100, "failed to open required file:");
			text_out16(10, 110, CODE940_FILE);
			gp2x_video_flip2();
			printf("failed to open %s\n", binpath);
			exit(1);
		}

		while(1)
		{
			nRead = fread(ucData, 1, 1024, fp);
			if(nRead <= 0)
				break;
			memcpy(shared_mem + nLen, ucData, nRead);
			nLen += nRead;
		}
		fclose(fp);
		crashed_940 = 0;
	}

	memset(shared_data, 0, sizeof(*shared_data));
	memset(shared_ctl,  0, sizeof(*shared_ctl));

	/* cause local ym2612 to init REGS */
	YM2612Init_(baseclock, rate);

	REGS = YM2612GetRegs();
	addr_A1 = (INT32 *) (REGS + 0x200);

	ym2612_dacen  = &dacen;
	ym2612_dacout = &dacout;

	internal_reset();

	loaded_mp3 = 0;

	gp2x_memregs[0x3B46>>1] = 0xffff; // clear pending DUALCPU interrupts for 940
	gp2x_memregl[0x4500>>2] = 0xffffffff; // clear pending IRQs in SRCPND
	gp2x_memregl[0x4510>>2] = 0xffffffff; // clear pending IRQs in INTPND

	/* start the 940 */
	Reset940(0, 2);
	Pause940(0);

	// YM2612ResetChip_940(); // will be done on JOB940_YM2612INIT

	/* now cause 940 to init it's ym2612 stuff */
	shared_ctl->baseclock = baseclock;
	shared_ctl->rate = rate;
	add_job_940(JOB940_INITALL);
}


void YM2612ResetChip_940(void)
{
	//printf("YM2612ResetChip_940()\n");
	if (shared_data == NULL) {
		printf("YM2612ResetChip_940: reset before init?\n");
		return;
	}

	internal_reset();

	add_job_940(JOB940_YM2612RESETCHIP);
}


int YM2612UpdateOne_940(int *buffer, int length, int stereo, int is_buf_empty)
{
	int *ym_buf = shared_data->ym_buffer;
	int ym_active_chs;

	//printf("YM2612UpdateOne_940()\n");

	if (CHECK_BUSY(JOB940_YM2612UPDATEONE)) wait_busy_940(JOB940_YM2612UPDATEONE);

	ym_active_chs = shared_ctl->ym_active_chs;

	// mix in ym buffer. is_buf_empty means nobody mixed there anything yet and it may contain trash
	if (is_buf_empty && ym_active_chs) memcpy32(buffer, ym_buf, length<<stereo);
	else memset32(buffer, 0, length<<stereo);

	if (shared_ctl->writebuffsel == 1) {
		shared_ctl->writebuff0[writebuff_ptr & 0xffff] = 0xffff;
	} else {
		shared_ctl->writebuff1[writebuff_ptr & 0xffff] = 0xffff;
	}
	writebuff_ptr = 0;

	/* predict sample counter for next frame */
	if (PsndLen_exc_add) {
		if (PsndLen_exc_cnt + PsndLen_exc_add >= 0x10000) length = PsndLen + 1;
		else length = PsndLen;
	}

	/* give 940 ym job */
	shared_ctl->writebuffsel ^= 1;
	shared_ctl->length = length;
	shared_ctl->stereo = stereo;

	add_job_940(JOB940_YM2612UPDATEONE);

	return ym_active_chs;
}


static int mp3_samples_ready = 0, mp3_buffer_offs = 0;
static int mp3_play_bufsel = 0, mp3_job_started = 0;

void mp3_update(int *buffer, int length, int stereo)
{
	int length_mp3;
	int cdda_on;

	// playback was started, track not ended
	cdda_on = loaded_mp3 && shared_ctl->mp3_offs < shared_ctl->mp3_len;

	if (!cdda_on) return;

	if (!(PicoOpt&0x200)) {
		mp3_update_local(buffer, length, stereo);
		return;
	}

	length_mp3 = length;
	if (PsndRate == 22050) length_mp3 <<= 1;	// mp3s are locked to 44100Hz stereo
	else if (PsndRate == 11025) length_mp3 <<= 2;	// so make length 44100ish

	/* do we have to wait? */
	if (mp3_job_started && mp3_samples_ready < length_mp3) {
		if (CHECK_BUSY(JOB940_MP3DECODE)) wait_busy_940(JOB940_MP3DECODE);
		mp3_job_started = 0;
		mp3_samples_ready += 1152;
	}

	/* mix mp3 data, only stereo */
	if (mp3_samples_ready >= length_mp3)
	{
		int shr = 0;
		void (*mix_samples)(int *dest_buf, short *mp3_buf, int count) = mix_16h_to_32;
		if (PsndRate == 22050) { mix_samples = mix_16h_to_32_s1; shr = 1; }
		else if (PsndRate == 11025) { mix_samples = mix_16h_to_32_s2; shr = 2; }

		if (1152 - mp3_buffer_offs >= length_mp3) {
			mix_samples(buffer, shared_data->mp3_buffer[mp3_play_bufsel] + mp3_buffer_offs*2, length<<1);

			mp3_buffer_offs += length_mp3;
		} else {
			// collect samples from both buffers..
			int left = 1152 - mp3_buffer_offs;
			if (mp3_play_bufsel == 0)
			{
				mix_samples(buffer, shared_data->mp3_buffer[0] + mp3_buffer_offs*2, length<<1);
				mp3_buffer_offs = length_mp3 - left;
				mp3_play_bufsel = 1;
			} else {
				mix_samples(buffer, shared_data->mp3_buffer[1] + mp3_buffer_offs*2, (left>>shr)<<1);
				mp3_buffer_offs = length_mp3 - left;
				mix_samples(buffer + ((left>>shr)<<1),
					shared_data->mp3_buffer[0], (mp3_buffer_offs>>shr)<<1);
				mp3_play_bufsel = 0;
			}
		}
		mp3_samples_ready -= length_mp3;
	}

	// ask to decode more if we already can
	if (!mp3_job_started)
	{
		mp3_job_started = 1;
		shared_ctl->mp3_buffsel ^= 1;
		add_job_940(JOB940_MP3DECODE);
	}
}


/***********************************************************/

void mp3_start_play(FILE *f, int pos) // pos is 0-1023
{
	int byte_offs = 0;

	if (!(PicoOpt&0x800)) { // cdda disabled?
		return;
	}

	if (loaded_mp3 != f)
	{
		if (PicoMessage != NULL)
		{
			fseek(f, 0, SEEK_END);
			if (ftell(f) > 2*1024*1024)
				PicoMessage("Loading MP3...");
		}
		fseek(f, 0, SEEK_SET);
		fread(mp3_mem, 1, MP3_SIZE_MAX, f);
		if (!feof(f)) printf("Warning: mp3 was too large, not all data loaded.\n");
		shared_ctl->mp3_len = ftell(f);
		loaded_mp3 = f;

		if (PicoOpt&0x200) {
			// as we are going to change 940's cacheable area, we must invalidate it's cache..
			if (CHECK_BUSY(JOB940_MP3DECODE)) wait_busy_940(JOB940_MP3DECODE);
			add_job_940(JOB940_INVALIDATE_DCACHE);
		}
		reset_timing = 1;
	}

	// seek..
	if (pos) {
		byte_offs  = (shared_ctl->mp3_len << 6) >> 10;
		byte_offs *= pos;
		byte_offs >>= 6;
	}
	// printf("mp3 pos1024: %i, byte_offs %i/%i\n", pos, byte_offs, shared_ctl->mp3_len);

	shared_ctl->mp3_offs = byte_offs;

	// reset buffer pointers and stuff..
	mp3_samples_ready = mp3_buffer_offs = mp3_play_bufsel = 0;
	mp3_job_started = 0;
	shared_ctl->mp3_buffsel = 1; // will change to 0 on first decode

	if (!(PicoOpt&0x200)) mp3_start_local();
}


int mp3_get_offset(void)
{
	unsigned int offs1024 = 0;
	int cdda_on;

	cdda_on = (PicoAHW & PAHW_MCD) && (PicoOpt&0x800) && !(Pico_mcd->s68k_regs[0x36] & 1) &&
			(Pico_mcd->scd.Status_CDC & 1) && loaded_mp3;

	if (cdda_on) {
		offs1024  = shared_ctl->mp3_offs << 7;
		offs1024 /= shared_ctl->mp3_len  >> 3;
	}
	printf("offs1024=%u (%i/%i)\n", offs1024, shared_ctl->mp3_offs, shared_ctl->mp3_len);

	return offs1024;
}


