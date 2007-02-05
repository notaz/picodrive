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
#include "asmutils.h"
#include "../../Pico/PicoInt.h"

// tmp
#include "../../Pico/sound/mix.h"

/* we will need some gp2x internals here */
extern volatile unsigned short *gp2x_memregs; /* from minimal library rlyeh */
extern volatile unsigned long  *gp2x_memregl;

static unsigned char *shared_mem = 0;
static _940_data_t *shared_data = 0;
static _940_ctl_t *shared_ctl = 0;
static unsigned char *mp3_mem = 0;

#define MP3_SIZE_MAX (0x1000000 - 4*640*480)

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
extern int   *ym2612_dacen;
extern INT32 *ym2612_dacout;
extern void  *ym2612_regs;

static UINT8 *REGS = 0;		/* we will also keep local copy of regs for savestates and such */
static INT32 addr_A1;		/* address line A1      */
static int	 dacen;
static INT32 dacout;
static UINT8 ST_address;	/* address register     */
static UINT8 ST_status;		/* status flag          */
static UINT8 ST_mode;		/* mode  CSM / 3SLOT    */
static int   ST_TA;			/* timer a              */
static int   ST_TAC;		/* timer a maxval       */
static int   ST_TAT;		/* timer a ticker       */
static UINT8 ST_TB;			/* timer b              */
static int   ST_TBC;		/* timer b maxval       */
static int   ST_TBT;		/* timer b ticker       */

static int   writebuff_ptr = 0;


/* OPN Mode Register Write */
static void set_timers( int v )
{
	/* b7 = CSM MODE */
	/* b6 = 3 slot mode */
	/* b5 = reset b */
	/* b4 = reset a */
	/* b3 = timer enable b */
	/* b2 = timer enable a */
	/* b1 = load b */
	/* b0 = load a */
	ST_mode = v;

	/* reset Timer b flag */
	if( v & 0x20 )
		ST_status &= ~2;

	/* reset Timer a flag */
	if( v & 0x10 )
		ST_status &= ~1;
}

/* YM2612 write */
/* a = address */
/* v = value   */
/* returns 1 if sample affecting state changed */
int YM2612Write_940(unsigned int a, unsigned int v)
{
	int addr; //, ret=1;

	v &= 0xff;	/* adjust to 8 bit bus */
	a &= 3;

	switch( a ) {
	case 0:	/* address port 0 */
		ST_address = v;
		addr_A1 = 0;
		//ret=0;
		break;

	case 1:	/* data port 0    */
		if (addr_A1 != 0) {
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
					int TAnew = (ST_TA & 0x03)|(((int)v)<<2);
					if(ST_TA != TAnew) {
						// we should reset ticker only if new value is written. Outrun requires this.
						ST_TA = TAnew;
						ST_TAC = (1024-TAnew)*18;
						ST_TAT = 0;
					}
					return 0;
				}
			case 0x25: { // timer A Low 2
					int TAnew = (ST_TA & 0x3fc)|(v&3);
					if(ST_TA != TAnew) {
						ST_TA = TAnew;
						ST_TAC = (1024-TAnew)*18;
						ST_TAT = 0;
					}
					return 0;
				}
			case 0x26: // timer B
				if(ST_TB != v) {
					ST_TB = v;
					ST_TBC  = (256-v)<<4;
					ST_TBC *= 18;
					ST_TBT  = 0;
				}
				return 0;
			case 0x27:	/* mode, timer control */
				set_timers( v );
				break; // other side needs ST.mode for 3slot mode
			case 0x2a:	/* DAC data (YM2612) */
				dacout = ((int)v - 0x80) << 6;	/* level unknown (notaz: 8 seems to be too much) */
				return 0;
			case 0x2b:	/* DAC Sel  (YM2612) */
				/* b7 = dac enable */
				dacen = v & 0x80;
				break; // other side has to know this
			default:
				break;
			}
			break;
		}
		break;

	case 2:	/* address port 1 */
		ST_address = v;
		addr_A1 = 1;
		//ret=0;
		break;

	case 3:	/* data port 1    */
		if (addr_A1 != 1) {
			return 0;	/* verified on real YM2608 */
		}

		addr = ST_address | 0x100;
		REGS[addr] = v;
		break;
	}

	if(currentConfig.EmuOpt & 4) {
		/* queue this write for 940 */
		if (writebuff_ptr < 2047) {
			if (shared_ctl->writebuffsel == 1) {
				shared_ctl->writebuff0[writebuff_ptr++] = (a<<8)|v;
			} else {
				shared_ctl->writebuff1[writebuff_ptr++] = (a<<8)|v;
			}
		} else {
			printf("warning: writebuff_ptr > 2047\n");
		}
	}

	return 0; // cause the engine to do updates once per frame only
}

UINT8 YM2612Read_940(void)
{
	return ST_status;
}


int YM2612PicoTick_940(int n)
{
	//int ret = 0;

	// timer A
	if(ST_mode & 0x01 && (ST_TAT+=64*n) >= ST_TAC) {
		ST_TAT -= ST_TAC;
		if(ST_mode & 0x04) ST_status |= 1;
		// CSM mode total level latch and auto key on
/*		FIXME
		if(ST_mode & 0x80) {
			CSMKeyControll( &(ym2612_940->CH[2]) ); // Vectorman2, etc.
			ret = 1;
		}
*/
	}

	// timer B
	if(ST_mode & 0x02 && (ST_TBT+=64*n) >= ST_TBC) {
		ST_TBT -= ST_TBC;
		if(ST_mode & 0x08) ST_status |= 2;
	}

	return 0;
}

static int g_busy = 10;

static void wait_busy_940(void)
{
	int i;
#if 0
	printf("940 busy, entering wait loop.. (cnt: %i, wc: %i, ve: ", shared_ctl->loopc, shared_ctl->waitc);
	for (i = 0; i < 8; i++)
		printf("%i ", shared_ctl->vstarts[i]);
	printf(")\n");

	for (i = 0; shared_ctl->busy; i++)
	{
		spend_cycles(1024); /* needs tuning */
	}
	printf("wait iterations: %i\n", i);
#else
	for (i = 0; /*shared_ctl->busy*/gp2x_memregs[0x3B3E>>1] && i < 0x10000; i++)
		spend_cycles(8*1024); // tested to be best for mp3 dec
	//printf("i = 0x%x\n", i);
	if (i < 0x10000) return;

	/* 940 crashed */
	printf("940 crashed (cnt: %i, ve: ", shared_ctl->loopc);
	for (i = 0; i < 8; i++)
		printf("%i ", shared_ctl->vstarts[i]);
	printf(")\n");
	printf("irq pending flags: DUALCPU %04x (see 15?), SRCPND %08lx (see 26), INTPND %08lx\n",
		gp2x_memregs[0x3b46>>1], gp2x_memregl[0x4500>>2], gp2x_memregl[0x4510>>2]);
	printf("last irq PC: %08x, lastjob: 0x%03x, busy: %x, lastbusy: %x, g_busy: %x\n", shared_ctl->last_irq_pc,
			shared_ctl->lastjob, gp2x_memregs[0x3B3E>>1]/*shared_ctl->busy*/, shared_ctl->lastbusy, g_busy);
	printf("trying to interrupt..\n");
	gp2x_memregs[0x3B3E>>1] = 0xffff;
	for (i = 0; /*shared_ctl->busy*/gp2x_memregs[0x3B3E>>1] && i < 0x10000; i++)
		spend_cycles(8*1024);
	printf("i = 0x%x\n", i);
	printf("irq pending flags: DUALCPU %04x (see 15?), SRCPND %08lx (see 26), INTPND %08lx\n",
		gp2x_memregs[0x3b46>>1], gp2x_memregl[0x4500>>2], gp2x_memregl[0x4510>>2]);
	printf("last irq PC: %08x, lastjob: 0x%03x, busy: %x\n", shared_ctl->last_irq_pc, shared_ctl->lastjob, gp2x_memregs[0x3B3E>>1]/*shared_ctl->busy*/);

	strcpy(menuErrorMsg, "940 crashed.");
	engineState = PGS_Menu;
	crashed_940 = 1;
#endif
}


static void add_job_940(int job0, int job1)
{
/*	if (gp2x_memregs[0x3b46>>1] || shared_ctl->busy)
	{
		printf("!!add_job_940: irq pending flags: DUALCPU %04x (see 15?), SRCPND %08lx (see 26), INTPND %08lx, busy: %x\n",
				gp2x_memregs[0x3b46>>1], gp2x_memregl[0x4500>>2], gp2x_memregl[0x4510>>2], shared_ctl->busy);
	}
*/
	shared_ctl->jobs[0] = job0;
	shared_ctl->jobs[1] = job1;
	//shared_ctl->busy = ++g_busy; // 1;
	gp2x_memregs[0x3B3E>>1] = ++g_busy; // set busy flag
//	gp2x_memregs[0x3B3E>>1] = 0xffff; // cause interrupt
}


void YM2612PicoStateLoad_940(void)
{
	int i, old_A1 = addr_A1;

	if (/*shared_ctl->busy*/gp2x_memregs[0x3B3E>>1]) wait_busy_940();

	// feed all the registers and update internal state
	for(i = 0; i < 0x100; i++) {
		YM2612Write_940(0, i);
		YM2612Write_940(1, REGS[i]);
	}
	for(i = 0; i < 0x100; i++) {
		YM2612Write_940(2, i);
		YM2612Write_940(3, REGS[i|0x100]);
	}

	addr_A1 = old_A1;

	add_job_940(JOB940_PICOSTATELOAD, 0);
}


static void internal_reset(void)
{
	writebuff_ptr = 0;
	ST_mode   = 0;
	ST_status = 0;	/* normal mode */
	ST_TA     = 0;
	ST_TAC    = 0;
	ST_TB     = 0;
	ST_TBC    = 0;
	dacen = 0;
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

	if (shared_mem == NULL)
	{
		shared_mem = (unsigned char *) mmap(0, 0x210000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0x2000000);
		if(shared_mem == MAP_FAILED)
		{
			printf("mmap(shared_data) failed with %i\n", errno);
			exit(1);
		}
		shared_data = (_940_data_t *) (shared_mem+0x100000);
		/* this area must not get buffered on either side */
		shared_ctl =  (_940_ctl_t *)  (shared_mem+0x200000);
		mp3_mem = (unsigned char *) mmap(0, MP3_SIZE_MAX, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0x3000000);
		if (mp3_mem == MAP_FAILED)
		{
			printf("mmap(mp3_mem) failed with %i\n", errno);
			exit(1);
		}
		crashed_940 = 1;
	}

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
		strcat(binpath, "/code940.bin");

		fp = fopen(binpath, "rb");
		if(!fp)
		{
			memset(gp2x_screen, 0, 320*240);
			gp2x_text_out8(10, 100, "failed to open required file:");
			gp2x_text_out8(10, 110, "code940.bin");
			gp2x_video_flip();
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

	REGS = YM2612GetRegs();

	ym2612_dacen  = &dacen;
	ym2612_dacout = &dacout;

	internal_reset();

	loaded_mp3 = 0;

	/* now cause 940 to init it's ym2612 stuff */
	shared_ctl->baseclock = baseclock;
	shared_ctl->rate = rate;
	shared_ctl->jobs[0] = JOB940_INITALL;
	shared_ctl->jobs[1] = 0;
	//shared_ctl->busy = 1;
	gp2x_memregs[0x3B3E>>1] = 1; // set busy flag

	gp2x_memregs[0x3B46>>1] = 0xffff; // clear pending DUALCPU interrupts for 940
	gp2x_memregl[0x4500>>2] = 0; // clear pending IRQs in SRCPND
	gp2x_memregl[0x4510>>2] = 0; // clear pending IRQs in INTPND
	gp2x_memregl[0x4508>>2] = ~(1<<26); // unmask DUALCPU ints in the undocumented 940's interrupt controller

	/* start the 940 */
	Reset940(0, 2);
	Pause940(0);

	// YM2612ResetChip_940(); // will be done on JOB940_YM2612INIT
}


void YM2612ResetChip_940(void)
{
	printf("YM2612ResetChip_940()\n");
	if (shared_data == NULL) {
		printf("YM2612ResetChip_940: reset before init?\n");
		return;
	}

	if (/*shared_ctl->busy*/gp2x_memregs[0x3B3E>>1]) wait_busy_940();

	internal_reset();

	add_job_940(JOB940_YM2612RESETCHIP, 0);
}


//extern int pcm_buffer[2*44100/50];
/*
static void mix_samples(int *dest_buf, short *mp3_buf, int len, int stereo)
{
//	int *pcm = pcm_buffer + offset * 2;

	if (stereo)
	{
		for (; len > 0; len--)
		{
			int lm, rm;
			lm = *mp3_buf++; rm = *mp3_buf++;
			*dest_buf++ += lm - lm/2; *dest_buf++ += rm - rm/2;
		}
	} else {
		for (; len > 0; len--)
		{
			int l = *mp3_buf++;
			*dest_buf++ = l - l/2;
		}
	}
}
*/

// here we assume that length is different between games, but constant in one game

static int mp3_samples_ready = 0, mp3_buffer_offs = 0;
static int mp3_play_bufsel = 0;

int YM2612UpdateOne_940(int *buffer, int length, int stereo, int is_buf_empty)
{
	int length_mp3 = Pico.m.pal ? 44100/50 : 44100/60; // mp3s are locked to 44100Hz stereo
	int *ym_buf = shared_data->mix_buffer;
//	int *dest_buf = buffer;
	int cdda_on, mp3_job = 0;
//	int len;

	// emulating CD, enabled in opts, not data track, CDC is reading, playback was started, track not ended
	cdda_on = (PicoMCD & 1) && (PicoOpt & 0x800) && !(Pico_mcd->s68k_regs[0x36] & 1) &&
		(Pico_mcd->scd.Status_CDC & 1) && loaded_mp3;

	//printf("YM2612UpdateOne_940()\n");
	if (/*shared_ctl->busy*/gp2x_memregs[0x3B3E>>1]) wait_busy_940();

	// track ended?
	cdda_on = cdda_on && shared_ctl->mp3_offs < shared_ctl->mp3_len;

	// mix in ym buffer
	if (is_buf_empty) memcpy32(buffer, ym_buf, length<<stereo);
	// else TODO

//	for (len = length << stereo; len > 0; len--)
//	{
//		*dest_buf++ += *ym_buf++;
//	}

	/* mix mp3 data, only stereo */
	if (cdda_on && mp3_samples_ready >= length_mp3)
	{
		int shr = 0;
		void (*mix_samples)(int *dest_buf, short *mp3_buf, int count) = mix_16h_to_32;
		if (PsndRate == 22050) { mix_samples = mix_16h_to_32_s1; shr = 1; }
		else if (PsndRate == 11025) { mix_samples = mix_16h_to_32_s2; shr = 2; }

		if (1152 - mp3_buffer_offs >= length_mp3) {
			mix_samples(buffer, shared_data->mp3_buffer[mp3_play_bufsel] + mp3_buffer_offs*2, (length_mp3>>shr)<<1);

			mp3_buffer_offs += length_mp3;
		} else {
			// collect from both buffers..
			int left = 1152 - mp3_buffer_offs;
			if (mp3_play_bufsel == 0)
			{
				mix_samples(buffer, shared_data->mp3_buffer[0] + mp3_buffer_offs*2, (length_mp3>>shr)<<1);
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

	if (shared_ctl->writebuffsel == 1) {
		shared_ctl->writebuff0[writebuff_ptr] = 0xffff;
	} else {
		shared_ctl->writebuff1[writebuff_ptr] = 0xffff;
	}
	writebuff_ptr = 0;

	/* give 940 ym job */
	shared_ctl->writebuffsel ^= 1;
	shared_ctl->length = length;
	shared_ctl->stereo = stereo;

	// make sure we will have enough mp3 samples next frame
	if (cdda_on && mp3_samples_ready < length_mp3)
	{
		shared_ctl->mp3_buffsel ^= 1;
		mp3_job = JOB940_MP3DECODE;
		mp3_samples_ready += 1152;
	}

	add_job_940(JOB940_YM2612UPDATEONE, mp3_job);
	//spend_cycles(512);
	//printf("SRCPND: %08lx, INTMODE: %08lx, INTMASK: %08lx, INTPEND: %08lx\n",
	//	gp2x_memregl[0x4500>>2], gp2x_memregl[0x4504>>2], gp2x_memregl[0x4508>>2], gp2x_memregl[0x4510>>2]);

	return 1;
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
		// printf("loading mp3... "); fflush(stdout);
		fseek(f, 0, SEEK_SET);
		fread(mp3_mem, 1, MP3_SIZE_MAX, f);
		// if (feof(f)) printf("done.\n");
		// else printf("done. mp3 too large, not all data loaded.\n");
		shared_ctl->mp3_len = ftell(f);
		loaded_mp3 = f;
	}

	// seek..
	if (pos) {
		byte_offs  = (shared_ctl->mp3_len << 6) >> 10;
		byte_offs *= pos;
		byte_offs >>= 6;
	}
	// printf("mp3 pos1024: %i, byte_offs %i/%i\n", pos, byte_offs, shared_ctl->mp3_len);

	shared_ctl->mp3_offs = byte_offs;

	// reset buffer pointers..
	mp3_samples_ready = mp3_buffer_offs = mp3_play_bufsel = 0;
	shared_ctl->mp3_buffsel = 1; // will change to 0 on first decode
}


int mp3_get_offset(void)
{
	int offs1024 = 0;
	int cdda_on;

	cdda_on = (PicoMCD & 1) && (PicoOpt&0x800) && !(Pico_mcd->s68k_regs[0x36] & 1) &&
			(Pico_mcd->scd.Status_CDC & 1) && loaded_mp3 && shared_ctl->mp3_offs < shared_ctl->mp3_len;

	if (cdda_on) {
		offs1024  = shared_ctl->mp3_offs << 7;
		offs1024 /= shared_ctl->mp3_len;
		offs1024 <<= 3;
	}
	printf("offs1024=%i (%i/%i)\n", offs1024, shared_ctl->mp3_offs, shared_ctl->mp3_len);

	return offs1024;
}


