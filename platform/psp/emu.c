#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syslimits.h> // PATH_MAX

#include <pspthreadman.h>

#include "psp.h"
#include "menu.h"
#include "emu.h"
#include "../common/emu.h"
#include "../common/lprintf.h"
#include "../../Pico/PicoInt.h"

#ifdef BENCHMARK
#define OSD_FPS_X 220
#else
#define OSD_FPS_X 260
#endif

// vram usage map:
// 000000-044000 fb0
// 044000-088000 fb1
// 088000-0ae000 texture0
// 0ae000-0d4000 texture1

char romFileName[PATH_MAX];
static unsigned char picoD2FB[(8+320)*(8+240+8)];
unsigned char *PicoDraw2FB = picoD2FB;  // temporary buffer for alt renderer ( (8+320)*(8+240+8) )
int engineState;

static int combo_keys = 0, combo_acts = 0; // keys and actions which need button combos
static unsigned int noticeMsgTime = 0;
int reset_timing = 0; // do we need this?


static void blit(const char *fps, const char *notice);
static void clearArea(int full);

void emu_noticeMsgUpdated(void)
{
	noticeMsgTime = sceKernelGetSystemTimeLow();
}

void emu_getMainDir(char *dst, int len)
{
	if (len > 0) *dst = 0;
}

static void emu_msg_cb(const char *msg)
{
	void *fb = psp_video_get_active_fb();

	memset32((int *)((char *)fb + 512*264*2), 0, 512*8*2/4);
	emu_textOut16(4, 264, msg);
	noticeMsgTime = sceKernelGetSystemTimeLow() - 2000000;

	/* assumption: emu_msg_cb gets called only when something slow is about to happen */
	reset_timing = 1;
}

void emu_stateCb(const char *str)
{
	clearArea(0);
	blit("", str);
}

static void emu_msg_tray_open(void)
{
	strcpy(noticeMsg, "CD tray opened");
	noticeMsgTime = sceKernelGetSystemTimeLow();
}


void emu_Init(void)
{
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

void emu_Deinit(void)
{
	// save SRAM
	if ((currentConfig.EmuOpt & 1) && SRam.changed) {
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
	currentConfig.EmuOpt  = 0x1f | 0x680; // | confirm_save, cd_leds, 16bit rend
	currentConfig.PicoOpt = 0x07 | 0xc00; // | cd_pcm, cd_cdda
	currentConfig.PsndRate = 22050;
	currentConfig.PicoRegion = 0; // auto
	currentConfig.PicoAutoRgnOrder = 0x184; // US, EU, JP
	currentConfig.Frameskip = -1; // auto
	currentConfig.volume = 50;
	currentConfig.KeyBinds[ 4] = 1<<0; // SACB RLDU
	currentConfig.KeyBinds[ 6] = 1<<1;
	currentConfig.KeyBinds[ 7] = 1<<2;
	currentConfig.KeyBinds[ 5] = 1<<3;
	currentConfig.KeyBinds[14] = 1<<4;
	currentConfig.KeyBinds[13] = 1<<5;
	currentConfig.KeyBinds[15] = 1<<6;
	currentConfig.KeyBinds[ 3] = 1<<7;
	currentConfig.KeyBinds[23] = 1<<26; // switch rend
	currentConfig.KeyBinds[ 8] = 1<<27; // save state
	currentConfig.KeyBinds[ 9] = 1<<28; // load state
	currentConfig.PicoCDBuffers = 0;
	currentConfig.scaling = 0;
}


static int EmuScan16(unsigned int num, void *sdata)
{
	if (!(Pico.video.reg[1]&8)) num += 8;
	DrawLineDest = (unsigned short *) psp_screen + 512*(num+1);

	return 0;
}

static int EmuScan8(unsigned int num, void *sdata)
{
	// draw like the fast renderer
	// TODO?
	//if (!(Pico.video.reg[1]&8)) num += 8;
	//HighCol = gfx_buffer + 328*(num+1);

	return 0;
}

static void osd_text(int x, const char *text)
{
	int len = strlen(text) * 8 / 2;
	int *p, h;
	for (h = 0; h < 8; h++) {
		p = (int *) ((unsigned short *) psp_screen+x+512*(264+h));
		p = (int *) ((int)p & ~3); // align
		memset32(p, 0, len);
	}
	emu_textOut16(x, 264, text);
}


static void cd_leds(void)
{
	static int old_reg = 0;
	unsigned int col_g, col_r, *p;

	if (!((Pico_mcd->s68k_regs[0] ^ old_reg) & 3)) return; // no change
	old_reg = Pico_mcd->s68k_regs[0];

	p = (unsigned int *)((short *)psp_screen + 512*2+4+2);
	col_g = (old_reg & 2) ? 0x06000600 : 0;
	col_r = (old_reg & 1) ? 0xc000c000 : 0;
	*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += 512/2 - 12/2;
	*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += 512/2 - 12/2;
	*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r;
}


static short localPal[0x100];

static void blit(const char *fps, const char *notice)
{
	int emu_opt = currentConfig.EmuOpt;

#if 0
	if (PicoOpt&0x10)
	{
		int lines_flags = 224;
		// 8bit fast renderer
		if (Pico.m.dirtyPal) {
			Pico.m.dirtyPal = 0;
			vidConvCpyRGB565(localPal, Pico.cram, 0x40);
		}
		if (!(Pico.video.reg[12]&1)) lines_flags|=0x10000;
		if (currentConfig.EmuOpt&0x4000)
			lines_flags|=0x40000; // (Pico.m.frame_count&1)?0x20000:0x40000;
		vidCpy8to16((unsigned short *)giz_screen+321*8, PicoDraw2FB+328*8, localPal, lines_flags);
	}
	else if (!(emu_opt&0x80))
	{
		int lines_flags;
		// 8bit accurate renderer
		if (Pico.m.dirtyPal) {
			Pico.m.dirtyPal = 0;
			vidConvCpyRGB565(localPal, Pico.cram, 0x40);
			if (Pico.video.reg[0xC]&8) { // shadow/hilight mode
				//vidConvCpyRGB32sh(localPal+0x40, Pico.cram, 0x40);
				//vidConvCpyRGB32hi(localPal+0x80, Pico.cram, 0x40); // TODO?
				blockcpy(localPal+0xc0, localPal+0x40, 0x40*2);
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
		lines_flags = (Pico.video.reg[1]&8) ? 240 : 224;
		if (!(Pico.video.reg[12]&1)) lines_flags|=0x10000;
		if (currentConfig.EmuOpt&0x4000)
			lines_flags|=0x40000; // (Pico.m.frame_count&1)?0x20000:0x40000;
		vidCpy8to16((unsigned short *)giz_screen+321*8, PicoDraw2FB+328*8, localPal, lines_flags);
	}
#endif

	if (notice || (emu_opt & 2)) {
		if (notice)      osd_text(4, notice);
		if (emu_opt & 2) osd_text(OSD_FPS_X, fps);
	}

	if ((emu_opt & 0x400) && (PicoMCD & 1))
		cd_leds();

	psp_video_flip(0);
}

// clears whole screen or just the notice area (in all buffers)
static void clearArea(int full)
{
	if (full) {
		memset32(psp_screen, 0, 512*272*2/4);
		psp_video_flip(0);
		memset32(psp_screen, 0, 512*272*2/4);
	} else {
		void *fb = psp_video_get_active_fb();
		memset32((int *)((char *)psp_screen + 512*264*2), 0, 512*8*2/4);
		memset32((int *)((char *)fb         + 512*264*2), 0, 512*8*2/4);
	}
}

static void vidResetMode(void)
{
	if (PicoOpt&0x10) {
	} else if (currentConfig.EmuOpt&0x80) {
		PicoDrawSetColorFormat(1);
		PicoScan = EmuScan16;
	} else {
		PicoDrawSetColorFormat(-1);
		PicoScan = EmuScan8;
	}
	if ((PicoOpt&0x10) || !(currentConfig.EmuOpt&0x80)) {
		// setup pal for 8-bit modes
		localPal[0xc0] = 0x0600;
		localPal[0xd0] = 0xc000;
		localPal[0xe0] = 0x0000; // reserved pixels for OSD
		localPal[0xf0] = 0xffff;
	}
	Pico.m.dirtyPal = 1;

	clearArea(1);
}

static void updateSound(int len)
{
	if (PicoOpt&8) len<<=1;

	// TODO..
}


static void SkipFrame(void)
{
	PicoSkipFrame=1;
	PicoFrame();
	PicoSkipFrame=0;
}

void emu_forcedFrame(void)
{
	int po_old = PicoOpt;
	int eo_old = currentConfig.EmuOpt;

	PicoOpt &= ~0x0010;
	PicoOpt |=  0x4080; // soft_scale | acc_sprites
	currentConfig.EmuOpt |= 0x80;

	PicoDrawSetColorFormat(1);
	PicoScan = EmuScan16;
	PicoScan((unsigned) -1, NULL);
	Pico.m.dirtyPal = 1;
	PicoFrameDrawOnly();

	PicoOpt = po_old;
	currentConfig.EmuOpt = eo_old;
}


static void RunEvents(unsigned int which)
{
	if (which & 0x1800) // save or load (but not both)
	{
		int do_it = 1;

		if ( emu_checkSaveFile(state_slot) &&
				(( (which & 0x1000) && (currentConfig.EmuOpt & 0x800)) || // load
				 (!(which & 0x1000) && (currentConfig.EmuOpt & 0x200))) ) // save
		{
			int keys;
			blit("", (which & 0x1000) ? "LOAD STATE? (X=yes, O=no)" : "OVERWRITE SAVE? (X=yes, O=no)");
			while( !((keys = psp_pad_read(1)) & (BTN_X|BTN_CIRCLE)) )
				psp_msleep(50);
			if (keys & BTN_CIRCLE) do_it = 0;
			while(  ((keys = psp_pad_read(1)) & (BTN_X|BTN_CIRCLE)) ) // wait for release
				psp_msleep(50);
			clearArea(0);
		}

		if (do_it)
		{
			osd_text(4, (which & 0x1000) ? "LOADING GAME" : "SAVING GAME");
			PicoStateProgressCB = emu_stateCb;
			emu_SaveLoadGame((which & 0x1000) >> 12, 0);
			PicoStateProgressCB = NULL;
			psp_msleep(0);
		}

		reset_timing = 1;
	}
	if (which & 0x0400) // switch renderer
	{
		if (PicoOpt&0x10) { PicoOpt&=~0x10; currentConfig.EmuOpt |=  0x80; }
		else              { PicoOpt|= 0x10; currentConfig.EmuOpt &= ~0x80; }

		vidResetMode();

		if (PicoOpt&0x10) {
			strcpy(noticeMsg, " 8bit fast renderer");
		} else if (currentConfig.EmuOpt&0x80) {
			strcpy(noticeMsg, "16bit accurate renderer");
		} else {
			strcpy(noticeMsg, " 8bit accurate renderer");
		}

		noticeMsgTime = sceKernelGetSystemTimeLow();
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
		noticeMsgTime = sceKernelGetSystemTimeLow();
	}
}

static void updateKeys(void)
{
	unsigned int keys, allActions[2] = { 0, 0 }, events;
	static unsigned int prevEvents = 0;
	int i;

	keys = psp_pad_read(0);
	if (keys & BTN_SELECT)
		engineState = PGS_Menu;

	keys &= CONFIGURABLE_KEYS;

	for (i = 0; i < 32; i++)
	{
		if (keys & (1 << i))
		{
			int pl, acts = currentConfig.KeyBinds[i];
			if (!acts) continue;
			pl = (acts >> 16) & 1;
			if (combo_keys & (1 << i))
			{
				int u = i+1, acts_c = acts & combo_acts;
				// let's try to find the other one
				if (acts_c)
					for (; u < 32; u++)
						if ( (currentConfig.KeyBinds[u] & acts_c) && (keys & (1 << u)) ) {
							allActions[pl] |= acts_c;
							keys &= ~((1 << i) | (1 << u));
							break;
						}
				// add non-combo actions if combo ones were not found
				if (!acts_c || u == 32)
					allActions[pl] |= acts & ~combo_acts;
			} else {
				allActions[pl] |= acts;
			}
		}
	}

	PicoPad[0] = (unsigned short) allActions[0];
	PicoPad[1] = (unsigned short) allActions[1];

	events = (allActions[0] | allActions[1]) >> 16;

	// volume is treated in special way and triggered every frame
	if ((events & 0x6000) && PsndOut != NULL)
	{
		int vol = currentConfig.volume;
		if (events & 0x2000) {
			if (vol < 100) vol++;
		} else {
			if (vol >   0) vol--;
		}
		// FrameworkAudio_SetVolume(vol, vol); // TODO
		sprintf(noticeMsg, "VOL: %02i ", vol);
		noticeMsgTime = sceKernelGetSystemTimeLow();
		currentConfig.volume = vol;
	}

	events &= ~prevEvents;
	if (events) RunEvents(events);
	if (movie_data) emu_updateMovie();

	prevEvents = (allActions[0] | allActions[1]) >> 16;
}

static void find_combos(void)
{
	int act, u;

	// find out which keys and actions are combos
	combo_keys = combo_acts = 0;
	for (act = 0; act < 32; act++)
	{
		int keyc = 0;
		if (act == 16 || act == 17) continue; // player2 flag
		for (u = 0; u < 32; u++)
		{
			if (currentConfig.KeyBinds[u] & (1 << act)) keyc++;
		}
		if (keyc > 1)
		{
			// loop again and mark those keys and actions as combo
			for (u = 0; u < 32; u++)
			{
				if (currentConfig.KeyBinds[u] & (1 << act)) {
					combo_keys |= 1 << u;
					combo_acts |= 1 << act;
				}
			}
		}
	}
}


static void simpleWait(unsigned int until)
{
	unsigned int tval;
	int diff;

	tval = sceKernelGetSystemTimeLow();
	diff = (int)until - (int)tval;
	if (diff >= 512 && diff < 100*1024)
		sceKernelDelayThread(diff);
}

void emu_Loop(void)
{
	//static int PsndRate_old = 0, PicoOpt_old = 0, pal_old = 0;
	char fpsbuff[24]; // fps count c string
	unsigned int tval, tval_prev = 0, tval_thissec = 0; // timing
	int frames_done = 0, frames_shown = 0, oldmodes = 0;
	int target_fps, target_frametime, lim_time, tval_diff, i;
	char *notice = NULL;

	lprintf("entered emu_Loop()\n");

	fpsbuff[0] = 0;

	// make sure we are in correct mode
	vidResetMode();
	Pico.m.dirtyPal = 1;
	oldmodes = ((Pico.video.reg[12]&1)<<2) ^ 0xc;
	find_combos();

	// pal/ntsc might have changed, reset related stuff
	target_fps = Pico.m.pal ? 50 : 60;
	target_frametime = Pico.m.pal ? (1000000<<8)/50 : (1000000<<8)/60+1;
	reset_timing = 1;

	// prepare CD buffer
	if (PicoMCD & 1) PicoCDBufferInit();

	// prepare sound stuff
	PsndOut = NULL;
#if 0 // TODO
	if (currentConfig.EmuOpt & 4)
	{
		int ret, snd_excess_add, stereo;
		if (PsndRate != PsndRate_old || (PicoOpt&0x0b) != (PicoOpt_old&0x0b) || Pico.m.pal != pal_old) {
			sound_rerate(Pico.m.frame_count ? 1 : 0);
		}
		stereo=(PicoOpt&8)>>3;
		snd_excess_add = ((PsndRate - PsndLen*target_fps)<<16) / target_fps;
		snd_cbuf_samples = (PsndRate<<stereo) * 16 / target_fps;
		lprintf("starting audio: %i len: %i (ex: %04x) stereo: %i, pal: %i\n",
			PsndRate, PsndLen, snd_excess_add, stereo, Pico.m.pal);
		ret = FrameworkAudio_Init(PsndRate, snd_cbuf_samples, stereo);
		if (ret != 0) {
			lprintf("FrameworkAudio_Init() failed: %i\n", ret);
			sprintf(noticeMsg, "sound init failed (%i), snd disabled", ret);
			noticeMsgTime = sceKernelGetSystemTimeLow();
			currentConfig.EmuOpt &= ~4;
		} else {
			FrameworkAudio_SetVolume(currentConfig.volume, currentConfig.volume);
			PicoWriteSound = updateSound;
			snd_cbuff = FrameworkAudio_56448Buffer();
			PsndOut = snd_cbuff + snd_cbuf_samples / 2; // start writing at the middle
			snd_all_samples = 0;
			PsndRate_old = PsndRate;
			PicoOpt_old  = PicoOpt;
			pal_old = Pico.m.pal;
		}
	}
#endif

	// loop?
	while (engineState == PGS_Running)
	{
		int modes;

		tval = sceKernelGetSystemTimeLow();
		if (reset_timing || tval < tval_prev) {
			//stdbg("timing reset");
			reset_timing = 0;
			tval_thissec = tval;
			frames_shown = frames_done = 0;
		}

		// show notice message?
		if (noticeMsgTime) {
			static int noticeMsgSum;
			if (tval - noticeMsgTime > 2000000) { // > 2.0 sec
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
			oldmodes = modes;
			clearArea(1);
		}

		// second passed?
		if (tval - tval_thissec >= 1000000)
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
			tval_thissec += 1000000;

			if (currentConfig.Frameskip < 0) {
				frames_done  -= target_fps; if (frames_done  < 0) frames_done  = 0;
				frames_shown -= target_fps; if (frames_shown < 0) frames_shown = 0;
				if (frames_shown > frames_done) frames_shown = frames_done;
			} else {
				frames_done = frames_shown = 0;
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
				SkipFrame(); frames_done++;
				if (PsndOut) { // do framelimitting if sound is enabled
					int tval_diff;
					tval = sceKernelGetSystemTimeLow();
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
			tval = sceKernelGetSystemTimeLow();
			tval_diff = (int)(tval - tval_thissec) << 8;
			if (tval_diff > lim_time)
			{
				// no time left for this frame - skip
				if (tval_diff - lim_time >= (300000<<8)) {
					/* something caused a slowdown for us (disk access? cache flush?)
					 * try to recover by resetting timing... */
					reset_timing = 1;
					continue;
				}
				updateKeys();
				SkipFrame(); frames_done++;
				continue;
			}
		}

		updateKeys();

		if (!(PicoOpt&0x10))
			PicoScan((unsigned) -1, NULL);

		PicoFrame();

		blit(fpsbuff, notice);

		// check time
		tval = sceKernelGetSystemTimeLow();
		tval_diff = (int)(tval - tval_thissec) << 8;

		if (currentConfig.Frameskip < 0 && tval_diff - lim_time >= (300000<<8)) // slowdown detection
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
/*
	if (PsndOut != NULL) {
		PsndOut = snd_cbuff = NULL;
		FrameworkAudio_Close();
	}
*/
	// save SRAM
	if ((currentConfig.EmuOpt & 1) && SRam.changed) {
		emu_stateCb("Writing SRAM/BRAM..");
		emu_SaveLoadGame(0, 1);
		SRam.changed = 0;
	}
}


void emu_ResetGame(void)
{
	PicoReset(0);
	reset_timing = 1;
}

