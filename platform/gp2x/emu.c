// (c) Copyright 2006-2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <ctype.h>
#include <unistd.h>

#include <stdarg.h>

#include "emu.h"
#include "gp2x.h"
#include "menu.h"
#include "../linux/usbjoy.h"
#include "../common/arm_utils.h"
#include "../common/fonts.h"
#include "../common/emu.h"
#include "../common/config.h"
#include "../common/input.h"
#include "../linux/sndout_oss.h"
#include "cpuctrl.h"

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


int engineState;
int select_exits = 0;

char romFileName[PATH_MAX];

extern int crashed_940;

static short __attribute__((aligned(4))) sndBuffer[2*44100/50];
static struct timeval noticeMsgTime = { 0, 0 };	// when started showing
static int osd_fps_x;
static int gp2x_old_gamma = 100;
unsigned char *PicoDraw2FB = NULL;  // temporary buffer for alt renderer
int reset_timing = 0;

#define PICO_PEN_ADJUST_X 4
#define PICO_PEN_ADJUST_Y 2
static int pico_pen_x = 320/2, pico_pen_y = 240/2;

static void emu_msg_cb(const char *msg);
static void emu_msg_tray_open(void);


void emu_noticeMsgUpdated(void)
{
	gettimeofday(&noticeMsgTime, 0);
}

int emu_getMainDir(char *dst, int len)
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

void emu_Init(void)
{
	// make temp buffer for alt renderer
	PicoDraw2FB = malloc((8+320)*(8+240+8));
	if (!PicoDraw2FB)
	{
		printf("PicoDraw2FB == 0\n");
	}

	// make dirs for saves, cfgs, etc.
	mkdir("mds", 0777);
	mkdir("srm", 0777);
	mkdir("brm", 0777);
	mkdir("cfg", 0777);

	PicoInit();
	PicoMessage = emu_msg_cb;
	PicoMCDopenTray = emu_msg_tray_open;
	PicoMCDcloseTray = menu_loop_tray;
}


static void scaling_update(void)
{
	PicoOpt &= ~0x4100;
	switch (currentConfig.scaling) {
		default: break; // off
		case 1:  // hw hor
		case 2:  PicoOpt |=  0x0100; break; // hw hor+vert
		case 3:  PicoOpt |=  0x4000; break; // sw hor
	}
}


void emu_Deinit(void)
{
	// save SRAM
	if((currentConfig.EmuOpt & 1) && SRam.changed) {
		emu_SaveLoadGame(0, 1);
		SRam.changed = 0;
	}

	if (!(currentConfig.EmuOpt & EOPT_NO_AUTOSVCFG))
		emu_writelrom();

	free(PicoDraw2FB);

	PicoExit();

	// restore gamma
	if (gp2x_old_gamma != 100)
		set_gamma(100, 0);
}

void emu_prepareDefaultConfig(void)
{
	memset(&defaultConfig, 0, sizeof(defaultConfig));
	defaultConfig.EmuOpt    = 0x9d | 0x00700; // | <- ram_tmng, confirm_save, cd_leds
	defaultConfig.s_PicoOpt = 0x0f | POPT_EXT_FM|POPT_EN_MCD_PCM|POPT_EN_MCD_CDDA|POPT_EN_SVP_DRC|POPT_ACC_SPRITES;
	defaultConfig.s_PsndRate = 44100;
	defaultConfig.s_PicoRegion = 0; // auto
	defaultConfig.s_PicoAutoRgnOrder = 0x184; // US, EU, JP
	defaultConfig.s_PicoCDBuffers = 0;
	defaultConfig.Frameskip = -1; // auto
	defaultConfig.CPUclock = 200;
	defaultConfig.volume = 50;
	defaultConfig.KeyBinds[ 0] = 1<<0; // SACB RLDU
	defaultConfig.KeyBinds[ 4] = 1<<1;
	defaultConfig.KeyBinds[ 2] = 1<<2;
	defaultConfig.KeyBinds[ 6] = 1<<3;
	defaultConfig.KeyBinds[14] = 1<<4;
	defaultConfig.KeyBinds[13] = 1<<5;
	defaultConfig.KeyBinds[12] = 1<<6;
	defaultConfig.KeyBinds[ 8] = 1<<7;
	defaultConfig.KeyBinds[15] = 1<<26; // switch rend
	defaultConfig.KeyBinds[10] = 1<<27; // save state
	defaultConfig.KeyBinds[11] = 1<<28; // load state
	defaultConfig.KeyBinds[23] = 1<<29; // vol up
	defaultConfig.KeyBinds[22] = 1<<30; // vol down
	defaultConfig.gamma = 100;
	defaultConfig.scaling = 0;
	defaultConfig.turbo_rate = 15;
}

void osd_text(int x, int y, const char *text)
{
	int len = strlen(text)*8;

	if ((PicoOpt&0x10)||!(currentConfig.EmuOpt&0x80)) {
		int *p, i, h;
		x &= ~3; // align x
		len = (len+3) >> 2;
		for (h = 0; h < 8; h++) {
			p = (int *) ((unsigned char *) gp2x_screen+x+320*(y+h));
			for (i = len; i; i--, p++) *p = 0xe0e0e0e0;
		}
		emu_textOut8(x, y, text);
	} else {
		int *p, i, h;
		x &= ~1; // align x
		len = (len+1) >> 1;
		for (h = 0; h < 8; h++) {
			p = (int *) ((unsigned short *) gp2x_screen+x+320*(y+h));
			for (i = len; i; i--, p++) *p = (*p>>2)&0x39e7;
		}
		emu_textOut16(x, y, text);
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
		*(unsigned int *)((char *)gp2x_screen + 320*2+ 4) =
		*(unsigned int *)((char *)gp2x_screen + 320*3+ 4) =
		*(unsigned int *)((char *)gp2x_screen + 320*4+ 4) = col_g;
		*(unsigned int *)((char *)gp2x_screen + 320*2+12) =
		*(unsigned int *)((char *)gp2x_screen + 320*3+12) =
		*(unsigned int *)((char *)gp2x_screen + 320*4+12) = col_r;
	} else {
		// 16-bit modes
		unsigned int *p = (unsigned int *)((short *)gp2x_screen + 320*2+4);
		unsigned int col_g = (old_reg & 2) ? 0x06000600 : 0;
		unsigned int col_r = (old_reg & 1) ? 0xc000c000 : 0;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += 320/2 - 12/2;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += 320/2 - 12/2;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r;
	}
}

static void draw_pico_ptr(void)
{
	unsigned short *p = (unsigned short *)gp2x_screen;

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
	DrawLineDest = (unsigned short *) gp2x_screen + 320 * num;

	return 0;
}

static int EmuScanBegin8(unsigned int num)
{
	if (!(Pico.video.reg[1]&8)) num += 8;
	DrawLineDest = (unsigned char *)  gp2x_screen + 320 * num;

	return 0;
}

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
			gp2x_video_setpalette(localPal, 0x40);
		}
		// a hack for VR
		if (PicoRead16Hook == PicoSVPRead16)
			memset32((int *)(PicoDraw2FB+328*8+328*223), 0xe0e0e0e0, 328);
		// do actual copy
		vidCpyM2((unsigned char *)gp2x_screen+320*8, PicoDraw2FB+328*8);
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
		if (currentConfig.scaling == 2 && !(Pico.video.reg[1]&8)) h -= 8;
		if (notice) osd_text(4, h, notice);
		if (emu_opt & 2)
			osd_text(osd_fps_x, h, fps);
	}
	if ((emu_opt & 0x400) && (PicoAHW & PAHW_MCD))
		draw_cd_leds();
	if (PicoAHW & PAHW_PICO)
		draw_pico_ptr();

	//gp2x_video_wait_vsync();
	gp2x_video_flip();

	if (!(PicoOpt&0x10)) {
		if (!(Pico.video.reg[1]&8)) {
			if (currentConfig.EmuOpt&0x80) {
				DrawLineDest = (unsigned short *) gp2x_screen + 320*8;
			} else {
				DrawLineDest = (unsigned char  *) gp2x_screen + 320*8;
			}
		} else {
			DrawLineDest = gp2x_screen;
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


static void vidResetMode(void)
{
	if (PicoOpt&0x10) {
		gp2x_video_changemode(8);
	} else if (currentConfig.EmuOpt&0x80) {
		gp2x_video_changemode(16);
		PicoDrawSetColorFormat(1);
		PicoScanBegin = EmuScanBegin16;
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
}


static void emu_msg_cb(const char *msg)
{
	if ((PicoOpt&0x10)||!(currentConfig.EmuOpt&0x80)) {
		// 8-bit renderers
		gp2x_memset_all_buffers(320*232, 0xe0, 320*8);
		osd_text(4, 232, msg);
		gp2x_memcpy_all_buffers((char *)gp2x_screen+320*232, 320*232, 320*8);
	} else {
		// 16bit accurate renderer
		gp2x_memset_all_buffers(320*232*2, 0, 320*8*2);
		osd_text(4, 232, msg);
		gp2x_memcpy_all_buffers((char *)gp2x_screen+320*232*2, 320*232*2, 320*8*2);
	}
	gettimeofday(&noticeMsgTime, 0);
	noticeMsgTime.tv_sec -= 2;

	/* assumption: emu_msg_cb gets called only when something slow is about to happen */
	reset_timing = 1;
}

static void emu_state_cb(const char *str)
{
	clearArea(0);
	blit("", str);
}

static void emu_msg_tray_open(void)
{
	strcpy(noticeMsg, "CD tray opened");
	gettimeofday(&noticeMsgTime, 0);
}

static void RunEventsPico(unsigned int events, unsigned int gp2x_keys)
{
	int ret, px, py, lim_x;
	static int pdown_frames = 0;

	emu_RunEventsPico(events);

	if (pico_inp_mode == 0) return;

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

	PicoPad[0] &= ~0x0f; // release UDLR
	if (gp2x_keys & GP2X_UP)    pico_pen_y--;
	if (gp2x_keys & GP2X_DOWN)  pico_pen_y++;
	if (gp2x_keys & GP2X_LEFT)  pico_pen_x--;
	if (gp2x_keys & GP2X_RIGHT) pico_pen_x++;

	lim_x = (Pico.video.reg[12]&1) ? 319 : 255;
	if (pico_pen_y < 8) pico_pen_y = 8;
	if (pico_pen_y > 224-PICO_PEN_ADJUST_Y) pico_pen_y = 224-PICO_PEN_ADJUST_Y;
	if (pico_pen_x < 0) pico_pen_x = 0;
	if (pico_pen_x > lim_x-PICO_PEN_ADJUST_X) pico_pen_x = lim_x-PICO_PEN_ADJUST_X;

	PicoPicohw.pen_pos[0] = pico_pen_x;
	if (!(Pico.video.reg[12]&1)) PicoPicohw.pen_pos[0] += pico_pen_x/4;
	PicoPicohw.pen_pos[0] += 0x3c;
	PicoPicohw.pen_pos[1] = pico_inp_mode == 1 ? (0x2f8 + pico_pen_y) : (0x1fc + pico_pen_y);
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

static void RunEvents(unsigned int which)
{
	if (which & 0x1800) // save or load (but not both)
	{
		int do_it = 1;
		if ( emu_checkSaveFile(state_slot) &&
				(( (which & 0x1000) && (currentConfig.EmuOpt & 0x800)) ||   // load
				 (!(which & 0x1000) && (currentConfig.EmuOpt & 0x200))) ) { // save
			unsigned long keys;
			blit("", (which & 0x1000) ? "LOAD STATE? (Y=yes, X=no)" : "OVERWRITE SAVE? (Y=yes, X=no)");
			while ( !((keys = gp2x_joystick_read(1)) & (GP2X_X|GP2X_Y)) )
				usleep(50*1024);
			if (keys & GP2X_X) do_it = 0;
			while ( gp2x_joystick_read(1) & (GP2X_X|GP2X_Y) ) // wait for release
				usleep(50*1024);
			clearArea(0);
		}
		if (do_it) {
			osd_text(4, 232, (which & 0x1000) ? "LOADING GAME" : "SAVING GAME");
			PicoStateProgressCB = emu_state_cb;
			gp2x_memcpy_all_buffers(gp2x_screen, 0, 320*240*2);
			emu_SaveLoadGame((which & 0x1000) >> 12, 0);
			PicoStateProgressCB = NULL;
		}

		reset_timing = 1;
	}
	if (which & 0x0400) // switch renderer
	{
		if      (  PicoOpt&0x10)             { PicoOpt&=~0x10; currentConfig.EmuOpt |= 0x80; }
		else if (!(currentConfig.EmuOpt&0x80)) PicoOpt|= 0x10;
		else   currentConfig.EmuOpt &= ~0x80;

		vidResetMode();

		if (PicoOpt&0x10) {
			strcpy(noticeMsg, " 8bit fast renderer");
		} else if (currentConfig.EmuOpt&0x80) {
			strcpy(noticeMsg, "16bit accurate renderer");
		} else {
			strcpy(noticeMsg, " 8bit accurate renderer");
		}

		gettimeofday(&noticeMsgTime, 0);
	}
	if (which & 0x0300)
	{
		if(which&0x0200) {
			state_slot -= 1;
			if(state_slot < 0) state_slot = 9;
		} else {
			state_slot += 1;
			if(state_slot > 9) state_slot = 0;
		}
		sprintf(noticeMsg, "SAVE SLOT %i [%s]", state_slot, emu_checkSaveFile(state_slot) ? "USED" : "FREE");
		gettimeofday(&noticeMsgTime, 0);
	}
	if (which & 0x0080) {
		engineState = PGS_Menu;
	}
}

static void updateKeys(void)
{
	unsigned int keys, keys2, allActions[2] = { 0, 0 }, events;
	static unsigned int prevEvents = 0;
	int joy, i;

	keys = gp2x_joystick_read(0);
	if (keys & GP2X_SELECT) {
		engineState = select_exits ? PGS_Quit : PGS_Menu;
		// wait until select is released, so menu would not resume game
		while (gp2x_joystick_read(1) & GP2X_SELECT) usleep(50*1000);
	}

	keys &= CONFIGURABLE_KEYS;
	keys2 = keys;

#if 1
	{
		/* FIXME: combos */
		int acts = in_update();
		int pl = (acts >> 16) & 1;
		allActions[pl] |= acts;
	}
#else
	for (i = 0; i < 32; i++)
	{
		if (keys2 & (1 << i))
		{
			int pl, acts = currentConfig.KeyBinds[i];
			if (!acts) continue;
			pl = (acts >> 16) & 1;
			if (kb_combo_keys & (1 << i))
			{
				int u = i+1, acts_c = acts & kb_combo_acts;
				// let's try to find the other one
				if (acts_c) {
					for (; u < 32; u++)
						if ( (keys2 & (1 << u)) && (currentConfig.KeyBinds[u] & acts_c) ) {
							allActions[pl] |= acts_c & currentConfig.KeyBinds[u];
							keys2 &= ~((1 << i) | (1 << u));
							break;
						}
				}
				// add non-combo actions if combo ones were not found
				if (!acts_c || u == 32)
					allActions[pl] |= acts & ~kb_combo_acts;
			} else {
				allActions[pl] |= acts;
			}
		}
	}
#endif

	// add joy inputs
	if (num_of_joys > 0)
	{
		usbjoy_update();
		for (joy = 0; joy < num_of_joys; joy++) {
			int btns = usbjoy_check2(joy);
			for (i = 0; i < 32; i++) {
				if (btns & (1 << i)) {
					int acts = currentConfig.JoyBinds[joy][i];
					int pl = (acts >> 16) & 1;
					allActions[pl] |= acts;
				}
			}
		}
	}

	PicoPad[0] = allActions[0] & 0xfff;
	PicoPad[1] = allActions[1] & 0xfff;

	if (allActions[0] & 0x7000) emu_DoTurbo(&PicoPad[0], allActions[0]);
	if (allActions[1] & 0x7000) emu_DoTurbo(&PicoPad[1], allActions[1]);

	events = (allActions[0] | allActions[1]) >> 16;

	// volume is treated in special way and triggered every frame
	if (events & 0x6000)
		update_volume(1, events & 0x2000);

	if ((events ^ prevEvents) & 0x40) {
		emu_changeFastForward(events & 0x40);
		update_volume(0, 0);
		reset_timing = 1;
	}

	events &= ~prevEvents;

	if (PicoAHW == PAHW_PICO)
		RunEventsPico(events, keys);
	if (events) RunEvents(events);
	if (movie_data) emu_updateMovie();

	prevEvents = (allActions[0] | allActions[1]) >> 16;
}


static void updateSound(int len)
{
	if (PicoOpt&8) len<<=1;

	/* avoid writing audio when lagging behind to prevent audio lag */
	if (PicoSkipFrame != 2)
		sndout_oss_write(PsndOut, len<<1);
}

void emu_startSound(void)
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
		update_volume(0, 0);
		memset(sndBuffer, 0, sizeof(sndBuffer));
		PsndOut = sndBuffer;
		PsndRate_old = PsndRate;
		PicoOpt_old  = PicoOpt;
		pal_old = Pico.m.pal;
	}
}

void emu_endSound(void)
{
}

/* wait until we can write more sound */
void emu_waitSound(void)
{
	// don't need to do anything, writes will block by themselves
}


static void SkipFrame(int do_audio)
{
	PicoSkipFrame=do_audio ? 1 : 2;
	PicoFrame();
	PicoSkipFrame=0;
}


void emu_forcedFrame(int opts)
{
	int po_old = PicoOpt;
	int eo_old = currentConfig.EmuOpt;

	PicoOpt &= ~0x10;
	PicoOpt |= opts|POPT_ACC_SPRITES; // acc_sprites
	currentConfig.EmuOpt |= 0x80;

	//vidResetMode();
	PicoDrawSetColorFormat(1);
	PicoScanBegin = EmuScanBegin16;
	Pico.m.dirtyPal = 1;
	PicoFrameDrawOnly();

/*
	if (!(Pico.video.reg[12]&1)) {
		vidCpyM2 = vidCpyM2_32col;
		clearArea(1);
	} else	vidCpyM2 = vidCpyM2_40col;

	vidCpyM2((unsigned char *)gp2x_screen+320*8, PicoDraw2FB+328*8);
	vidConvCpyRGB32(localPal, Pico.cram, 0x40);
	gp2x_video_setpalette(localPal, 0x40);
*/
	PicoOpt = po_old;
	currentConfig.EmuOpt = eo_old;
}

void emu_platformDebugCat(char *str)
{
	// nothing
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
		if(oldscr[i] != CONV(((unsigned short *)gp2x_screen)[i])) break;
	if (i < 320*240)
	{
		for (i = 0; i < 320*240; i++)
			oldscr[i] = CONV(((unsigned short *)gp2x_screen)[i]);
		sprintf(name, "%05i.tga", Pico.m.frame_count);
		f = fopen(name, "wb");
		if (!f) { printf("!f\n"); exit(1); }
		fwrite(&TGAHEAD, 1, sizeof(TGAHEAD), f);
		fwrite(oldscr, 1, 320*240*2, f);
		fclose(f);
	}
}
#endif


void emu_Loop(void)
{
	static int gp2x_old_clock = 200, EmuOpt_old = 0;
	char fpsbuff[24]; // fps count c string
	struct timeval tval; // timing
	int pframes_done, pframes_shown, pthissec; // "period" frames, used for sync
	int  frames_done,  frames_shown,  thissec; // actual frames
	int oldmodes = 0, target_fps, target_frametime, lim_time, vsync_offset, i;
	char *notice = 0;

	printf("entered emu_Loop()\n");

	if (gp2x_old_clock != currentConfig.CPUclock) {
		printf("changing clock to %i...", currentConfig.CPUclock); fflush(stdout);
		set_FCLK(currentConfig.CPUclock);
		gp2x_old_clock = currentConfig.CPUclock;
		printf(" done\n");
	}

	if (gp2x_old_gamma != currentConfig.gamma || (EmuOpt_old&0x1000) != (currentConfig.EmuOpt&0x1000)) {
		set_gamma(currentConfig.gamma, !!(currentConfig.EmuOpt&0x1000));
		gp2x_old_gamma = currentConfig.gamma;
		printf("updated gamma to %i, A_SN's curve: %i\n", currentConfig.gamma, !!(currentConfig.EmuOpt&0x1000));
	}

	if ((EmuOpt_old&0x2000) != (currentConfig.EmuOpt&0x2000)) {
		if (currentConfig.EmuOpt&0x2000)
		     set_LCD_custom_rate(Pico.m.pal ? LCDR_100 : LCDR_120);
		else unset_LCD_custom_rate();
	}

	EmuOpt_old = currentConfig.EmuOpt;
	fpsbuff[0] = 0;

	// make sure we are in correct mode
	vidResetMode();
	scaling_update();
	Pico.m.dirtyPal = 1;
	oldmodes = ((Pico.video.reg[12]&1)<<2) ^ 0xc;
	emu_findKeyBindCombos();

	// pal/ntsc might have changed, reset related stuff
	target_fps = Pico.m.pal ? 50 : 60;
	target_frametime = 1000000/target_fps;
	reset_timing = 1;

	emu_startSound();

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
			if (currentConfig.scaling == 2 && !(modes&8)) // want vertical scaling and game is not in 240 line mode
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
				if(PsndOut && pframes_done < target_fps && pframes_done > target_fps-5) {
					updateKeys();
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
				updateKeys();
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
			updateKeys();
			SkipFrame(tval.tv_usec < lim_time+target_frametime*2); pframes_done++; frames_done++;
			continue;
		}

		updateKeys();
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

	emu_changeFastForward(0);

	if (PicoAHW & PAHW_MCD) PicoCDBufferFree();

	// save SRAM
	if((currentConfig.EmuOpt & 1) && SRam.changed) {
		emu_state_cb("Writing SRAM/BRAM..");
		emu_SaveLoadGame(0, 1);
		SRam.changed = 0;
	}

	// if in 8bit mode, generate 16bit image for menu background
	if ((PicoOpt&0x10) || !(currentConfig.EmuOpt&0x80))
		emu_forcedFrame(POPT_EN_SOFTSCALE);
}


void emu_ResetGame(void)
{
	PicoReset();
	reset_timing = 1;
}

