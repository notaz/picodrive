// (c) Copyright 2006-2009 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdarg.h>

#include "../common/arm_utils.h"
#include "../common/fonts.h"
#include "../common/emu.h"
#include "../common/menu.h"
#include "../common/config.h"
#include "../common/input.h"
#include "../linux/sndout_oss.h"
#include "asm_utils.h"

#include <pico/pico_int.h>
#include <pico/patch.h>
#include <pico/sound/mix.h>
#include <zlib/zlib.h>

//#define PFRAMES
#define BENCHMARK
//#define USE_320_SCREEN 1

#ifdef BENCHMARK
#define OSD_FPS_X (800-200)
#else
#define OSD_FPS_X (800-120)
#endif


static short __attribute__((aligned(4))) sndBuffer[2*44100/50];
static int osd_fps_x;
unsigned char *PicoDraw2FB = NULL;  // temporary buffer for alt renderer

#define PICO_PEN_ADJUST_X 4
#define PICO_PEN_ADJUST_Y 2
static int pico_pen_x = 0, pico_pen_y = 240/2;


int plat_get_root_dir(char *dst, int len)
{
	extern char **g_argv;
	int j;

	strncpy(dst, g_argv[0], len);
	len -= 32; // reserve
	if (len < 0) len = 0;
	dst[len] = 0;
	for (j = strlen(dst); j > 0; j--)
		if (dst[j] == '/') { dst[j+1] = 0; break; }

	return j + 1;
}

void pemu_prep_defconfig(void)
{
	memset(&defaultConfig, 0, sizeof(defaultConfig));
	defaultConfig.EmuOpt    = 0x8f | 0x00600; // | <- confirm_save, cd_leds
	defaultConfig.s_PicoOpt  = 0x0f | POPT_EXT_FM|POPT_EN_MCD_PCM|POPT_EN_MCD_CDDA|POPT_EN_SVP_DRC;
	defaultConfig.s_PicoOpt |= POPT_ACC_SPRITES|POPT_EN_MCD_GFX;
	defaultConfig.EmuOpt    &= ~8; // no save gzip
	defaultConfig.s_PsndRate = 44100;
	defaultConfig.s_PicoRegion = 0;
	defaultConfig.s_PicoAutoRgnOrder = 0x184; // US, EU, JP
	defaultConfig.s_PicoCDBuffers = 0;
	defaultConfig.Frameskip = 0;
	defaultConfig.CPUclock = 200;
	defaultConfig.volume = 50;
	defaultConfig.scaling = 0;
	defaultConfig.turbo_rate = 15;
}

static void textOut16(int x, int y, const char *text)
{
	int i,l,len=strlen(text);
	unsigned int *screen = (unsigned int *)((unsigned short *)g_screen_ptr + (x&~1) + y*g_screen_width);

	for (i = 0; i < len; i++)
	{
		for (l=0;l<16;)
		{
			unsigned char fd = fontdata8x8[((text[i])*8)+l/2];
			unsigned int *d = &screen[l*g_screen_width/2];
			if (fd&0x80) d[0]=0xffffffff;
			if (fd&0x40) d[1]=0xffffffff;
			if (fd&0x20) d[2]=0xffffffff;
			if (fd&0x10) d[3]=0xffffffff;
			if (fd&0x08) d[4]=0xffffffff;
			if (fd&0x04) d[5]=0xffffffff;
			if (fd&0x02) d[6]=0xffffffff;
			if (fd&0x01) d[7]=0xffffffff;
			l++; d = &screen[l*g_screen_width/2];
			if (fd&0x80) d[0]=0xffffffff;
			if (fd&0x40) d[1]=0xffffffff;
			if (fd&0x20) d[2]=0xffffffff;
			if (fd&0x10) d[3]=0xffffffff;
			if (fd&0x08) d[4]=0xffffffff;
			if (fd&0x04) d[5]=0xffffffff;
			if (fd&0x02) d[6]=0xffffffff;
			if (fd&0x01) d[7]=0xffffffff;
			l++;
		}
		screen += 8;
	}
}


static void osd_text(int x, int y, const char *text)
{
	int len = strlen(text)*8;

	if ((PicoOpt&0x10)||!(currentConfig.EmuOpt&0x80)) {
		int *p, i, h;
		x &= ~3; // align x
		len = (len+3) >> 2;
		for (h = 0; h < 8; h++) {
			p = (int *) ((unsigned char *) g_screen_ptr+x+g_screen_width*(y+h));
			for (i = len; i; i--, p++) *p = 0xe0e0e0e0;
		}
		emu_text_out8(x, y, text);
	} else {
		int *p, i, h;
		x &= ~1; // align x
		len++;
		for (h = 0; h < 16; h++) {
			p = (int *) ((unsigned short *) g_screen_ptr+x+g_screen_width*(y+h));
			for (i = len; i; i--, p++) *p = 0;//(*p>>2)&0x39e7;
		}
		text_out16(x, y, text);
	}
}

static void draw_cd_leds(void)
{
//	static
	int old_reg;
//	if (!((Pico_mcd->s68k_regs[0] ^ old_reg) & 3)) return; // no change // mmu hack problems?
	old_reg = Pico_mcd->s68k_regs[0];

	if ((PicoOpt&0x10)||!(currentConfig.EmuOpt&0x80)) {
		// 8-bit modes
		unsigned int col_g = (old_reg & 2) ? 0xc0c0c0c0 : 0xe0e0e0e0;
		unsigned int col_r = (old_reg & 1) ? 0xd0d0d0d0 : 0xe0e0e0e0;
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*2+ 4) =
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*3+ 4) =
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*4+ 4) = col_g;
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*2+12) =
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*3+12) =
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*4+12) = col_r;
	} else {
		// 16-bit modes
		unsigned int *p = (unsigned int *)((short *)g_screen_ptr + g_screen_width*2+4);
		unsigned int col_g = (old_reg & 2) ? 0x06000600 : 0;
		unsigned int col_r = (old_reg & 1) ? 0xc000c000 : 0;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += g_screen_width/2 - 12/2;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += g_screen_width/2 - 12/2;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r;
	}
}

static void draw_pico_ptr(void)
{
	unsigned short *p = (unsigned short *)g_screen_ptr;

	// only if pen enabled and for 16bit modes
	if (pico_inp_mode == 0 || (PicoOpt&0x10) || !(currentConfig.EmuOpt&0x80)) return;

	if (!(Pico.video.reg[12]&1) && !(PicoOpt&POPT_DIS_32C_BORDER))
		p += 32;

	p += g_screen_width * (pico_pen_y + PICO_PEN_ADJUST_Y);
	p += pico_pen_x + PICO_PEN_ADJUST_X;
	p[0]   ^= 0xffff;
	p[319] ^= 0xffff;
	p[320] ^= 0xffff;
	p[321] ^= 0xffff;
	p[640] ^= 0xffff;
}

#ifdef USE_320_SCREEN

static int EmuScanBegin16(unsigned int num)
{
	DrawLineDest = (unsigned short *)g_screen_ptr + num*800 + 800/2 - 320/2;
	//int w = (Pico.video.reg[12]&1) ? 320 : 256;
	//DrawLineDest = (unsigned short *)g_screen_ptr + num*w;

	return 0;
}

#else // USE_320_SCREEN

static int EmuScanEnd16(unsigned int num)
{
	unsigned char  *ps=HighCol+8;
	unsigned short *pd;
	unsigned short *pal=HighPal;
	int sh = Pico.video.reg[0xC]&8;
	int len, mask = 0xff;

	pd=(unsigned short *)g_screen_ptr + num*800*2 + 800/2 - 320*2/2;

	if (Pico.m.dirtyPal)
		PicoDoHighPal555(sh);

	if (Pico.video.reg[12]&1) {
		len = 320;
	} else {
		pd += 32*2;
		len = 256;
	}

	if (!sh && (rendstatus & PDRAW_SPR_LO_ON_HI))
		mask=0x3f; // messed sprites, upper bits are priority stuff

#if 1
	clut_line(pd, ps, pal, (mask<<16) | len);
#else
	for (; len > 0; len--)
	{
		unsigned int p = pal[*ps++ & mask];
		p |= p << 16;
		*(unsigned int *)pd = p;
		*(unsigned int *)(&pd[800]) = p;
		pd += 2;
	}
#endif

	return 0;
}

#endif // USE_320_SCREEN

int localPal[0x100];
static void (*vidCpyM2)(void *dest, void *src) = NULL;

static void blit(const char *fps, const char *notice)
{
	int emu_opt = currentConfig.EmuOpt;

	if (PicoOpt&0x10)
	{
		// 8bit fast renderer
		if (Pico.m.dirtyPal) {
			Pico.m.dirtyPal = 0;
			vidConvCpyRGB32(localPal, Pico.cram, 0x40);
			// feed new palette to our device
			// gp2x_video_setpalette(localPal, 0x40);
		}
		// a hack for VR
		if (PicoRead16Hook == PicoSVPRead16)
			memset32((int *)(PicoDraw2FB+328*8+328*223), 0xe0e0e0e0, 328);
		// do actual copy
		vidCpyM2((unsigned char *)g_screen_ptr+g_screen_width*8, PicoDraw2FB+328*8);
	}
	else if (!(emu_opt&0x80))
	{
		// 8bit accurate renderer
		if (Pico.m.dirtyPal)
		{
			int pallen = 0xc0;
			Pico.m.dirtyPal = 0;
			if (Pico.video.reg[0xC]&8) // shadow/hilight mode
			{
				vidConvCpyRGB32(localPal, Pico.cram, 0x40);
				vidConvCpyRGB32sh(localPal+0x40, Pico.cram, 0x40);
				vidConvCpyRGB32hi(localPal+0x80, Pico.cram, 0x40);
				memcpy32(localPal+0xc0, localPal+0x40, 0x40);
				pallen = 0x100;
			}
			else if (rendstatus & PDRAW_SONIC_MODE) { // mid-frame palette changes
				vidConvCpyRGB32(localPal, Pico.cram, 0x40);
				vidConvCpyRGB32(localPal+0x40, HighPal, 0x40);
				vidConvCpyRGB32(localPal+0x80, HighPal+0x40, 0x40);
				pallen = 0xc0;
			}
			else {
				vidConvCpyRGB32(localPal, Pico.cram, 0x40);
				memcpy32(localPal+0x80, localPal, 0x40);
			}
			if (pallen > 0xc0) {
				localPal[0xc0] = 0x0000c000;
				localPal[0xd0] = 0x00c00000;
				localPal[0xe0] = 0x00000000; // reserved pixels for OSD
				localPal[0xf0] = 0x00ffffff;
			}
			// gp2x_video_setpalette(localPal, pallen);
		}
	}

	if (notice || (emu_opt & 2)) {
		int h = g_screen_height-16;
		if (currentConfig.scaling == 2 && !(Pico.video.reg[1]&8)) h -= 16;
		if (notice) osd_text(4, h, notice);
		if (emu_opt & 2)
			osd_text(osd_fps_x, h, fps);
	}
	if ((emu_opt & 0x400) && (PicoAHW & PAHW_MCD))
		draw_cd_leds();
	if (PicoAHW & PAHW_PICO)
		draw_pico_ptr();

	//gp2x_video_wait_vsync();
	// gp2x_video_flip();
}


// clears whole screen or just the notice area (in all buffers)
static void clearArea(int full)
{
	if (full) memset(g_screen_ptr, 0, g_screen_width*g_screen_height*2);
	else      memset((short *)g_screen_ptr + g_screen_width * (g_screen_height - 16), 0,
			g_screen_width * 16 * 2);
}


static void vidResetMode(void)
{
#if 0
	if (PicoOpt&0x10) {
		gp2x_video_changemode(8);
	} else if (currentConfig.EmuOpt&0x80) {
		gp2x_video_changemode(16);
#ifdef USE_320_SCREEN
		PicoDrawSetColorFormat(1);
		PicoScanBegin = EmuScanBegin16;
#else
		PicoDrawSetColorFormat(-1);
		PicoScanEnd = EmuScanEnd16;
#endif
	} else {
		gp2x_video_changemode(8);
		PicoDrawSetColorFormat(2);
		PicoScanBegin = EmuScanBegin8;
	}
	if ((PicoOpt&0x10)||!(currentConfig.EmuOpt&0x80)) {
		// setup pal for 8-bit modes
		localPal[0xc0] = 0x0000c000; // MCD LEDs
		localPal[0xd0] = 0x00c00000;
		localPal[0xe0] = 0x00000000; // reserved pixels for OSD
		localPal[0xf0] = 0x00ffffff;
		gp2x_video_setpalette(localPal, 0x100);
		gp2x_memset_all_buffers(0, 0xe0, 320*240);
		gp2x_video_flip();
	}
	Pico.m.dirtyPal = 1;
	// reset scaling
	if (currentConfig.scaling == 2 && !(Pico.video.reg[1]&8))
	     gp2x_video_RGB_setscaling(8, (PicoOpt&0x100)&&!(Pico.video.reg[12]&1) ? 256 : 320, 224);
	else gp2x_video_RGB_setscaling(0, (PicoOpt&0x100)&&!(Pico.video.reg[12]&1) ? 256 : 320, 240);
#else
#ifdef USE_320_SCREEN
	PicoDrawSetColorFormat(1);
	PicoScanBegin = EmuScanBegin16;
#else
	PicoDrawSetColorFormat(-1);
	PicoScanEnd = EmuScanEnd16;
#endif
#endif
}


static void update_volume(int has_changed, int is_up)
{
	static int prev_frame = 0, wait_frames = 0;
	int vol = currentConfig.volume;

	if (has_changed)
	{
		if (vol < 5 && (PicoOpt&8) && prev_frame == Pico.m.frame_count - 1 && wait_frames < 12)
			wait_frames++;
		else {
			if (is_up) {
				if (vol < 99) vol++;
			} else {
				if (vol >  0) vol--;
			}
			wait_frames = 0;
			sndout_oss_setvol(vol, vol);
			currentConfig.volume = vol;
		}
		sprintf(noticeMsg, "VOL: %02i", vol);
		gettimeofday(&noticeMsgTime, 0);
		prev_frame = Pico.m.frame_count;
	}

	// set the right mixer func
	if (!(PicoOpt&8)) return; // just use defaults for mono
	if (vol >= 5)
		PsndMix_32_to_16l = mix_32_to_16l_stereo;
	else {
		mix_32_to_16l_level = 5 - vol;
		PsndMix_32_to_16l = mix_32_to_16l_stereo_lvl;
	}
}


static void updateSound(int len)
{
	if (PicoOpt&8) len<<=1;

	/* avoid writing audio when lagging behind to prevent audio lag */
	if (PicoSkipFrame != 2)
		sndout_oss_write(PsndOut, len<<1);
}


static void SkipFrame(int do_audio)
{
	PicoSkipFrame=do_audio ? 1 : 2;
	PicoFrame();
	PicoSkipFrame=0;
}


void pemu_forced_frame(int opts)
{
	int po_old = PicoOpt;
	int eo_old = currentConfig.EmuOpt;

	PicoOpt &= ~0x10;
	PicoOpt |= opts|POPT_ACC_SPRITES; // acc_sprites
	currentConfig.EmuOpt |= 0x80;

	//vidResetMode();
#ifdef USE_320_SCREEN
	PicoDrawSetColorFormat(1);
	PicoScanBegin = EmuScanBegin16;
#else
	PicoDrawSetColorFormat(-1);
	PicoScanEnd = EmuScanEnd16;
#endif
	Pico.m.dirtyPal = 1;
	PicoFrameDrawOnly();

/*
	if (!(Pico.video.reg[12]&1)) {
		vidCpyM2 = vidCpyM2_32col;
		clearArea(1);
	} else	vidCpyM2 = vidCpyM2_40col;

	vidCpyM2((unsigned char *)g_screen_ptr+g_screen_width*8, PicoDraw2FB+328*8);
	vidConvCpyRGB32(localPal, Pico.cram, 0x40);
	gp2x_video_setpalette(localPal, 0x40);
*/
	PicoOpt = po_old;
	currentConfig.EmuOpt = eo_old;
}

void plat_debug_cat(char *str)
{
}

static void simpleWait(int thissec, int lim_time)
{
	struct timeval tval;

	spend_cycles(1024);
	gettimeofday(&tval, 0);
	if (thissec != tval.tv_sec) tval.tv_usec+=1000000;

	while (tval.tv_usec < lim_time)
	{
		spend_cycles(1024);
		gettimeofday(&tval, 0);
		if (thissec != tval.tv_sec) tval.tv_usec+=1000000;
	}
}

void pemu_sound_start(void)
{
	static int PsndRate_old = 0, PicoOpt_old = 0, pal_old = 0;
	int target_fps = Pico.m.pal ? 50 : 60;

	PsndOut = NULL;

	if (currentConfig.EmuOpt & 4)
	{
		int snd_excess_add;
		if (PsndRate != PsndRate_old || (PicoOpt&0x20b) != (PicoOpt_old&0x20b) || Pico.m.pal != pal_old)
			PsndRerate(Pico.m.frame_count ? 1 : 0);

		snd_excess_add = ((PsndRate - PsndLen*target_fps)<<16) / target_fps;
		printf("starting audio: %i len: %i (ex: %04x) stereo: %i, pal: %i\n",
			PsndRate, PsndLen, snd_excess_add, (PicoOpt&8)>>3, Pico.m.pal);
		sndout_oss_start(PsndRate, 16, (PicoOpt&8)>>3);
		sndout_oss_setvol(currentConfig.volume, currentConfig.volume);
		PicoWriteSound = updateSound;
		update_volume(0, 0);
		memset(sndBuffer, 0, sizeof(sndBuffer));
		PsndOut = sndBuffer;
		PsndRate_old = PsndRate;
		PicoOpt_old  = PicoOpt;
		pal_old = Pico.m.pal;
	}
}

void pemu_sound_stop(void)
{
}

void pemu_sound_wait(void)
{
	// don't need to do anything, writes will block by themselves
}

void pemu_loop(void)
{
	char fpsbuff[24]; // fps count c string
	struct timeval tval; // timing
	int pframes_done, pframes_shown, pthissec; // "period" frames, used for sync
	int  frames_done,  frames_shown,  thissec; // actual frames
	int oldmodes = 0, target_fps, target_frametime, lim_time, vsync_offset, i;
	char *notice = 0;

	printf("entered emu_Loop()\n");

	fpsbuff[0] = 0;

	// make sure we are in correct mode
	vidResetMode();
	Pico.m.dirtyPal = 1;
	oldmodes = ((Pico.video.reg[12]&1)<<2) ^ 0xc;

	// pal/ntsc might have changed, reset related stuff
	target_fps = Pico.m.pal ? 50 : 60;
	target_frametime = 1000000/target_fps;
	reset_timing = 1;

	pemu_sound_start();

	// prepare CD buffer
	if (PicoAHW & PAHW_MCD) PicoCDBufferInit();

	// calc vsync offset to sync timing code with vsync
	if (currentConfig.EmuOpt&0x2000) {
		gettimeofday(&tval, 0);
		//gp2x_video_wait_vsync();
		gettimeofday(&tval, 0);
		vsync_offset = tval.tv_usec;
		while (vsync_offset >= target_frametime)
			vsync_offset -= target_frametime;
		if (!vsync_offset) vsync_offset++;
		printf("vsync_offset: %i\n", vsync_offset);
	} else
		vsync_offset = 0;

	frames_done = frames_shown = thissec =
	pframes_done = pframes_shown = pthissec = 0;

	// loop
	while (engineState == PGS_Running)
	{
		int modes;

		gettimeofday(&tval, 0);
		if (reset_timing) {
			reset_timing = 0;
			pthissec = tval.tv_sec;
			pframes_shown = pframes_done = tval.tv_usec/target_frametime;
		}

		// show notice message?
		if (noticeMsgTime.tv_sec)
		{
			static int noticeMsgSum;
			if((tval.tv_sec*1000000+tval.tv_usec) - (noticeMsgTime.tv_sec*1000000+noticeMsgTime.tv_usec) > 2000000) { // > 2.0 sec
				noticeMsgTime.tv_sec = noticeMsgTime.tv_usec = 0;
				clearArea(0);
				notice = 0;
			} else {
				int sum = noticeMsg[0]+noticeMsg[1]+noticeMsg[2];
				if (sum != noticeMsgSum) { clearArea(0); noticeMsgSum = sum; }
				notice = noticeMsg;
			}
		}

		// check for mode changes
		modes = ((Pico.video.reg[12]&1)<<2)|(Pico.video.reg[1]&8);
		if (modes != oldmodes)
		{
			int scalex = g_screen_width;
			osd_fps_x = OSD_FPS_X;
			if (modes & 4) {
				vidCpyM2 = vidCpyM2_40col;
			} else {
				if (PicoOpt & 0x100) {
					vidCpyM2 = vidCpyM2_32col_nobord;
					scalex = 256;
					osd_fps_x = OSD_FPS_X - 64;
				} else {
					vidCpyM2 = vidCpyM2_32col;
				}
			}
			//if (currentConfig.scaling == 2 && !(modes&8)) // want vertical scaling and game is not in 240 line mode
			//      gp2x_video_RGB_setscaling(8, scalex, 224);
			// else gp2x_video_RGB_setscaling(0, scalex, 240);
			oldmodes = modes;
			clearArea(1);
		}

		// second changed?
		if (thissec != tval.tv_sec)
		{
#ifdef BENCHMARK
			static int bench = 0, bench_fps = 0, bench_fps_s = 0, bfp = 0, bf[4];
			if (++bench == 4) {
				bench = 0;
				bench_fps_s = bench_fps / 4;
				bf[bfp++ & 3] = bench_fps / 4;
				bench_fps = 0;
			}
			bench_fps += frames_shown;
			sprintf(fpsbuff, "%3i/%3i/%3i", frames_shown, bench_fps_s, (bf[0]+bf[1]+bf[2]+bf[3])>>2);
			printf("%s\n", fpsbuff);
#else
			if (currentConfig.EmuOpt & 2) {
				sprintf(fpsbuff, "%3i/%3i", frames_shown, frames_done);
				if (fpsbuff[5] == 0) { fpsbuff[5] = fpsbuff[6] = ' '; fpsbuff[7] = 0; }
			}
#endif
			frames_shown = frames_done = 0;
			thissec = tval.tv_sec;
		}
#ifdef PFRAMES
		sprintf(fpsbuff, "%i", Pico.m.frame_count);
#endif

		if (pthissec != tval.tv_sec)
		{
			if (PsndOut == 0 && currentConfig.Frameskip >= 0) {
				pframes_done = pframes_shown = 0;
			} else {
				// it is quite common for this implementation to leave 1 fame unfinished
				// when second changes, but we don't want buffer to starve.
				if(PsndOut && pframes_done < target_fps && pframes_done > target_fps-5) {
					emu_update_input();
					SkipFrame(1); pframes_done++;
				}

				pframes_done  -= target_fps; if (pframes_done  < 0) pframes_done  = 0;
				pframes_shown -= target_fps; if (pframes_shown < 0) pframes_shown = 0;
				if (pframes_shown > pframes_done) pframes_shown = pframes_done;
			}
			pthissec = tval.tv_sec;
		}

		lim_time = (pframes_done+1) * target_frametime + vsync_offset;
		if (currentConfig.Frameskip >= 0) // frameskip enabled
		{
			for(i = 0; i < currentConfig.Frameskip; i++) {
				emu_update_input();
				SkipFrame(1); pframes_done++; frames_done++;
				if (PsndOut && !reset_timing) { // do framelimitting if sound is enabled
					gettimeofday(&tval, 0);
					if (pthissec != tval.tv_sec) tval.tv_usec+=1000000;
					if (tval.tv_usec < lim_time) { // we are too fast
						simpleWait(pthissec, lim_time);
					}
				}
				lim_time += target_frametime;
			}
		}
		else if (tval.tv_usec > lim_time) // auto frameskip
		{
			// no time left for this frame - skip
			if (tval.tv_usec - lim_time >= 300000) {
				/* something caused a slowdown for us (disk access? cache flush?)
				 * try to recover by resetting timing... */
				reset_timing = 1;
				continue;
			}
			emu_update_input();
			SkipFrame(tval.tv_usec < lim_time+target_frametime*2); pframes_done++; frames_done++;
			continue;
		}

		emu_update_input();
		PicoFrame();

		// check time
		gettimeofday(&tval, 0);
		if (pthissec != tval.tv_sec) tval.tv_usec+=1000000;

		if (currentConfig.Frameskip < 0 && tval.tv_usec - lim_time >= 300000) // slowdown detection
			reset_timing = 1;
#if 1
		else if (PsndOut != NULL || currentConfig.Frameskip < 0)
		{
			// sleep or vsync if we are still too fast
			// usleep sleeps for ~20ms minimum, so it is not a solution here
			if (!reset_timing && tval.tv_usec < lim_time)
			{
				// we are too fast
				if (vsync_offset) {
					if (lim_time - tval.tv_usec > target_frametime/2)
						simpleWait(pthissec, lim_time - target_frametime/4);
					// gp2x_video_wait_vsync();
				} else {
					simpleWait(pthissec, lim_time);
				}
			}
		}
#endif
		blit(fpsbuff, notice);

		pframes_done++; pframes_shown++;
		 frames_done++;  frames_shown++;
	}

	emu_set_fastforward(0);

	if (PicoAHW & PAHW_MCD) PicoCDBufferFree();

	// save SRAM
	if ((currentConfig.EmuOpt & EOPT_EN_SRAM) && SRam.changed) {
		/* FIXME: plat_status_msg_busy_first */
		emu_state_cb("Writing SRAM/BRAM..");
		emu_save_load_game(0, 1);
		SRam.changed = 0;
	}

	// if in 8bit mode, generate 16bit image for menu background
	if ((PicoOpt&0x10) || !(currentConfig.EmuOpt&0x80))
		pemu_forced_frame(POPT_EN_SOFTSCALE);
}

