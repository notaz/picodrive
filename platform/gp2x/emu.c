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
#include "usbjoy.h"
#include "menu.h"
#include "asmutils.h"
#include "cpuctrl.h"

#include <Pico/PicoInt.h>
#include <Pico/Patch.h>
#include <zlib/zlib.h>


#ifdef BENCHMARK
#define OSD_FPS_X 220
#else
#define OSD_FPS_X 260
#endif

// PicoPad[] format: SACB RLDU
char *actionNames[] = {
	"UP", "DOWN", "LEFT", "RIGHT", "B", "C", "A", "START",
	0, 0, 0, 0, 0, 0, 0, 0, // Z, Y, X, MODE (enabled only when needed), ?, ?, ?, ?
	0, 0, 0, 0, 0, 0, 0, "ENTER MENU", // player2_flag, ?, ?, ?, ?, ?, ?, menu
	"NEXT SAVE SLOT", "PREV SAVE SLOT", "SWITCH RENDERER", "SAVE STATE",
	"LOAD STATE", "VOLUME UP", "VOLUME DOWN", "DONE"
};

int engineState;
int select_exits = 0;
char *PicoConfigFile = "picoconfig.bin";
currentConfig_t currentConfig;

char romFileName[PATH_MAX];
unsigned char *rom_data = NULL;

extern int crashed_940;

static short sndBuffer[2*44100/50];
static char noticeMsg[64];					// notice msg to draw
static struct timeval noticeMsgTime = { 0, 0 };	// when started showing
static int osd_fps_x;
static int combo_keys = 0, combo_acts = 0;	// keys and actions which need button combos
static int gp2x_old_gamma = 100;
static unsigned char *movie_data = NULL;
static int movie_size = 0;
unsigned char *framebuff = 0;  // temporary buffer for alt renderer
int state_slot = 0;
int reset_timing = 0;
int config_slot = 0, config_slot_current = 0;


// utilities
static void strlwr(char* string)
{
	while ( (*string++ = (char)tolower(*string)) );
}

static int try_rfn_cut(void)
{
	FILE *tmp;
	char *p;

	p = romFileName + strlen(romFileName) - 1;
	for (; p > romFileName; p--)
		if (*p == '.') break;
	*p = 0;

	if((tmp = fopen(romFileName, "rb"))) {
		fclose(tmp);
		return 1;
	}
	return 0;
}

static void get_ext(char *file, char *ext)
{
	char *p;

	p = file + strlen(file) - 4;
	if (p < file) p = file;
	strncpy(ext, p, 4);
	ext[4] = 0;
	strlwr(ext);
}

char *biosfiles_us[] = { "us_scd2_9306", "SegaCDBIOS9303", "us_scd1_9210" };
char *biosfiles_eu[] = { "eu_mcd2_9306", "eu_mcd2_9303",   "eu_mcd1_9210" };
char *biosfiles_jp[] = { "jp_mcd1_9112", "jp_mcd1_9111" };

extern char **g_argv;

int find_bios(int region, char **bios_file)
{
	static char bios_path[1024];
	int i, j, count;
	char **files;
	FILE *f = NULL;

	if (region == 4) { // US
		files = biosfiles_us;
		count = sizeof(biosfiles_us) / sizeof(char *);
	} else if (region == 8) { // EU
		files = biosfiles_eu;
		count = sizeof(biosfiles_eu) / sizeof(char *);
	} else if (region == 1 || region == 2) {
		files = biosfiles_jp;
		count = sizeof(biosfiles_jp) / sizeof(char *);
	} else {
		return 0;
	}

	for (i = 0; i < count; i++)
	{
		strncpy(bios_path, g_argv[0], 1023);
		bios_path[1024-32] = 0;
		for (j = strlen(bios_path); j > 0; j--)
			if (bios_path[j] == '/') { bios_path[j+1] = 0; break; }
		strcat(bios_path, files[i]);
		strcat(bios_path, ".bin");
		f = fopen(bios_path, "rb");
		if (f) break;

		bios_path[strlen(bios_path) - 4] = 0;
		strcat(bios_path, ".zip");
		f = fopen(bios_path, "rb");
		if (f) break;
	}

	if (f) {
		printf("using bios: %s\n", bios_path);
		fclose(f);
		if (bios_file) *bios_file = bios_path;
		return 1;
	} else {
		sprintf(menuErrorMsg, "no %s BIOS files found, read docs",
			region != 4 ? (region == 8 ? "EU" : "JAP") : "USA");
		printf("%s\n", menuErrorMsg);
		return 0;
	}
}

/* checks if romFileName points to valid MegaCD image
 * if so, checks for suitable BIOS */
int emu_cd_check(char **bios_file)
{
	unsigned char buf[32];
	pm_file *cd_f;
	int type = 0, region = 4; // 1: Japan, 4: US, 8: Europe

	cd_f = pm_open(romFileName);
	if (!cd_f) return 0; // let the upper level handle this

	if (pm_read(buf, 32, cd_f) != 32) {
		pm_close(cd_f);
		return 0;
	}

	if (!strncasecmp("SEGADISCSYSTEM", (char *)buf+0x00, 14)) type = 1;       // Sega CD (ISO)
	if (!strncasecmp("SEGADISCSYSTEM", (char *)buf+0x10, 14)) type = 2;       // Sega CD (BIN)
	if (type == 0) {
		pm_close(cd_f);
		return 0;
	}

	/* it seems we have a CD image here. Try to detect region now.. */
	pm_seek(cd_f, (type == 1) ? 0x100+0x10B : 0x110+0x10B, SEEK_SET);
	pm_read(buf, 1, cd_f);
	pm_close(cd_f);

	if (buf[0] == 0x64) region = 8; // EU
	if (buf[0] == 0xa1) region = 1; // JAP

	printf("detected %s Sega/Mega CD image with %s region\n",
		type == 2 ? "BIN" : "ISO", region != 4 ? (region == 8 ? "EU" : "JAP") : "USA");

	if (PicoRegionOverride) {
		region = PicoRegionOverride;
		printf("overrided region to %s\n", region != 4 ? (region == 8 ? "EU" : "JAP") : "USA");
	}

	if (bios_file == NULL) return type;

	if (find_bios(region, bios_file))
		 return type;	// CD and BIOS detected

	return -1;     		// CD detected but load failed
}

int emu_ReloadRom(void)
{
	unsigned int rom_size = 0;
	char *used_rom_name = romFileName;
	char ext[5];
	pm_file *rom;
	int ret, cd_state;

	printf("emu_ReloadRom(%s)\n", romFileName);

	get_ext(romFileName, ext);

	// detect wrong extensions
	if(!strcmp(ext, ".srm") || !strcmp(ext, "s.gz") || !strcmp(ext, ".mds")) { // s.gz ~ .mds.gz
		sprintf(menuErrorMsg, "Not a ROM selected.");
		return 0;
	}

	PicoPatchUnload();

	// check for movie file
	if(movie_data) {
		free(movie_data);
		movie_data = 0;
	}
	if(!strcmp(ext, ".gmv")) {
		// check for both gmv and rom
		int dummy;
		FILE *movie_file = fopen(romFileName, "rb");
		if(!movie_file) {
			sprintf(menuErrorMsg, "Failed to open movie.");
			return 0;
		}
		fseek(movie_file, 0, SEEK_END);
		movie_size = ftell(movie_file);
		fseek(movie_file, 0, SEEK_SET);
		if(movie_size < 64+3) {
			sprintf(menuErrorMsg, "Invalid GMV file.");
			fclose(movie_file);
			return 0;
		}
		movie_data = malloc(movie_size);
		if(movie_data == NULL) {
			sprintf(menuErrorMsg, "low memory.");
			fclose(movie_file);
			return 0;
		}
		fread(movie_data, 1, movie_size, movie_file);
		fclose(movie_file);
		if (strncmp((char *)movie_data, "Gens Movie TEST", 15) != 0) {
			sprintf(menuErrorMsg, "Invalid GMV file.");
			return 0;
		}
		dummy = try_rfn_cut() || try_rfn_cut();
		if (!dummy) {
			sprintf(menuErrorMsg, "Could't find a ROM for movie.");
			return 0;
		}
		get_ext(romFileName, ext);
	}
	else if (!strcmp(ext, ".pat")) {
		int dummy;
		PicoPatchLoad(romFileName);
		dummy = try_rfn_cut() || try_rfn_cut();
		if (!dummy) {
			sprintf(menuErrorMsg, "Could't find a ROM to patch.");
			return 0;
		}
		get_ext(romFileName, ext);
	}

	if ((PicoMCD & 1) && Pico_mcd != NULL)
		Stop_CD();

	// check for MegaCD image
	cd_state = emu_cd_check(&used_rom_name);
	if (cd_state > 0) {
		PicoMCD |= 1;
		get_ext(used_rom_name, ext);
	} else if (cd_state == -1) {
		// bios_help() ?
		return 0;
	} else {
		if (PicoMCD & 1) PicoExitMCD();
		PicoMCD &= ~1;
	}

	rom = pm_open(used_rom_name);
	if(!rom) {
		sprintf(menuErrorMsg, "Failed to open rom.");
		return 0;
	}

	if(rom_data) {
		free(rom_data);
		rom_data = 0;
		rom_size = 0;
	}

	if( (ret = PicoCartLoad(rom, &rom_data, &rom_size)) ) {
		sprintf(menuErrorMsg, "PicoCartLoad() failed.");
		printf("%s\n", menuErrorMsg);
		pm_close(rom);
		return 0;
	}
	pm_close(rom);

	// detect wrong files (Pico crashes on very small files), also see if ROM EP is good
	if(rom_size <= 0x200 || strncmp((char *)rom_data, "Pico", 4) == 0 ||
	  ((*(unsigned char *)(rom_data+4)<<16)|(*(unsigned short *)(rom_data+6))) >= (int)rom_size) {
		if (rom_data) free(rom_data);
		rom_data = 0;
		sprintf(menuErrorMsg, "Not a ROM selected.");
		return 0;
	}

	// load config for this ROM (do this before insert to get correct region)
	ret = emu_ReadConfig(1);
	if (!ret)
		emu_ReadConfig(0);

	printf("PicoCartInsert(%p, %d);\n", rom_data, rom_size);
	if(PicoCartInsert(rom_data, rom_size)) {
		sprintf(menuErrorMsg, "Failed to load ROM.");
		return 0;
	}

	Pico.m.frame_count = 0;

	// insert CD if it was detected
	if (cd_state > 0) {
		ret = Insert_CD(romFileName, cd_state == 2);
		if (ret != 0) {
			sprintf(menuErrorMsg, "Insert_CD() failed, invalid CD image?");
			printf("%s\n", menuErrorMsg);
			return 0;
		}
	}

	// emu_ReadConfig() might have messed currentConfig.lastRomFile
	strncpy(currentConfig.lastRomFile, romFileName, sizeof(currentConfig.lastRomFile)-1);
	currentConfig.lastRomFile[sizeof(currentConfig.lastRomFile)-1] = 0;

	if (PicoPatches) {
		PicoPatchPrepare();
		PicoPatchApply();
	}

	// additional movie stuff
	if(movie_data) {
		if(movie_data[0x14] == '6')
		     PicoOpt |=  0x20; // 6 button pad
		else PicoOpt &= ~0x20;
		PicoOpt |= 0x40; // accurate timing
		if(movie_data[0xF] >= 'A') {
			if(movie_data[0x16] & 0x80) {
				PicoRegionOverride = 8;
			} else {
				PicoRegionOverride = 4;
			}
			PicoReset(0);
			// TODO: bits 6 & 5
		}
		movie_data[0x18+30] = 0;
		sprintf(noticeMsg, "MOVIE: %s", (char *) &movie_data[0x18]);
	}
	else
	{
		if(Pico.m.pal) {
			strcpy(noticeMsg, "PAL SYSTEM / 50 FPS");
		} else {
			strcpy(noticeMsg, "NTSC SYSTEM / 60 FPS");
		}
	}
	gettimeofday(&noticeMsgTime, 0);

	// load SRAM for this ROM
	if(currentConfig.EmuOpt & 1)
		emu_SaveLoadGame(1, 1);

	return 1;
}


static void emu_msg_cb(const char *msg);
static void emu_msg_tray_open(void);

void emu_Init(void)
{
	// make temp buffer for alt renderer
	framebuff = malloc((8+320)*(8+240+8));
	if (!framebuff)
	{
		printf("framebuff == 0\n");
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


static void romfname_ext(char *dst, const char *prefix, const char *ext)
{
	char *p;
	int prefix_len = 0;

	// make save filename
	for (p = romFileName+strlen(romFileName)-1; p >= romFileName && *p != '/'; p--); p++;
	*dst = 0;
	if (prefix) {
		strcpy(dst, prefix);
		prefix_len = strlen(prefix);
	}
	strncpy(dst + prefix_len, p, 511-prefix_len);
	dst[511-8] = 0;
	if (dst[strlen(dst)-4] == '.') dst[strlen(dst)-4] = 0;
	if (ext) strcat(dst, ext);
}


static void find_combos(void)
{
	int act, u;

	// find out which keys and actions are combos
	combo_keys = combo_acts = 0;
	for (act = 0; act < 32; act++)
	{
		int keyc = 0;
		if (act == 16) continue; // player2 flag
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
	// printf("combo keys/acts: %08x %08x\n", combo_keys, combo_acts);
}


void scaling_update(void)
{
	PicoOpt &= ~0x4100;
	switch (currentConfig.scaling) {
		default: break; // off
		case 1:  // hw hor
		case 2:  PicoOpt |=  0x0100; break; // hw hor+vert
		case 3:  PicoOpt |=  0x4000; break; // sw hor
	}
}


int emu_ReadConfig(int game)
{
	FILE *f;
	char cfg[512], extbuf[16];
	int bread = 0;

	if (!game)
	{
		// set default config
		memset(&currentConfig, 0, sizeof(currentConfig));
		currentConfig.lastRomFile[0] = 0;
		currentConfig.EmuOpt  = 0x1f | 0x600; // | confirm_save, cd_leds
		currentConfig.PicoOpt = 0x0f | 0xe00; // | use_940, cd_pcm, cd_cdda
		currentConfig.PsndRate = 22050; // 44100;
		currentConfig.PicoRegion = 0; // auto
		currentConfig.PicoAutoRgnOrder = 0x184; // US, EU, JP
		currentConfig.Frameskip = -1; // auto
		currentConfig.CPUclock = 200;
		currentConfig.volume = 50;
		currentConfig.KeyBinds[ 0] = 1<<0; // SACB RLDU
		currentConfig.KeyBinds[ 4] = 1<<1;
		currentConfig.KeyBinds[ 2] = 1<<2;
		currentConfig.KeyBinds[ 6] = 1<<3;
		currentConfig.KeyBinds[14] = 1<<4;
		currentConfig.KeyBinds[13] = 1<<5;
		currentConfig.KeyBinds[12] = 1<<6;
		currentConfig.KeyBinds[ 8] = 1<<7;
		currentConfig.KeyBinds[15] = 1<<26; // switch rend
		currentConfig.KeyBinds[10] = 1<<27; // save state
		currentConfig.KeyBinds[11] = 1<<28; // load state
		currentConfig.KeyBinds[23] = 1<<29; // vol up
		currentConfig.KeyBinds[22] = 1<<30; // vol down
		currentConfig.gamma = 100;
		currentConfig.PicoCDBuffers = 64;
		currentConfig.scaling = 0;
		strncpy(cfg, PicoConfigFile, 511);
		if (config_slot != 0)
		{
			char *p = strrchr(cfg, '.');
			if (p == NULL) p = cfg + strlen(cfg);
			sprintf(extbuf, ".%i.pbcfg", config_slot);
			strncpy(p, extbuf, 511 - (p - cfg));
		}
		cfg[511] = 0;
	} else {
		if (config_slot != 0)
		     sprintf(extbuf, ".%i.pbcfg", config_slot);
		else strcpy(extbuf, ".pbcfg");
		romfname_ext(cfg, "cfg/", extbuf);
		f = fopen(cfg, "rb");
		if (!f) romfname_ext(cfg, NULL, ".pbcfg");
		else fclose(f);
	}

	printf("emu_ReadConfig: %s ", cfg);
	f = fopen(cfg, "rb");
	if (f) {
		bread = fread(&currentConfig, 1, sizeof(currentConfig), f);
		fclose(f);
	}
	printf(bread > 0 ? "(ok)\n" : "(failed)\n");

	PicoOpt = currentConfig.PicoOpt;
	PsndRate = currentConfig.PsndRate;
	PicoRegionOverride = currentConfig.PicoRegion;
	PicoAutoRgnOrder = currentConfig.PicoAutoRgnOrder;
	PicoCDBuffers = currentConfig.PicoCDBuffers;
	if (PicoOpt & 0x20) {
		actionNames[ 8] = "Z"; actionNames[ 9] = "Y";
		actionNames[10] = "X"; actionNames[11] = "MODE";
	}
	scaling_update();
	// some sanity checks
	if (currentConfig.CPUclock < 10 || currentConfig.CPUclock > 4096) currentConfig.CPUclock = 200;
	if (currentConfig.gamma < 10 || currentConfig.gamma > 300) currentConfig.gamma = 100;
	if (currentConfig.volume < 0 || currentConfig.volume > 99) currentConfig.volume = 50;
	// if volume keys are unbound, bind them to volume control
	if (!currentConfig.KeyBinds[23] && !currentConfig.KeyBinds[22]) {
		currentConfig.KeyBinds[23] = 1<<29; // vol up
		currentConfig.KeyBinds[22] = 1<<30; // vol down
	}

	if (bread > 0) config_slot_current = config_slot;
	return (bread > 0); // == sizeof(currentConfig));
}


int emu_WriteConfig(int game)
{
	FILE *f;
	char cfg[512], extbuf[16];
	int bwrite = 0;

	if (!game)
	{
		strncpy(cfg, PicoConfigFile, 511);
		if (config_slot != 0)
		{
			char *p = strrchr(cfg, '.');
			if (p == NULL) p = cfg + strlen(cfg);
			sprintf(extbuf, ".%i.pbcfg", config_slot);
			strncpy(p, extbuf, 511 - (p - cfg));
		}
		cfg[511] = 0;
	} else {
		if (config_slot != 0)
		     sprintf(extbuf, ".%i.pbcfg", config_slot);
		else strcpy(extbuf, ".pbcfg");
		romfname_ext(cfg, "cfg/", extbuf);
	}

	printf("emu_WriteConfig: %s ", cfg);
	f = fopen(cfg, "wb");
	if (f) {
		currentConfig.PicoOpt = PicoOpt;
		currentConfig.PsndRate = PsndRate;
		currentConfig.PicoRegion = PicoRegionOverride;
		currentConfig.PicoAutoRgnOrder = PicoAutoRgnOrder;
		currentConfig.PicoCDBuffers = PicoCDBuffers;
		bwrite = fwrite(&currentConfig, 1, sizeof(currentConfig), f);
		fflush(f);
		fclose(f);
#ifndef NO_SYNC
		sync();
#endif
	}
	printf((bwrite == sizeof(currentConfig)) ? "(ok)\n" : "(failed)\n");

	if (bwrite == sizeof(currentConfig)) config_slot_current = config_slot;
	return (bwrite == sizeof(currentConfig));
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
#ifndef NO_SYNC
			sync();
#endif
		}
	}

	free(framebuff);

	PicoExit();

	// restore gamma
	if (gp2x_old_gamma != 100)
		set_gamma(100, 0);
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
		gp2x_text_out8_2(x, y, text, 0xf0);
	} else {
		int *p, i, h;
		x &= ~1; // align x
		len = (len+1) >> 1;
		for (h = 0; h < 8; h++) {
			p = (int *) ((unsigned short *) gp2x_screen+x+320*(y+h));
			for (i = len; i; i--, p++) *p = (*p>>2)&0x39e7;
		}
		gp2x_text_out15(x, y, text);
	}
}

static void cd_leds(void)
{
	// mmu problems?
//	static
	int old_reg;
//	if (!((Pico_mcd->s68k_regs[0] ^ old_reg) & 3)) return; // no change
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
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += 320/2 - 12/2;
	}
}

static int EmuScan16(unsigned int num, void *sdata)
{
	if (!(Pico.video.reg[1]&8)) num += 8;
	DrawLineDest = (unsigned short *) gp2x_screen + 320*(num+1);

	return 0;
}

static int EmuScan8(unsigned int num, void *sdata)
{
	if (!(Pico.video.reg[1]&8)) num += 8;
	DrawLineDest = (unsigned char *)  gp2x_screen + 320*(num+1);

	return 0;
}

int localPal[0x100];
static void (*vidCpyM2)(void *dest, void *src) = NULL;

static void blit(const char *fps, const char *notice)
{
	int emu_opt = currentConfig.EmuOpt;

	if (PicoOpt&0x10) {
		// 8bit fast renderer
		if (Pico.m.dirtyPal) {
			Pico.m.dirtyPal = 0;
			vidConvCpyRGB32(localPal, Pico.cram, 0x40);
			// feed new palette to our device
			gp2x_video_setpalette(localPal, 0x40);
		}
		vidCpyM2((unsigned char *)gp2x_screen+320*8, framebuff+328*8);
	} else if (!(emu_opt&0x80)) {
		// 8bit accurate renderer
		if (Pico.m.dirtyPal) {
			Pico.m.dirtyPal = 0;
			if(Pico.video.reg[0xC]&8) { // shadow/hilight mode
				vidConvCpyRGB32(localPal, Pico.cram, 0x40);
				vidConvCpyRGB32sh(localPal+0x40, Pico.cram, 0x40);
				vidConvCpyRGB32hi(localPal+0x80, Pico.cram, 0x40);
				blockcpy(localPal+0xc0, localPal+0x40, 0x40*4);
				localPal[0xc0] = 0x0000c000;
				localPal[0xd0] = 0x00c00000;
				localPal[0xe0] = 0x00000000; // reserved pixels for OSD
				localPal[0xf0] = 0x00ffffff;
				gp2x_video_setpalette(localPal, 0x100);
			} else if (rendstatus & 0x20) { // mid-frame palette changes
				vidConvCpyRGB32(localPal, Pico.cram, 0x40);
				vidConvCpyRGB32(localPal+0x40, HighPal, 0x40);
				vidConvCpyRGB32(localPal+0x80, HighPal+0x40, 0x40);
				gp2x_video_setpalette(localPal, 0xc0);
			} else {
				vidConvCpyRGB32(localPal, Pico.cram, 0x40);
				gp2x_video_setpalette(localPal, 0x40);
			}
		}
	}

	if (notice || (emu_opt & 2)) {
		int h = 232;
		if (currentConfig.scaling == 2 && !(Pico.video.reg[1]&8)) h -= 8;
		if (notice) osd_text(4, h, notice);
		if (emu_opt & 2)
			osd_text(osd_fps_x, h, fps);
	}
	if ((emu_opt & 0x400) && (PicoMCD & 1))
		cd_leds();

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
		gp2x_video_changemode(15);
		PicoDrawSetColorFormat(1);
		PicoScan = EmuScan16;
		PicoScan(0, 0);
	} else {
		gp2x_video_changemode(8);
		PicoDrawSetColorFormat(2);
		PicoScan = EmuScan8;
		PicoScan(0, 0);
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

static void RunEvents(unsigned int which)
{
	if(which & 0x1800) { // save or load (but not both)
		int do_it = 1;
		if ( emu_check_save_file(state_slot) &&
				(( (which & 0x1000) && (currentConfig.EmuOpt & 0x800)) ||   // load
				 (!(which & 0x1000) && (currentConfig.EmuOpt & 0x200))) ) { // save
			unsigned long keys;
			blit("", (which & 0x1000) ? "LOAD STATE? (Y=yes, X=no)" : "OVERWRITE SAVE? (Y=yes, X=no)");
			while( !((keys = gp2x_joystick_read(1)) & (GP2X_X|GP2X_Y)) )
				usleep(50*1024);
			if (keys & GP2X_X) do_it = 0;
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
	if(which & 0x0400) { // switch renderer
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
	if(which & 0x0300) {
		if(which&0x0200) {
			state_slot -= 1;
			if(state_slot < 0) state_slot = 9;
		} else {
			state_slot += 1;
			if(state_slot > 9) state_slot = 0;
		}
		sprintf(noticeMsg, "SAVE SLOT %i [%s]", state_slot, emu_check_save_file(state_slot) ? "USED" : "FREE");
		gettimeofday(&noticeMsgTime, 0);
	}
	if(which & 0x0080) {
		engineState = PGS_Menu;
	}
}


static void updateMovie(void)
{
	int offs = Pico.m.frame_count*3 + 0x40;
	if (offs+3 > movie_size) {
		free(movie_data);
		movie_data = 0;
		strcpy(noticeMsg, "END OF MOVIE.");
		printf("END OF MOVIE.\n");
		gettimeofday(&noticeMsgTime, 0);
	} else {
		// MXYZ SACB RLDU
		PicoPad[0] = ~movie_data[offs]   & 0x8f; // ! SCBA RLDU
		if(!(movie_data[offs]   & 0x10)) PicoPad[0] |= 0x40; // A
		if(!(movie_data[offs]   & 0x20)) PicoPad[0] |= 0x10; // B
		if(!(movie_data[offs]   & 0x40)) PicoPad[0] |= 0x20; // A
		PicoPad[1] = ~movie_data[offs+1] & 0x8f; // ! SCBA RLDU
		if(!(movie_data[offs+1] & 0x10)) PicoPad[1] |= 0x40; // A
		if(!(movie_data[offs+1] & 0x20)) PicoPad[1] |= 0x10; // B
		if(!(movie_data[offs+1] & 0x40)) PicoPad[1] |= 0x20; // A
		PicoPad[0] |= (~movie_data[offs+2] & 0x0A) << 8; // ! MZYX
		if(!(movie_data[offs+2] & 0x01)) PicoPad[0] |= 0x0400; // X
		if(!(movie_data[offs+2] & 0x04)) PicoPad[0] |= 0x0100; // Z
		PicoPad[1] |= (~movie_data[offs+2] & 0xA0) << 4; // ! MZYX
		if(!(movie_data[offs+2] & 0x10)) PicoPad[1] |= 0x0400; // X
		if(!(movie_data[offs+2] & 0x40)) PicoPad[1] |= 0x0100; // Z
	}
}


static void updateKeys(void)
{
	unsigned long keys, allActions[2] = { 0, 0 }, events;
	static unsigned long prevEvents = 0;
	int joy, i;

	keys = gp2x_joystick_read(0);
	if (keys & GP2X_SELECT) {
		engineState = select_exits ? PGS_Quit : PGS_Menu;
		// wait until select is released, so menu would not resume game
		while (gp2x_joystick_read(1) & GP2X_SELECT) usleep(50*1000);
	}

	keys &= CONFIGURABLE_KEYS;

	for (i = 0; i < 32; i++)
	{
		if (keys & (1 << i)) {
			int pl, acts = currentConfig.KeyBinds[i];
			if (!acts) continue;
			pl = (acts >> 16) & 1;
			if (combo_keys & (1 << i)) {
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

	// add joy inputs
	if (num_of_joys > 0)
	{
		gp2x_usbjoy_update();
		for (joy = 0; joy < num_of_joys; joy++) {
			int keys = gp2x_usbjoy_check2(joy);
			for (i = 0; i < 32; i++) {
				if (keys & (1 << i)) {
					int acts = currentConfig.JoyBinds[joy][i];
					int pl = (acts >> 16) & 1;
					allActions[pl] |= acts;
				}
			}
		}
	}

	PicoPad[0] = (unsigned short) allActions[0];
	PicoPad[1] = (unsigned short) allActions[1];

	events = (allActions[0] | allActions[1]) >> 16;

	// volume is treated in special way and triggered every frame
	if(events & 0x6000) {
		int vol = currentConfig.volume;
		if (events & 0x2000) {
			if (vol < 99) vol++;
		} else {
			if (vol >  0) vol--;
		}
		gp2x_sound_volume(vol, vol);
		sprintf(noticeMsg, "VOL: %02i", vol);
		gettimeofday(&noticeMsgTime, 0);
		currentConfig.volume = vol;
	}

	events &= ~prevEvents;
	if (events) RunEvents(events);
	if (movie_data) updateMovie();

	prevEvents = (allActions[0] | allActions[1]) >> 16;
}


static void updateSound(int len)
{
	if (PicoOpt&8) len<<=1;

	/* avoid writing audio when lagging behind to prevent audio lag */
	if (PicoSkipFrame != 2)
		gp2x_sound_write(PsndOut, len<<1);
}


static void SkipFrame(int do_audio)
{
	PicoSkipFrame=do_audio ? 1 : 2;
	PicoFrame();
	PicoSkipFrame=0;
}


void emu_forced_frame(void)
{
	int po_old = PicoOpt;

	PicoOpt |= 0x10;
	PicoFrameFull();

	if (!(Pico.video.reg[12]&1)) {
		vidCpyM2 = vidCpyM2_32col;
		clearArea(1);
	} else	vidCpyM2 = vidCpyM2_40col;

	vidCpyM2((unsigned char *)gp2x_screen+320*8, framebuff+328*8);
	vidConvCpyRGB32(localPal, Pico.cram, 0x40);
	gp2x_video_setpalette(localPal, 0x40);

	PicoOpt = po_old;
}

static void simpleWait(int thissec, int lim_time)
{
	struct timeval tval;

	spend_cycles(1024);
	gettimeofday(&tval, 0);
	if(thissec != tval.tv_sec) tval.tv_usec+=1000000;

	while(tval.tv_usec < lim_time)
	{
		spend_cycles(1024);
		gettimeofday(&tval, 0);
		if(thissec != tval.tv_sec) tval.tv_usec+=1000000;
	}
}


void emu_Loop(void)
{
	static int gp2x_old_clock = 200;
	static int PsndRate_old = 0, PicoOpt_old = 0, EmuOpt_old = 0, PsndLen_real = 0, pal_old = 0;
	char fpsbuff[24]; // fps count c string
	struct timeval tval; // timing
	int thissec = 0, frames_done = 0, frames_shown = 0, oldmodes = 0;
	int target_fps, target_frametime, lim_time, vsync_offset, i;
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
	Pico.m.dirtyPal = 1;
	oldmodes = ((Pico.video.reg[12]&1)<<2) ^ 0xc;
	find_combos();

	// pal/ntsc might have changed, reset related stuff
	target_fps = Pico.m.pal ? 50 : 60;
	target_frametime = 1000000/target_fps;
	reset_timing = 1;

	// prepare sound stuff
	if(currentConfig.EmuOpt & 4) {
		int snd_excess_add;
		if(PsndRate != PsndRate_old || (PicoOpt&0x20b) != (PicoOpt_old&0x20b) || Pico.m.pal != pal_old || crashed_940) {
			/* if 940 is turned off, we need it to be put back to sleep */
			if (!(PicoOpt&0x200) && ((PicoOpt^PicoOpt_old)&0x200)) {
				Reset940(1, 2);
				Pause940(1);
			}
			sound_rerate(1);
		}
		//excess_samples = PsndRate - PsndLen*target_fps;
		snd_excess_add = ((PsndRate - PsndLen*target_fps)<<16) / target_fps;
		printf("starting audio: %i len: %i (ex: %04x) stereo: %i, pal: %i\n", PsndRate, PsndLen, snd_excess_add, (PicoOpt&8)>>3, Pico.m.pal);
		gp2x_start_sound(PsndRate, 16, (PicoOpt&8)>>3);
		gp2x_sound_volume(currentConfig.volume, currentConfig.volume);
		PicoWriteSound = updateSound;
		memset(sndBuffer, 0, sizeof(sndBuffer));
		PsndOut = sndBuffer;
		PsndRate_old = PsndRate;
		PsndLen_real = PsndLen;
		PicoOpt_old  = PicoOpt;
		pal_old = Pico.m.pal;
	} else {
		PsndOut = 0;
	}

	// prepare CD buffer
	if (PicoMCD & 1) PicoCDBufferInit();

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

	// loop?
	while (engineState == PGS_Running)
	{
		int modes;

		gettimeofday(&tval, 0);
		if(reset_timing) {
			reset_timing = 0;
			thissec = tval.tv_sec;
			frames_shown = frames_done = tval.tv_usec/target_frametime;
		}

		// show notice message?
		if(noticeMsgTime.tv_sec) {
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
		if (modes != oldmodes) {
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
		if(thissec != tval.tv_sec) {
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
			thissec = tval.tv_sec;

			if(PsndOut == 0 && currentConfig.Frameskip >= 0) {
				frames_done = frames_shown = 0;
			} else {
				// it is quite common for this implementation to leave 1 fame unfinished
				// when second changes, but we don't want buffer to starve.
				if(PsndOut && frames_done < target_fps && frames_done > target_fps-5) {
					updateKeys();
					SkipFrame(1); frames_done++;
				}

				frames_done  -= target_fps; if (frames_done  < 0) frames_done  = 0;
				frames_shown -= target_fps; if (frames_shown < 0) frames_shown = 0;
				if (frames_shown > frames_done) frames_shown = frames_done;
			}
		}

		lim_time = (frames_done+1) * target_frametime + vsync_offset;
		if(currentConfig.Frameskip >= 0) { // frameskip enabled
			for(i = 0; i < currentConfig.Frameskip; i++) {
				updateKeys();
				SkipFrame(1); frames_done++;
				if (PsndOut) { // do framelimitting if sound is enabled
					gettimeofday(&tval, 0);
					if(thissec != tval.tv_sec) tval.tv_usec+=1000000;
					if(tval.tv_usec < lim_time) { // we are too fast
						simpleWait(thissec, lim_time);
					}
				}
				lim_time += target_frametime;
			}
		} else if(tval.tv_usec > lim_time) { // auto frameskip
			// no time left for this frame - skip
			if (tval.tv_usec - lim_time >= 0x300000) {
				/* something caused a slowdown for us (disk access? cache flush?)
				 * try to recover by resetting timing... */
				reset_timing = 1;
				continue;
			}
			updateKeys();
			SkipFrame(tval.tv_usec < lim_time+target_frametime*2); frames_done++;
			continue;
		}

		updateKeys();
		PicoFrame();

#if 0
if (Pico.m.frame_count == 31563) {
	FILE *f;
	f = fopen("ram_p.bin", "wb");
	if (!f) { printf("!f\n"); exit(1); }
	fwrite(Pico.ram, 1, 0x10000, f);
	fclose(f);
	exit(0);
}
#endif
#if 0
		// debug
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

		// check time
		gettimeofday(&tval, 0);
		if (thissec != tval.tv_sec) tval.tv_usec+=1000000;

		if (currentConfig.Frameskip < 0 && tval.tv_usec - lim_time >= 0x300000) // slowdown detection
			reset_timing = 1;
		else if (PsndOut != NULL || currentConfig.Frameskip < 0)
		{
			// sleep or vsync if we are still too fast
			// usleep sleeps for ~20ms minimum, so it is not a solution here
			if(tval.tv_usec < lim_time)
			{
				// we are too fast
				if (vsync_offset) {
					if (lim_time - tval.tv_usec > target_frametime/2)
						simpleWait(thissec, lim_time - target_frametime/4);
					gp2x_video_wait_vsync();
				} else {
					simpleWait(thissec, lim_time);
				}
			}
		}

		blit(fpsbuff, notice);

		frames_done++; frames_shown++;
	}


	if (PicoMCD & 1) PicoCDBufferFree();

	// save SRAM
	if((currentConfig.EmuOpt & 1) && SRam.changed) {
		emu_state_cb("Writing SRAM/BRAM..");
		emu_SaveLoadGame(0, 1);
		SRam.changed = 0;
	}

	// if in 16bit mode, generate 8it image for menu background
	if (!(PicoOpt&0x10) && (currentConfig.EmuOpt&0x80))
		emu_forced_frame();

	// for menu bg
	gp2x_memcpy_buffers((1<<2), gp2x_screen, 0, 320*240*2);
}


void emu_ResetGame(void)
{
	PicoReset(0);
	reset_timing = 1;
}


size_t gzRead2(void *p, size_t _size, size_t _n, void *file)
{
	return gzread(file, p, _n);
}


size_t gzWrite2(void *p, size_t _size, size_t _n, void *file)
{
	return gzwrite(file, p, _n);
}

static int try_ropen_file(const char *fname)
{
	FILE *f;

	f = fopen(fname, "rb");
	if (f) {
		fclose(f);
		return 1;
	}
	return 0;
}

char *emu_GetSaveFName(int load, int is_sram, int slot)
{
	static char saveFname[512];
	char ext[16];

	if (is_sram)
	{
		romfname_ext(saveFname, (PicoMCD&1) ? "brm/" : "srm/", (PicoMCD&1) ? ".brm" : ".srm");
		if (load) {
			if (try_ropen_file(saveFname)) return saveFname;
			// try in current dir..
			romfname_ext(saveFname, NULL, (PicoMCD&1) ? ".brm" : ".srm");
			if (try_ropen_file(saveFname)) return saveFname;
			return NULL; // give up
		}
	}
	else
	{
		ext[0] = 0;
		if(slot > 0 && slot < 10) sprintf(ext, ".%i", slot);
		strcat(ext, (currentConfig.EmuOpt & 8) ? ".mds.gz" : ".mds");

		romfname_ext(saveFname, "mds/", ext);
		if (load) {
			if (try_ropen_file(saveFname)) return saveFname;
			romfname_ext(saveFname, NULL, ext);
			if (try_ropen_file(saveFname)) return saveFname;
			if (currentConfig.EmuOpt & 8) {
				ext[0] = 0;
				if(slot > 0 && slot < 10) sprintf(ext, ".%i", slot);
				strcat(ext, ".mds");

				romfname_ext(saveFname, "mds/", ext);
				if (try_ropen_file(saveFname)) return saveFname;
				romfname_ext(saveFname, NULL, ext);
				if (try_ropen_file(saveFname)) return saveFname;
			}
			return NULL;
		}
	}

	return saveFname;
}

int emu_check_save_file(int slot)
{
	return emu_GetSaveFName(1, 0, slot) ? 1 : 0;
}

void emu_set_save_cbs(int gz)
{
	if (gz) {
		areaRead  = gzRead2;
		areaWrite = gzWrite2;
		areaEof   = (areaeof *) gzeof;
		areaSeek  = (areaseek *) gzseek;
		areaClose = (areaclose *) gzclose;
	} else {
		areaRead  = (arearw *) fread;
		areaWrite = (arearw *) fwrite;
		areaEof   = (areaeof *) feof;
		areaSeek  = (areaseek *) fseek;
		areaClose = (areaclose *) fclose;
	}
}

int emu_SaveLoadGame(int load, int sram)
{
	int ret = 0;
	char *saveFname;

	// make save filename
	saveFname = emu_GetSaveFName(load, sram, state_slot);
	if (saveFname == NULL) {
		if (!sram) {
			strcpy(noticeMsg, load ? "LOAD FAILED (missing file)" : "SAVE FAILED  ");
			gettimeofday(&noticeMsgTime, 0);
		}
		return -1;
	}

	printf("saveLoad (%i, %i): %s\n", load, sram, saveFname);

	if(sram) {
		FILE *sramFile;
		int sram_size;
		unsigned char *sram_data;
		int truncate = 1;
		if (PicoMCD&1) {
			if (PicoOpt&0x8000) { // MCD RAM cart?
				sram_size = 0x12000;
				sram_data = SRam.data;
				if (sram_data)
					memcpy32((int *)sram_data, (int *)Pico_mcd->bram, 0x2000/4);
			} else {
				sram_size = 0x2000;
				sram_data = Pico_mcd->bram;
				truncate  = 0; // the .brm may contain RAM cart data after normal brm
			}
		} else {
			sram_size = SRam.end-SRam.start+1;
			if(SRam.reg_back & 4) sram_size=0x2000;
			sram_data = SRam.data;
		}
		if (!sram_data) return 0; // SRam forcefully disabled for this game

		if (load) {
			sramFile = fopen(saveFname, "rb");
			if(!sramFile) return -1;
			fread(sram_data, 1, sram_size, sramFile);
			fclose(sramFile);
			if ((PicoMCD&1) && (PicoOpt&0x8000))
				memcpy32((int *)Pico_mcd->bram, (int *)sram_data, 0x2000/4);
		} else {
			// sram save needs some special processing
			// see if we have anything to save
			for (; sram_size > 0; sram_size--)
				if (sram_data[sram_size-1]) break;

			if (sram_size) {
				sramFile = fopen(saveFname, truncate ? "wb" : "r+b");
				if (!sramFile) sramFile = fopen(saveFname, "wb"); // retry
				if (!sramFile) return -1;
				ret = fwrite(sram_data, 1, sram_size, sramFile);
				ret = (ret != sram_size) ? -1 : 0;
				fclose(sramFile);
#ifndef NO_SYNC
				sync();
#endif
			}
		}
		return ret;
	}
	else
	{
		void *PmovFile = NULL;
		if (strcmp(saveFname + strlen(saveFname) - 3, ".gz") == 0) {
			if( (PmovFile = gzopen(saveFname, load ? "rb" : "wb")) ) {
				emu_set_save_cbs(1);
				if(!load) gzsetparams(PmovFile, 9, Z_DEFAULT_STRATEGY);
			}
		}
		else
		{
			if( (PmovFile = fopen(saveFname, load ? "rb" : "wb")) ) {
				emu_set_save_cbs(0);
			}
		}
		if(PmovFile) {
			ret = PmovState(load ? 6 : 5, PmovFile);
			areaClose(PmovFile);
			PmovFile = 0;
			if (load) Pico.m.dirtyPal=1;
#ifndef NO_SYNC
			else sync();
#endif
		}
		else	ret = -1;
		if (!ret)
			strcpy(noticeMsg, load ? "GAME LOADED  " : "GAME SAVED   ");
		else
		{
			strcpy(noticeMsg, load ? "LOAD FAILED  " : "SAVE FAILED  ");
			ret = -1;
		}

		gettimeofday(&noticeMsgTime, 0);
		return ret;
	}
}
