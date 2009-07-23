// (c) Copyright 2006-2009 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdarg.h>

#include "plat_gp2x.h"
#include "soc.h"
#include "../common/plat.h"
#include "../common/menu.h"
#include "../common/arm_utils.h"
#include "../common/fonts.h"
#include "../common/emu.h"
#include "../common/config.h"
#include "../linux/sndout_oss.h"
#include "version.h"

#include <pico/pico_int.h>
#include <pico/patch.h>
#include <pico/sound/mix.h>
#include <zlib/zlib.h>

//#define PFRAMES

#ifdef BENCHMARK
#define OSD_FPS_X 220
#else
#define OSD_FPS_X 260
#endif


extern int crashed_940;

static short __attribute__((aligned(4))) sndBuffer[2*44100/50];
static struct timeval noticeMsgTime = { 0, 0 };	// when started showing
static int osd_fps_x;
static int gp2x_old_gamma = 100;
static char noticeMsg[40];
static unsigned char PicoDraw2FB_[(8+320) * (8+240+8)];
unsigned char *PicoDraw2FB = PicoDraw2FB_;


void plat_status_msg(const char *format, ...)
{
	va_list vl;

	va_start(vl, format);
	vsnprintf(noticeMsg, sizeof(noticeMsg), format, vl);
	va_end(vl);

	gettimeofday(&noticeMsgTime, 0);
}

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


static void scaling_update(void)
{
	PicoOpt &= ~(POPT_DIS_32C_BORDER|POPT_EN_SOFTSCALE);
	switch (currentConfig.scaling) {
		default:break;
		case EOPT_SCALE_HW_H:
		case EOPT_SCALE_HW_HV:
			PicoOpt |= POPT_DIS_32C_BORDER;
			break;
		case EOPT_SCALE_SW_H:
			PicoOpt |= POPT_EN_SOFTSCALE;
			break;
	}
}


void pemu_prep_defconfig(void)
{
	gp2x_soc_t soc;

	memset(&defaultConfig, 0, sizeof(defaultConfig));
	defaultConfig.EmuOpt    = 0x9d | EOPT_RAM_TIMINGS | 0x600; // | <- confirm_save, cd_leds
	defaultConfig.s_PicoOpt = 0x0f | POPT_EN_MCD_PCM|POPT_EN_MCD_CDDA|POPT_EN_SVP_DRC|POPT_ACC_SPRITES;
	defaultConfig.s_PsndRate = 44100;
	defaultConfig.s_PicoRegion = 0; // auto
	defaultConfig.s_PicoAutoRgnOrder = 0x184; // US, EU, JP
	defaultConfig.s_PicoCDBuffers = 0;
	defaultConfig.Frameskip = -1; // auto
	defaultConfig.CPUclock = default_cpu_clock;
	defaultConfig.volume = 50;
	defaultConfig.gamma = 100;
	defaultConfig.scaling = 0;
	defaultConfig.turbo_rate = 15;

	soc = soc_detect();
	if (soc == SOCID_MMSP2)
		defaultConfig.s_PicoOpt |= POPT_EXT_FM;
}

static void osd_text(int x, int y, const char *text)
{
	int len = strlen(text)*8;
	int *p, i, h, offs;

	if ((PicoOpt&0x10)||!(currentConfig.EmuOpt&0x80)) {
		len = (len+3) >> 2;
		for (h = 0; h < 8; h++) {
			offs = (x + g_screen_width * (y+h)) & ~3;
			p = (int *) ((char *)g_screen_ptr + offs);
			for (i = len; i; i--, p++)
				*p = 0xe0e0e0e0;
		}
		emu_textOut8(x, y, text);
	} else {
		len = (len+1) >> 1;
		for (h = 0; h < 8; h++) {
			offs = (x + g_screen_width * (y+h)) & ~1;
			p = (int *) ((short *)g_screen_ptr + offs);
			for (i = len; i; i--, p++)
				*p = (*p >> 2) & 0x39e7;
		}
		emu_textOut16(x, y, text);
	}
}

static void draw_cd_leds(void)
{
	int old_reg;
	old_reg = Pico_mcd->s68k_regs[0];

	if ((PicoOpt & POPT_ALT_RENDERER) || !(currentConfig.EmuOpt & EOPT_16BPP)) {
		// 8-bit modes
		unsigned int col_g = (old_reg & 2) ? 0xc0c0c0c0 : 0xe0e0e0e0;
		unsigned int col_r = (old_reg & 1) ? 0xd0d0d0d0 : 0xe0e0e0e0;
		*(unsigned int *)((char *)g_screen_ptr + 320*2+ 4) =
		*(unsigned int *)((char *)g_screen_ptr + 320*3+ 4) =
		*(unsigned int *)((char *)g_screen_ptr + 320*4+ 4) = col_g;
		*(unsigned int *)((char *)g_screen_ptr + 320*2+12) =
		*(unsigned int *)((char *)g_screen_ptr + 320*3+12) =
		*(unsigned int *)((char *)g_screen_ptr + 320*4+12) = col_r;
	} else {
		// 16-bit modes
		unsigned int *p = (unsigned int *)((short *)g_screen_ptr + 320*2+4);
		unsigned int col_g = (old_reg & 2) ? 0x06000600 : 0;
		unsigned int col_r = (old_reg & 1) ? 0xc000c000 : 0;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += 320/2 - 12/2;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += 320/2 - 12/2;
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

	p += 320 * (pico_pen_y + PICO_PEN_ADJUST_Y);
	p += pico_pen_x + PICO_PEN_ADJUST_X;
	p[0]   ^= 0xffff;
	p[319] ^= 0xffff;
	p[320] ^= 0xffff;
	p[321] ^= 0xffff;
	p[640] ^= 0xffff;
}

static int EmuScanBegin16(unsigned int num)
{
	if (!(Pico.video.reg[1]&8)) num += 8;
	DrawLineDest = (unsigned short *) g_screen_ptr + g_screen_width * num;

	return 0;
}

static int EmuScanBegin8(unsigned int num)
{
	if (!(Pico.video.reg[1]&8)) num += 8;
	DrawLineDest = (unsigned char *)  g_screen_ptr + g_screen_width * num;

	return 0;
}

int localPal[0x100];
static void (*vidCpyM2)(void *dest, void *src) = NULL;

static void blit(const char *fps, const char *notice)
{
	int emu_opt = currentConfig.EmuOpt;

	if (PicoOpt & POPT_ALT_RENDERER)
	{
		// 8bit fast renderer
		if (Pico.m.dirtyPal) {
			Pico.m.dirtyPal = 0;
			vidConvCpyRGB32(localPal, Pico.cram, 0x40);
			// feed new palette to our device
			gp2x_video_setpalette(localPal, 0x40);
		}
		// a hack for VR
		if (PicoRead16Hook == PicoSVPRead16)
			memset32((int *)(PicoDraw2FB+328*8+328*223), 0xe0e0e0e0, 328);
		// do actual copy
		vidCpyM2((unsigned char *)g_screen_ptr+320*8, PicoDraw2FB+328*8);
	}
	else if (!(emu_opt & EOPT_16BPP))
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
			}
			else {
				vidConvCpyRGB32(localPal, Pico.cram, 0x40);
				memcpy32(localPal+0x80, localPal, 0x40); // for spr prio mess
			}
			if (pallen > 0xc0) {
				localPal[0xc0] = 0x0000c000;
				localPal[0xd0] = 0x00c00000;
				localPal[0xe0] = 0x00000000; // reserved pixels for OSD
				localPal[0xf0] = 0x00ffffff;
			}
			gp2x_video_setpalette(localPal, pallen);
		}
	}

	if (notice || (emu_opt & 2)) {
		int h = 232;
		if (currentConfig.scaling == EOPT_SCALE_HW_HV && !(Pico.video.reg[1]&8))
			h -= 8;
		if (notice)
			osd_text(4, h, notice);
		if (emu_opt & 2)
			osd_text(osd_fps_x, h, fps);
	}
	if ((emu_opt & 0x400) && (PicoAHW & PAHW_MCD))
		draw_cd_leds();
	if (PicoAHW & PAHW_PICO)
		draw_pico_ptr();

	gp2x_video_flip();

	if (!(PicoOpt & POPT_ALT_RENDERER)) {
		if (!(Pico.video.reg[1]&8)) {
			if (currentConfig.EmuOpt & EOPT_16BPP)
				DrawLineDest = (unsigned short *) g_screen_ptr + 320*8;
			else
				DrawLineDest = (unsigned char  *) g_screen_ptr + 320*8;
		} else {
			DrawLineDest = g_screen_ptr;
		}
	}
}

// clears whole screen or just the notice area (in all buffers)
static void clearArea(int full)
{
	if ((PicoOpt&0x10)||!(currentConfig.EmuOpt&0x80)) {
		// 8-bit renderers
		if (full) gp2x_memset_all_buffers(0, 0xe0, 320*240);
		else      gp2x_memset_all_buffers(320*232, 0xe0, 320*8);
	} else {
		// 16bit accurate renderer
		if (full) gp2x_memset_all_buffers(0, 0, 320*240*2);
		else      gp2x_memset_all_buffers(320*232*2, 0, 320*8*2);
	}
}

void plat_status_msg_busy_next(const char *msg)
{
	clearArea(0);
	blit("", msg);

	/* assumption: msg_busy_next gets called only when
	 * something slow is about to happen */
	reset_timing = 1;
}

void plat_status_msg_busy_first(const char *msg)
{
	gp2x_memcpy_all_buffers(g_screen_ptr, 0, 320*240*2);
	plat_status_msg_busy_next(msg);
}

static void vidResetMode(void)
{
	if (PicoOpt & POPT_ALT_RENDERER) {
		gp2x_video_changemode(8);
	} else if (currentConfig.EmuOpt & EOPT_16BPP) {
		gp2x_video_changemode(16);
		PicoDrawSetColorFormat(1);
		PicoScanBegin = EmuScanBegin16;
	} else {
		gp2x_video_changemode(8);
		PicoDrawSetColorFormat(2);
		PicoScanBegin = EmuScanBegin8;
	}
	if ((PicoOpt & POPT_ALT_RENDERER) || !(currentConfig.EmuOpt & EOPT_16BPP)) {
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
	if (currentConfig.scaling == EOPT_SCALE_HW_HV && !(Pico.video.reg[1]&8))
	     gp2x_video_RGB_setscaling(8, (PicoOpt&0x100)&&!(Pico.video.reg[12]&1) ? 256 : 320, 224);
	else gp2x_video_RGB_setscaling(0, (PicoOpt&0x100)&&!(Pico.video.reg[12]&1) ? 256 : 320, 240);
}

void plat_video_toggle_renderer(void)
{
	if (PicoOpt & POPT_ALT_RENDERER) {
		PicoOpt &= ~POPT_ALT_RENDERER;
		currentConfig.EmuOpt |= EOPT_16BPP;
	} else if (!(currentConfig.EmuOpt & EOPT_16BPP))
		PicoOpt |= POPT_ALT_RENDERER;
	else
		currentConfig.EmuOpt &= ~EOPT_16BPP;

	vidResetMode();

	if (PicoOpt & POPT_ALT_RENDERER) {
		plat_status_msg(" 8bit fast renderer");
	} else if (currentConfig.EmuOpt & EOPT_16BPP) {
		plat_status_msg("16bit accurate renderer");
	} else {
		plat_status_msg(" 8bit accurate renderer");
	}
}

#if 0 // TODO
static void RunEventsPico(unsigned int events)
{
	int ret, px, py, lim_x;
	static int pdown_frames = 0;

	// for F200
	ret = gp2x_touchpad_read(&px, &py);
	if (ret >= 0)
	{
		if (ret > 35000)
		{
			if (pdown_frames++ > 5)
				PicoPad[0] |= 0x20;

			pico_pen_x = px;
			pico_pen_y = py;
			if (!(Pico.video.reg[12]&1)) {
				pico_pen_x -= 32;
				if (pico_pen_x <   0) pico_pen_x = 0;
				if (pico_pen_x > 248) pico_pen_x = 248;
			}
			if (pico_pen_y > 224) pico_pen_y = 224;
		}
		else
			pdown_frames = 0;

		//if (ret == 0)
		//	PicoPicohw.pen_pos[0] = PicoPicohw.pen_pos[1] = 0x8000;
	}
}
#endif

void plat_update_volume(int has_changed, int is_up)
{
	static int prev_frame = 0, wait_frames = 0;
	int vol = currentConfig.volume;
	int need_low_volume = 0;
	gp2x_soc_t soc;

	soc = soc_detect();
	if ((PicoOpt & POPT_EN_STEREO) && soc == SOCID_MMSP2)
		need_low_volume = 1;

	if (has_changed)
	{
		if (need_low_volume && vol < 5 && prev_frame == Pico.m.frame_count - 1 && wait_frames < 12)
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
		plat_status_msg("VOL: %02i", vol);
		prev_frame = Pico.m.frame_count;
	}

	if (need_low_volume)
		return;

	/* set the right mixer func */
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

void pemu_sound_start(void)
{
	static int PsndRate_old = 0, PicoOpt_old = 0, pal_old = 0;
	int target_fps = Pico.m.pal ? 50 : 60;

	PsndOut = NULL;

	// prepare sound stuff
	if (currentConfig.EmuOpt & 4)
	{
		int snd_excess_add;
		if (PsndRate != PsndRate_old || (PicoOpt&0x20b) != (PicoOpt_old&0x20b) || Pico.m.pal != pal_old ||
				((PicoOpt&0x200) && crashed_940)) {
			PsndRerate(Pico.m.frame_count ? 1 : 0);
		}
		snd_excess_add = ((PsndRate - PsndLen*target_fps)<<16) / target_fps;
		printf("starting audio: %i len: %i (ex: %04x) stereo: %i, pal: %i\n",
			PsndRate, PsndLen, snd_excess_add, (PicoOpt&8)>>3, Pico.m.pal);
		sndout_oss_start(PsndRate, 16, (PicoOpt&8)>>3);
		sndout_oss_setvol(currentConfig.volume, currentConfig.volume);
		PicoWriteSound = updateSound;
		plat_update_volume(0, 0);
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

	PicoOpt &= ~POPT_ALT_RENDERER;
	PicoOpt |= opts|POPT_ACC_SPRITES;
	currentConfig.EmuOpt |= EOPT_16BPP;

	PicoDrawSetColorFormat(1);
	PicoScanBegin = EmuScanBegin16;
	Pico.m.dirtyPal = 1;
	PicoFrameDrawOnly();

/*
	if (!(Pico.video.reg[12]&1)) {
		vidCpyM2 = vidCpyM2_32col;
		clearArea(1);
	} else	vidCpyM2 = vidCpyM2_40col;

	vidCpyM2((unsigned char *)g_screen_ptr+320*8, PicoDraw2FB+328*8);
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


#if 0
static void tga_dump(void)
{
#define BYTE unsigned char
#define WORD unsigned short
	struct
	{
		BYTE IDLength;        /* 00h  Size of Image ID field */
		BYTE ColorMapType;    /* 01h  Color map type */
		BYTE ImageType;       /* 02h  Image type code */
		WORD CMapStart;       /* 03h  Color map origin */
		WORD CMapLength;      /* 05h  Color map length */
		BYTE CMapDepth;       /* 07h  Depth of color map entries */
		WORD XOffset;         /* 08h  X origin of image */
		WORD YOffset;         /* 0Ah  Y origin of image */
		WORD Width;           /* 0Ch  Width of image */
		WORD Height;          /* 0Eh  Height of image */
		BYTE PixelDepth;      /* 10h  Image pixel size */
		BYTE ImageDescriptor; /* 11h  Image descriptor byte */
	} __attribute__((packed)) TGAHEAD;
	static unsigned short oldscr[320*240];
	FILE *f; char name[128]; int i;

	memset(&TGAHEAD, 0, sizeof(TGAHEAD));
	TGAHEAD.ImageType = 2;
	TGAHEAD.Width = 320;
	TGAHEAD.Height = 240;
	TGAHEAD.PixelDepth = 16;
	TGAHEAD.ImageDescriptor = 2<<4; // image starts at top-left

#define CONV(X) (((X>>1)&0x7fe0)|(X&0x1f)) // 555?

	for (i = 0; i < 320*240; i++)
		if(oldscr[i] != CONV(((unsigned short *)g_screen_ptr)[i])) break;
	if (i < 320*240)
	{
		for (i = 0; i < 320*240; i++)
			oldscr[i] = CONV(((unsigned short *)g_screen_ptr)[i]);
		sprintf(name, "%05i.tga", Pico.m.frame_count);
		f = fopen(name, "wb");
		if (!f) { printf("!f\n"); exit(1); }
		fwrite(&TGAHEAD, 1, sizeof(TGAHEAD), f);
		fwrite(oldscr, 1, 320*240*2, f);
		fclose(f);
	}
}
#endif


void pemu_loop(void)
{
	static int gp2x_old_clock = -1, EmuOpt_old = 0;
	char fpsbuff[24]; // fps count c string
	struct timeval tval; // timing
	int pframes_done, pframes_shown, pthissec; // "period" frames, used for sync
	int  frames_done,  frames_shown,  thissec; // actual frames
	int oldmodes = 0, target_fps, target_frametime, lim_time, vsync_offset, i;
	char *notice = 0;

	printf("entered emu_Loop()\n");

	if ((EmuOpt_old ^ currentConfig.EmuOpt) & EOPT_RAM_TIMINGS) {
		if (currentConfig.EmuOpt & EOPT_RAM_TIMINGS)
			set_ram_timings();
		else
			unset_ram_timings();
	}

	if (gp2x_old_clock < 0)
		gp2x_old_clock = default_cpu_clock;
	if (gp2x_old_clock != currentConfig.CPUclock) {
		printf("changing clock to %i...", currentConfig.CPUclock); fflush(stdout);
		gp2x_set_cpuclk(currentConfig.CPUclock);
		gp2x_old_clock = currentConfig.CPUclock;
		printf(" done\n");
	}

	if (gp2x_old_gamma != currentConfig.gamma || (EmuOpt_old&0x1000) != (currentConfig.EmuOpt&0x1000)) {
		set_lcd_gamma(currentConfig.gamma, !!(currentConfig.EmuOpt&0x1000));
		gp2x_old_gamma = currentConfig.gamma;
		printf("updated gamma to %i, A_SN's curve: %i\n", currentConfig.gamma, !!(currentConfig.EmuOpt&0x1000));
	}

	if ((EmuOpt_old ^ currentConfig.EmuOpt) & EOPT_PSYNC) {
		if (currentConfig.EmuOpt & EOPT_PSYNC)
			set_lcd_custom_rate(Pico.m.pal);
		else
			unset_lcd_custom_rate();
	}

	if ((EmuOpt_old ^ currentConfig.EmuOpt) & EOPT_MMUHACK)
		gp2x_make_fb_bufferable(currentConfig.EmuOpt & EOPT_MMUHACK);

	EmuOpt_old = currentConfig.EmuOpt;
	fpsbuff[0] = 0;

	// make sure we are in correct mode
	vidResetMode();
	scaling_update();
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
		gp2x_video_wait_vsync();
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
			int scalex = 320;
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
			/* want vertical scaling and game is not in 240 line mode */
			if (currentConfig.scaling == EOPT_SCALE_HW_HV && !(modes&8))
			     gp2x_video_RGB_setscaling(8, scalex, 224);
			else gp2x_video_RGB_setscaling(0, scalex, 240);
			oldmodes = modes;
			clearArea(1);
		}

		// second changed?
		if (thissec != tval.tv_sec)
		{
#ifdef BENCHMARK
			static int bench = 0, bench_fps = 0, bench_fps_s = 0, bfp = 0, bf[4];
			if (++bench == 10) {
				bench = 0;
				bench_fps_s = bench_fps;
				bf[bfp++ & 3] = bench_fps;
				bench_fps = 0;
			}
			bench_fps += frames_shown;
			sprintf(fpsbuff, "%02i/%02i/%02i", frames_shown, bench_fps_s, (bf[0]+bf[1]+bf[2]+bf[3])>>2);
#else
			if (currentConfig.EmuOpt & 2) {
				sprintf(fpsbuff, "%02i/%02i", frames_shown, frames_done);
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
				if (PsndOut && pframes_done < target_fps && pframes_done > target_fps-5) {
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
					gp2x_video_wait_vsync();
				} else {
					simpleWait(pthissec, lim_time);
				}
			}
		}

		blit(fpsbuff, notice);

		pframes_done++; pframes_shown++;
		 frames_done++;  frames_shown++;
	}

	emu_set_fastforward(0);

	if (PicoAHW & PAHW_MCD)
		PicoCDBufferFree();

	// save SRAM
	if ((currentConfig.EmuOpt & EOPT_EN_SRAM) && SRam.changed) {
		plat_status_msg_busy_first("Writing SRAM/BRAM...");
		emu_save_load_game(0, 1);
		SRam.changed = 0;
	}

	// if in 8bit mode, generate 16bit image for menu background
	if ((PicoOpt & POPT_ALT_RENDERER) || !(currentConfig.EmuOpt & EOPT_16BPP))
		pemu_forced_frame(POPT_EN_SOFTSCALE);
}

const char *plat_get_credits(void)
{
	return "PicoDrive v" VERSION " (c) notaz, 2006-2009\n\n\n"
		"Credits:\n"
		"fDave: Cyclone 68000 core,\n"
		"      base code of PicoDrive\n"
		"Reesy & FluBBa: DrZ80 core\n"
		"MAME devs: YM2612 and SN76496 cores\n"
		"rlyeh and others: minimal SDK\n"
		"Squidge: mmuhack\n"
		"Dzz: ARM940 sample\n"
		"GnoStiC / Puck2099: USB joy code\n"
		"craigix: GP2X hardware\n"
		"ketchupgun: skin design\n"
		"\n"
		"special thanks (for docs, ideas):\n"
		" Charles MacDonald, Haze,\n"
		" Stephane Dallongeville,\n"
		" Lordus, Exophase, Rokas,\n"
		" Nemesis, Tasco Deluxe";
}
