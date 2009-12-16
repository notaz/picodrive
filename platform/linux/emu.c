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
#include "version.h"

#include <pico/pico_int.h>


static short __attribute__((aligned(4))) sndBuffer[2*44100/50];
unsigned char temp_frame[320 * 240 * 2];
unsigned char *PicoDraw2FB = temp_frame;
static int osd_fps_x;
char cpu_clk_name[] = "unused";

extern void update_screen(void);


void pemu_prep_defconfig(void)
{
	// XXX: move elsewhere
	g_menubg_ptr = temp_frame;
}

void pemu_validate_config(void)
{
	extern int PicoOpt;
//	PicoOpt &= ~POPT_EXT_FM;
	PicoOpt &= ~POPT_EN_SVP_DRC;
}

// FIXME: dupes from GP2X, need cleanup
static void (*osd_text)(int x, int y, const char *text);

static void osd_text8(int x, int y, const char *text)
{
	int len = strlen(text)*8;
	int *p, i, h, offs;

	len = (len+3) >> 2;
	for (h = 0; h < 8; h++) {
		offs = (x + g_screen_width * (y+h)) & ~3;
		p = (int *) ((char *)g_screen_ptr + offs);
		for (i = len; i; i--, p++)
			*p = 0xe0e0e0e0;
	}
	emu_text_out8(x, y, text);
}

static void osd_text16(int x, int y, const char *text)
{
	int len = strlen(text)*8;
	int *p, i, h, offs;

	len = (len+1) >> 1;
	for (h = 0; h < 8; h++) {
		offs = (x + g_screen_width * (y+h)) & ~1;
		p = (int *) ((short *)g_screen_ptr + offs);
		for (i = len; i; i--, p++)
			*p = (*p >> 2) & 0x39e7;
	}
	emu_text_out16(x, y, text);
}

static void draw_cd_leds(void)
{
	int led_reg, pitch, scr_offs, led_offs;
	led_reg = Pico_mcd->s68k_regs[0];

	pitch = 320;
	led_offs = 4;
	scr_offs = pitch * 2 + 4;

	if ((PicoOpt & POPT_ALT_RENDERER) || !(currentConfig.EmuOpt & EOPT_16BPP)) {
	#define p(x) px[(x) >> 2]
		// 8-bit modes
		unsigned int *px = (unsigned int *)((char *)g_screen_ptr + scr_offs);
		unsigned int col_g = (led_reg & 2) ? 0xc0c0c0c0 : 0xe0e0e0e0;
		unsigned int col_r = (led_reg & 1) ? 0xd0d0d0d0 : 0xe0e0e0e0;
		p(pitch*0) = p(pitch*1) = p(pitch*2) = col_g;
		p(pitch*0 + led_offs) = p(pitch*1 + led_offs) = p(pitch*2 + led_offs) = col_r;
	#undef p
	} else {
	#define p(x) px[(x)*2 >> 2] = px[((x)*2 >> 2) + 1]
		// 16-bit modes
		unsigned int *px = (unsigned int *)((short *)g_screen_ptr + scr_offs);
		unsigned int col_g = (led_reg & 2) ? 0x06000600 : 0;
		unsigned int col_r = (led_reg & 1) ? 0xc000c000 : 0;
		p(pitch*0) = p(pitch*1) = p(pitch*2) = col_g;
		p(pitch*0 + led_offs) = p(pitch*1 + led_offs) = p(pitch*2 + led_offs) = col_r;
	#undef p
	}
}

static int EmuScanBegin16(unsigned int num)
{
	DrawLineDest = (unsigned short *)g_screen_ptr + num * g_screen_width;

	return 0;
}

void pemu_update_display(const char *fps, const char *notice)
{
	if (notice || (currentConfig.EmuOpt & EOPT_SHOW_FPS)) {
		if (notice)
			osd_text(4, g_screen_height - 8, notice);
		if (currentConfig.EmuOpt & EOPT_SHOW_FPS)
			osd_text(osd_fps_x, g_screen_height - 8, fps);
	}
	if ((PicoAHW & PAHW_MCD) && (currentConfig.EmuOpt & EOPT_EN_CD_LEDS))
		draw_cd_leds();

	update_screen();
}

void plat_video_toggle_renderer(int is_next, int force_16bpp, int is_menu)
{
	// this will auto-select SMS/32X renderers
	PicoDrawSetColorFormat(1);
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
	update_screen();
}

void plat_status_msg_clear(void)
{
	unsigned short *d = (unsigned short *)g_screen_ptr + g_screen_width * g_screen_height;
	int l = g_screen_width * 8;
	memset32((int *)(d - l), 0, l * 2 / 4);
}

void plat_status_msg_busy_next(const char *msg)
{
	plat_status_msg_clear();
	pemu_update_display("", msg);
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
}

void pemu_forced_frame(int opts)
{
	int po_old = PicoOpt;
	int eo_old = currentConfig.EmuOpt;

	PicoOpt &= ~POPT_ALT_RENDERER;
	PicoOpt |= opts|POPT_ACC_SPRITES; // acc_sprites
	currentConfig.EmuOpt |= EOPT_16BPP;

	PicoDrawSetColorFormat(1);
	PicoScanBegin = EmuScanBegin16;

	Pico.m.dirtyPal = 1;
	PicoFrameDrawOnly();

	PicoOpt = po_old;
	currentConfig.EmuOpt = eo_old;
}

static void updateSound(int len)
{
	len <<= 1;
	if (PicoOpt & POPT_EN_STEREO)
		len <<= 1;

	if ((currentConfig.EmuOpt & EOPT_NO_FRMLIMIT) && !sndout_oss_can_write(len))
		return;

	/* avoid writing audio when lagging behind to prevent audio lag */
	if (PicoSkipFrame != 2)
		sndout_oss_write(PsndOut, len);
}

void pemu_sound_start(void)
{
	static int PsndRate_old = 0, PicoOpt_old = 0, pal_old = 0;
	int target_fps = Pico.m.pal ? 50 : 60;

	PsndOut = NULL;

	if (currentConfig.EmuOpt & EOPT_EN_SOUND)
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

void plat_debug_cat(char *str)
{
}

void emu_video_mode_change(int start_line, int line_count, int is_32cols)
{
	osd_fps_x = 260;

	// clear whole screen in all buffers
	memset32(g_screen_ptr, 0, g_screen_width * g_screen_height * 2 / 4);
}

void pemu_loop_prep(void)
{
	PicoDrawSetColorFormat(1);
	PicoScanBegin = EmuScanBegin16;
	osd_text = osd_text16;

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

	PicoDrawSetColorFormat(1);
	Pico.m.dirtyPal = 1;
	PicoFrame();

	PicoOpt = po_old;
	currentConfig.EmuOpt = eo_old;
}

void plat_wait_till_us(unsigned int us_to)
{
	unsigned int now;

	now = plat_get_ticks_us();

	while ((signed int)(us_to - now) > 512)
	{
		usleep(1024);
		now = plat_get_ticks_us();
	}
}

void plat_video_wait_vsync(void)
{
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
