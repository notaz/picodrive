// (c) Copyright 2006-2009 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <unistd.h>

#include "../common/emu.h"
#include "../common/menu.h"
#include "../common/plat.h"
#include "../common/arm_utils.h"
#include "../linux/sndout_oss.h"
#include "../linux/fbdev.h"
#include "asm_utils.h"
#include "version.h"

#include <pico/pico_int.h>

//#define USE_320_SCREEN 1


static short __attribute__((aligned(4))) sndBuffer[2*44100/50];
static unsigned char temp_frame[g_screen_width * g_screen_height * 2];
unsigned char *PicoDraw2FB = temp_frame;
const char **renderer_names = NULL;
const char **renderer_names32x = NULL;
char cpu_clk_name[] = "unused";


void pemu_prep_defconfig(void)
{
	// XXX: move elsewhere
	g_menubg_ptr = temp_frame;

	defaultConfig.EmuOpt |= EOPT_VSYNC;
}

void pemu_validate_config(void)
{
}

// FIXME: cleanup
static void osd_text(int x, int y, const char *text)
{
	int len = strlen(text)*8;

	if (0) {
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

	if (0) {
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

static int EmuScanEnd16_32x(unsigned int num)
{
	unsigned int *ps;
	unsigned int *pd;
	int len;

	ps = (unsigned int *)temp_frame;
	pd = (unsigned int *)g_screen_ptr + (num*800*2 + 800/2 - 320*2/2) / 2;

	for (len = 320/2; len > 0; len--, ps++) {
		unsigned int p, p1;
		p1 = *ps;
		p = p1 << 16;
		p |= p >> 16;
		*pd = pd[800/2] = p;
		pd++;

		p = p1 >> 16;
		p |= p << 16;
		*pd = pd[800/2] = p;
		pd++;
	}

	return 0;
}
#endif // USE_320_SCREEN

void pemu_finalize_frame(const char *fps, const char *notice)
{
	if (notice || (currentConfig.EmuOpt & EOPT_SHOW_FPS)) {
		if (notice)
			osd_text(4, 464, notice);
		if (currentConfig.EmuOpt & EOPT_SHOW_FPS)
			osd_text(640, 464, fps);
	}
	if ((PicoAHW & PAHW_MCD) && (currentConfig.EmuOpt & EOPT_EN_CD_LEDS))
		draw_cd_leds();
}

void plat_video_toggle_renderer(int change, int is_menu)
{
	// this will auto-select SMS/32X renderers
	PicoDrawSetOutFormat(PDF_RGB555, 1);
}

void plat_video_menu_enter(int is_rom_loaded)
{
}

void plat_video_menu_begin(void)
{
	memcpy32(g_screen_ptr, g_menubg_ptr, g_screen_width * g_screen_height * 2 / 4);
}

void plat_video_menu_end(void)
{
	plat_video_flip();
}

void plat_status_msg_clear(void)
{
	int s = g_screen_width * g_screen_height * 2;
	int l = g_screen_width * 16 * 2;
	int i;

	for (i = 0; i < fbdev_buffer_count; i++)
		memset32((int *)((char *)fbdev_buffers[i] + s - l), 0, l / 4);
}

void plat_status_msg_busy_next(const char *msg)
{
	plat_status_msg_clear();
	pemu_finalize_frame("", msg);
	plat_video_flip();
	emu_status_msg("");
	reset_timing = 1;
}

void plat_status_msg_busy_first(const char *msg)
{
//	memset32(g_screen_ptr, 0, g_screen_width * g_screen_height * 2 / 4);
	plat_status_msg_busy_next(msg);
}

void plat_update_volume(int has_changed, int is_up)
{
	static int prev_frame = 0, wait_frames = 0;
	int vol = currentConfig.volume;

	if (has_changed)
	{
		if (is_up) {
			if (vol < 99) vol++;
		} else {
			if (vol >  0) vol--;
		}
		wait_frames = 0;
		sndout_oss_setvol(vol, vol);
		currentConfig.volume = vol;
		emu_status_msg("VOL: %02i", vol);
		prev_frame = Pico.m.frame_count;
	}
}

void pemu_forced_frame(int opts)
{
	int po_old = PicoOpt;

	PicoOpt &= ~0x10;
	PicoOpt |= opts|POPT_ACC_SPRITES; // acc_sprites

	Pico.m.dirtyPal = 1;
	PicoFrameDrawOnly();

	PicoOpt = po_old;
}

static void updateSound(int len)
{
	unsigned int t;

	len <<= 1;
	if (PicoOpt & POPT_EN_STEREO)
		len <<= 1;

	// sndout_oss_can_write() not reliable..
	if ((currentConfig.EmuOpt & EOPT_NO_FRMLIMIT) && !sndout_oss_can_write(len))
		return;

	/* avoid writing audio when lagging behind to prevent audio lag */
	if (PicoSkipFrame == 2)
		return;

	t = plat_get_ticks_ms();
	sndout_oss_write(PsndOut, len);
	t = plat_get_ticks_ms() - t;
	if (t > 1)
		printf("audio lag %u\n", t);
}

void pemu_sound_start(void)
{
	int target_fps = Pico.m.pal ? 50 : 60;

	PsndOut = NULL;

	if (currentConfig.EmuOpt & EOPT_EN_SOUND)
	{
		int snd_excess_add, frame_samples;
		int is_stereo = (PicoOpt & POPT_EN_STEREO) ? 1 : 0;

		PsndRerate(Pico.m.frame_count ? 1 : 0);

		frame_samples = PsndLen;
		snd_excess_add = ((PsndRate - PsndLen * target_fps)<<16) / target_fps;
		if (snd_excess_add != 0)
			frame_samples++;

		printf("starting audio: %i len: %i (ex: %04x) stereo: %i, pal: %i\n",
			PsndRate, PsndLen, snd_excess_add, is_stereo, Pico.m.pal);
		sndout_oss_start(PsndRate, frame_samples * 2, is_stereo);
		//sndout_oss_setvol(currentConfig.volume, currentConfig.volume);
		PicoWriteSound = updateSound;
		plat_update_volume(0, 0);
		memset(sndBuffer, 0, sizeof(sndBuffer));
		PsndOut = sndBuffer;
	}
}

void pemu_sound_stop(void)
{
	sndout_oss_stop();
}

void pemu_sound_wait(void)
{
	// don't need to do anything, writes will block by themselves
}

void plat_debug_cat(char *str)
{
}

void emu_video_mode_change(int start_line, int line_count, int is_32cols)
{
	int i;

	// clear whole screen in all buffers
	for (i = 0; i < fbdev_buffer_count; i++)
		memset32(fbdev_buffers[i], 0, g_screen_width * g_screen_height * 2 / 4);

#ifdef USE_320_SCREEN
	PicoDrawSetOutFormat(PDF_RGB555, 1);
	PicoScanBegin = EmuScanBegin16;
#else
	if (PicoAHW & PAHW_32X) {
		DrawLineDest = (unsigned short *)temp_frame;
		PicoDrawSetOutFormat(PDF_RGB555, 1);
		PicoScanBegin = NULL;
		PicoScanEnd = EmuScanEnd16_32x;
	} else {
		PicoDrawSetOutFormat(PDF_NONE, 0);
		PicoScanBegin = NULL;
		PicoScanEnd = EmuScanEnd16;
	}
#endif
}

void pemu_loop_prep(void)
{
	emu_video_mode_change(0, 0, 0);
	pemu_sound_start();
}

void pemu_loop_end(void)
{
	int po_old = PicoOpt;
	int eo_old = currentConfig.EmuOpt;

	pemu_sound_stop();
	memset32(g_screen_ptr, 0, g_screen_width * g_screen_height * 2 / 4);

	/* do one more frame for menu bg */
	PicoOpt &= ~POPT_ALT_RENDERER;
	PicoOpt |= POPT_EN_SOFTSCALE|POPT_ACC_SPRITES;
	currentConfig.EmuOpt |= EOPT_16BPP;

	PicoDrawSetOutFormat(PDF_RGB555, 1);
	Pico.m.dirtyPal = 1;
	PicoFrame();

	PicoOpt = po_old;
	currentConfig.EmuOpt = eo_old;
}

void plat_wait_till_us(unsigned int us_to)
{
	unsigned int now;
	signed int diff;

	now = plat_get_ticks_us();

	// XXX: need to check NOHZ and djw kernel
	diff = (signed int)(us_to - now);
	if (diff > 10000) {
		//printf("sleep %d\n", us_to - now);
		usleep(diff * 15 / 16);
		now = plat_get_ticks_us();
		//printf(" wake %d\n", (signed)(us_to - now));
	}
/*
	while ((signed int)(us_to - now) > 512) {
		spend_cycles(1024);
		now = plat_get_ticks_us();
	}
*/
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
