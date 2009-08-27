// (c) Copyright 2006-2009 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <stdlib.h>

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

static short __attribute__((aligned(4))) sndBuffer[2*(44100+100)/50];
static unsigned char PicoDraw2FB_[(8+320) * (8+240+8)];
unsigned char *PicoDraw2FB = PicoDraw2FB_;
static int osd_fps_x;

extern void *gp2x_screens[4];

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
	defaultConfig.EmuOpt    = 0x9d | EOPT_RAM_TIMINGS|EOPT_CONFIRM_SAVE|EOPT_EN_CD_LEDS;
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
	else if (soc == SOCID_POLLUX)
		defaultConfig.EmuOpt |= EOPT_WIZ_TEAR_FIX|EOPT_SHOW_RTC;
}

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

static void osd_text8_rot(int x, int y, const char *text)
{
	int len = strlen(text) * 8;
	char *p = (char *)g_screen_ptr + 240*(320-x) + y;

	while (len--) {
		memset(p, 0xe0, 8);
		p -= 240;
	}

	emu_text_out8_rot(x, y, text);
}

static void osd_text16_rot(int x, int y, const char *text)
{
	int len = strlen(text) * 8;
	short *p = (short *)g_screen_ptr + 240*(320-x) + y;

	while (len--) {
		memset(p, 0, 8*2);
		p -= 240;
	}

	emu_text_out16_rot(x, y, text);
}

static void draw_cd_leds(void)
{
	int led_reg, pitch, scr_offs, led_offs;
	led_reg = Pico_mcd->s68k_regs[0];

	if (currentConfig.EmuOpt & EOPT_WIZ_TEAR_FIX) {
		pitch = 240;
		led_offs = -pitch * 6;
		scr_offs = pitch * (320 - 4);
	} else {
		pitch = 320;
		led_offs = 4;
		scr_offs = pitch * 2 + 4;
	}

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

static void draw_pico_ptr(void)
{
	unsigned short *p = (unsigned short *)g_screen_ptr;
	int x, y, pitch = 320;

	// only if pen enabled and for 16bit modes
	if (pico_inp_mode == 0 || (PicoOpt & POPT_ALT_RENDERER) || !(currentConfig.EmuOpt & EOPT_16BPP))
		return;

	x = pico_pen_x + PICO_PEN_ADJUST_X;
	y = pico_pen_y + PICO_PEN_ADJUST_Y;
	if (!(Pico.video.reg[12]&1) && !(PicoOpt & POPT_DIS_32C_BORDER))
		x += 32;

	if (currentConfig.EmuOpt & EOPT_WIZ_TEAR_FIX) {
		pitch = 240;
		p += (319 - x) * pitch + y;
	} else
		p += x + y * pitch;

	p[0]       ^= 0xffff;
	p[pitch-1] ^= 0xffff;
	p[pitch]   ^= 0xffff;
	p[pitch+1] ^= 0xffff;
	p[pitch*2] ^= 0xffff;
}

static int EmuScanBegin16(unsigned int num)
{
	DrawLineDest = (unsigned short *) g_screen_ptr + g_screen_width * num;

	return 0;
}

static int EmuScanBegin8(unsigned int num)
{
	DrawLineDest = (unsigned char *)  g_screen_ptr + g_screen_width * num;

	return 0;
}

/* rot thing for Wiz */
static unsigned char __attribute__((aligned(4))) rot_buff[320*4*2];

static int EmuScanBegin16_rot(unsigned int num)
{
	DrawLineDest = rot_buff + (num & 3) * 320 * 2;
	return 0;
}

static int EmuScanEnd16_rot(unsigned int num)
{
	if ((num & 3) != 3)
		return 0;
	rotated_blit16(g_screen_ptr, rot_buff, num + 1,
		!(Pico.video.reg[12] & 1) && !(PicoOpt & POPT_EN_SOFTSCALE));
	return 0;
}

static int EmuScanBegin8_rot(unsigned int num)
{
	DrawLineDest = rot_buff + (num & 3) * 320;
	return 0;
}

static int EmuScanEnd8_rot(unsigned int num)
{
	if ((num & 3) != 3)
		return 0;
	rotated_blit8(g_screen_ptr, rot_buff, num + 1,
		!(Pico.video.reg[12] & 1));
	return 0;
}

int localPal[0x100];
static void (*vidcpyM2)(void *dest, void *src, int m32col, int with_32c_border) = NULL;

void pemu_update_display(const char *fps, const char *notice)
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
		vidcpyM2(g_screen_ptr, PicoDraw2FB+328*8,
			!(Pico.video.reg[12] & 1), !(PicoOpt & POPT_DIS_32C_BORDER));
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
}

/* XXX */
#ifdef __GP2X__
unsigned int plat_get_ticks_ms(void)
{
	return gp2x_get_ticks_ms();
}

unsigned int plat_get_ticks_us(void)
{
	return gp2x_get_ticks_us();
}
#endif

void plat_wait_till_us(unsigned int us_to)
{
	unsigned int now;

	spend_cycles(1024);
	now = plat_get_ticks_us();

	while ((signed int)(us_to - now) > 512)
	{
		spend_cycles(1024);
		now = plat_get_ticks_us();
	}
}

void plat_video_wait_vsync(void)
{
	gp2x_video_wait_vsync();
}

void plat_status_msg_clear(void)
{
	int is_8bit = (PicoOpt & POPT_ALT_RENDERER) || !(currentConfig.EmuOpt & EOPT_16BPP);
	if (currentConfig.EmuOpt & EOPT_WIZ_TEAR_FIX) {
		/* ugh.. */
		int i, u, *p;
		if (is_8bit) {
			for (i = 0; i < 4; i++) {
				p = (int *)gp2x_screens[i] + (240-8) / 4;
				for (u = 320; u > 0; u--, p += 240/4)
					p[0] = p[1] = 0xe0e0e0e0;
			}
		} else {
			for (i = 0; i < 4; i++) {
				p = (int *)gp2x_screens[i] + (240-8)*2 / 4;
				for (u = 320; u > 0; u--, p += 240*2/4)
					p[0] = p[1] = p[2] = p[3] = 0;
			}
		}
		return;
	}

	if (is_8bit)
		gp2x_memset_all_buffers(320*232, 0xe0, 320*8);
	else
		gp2x_memset_all_buffers(320*232*2, 0, 320*8*2);
}

void plat_status_msg_busy_next(const char *msg)
{
	plat_status_msg_clear();
	pemu_update_display("", msg);
	emu_status_msg("");

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
	PicoScanEnd = NULL;

	if (PicoOpt & POPT_ALT_RENDERER) {
		if (currentConfig.EmuOpt & EOPT_WIZ_TEAR_FIX) {
			gp2x_video_changemode(-8);
			vidcpyM2 = vidcpy_m2_rot;
			osd_text = osd_text8_rot;
		} else {
			gp2x_video_changemode(8);
			vidcpyM2 = vidcpy_m2;
			osd_text = osd_text8;
		}
	}
	else if (currentConfig.EmuOpt & EOPT_16BPP) {
		PicoDrawSetColorFormat(1);
		if (currentConfig.EmuOpt & EOPT_WIZ_TEAR_FIX) {
			gp2x_video_changemode(-16);
			PicoScanBegin = EmuScanBegin16_rot;
			PicoScanEnd = EmuScanEnd16_rot;
			osd_text = osd_text16_rot;
		} else {
			gp2x_video_changemode(16);
			PicoScanBegin = EmuScanBegin16;
			osd_text = osd_text16;
		}
	}
	else {
		PicoDrawSetColorFormat(2);
		if (currentConfig.EmuOpt & EOPT_WIZ_TEAR_FIX) {
			gp2x_video_changemode(-8);
			PicoScanBegin = EmuScanBegin8_rot;
			PicoScanEnd = EmuScanEnd8_rot;
			osd_text = osd_text8_rot;
		} else {
			gp2x_video_changemode(8);
			PicoScanBegin = EmuScanBegin8;
			osd_text = osd_text8;
		}
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

void plat_video_toggle_renderer(int is_next, int is_menu)
{
	/* alt, 16bpp, 8bpp */
	if (PicoOpt & POPT_ALT_RENDERER) {
		PicoOpt &= ~POPT_ALT_RENDERER;
		if (is_next)
			currentConfig.EmuOpt |= EOPT_16BPP;
	} else if (!(currentConfig.EmuOpt & EOPT_16BPP)) {
		if (is_next)
			PicoOpt |= POPT_ALT_RENDERER;
		else
			currentConfig.EmuOpt |= EOPT_16BPP;
	} else {
		currentConfig.EmuOpt &= ~EOPT_16BPP;
		if (!is_next)
			PicoOpt |= POPT_ALT_RENDERER;
	}

	if (is_menu)
		return;

	vidResetMode();

	if (PicoOpt & POPT_ALT_RENDERER) {
		emu_status_msg(" 8bit fast renderer");
	} else if (currentConfig.EmuOpt & EOPT_16BPP) {
		emu_status_msg("16bit accurate renderer");
	} else {
		emu_status_msg(" 8bit accurate renderer");
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
		emu_status_msg("VOL: %02i", vol);
		prev_frame = Pico.m.frame_count;
	}

	if (!need_low_volume)
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

	PsndOut = NULL;

	// prepare sound stuff
	if (currentConfig.EmuOpt & EOPT_EN_SOUND)
	{
		int is_stereo = (PicoOpt & POPT_EN_STEREO) ? 1 : 0;
		int target_fps = Pico.m.pal ? 50 : 60;
		int frame_samples, snd_excess_add;
		int snd_rate_oss = PsndRate;
		gp2x_soc_t soc;

		soc = soc_detect();
		if (soc == SOCID_POLLUX) {
			/* POLLUX pain: DPLL1 / mclk_div / bitclk_div / 4 */
			switch (PsndRate) {
			case 44100: PsndRate = 44171; break; // 44170.673077
			case 22050: PsndRate = 22086; break; // 22085.336538
			case 11025: PsndRate = 11043; break; // 11042.668269
			default: break;
			}
		}

		#define SOUND_RERATE_FLAGS (POPT_EN_FM|POPT_EN_PSG|POPT_EN_STEREO|POPT_EXT_FM|POPT_EN_MCD_CDDA)
		if (PsndRate != PsndRate_old || Pico.m.pal != pal_old || ((PicoOpt & POPT_EXT_FM) && crashed_940) ||
				((PicoOpt ^ PicoOpt_old) & SOUND_RERATE_FLAGS)) {
			PsndRerate(Pico.m.frame_count ? 1 : 0);
		}

		memset(sndBuffer, 0, sizeof(sndBuffer));
		PsndOut = sndBuffer;
		PicoWriteSound = updateSound;
		PsndRate_old = PsndRate;
		PicoOpt_old  = PicoOpt;
		pal_old = Pico.m.pal;
		plat_update_volume(0, 0);

		frame_samples = PsndLen;
		snd_excess_add = ((PsndRate - PsndLen * target_fps)<<16) / target_fps;
		if (snd_excess_add != 0)
			frame_samples++;
		if (soc == SOCID_POLLUX)
			frame_samples *= 2;	/* force larger buffer */

		printf("starting audio: %i len: %i (ex: %04x) stereo: %i, pal: %i\n",
			PsndRate, PsndLen, snd_excess_add, is_stereo, Pico.m.pal);
		sndout_oss_setvol(currentConfig.volume, currentConfig.volume);
		sndout_oss_start(snd_rate_oss, frame_samples, is_stereo);

		/* Wiz's sound hardware needs more prebuffer */
		if (soc == SOCID_POLLUX)
			updateSound(frame_samples);
	}
}

void pemu_sound_stop(void)
{
	/* get back from Wiz pain */
	switch (PsndRate) {
		case 44171: PsndRate = 44100; break;
		case 22086: PsndRate = 22050; break;
		case 11043: PsndRate = 11025; break;
		default: break;
	}
}

void pemu_sound_wait(void)
{
	// don't need to do anything, writes will block by themselves
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
	PicoScanEnd = NULL;
	Pico.m.dirtyPal = 1;
	PicoFrameDrawOnly();

	PicoOpt = po_old;
	currentConfig.EmuOpt = eo_old;
}

void plat_debug_cat(char *str)
{
}

void pemu_video_mode_change(int is_32col, int is_240_lines)
{
	int scalex = 320;
	osd_fps_x = OSD_FPS_X;
	if (is_32col && (PicoOpt & POPT_DIS_32C_BORDER)) {
		scalex = 256;
		osd_fps_x = OSD_FPS_X - 64;
	}
	/* want vertical scaling and game is not in 240 line mode */
	if (currentConfig.scaling == EOPT_SCALE_HW_HV && !is_240_lines)
		gp2x_video_RGB_setscaling(8, scalex, 224);
	else
		gp2x_video_RGB_setscaling(0, scalex, 240);

	// clear whole screen in all buffers
	if ((PicoOpt & POPT_ALT_RENDERER) || !(currentConfig.EmuOpt & EOPT_16BPP))
		gp2x_memset_all_buffers(0, 0xe0, 320*240);
	else
		gp2x_memset_all_buffers(0, 0, 320*240*2);
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

void pemu_loop_prep(void)
{
	static int gp2x_old_clock = -1, EmuOpt_old = 0, pal_old = 0;
	static int gp2x_old_gamma = 100;
	gp2x_soc_t soc;

	soc = soc_detect();

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

	if (gp2x_old_gamma != currentConfig.gamma || ((EmuOpt_old ^ currentConfig.EmuOpt) & EOPT_A_SN_GAMMA)) {
		set_lcd_gamma(currentConfig.gamma, !!(currentConfig.EmuOpt & EOPT_A_SN_GAMMA));
		gp2x_old_gamma = currentConfig.gamma;
		printf("updated gamma to %i, A_SN's curve: %i\n", currentConfig.gamma, !!(currentConfig.EmuOpt&0x1000));
	}

	if (((EmuOpt_old ^ currentConfig.EmuOpt) & EOPT_VSYNC) || Pico.m.pal != pal_old) {
		if ((currentConfig.EmuOpt & EOPT_VSYNC) || soc == SOCID_POLLUX)
			set_lcd_custom_rate(Pico.m.pal);
		else if (EmuOpt_old & EOPT_VSYNC)
			unset_lcd_custom_rate();
	}

	if ((EmuOpt_old ^ currentConfig.EmuOpt) & EOPT_MMUHACK)
		gp2x_make_fb_bufferable(currentConfig.EmuOpt & EOPT_MMUHACK);

	EmuOpt_old = currentConfig.EmuOpt;
	pal_old = Pico.m.pal;

	// make sure we are in correct mode
	vidResetMode();
	scaling_update();

	pemu_sound_start();
}

void pemu_loop_end(void)
{
	int po_old = PicoOpt;
	int eo_old = currentConfig.EmuOpt;

	pemu_sound_stop();

	/* do one more frame for menu bg */
	PicoOpt &= ~POPT_ALT_RENDERER;
	PicoOpt |= POPT_EN_SOFTSCALE|POPT_ACC_SPRITES;
	currentConfig.EmuOpt |= EOPT_16BPP;

	PicoScanBegin = EmuScanBegin16;
	PicoScanEnd = NULL;
	PicoDrawSetColorFormat(1);
	Pico.m.dirtyPal = 1;
	PicoFrame();

	PicoOpt = po_old;
	currentConfig.EmuOpt = eo_old;
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
