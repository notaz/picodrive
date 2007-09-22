#include <windows.h>
#include <string.h>

#include "kgsdk/Framework2D.h"
#include "kgsdk/FrameworkAudio.h"
#include "../common/emu.h"
#include "../common/lprintf.h"
#include "../common/arm_utils.h"
#include "emu.h"
#include "menu.h"
#include "giz.h"
#include "asm_utils.h"

#include <Pico/PicoInt.h>

#ifdef BENCHMARK
#define OSD_FPS_X 220
#else
#define OSD_FPS_X 260
#endif

// main 300K gfx-related buffer. Used by menu and renderers.
unsigned char gfx_buffer[321*240*2*2];
char romFileName[MAX_PATH];
int engineState;

unsigned char *PicoDraw2FB = gfx_buffer;  // temporary buffer for alt renderer ( (8+320)*(8+240+8) )
int reset_timing = 0;

static DWORD noticeMsgTime = 0;
static int osd_fps_x;


static void blit(const char *fps, const char *notice);
static void clearArea(int full);

void emu_noticeMsgUpdated(void)
{
	noticeMsgTime = GetTickCount();
}

void emu_getMainDir(char *dst, int len)
{
	if (len > 0) *dst = 0;
}

static void emu_msg_cb(const char *msg)
{
	if (giz_screen == NULL)
		giz_screen = Framework2D_LockBuffer();

	memset32((int *)((char *)giz_screen + 321*232*2), 0, 321*8*2/4);
	emu_textOut16(4, 232, msg);
	noticeMsgTime = GetTickCount() - 2000;

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
	noticeMsgTime = GetTickCount();
}


void emu_Init(void)
{
	// make dirs for saves, cfgs, etc.
	CreateDirectory(L"mds", NULL);
	CreateDirectory(L"srm", NULL);
	CreateDirectory(L"brm", NULL);
	CreateDirectory(L"cfg", NULL);

	PicoInit();
	PicoMessage = emu_msg_cb;
	PicoMCDopenTray = emu_msg_tray_open;
	PicoMCDcloseTray = menu_loop_tray;
}

void emu_Deinit(void)
{
	// save SRAM
	if((currentConfig.EmuOpt & 1) && SRam.changed) {
		emu_SaveLoadGame(0, 1);
		SRam.changed = 0;
	}

	if (!(currentConfig.EmuOpt & 0x20)) {
		FILE *f = fopen(PicoConfigFile, "r+b");
		if (!f) emu_WriteConfig(0);
		else {
			// if we already have config, reload it, except last ROM
			fseek(f, sizeof(currentConfig.lastRomFile), SEEK_SET);
			fread(&currentConfig.EmuOpt, 1, sizeof(currentConfig) - sizeof(currentConfig.lastRomFile), f);
			fseek(f, 0, SEEK_SET);
			fwrite(&currentConfig, 1, sizeof(currentConfig), f);
			fflush(f);
			fclose(f);
		}
	}

	PicoExit();
}

void emu_setDefaultConfig(void)
{
	memset(&currentConfig, 0, sizeof(currentConfig));
	currentConfig.lastRomFile[0] = 0;
	currentConfig.EmuOpt  = 0x1f | 0x600; // | confirm_save, cd_leds
	currentConfig.PicoOpt = 0x0f | 0xc00; // | cd_pcm, cd_cdda
	currentConfig.PsndRate = 22050;
	currentConfig.PicoRegion = 0; // auto
	currentConfig.PicoAutoRgnOrder = 0x184; // US, EU, JP
	currentConfig.Frameskip = -1; // auto
	currentConfig.volume = 50;
	currentConfig.KeyBinds[ 2] = 1<<0; // SACB RLDU
	currentConfig.KeyBinds[ 3] = 1<<1;
	currentConfig.KeyBinds[ 0] = 1<<2;
	currentConfig.KeyBinds[ 1] = 1<<3;
	currentConfig.KeyBinds[ 5] = 1<<4;
	currentConfig.KeyBinds[ 6] = 1<<5;
	currentConfig.KeyBinds[ 7] = 1<<6;
	currentConfig.KeyBinds[ 8] = 1<<7;
	currentConfig.KeyBinds[ 4] = 1<<26; // switch rend
	currentConfig.KeyBinds[ 8] = 1<<27; // save state
	currentConfig.KeyBinds[ 9] = 1<<28; // load state
	currentConfig.KeyBinds[12] = 1<<29; // vol up
	currentConfig.KeyBinds[11] = 1<<30; // vol down
	currentConfig.PicoCDBuffers = 64;
	currentConfig.scaling = 0;
}


static int EmuScan16(unsigned int num, void *sdata)
{
	if (!(Pico.video.reg[1]&8)) num += 8;
	DrawLineDest = (unsigned short *) giz_screen + 321*(num+1);

	return 0;
}

static int EmuScan8(unsigned int num, void *sdata)
{
	// draw like the fast renderer
	if (!(Pico.video.reg[1]&8)) num += 8;
	HighCol = gfx_buffer + 328*8 + 328*(num+1);

	return 0;
}

static void osd_text(int x, int y, const char *text)
{
	int len = strlen(text) * 8;
	int *p, i, h;
	len = (len+1) >> 1;
	for (h = 0; h < 8; h++) {
		p = (int *) ((unsigned short *) giz_screen+x+321*(y+h));
		p = (int *) ((int)p & ~3); // align
		for (i = len; i; i--, p++) *p = 0;
	}
	emu_textOut16(x, y, text);
}


short localPal[0x100];
static void (*vidCpy8to16)(void *dest, void *src, short *pal, int lines) = NULL;

static void blit(const char *fps, const char *notice)
{
	int emu_opt = currentConfig.EmuOpt;

	if (PicoOpt&0x10) {
		// 8bit fast renderer
		if (Pico.m.dirtyPal) {
			Pico.m.dirtyPal = 0;
			vidConvCpyRGB565(localPal, Pico.cram, 0x40);
		}
		vidCpy8to16((unsigned short *)giz_screen+321*8, PicoDraw2FB+328*8, localPal, 224);
	} else if (!(emu_opt&0x80)) {
		// 8bit accurate renderer
		if (Pico.m.dirtyPal) {
			Pico.m.dirtyPal = 0;
			vidConvCpyRGB565(localPal, Pico.cram, 0x40);
			if(Pico.video.reg[0xC]&8) { // shadow/hilight mode
				//vidConvCpyRGB32sh(localPal+0x40, Pico.cram, 0x40);
				//vidConvCpyRGB32hi(localPal+0x80, Pico.cram, 0x40); // TODO
				blockcpy(localPal+0xc0, localPal+0x40, 0x40*4);
				localPal[0xc0] = 0x0600;
				localPal[0xd0] = 0xc000;
				localPal[0xe0] = 0x0000; // reserved pixels for OSD
				localPal[0xf0] = 0xffff;
			}
			/* no support
			else if (rendstatus & 0x20) { // mid-frame palette changes
				vidConvCpyRGB565(localPal+0x40, HighPal, 0x40);
				vidConvCpyRGB565(localPal+0x80, HighPal+0x40, 0x40);
			} */
		}
		// TODO...
		vidCpy8to16((unsigned short *)giz_screen+321*8, PicoDraw2FB+328*8, localPal, 224);
	}

	if (notice || (emu_opt & 2)) {
		int h = 232;
		if (notice)      osd_text(4, h, notice);
		if (emu_opt & 2) osd_text(osd_fps_x, h, fps);
	}
//	if ((emu_opt & 0x400) && (PicoMCD & 1))
//		cd_leds();

	//gp2x_video_wait_vsync();

	if (!(PicoOpt&0x10)) {
		if (Pico.video.reg[1] & 8) {
			if (currentConfig.EmuOpt&0x80)
				DrawLineDest = (unsigned short *) giz_screen;
			else
				HighCol = gfx_buffer;
		} else {
			if (currentConfig.EmuOpt&0x80)
				DrawLineDest = (unsigned short *) giz_screen + 320*8;
			else
				HighCol = gfx_buffer + 328*8;
		}
	}
}

// clears whole screen or just the notice area (in all buffers)
static void clearArea(int full)
{
	if (giz_screen == NULL)
		giz_screen = Framework2D_LockBuffer();
	if (full) memset32(giz_screen, 0, 320*240*2/4);
	else      memset32((int *)((char *)giz_screen + 320*232*2), 0, 320*8*2/4);
}

static void vidResetMode(void)
{
	void *screen;
	if (PicoOpt&0x10) {
	} else if (currentConfig.EmuOpt&0x80) {
		PicoDrawSetColorFormat(1);
		PicoScan = EmuScan16;
	} else {
		PicoDrawSetColorFormat(0);
		PicoScan = EmuScan8;
	}
	if ((PicoOpt&0x10)||!(currentConfig.EmuOpt&0x80)) {
		// setup pal for 8-bit modes
		localPal[0xc0] = 0x0600;
		localPal[0xd0] = 0xc000;
		localPal[0xe0] = 0x0000; // reserved pixels for OSD
		localPal[0xf0] = 0xffff;
	}
	Pico.m.dirtyPal = 1;
	screen = Framework2D_LockBuffer();
	memset32(screen, 0, 320*240*2/4);
	Framework2D_UnlockBuffer();
	giz_screen = NULL;
}


static void SkipFrame(int do_audio)
{
	PicoSkipFrame=do_audio ? 1 : 2;
	PicoFrame();
	PicoSkipFrame=0;
}

void emu_forcedFrame(void)
{
	// TODO
}

static void updateKeys(void)
{
}

static void simpleWait(DWORD until)
{
}

void emu_Loop(void)
{
	//static int PsndRate_old = 0, PicoOpt_old = 0, PsndLen_real = 0, pal_old = 0;
	char fpsbuff[24]; // fps count c string
	DWORD tval, tval_prev = 0, tval_thissec = 0; // timing
	int frames_done = 0, frames_shown = 0, oldmodes = 0;
	int target_fps, target_frametime, lim_time, tval_diff, i;
	char *notice = NULL;

	lprintf("entered emu_Loop()\n");

	fpsbuff[0] = 0;

	// make sure we are in correct mode
	vidResetMode();
	if (currentConfig.scaling) PicoOpt|=0x4000;
	else PicoOpt&=~0x4000;
	Pico.m.dirtyPal = 1;
	oldmodes = ((Pico.video.reg[12]&1)<<2) ^ 0xc;
	//find_combos(); // TODO

	// pal/ntsc might have changed, reset related stuff
	target_fps = Pico.m.pal ? 50 : 60;
	target_frametime = (1000<<8)/target_fps;
	reset_timing = 1;

	// prepare sound stuff
/*	if (currentConfig.EmuOpt & 4) {
		int snd_excess_add;
		if (PsndRate != PsndRate_old || (PicoOpt&0x0b) != (PicoOpt_old&0x0b) || Pico.m.pal != pal_old) {
			sound_rerate(Pico.m.frame_count ? 1 : 0);
		}
		snd_excess_add = ((PsndRate - PsndLen*target_fps)<<16) / target_fps;
		lprintf("starting audio: %i len: %i (ex: %04x) stereo: %i, pal: %i\n",
			PsndRate, PsndLen, snd_excess_add, (PicoOpt&8)>>3, Pico.m.pal);
		gp2x_start_sound(PsndRate, 16, (PicoOpt&8)>>3);
		gp2x_sound_volume(currentConfig.volume, currentConfig.volume);
		PicoWriteSound = updateSound;
		memset(sndBuffer, 0, sizeof(sndBuffer));
		PsndOut = sndBuffer;
		PsndRate_old = PsndRate;
		PsndLen_real = PsndLen;
		PicoOpt_old  = PicoOpt;
		pal_old = Pico.m.pal;
	} else*/ {
		PsndOut = 0;
	}

	// prepare CD buffer
	if (PicoMCD & 1) PicoCDBufferInit();

	// loop?
	while (engineState == PGS_Running)
	{
		int modes;

		tval = GetTickCount();
		if (reset_timing || tval < tval_prev) {
			reset_timing = 0;
			tval_thissec = tval;
			frames_shown = frames_done = 0;
		}

		// show notice message?
		if (noticeMsgTime) {
			static int noticeMsgSum;
			if (tval - noticeMsgTime > 2000) { // > 2.0 sec
				noticeMsgTime = 0;
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
		if (modes != oldmodes) {
			osd_fps_x = OSD_FPS_X;
			//if (modes & 4)
				vidCpy8to16 = vidCpy8to16_40;
			//else
			//	vidCpy8to16 = vidCpy8to16_32col;
			oldmodes = modes;
			clearArea(1);
		}

		// second passed?
		if (tval - tval_thissec >= 1000)
		{
#ifdef BENCHMARK
			static int bench = 0, bench_fps = 0, bench_fps_s = 0, bfp = 0, bf[4];
			if(++bench == 10) {
				bench = 0;
				bench_fps_s = bench_fps;
				bf[bfp++ & 3] = bench_fps;
				bench_fps = 0;
			}
			bench_fps += frames_shown;
			sprintf(fpsbuff, "%02i/%02i/%02i", frames_shown, bench_fps_s, (bf[0]+bf[1]+bf[2]+bf[3])>>2);
#else
			if(currentConfig.EmuOpt & 2)
				sprintf(fpsbuff, "%02i/%02i", frames_shown, frames_done);
#endif
			tval_thissec = tval;

			if (PsndOut == 0 && currentConfig.Frameskip >= 0) {
				frames_done = frames_shown = 0;
			} else {
				// it is quite common for this implementation to leave 1 fame unfinished
				// when second changes, but we don't want buffer to starve.
				if (PsndOut && frames_done < target_fps && frames_done > target_fps-5) {
					updateKeys();
					SkipFrame(1); frames_done++;
				}

				frames_done  -= target_fps; if (frames_done  < 0) frames_done  = 0;
				frames_shown -= target_fps; if (frames_shown < 0) frames_shown = 0;
				if (frames_shown > frames_done) frames_shown = frames_done;
			}
		}
#ifdef PFRAMES
		sprintf(fpsbuff, "%i", Pico.m.frame_count);
#endif

		tval_prev = tval;
		lim_time = (frames_done+1) * target_frametime;
		if (currentConfig.Frameskip >= 0) // frameskip enabled
		{
			for (i = 0; i < currentConfig.Frameskip; i++) {
				updateKeys();
				SkipFrame(1); frames_done++;
				if (PsndOut) { // do framelimitting if sound is enabled
					int tval_diff;
					tval = GetTickCount();
					tval_diff = (int)(tval - tval_thissec) << 8;
					if (tval_diff < lim_time) // we are too fast
						simpleWait(tval + ((lim_time - tval_diff)>>8));
				}
				lim_time += target_frametime;
			}
		}
		else // auto frameskip
		{
			int tval_diff;
			tval = GetTickCount();
			tval_diff = (int)(tval - tval_thissec) << 8;
			if (tval_diff > lim_time)
			{
				// no time left for this frame - skip
				if (tval_diff - lim_time >= (300<<8)) {
					/* something caused a slowdown for us (disk access? cache flush?)
					 * try to recover by resetting timing... */
					reset_timing = 1;
					continue;
				}
				updateKeys();
				SkipFrame(tval_diff < lim_time+target_frametime*2); frames_done++;
				continue;
			}
		}

		updateKeys();

		if (giz_screen == NULL)
			giz_screen = Framework2D_LockBuffer();

		PicoFrame();
		blit(fpsbuff, notice);

		if (giz_screen != NULL) {
			Framework2D_UnlockBuffer();
			giz_screen = NULL;
		}

		// check time
		tval = GetTickCount();
		tval_diff = (int)(tval - tval_thissec) << 8;

		if (currentConfig.Frameskip < 0 && tval_diff - lim_time >= (300<<8)) // slowdown detection
			reset_timing = 1;
		else if (PsndOut != NULL || currentConfig.Frameskip < 0)
		{
			// sleep if we are still too fast
			if (tval_diff < lim_time)
			{
				// we are too fast
				simpleWait(tval + ((lim_time - tval_diff) >> 8));
			}
		}

		frames_done++; frames_shown++;
	}


	if (PicoMCD & 1) PicoCDBufferFree();

	// save SRAM
	if ((currentConfig.EmuOpt & 1) && SRam.changed) {
		emu_state_cb("Writing SRAM/BRAM..");
		emu_SaveLoadGame(0, 1);
		SRam.changed = 0;
	}
}


void emu_ResetGame(void)
{
	PicoReset(0);
	reset_timing = 1;
}

