// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
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

#include "Pico/PicoInt.h"
#include "zlib/zlib.h"


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

static char noticeMsg[64];					// notice msg to draw
static struct timeval noticeMsgTime = { 0, 0 };	// when started showing
static int reset_timing, osd_fps_x;
static int combo_keys = 0, combo_acts = 0;	// keys and actions which need button combos
static int gp2x_old_gamma = 100;
static unsigned char *movie_data = NULL;
static int movie_size = 0;
unsigned char *framebuff = 0;  // temporary buffer for alt renderer
int state_slot = 0;

/*
// tmp
static FILE *logf = NULL;

void pprintf(char *texto, ...)
{
	va_list args;

	va_start(args,texto);
	vfprintf(logf,texto,args);
	va_end(args);
	fflush(logf);
	sync();
}
*/
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
static int cd_check(char *ext, char **bios_file)
{
	unsigned char buf[32];
	FILE *cd_f;
	int type = 0, region = 4; // 1: Japan, 4: US, 8: Europe

	cd_f = fopen(romFileName, "rb");
	if (!cd_f) return 0; // let the upper level handle this

	if (fread(buf, 1, 32, cd_f) != 32) {
		fclose(cd_f);
		return 0;
	}

	if (!strncasecmp("SEGADISCSYSTEM", (char *)buf+0x00, 14)) type = 1;       // Sega CD (ISO)
	if (!strncasecmp("SEGADISCSYSTEM", (char *)buf+0x10, 14)) type = 2;       // Sega CD (BIN)
	if (type == 0) {
		fclose(cd_f);
		return 0;
	}

	/* it seems we have a CD image here. Try to detect region and load a suitable BIOS now.. */
	fseek(cd_f, (type == 1) ? 0x100+0x10B : 0x110+0x10B, SEEK_SET);
	fread(buf, 1, 1, cd_f);
	fclose(cd_f);

	if (buf[0] == 0x64) region = 8; // EU
	if (buf[0] == 0xa1) region = 1; // JAP

	printf("detected %s Sega/Mega CD image with %s region\n",
		type == 2 ? "BIN" : "ISO", region != 4 ? (region == 8 ? "EU" : "JAP") : "USA");

	if (PicoRegionOverride) {
		region = PicoRegionOverride;
		printf("overrided region to %s\n", region != 4 ? (region == 8 ? "EU" : "JAP") : "USA");
	}

	if(find_bios(region, bios_file))
		 return type;	// CD and BIOS detected

	return -1;     		// CD detected but load failed
}

int emu_ReloadRom(void)
{
	unsigned int rom_size = 0;
	char *used_rom_name = romFileName;
	char ext[5];
	FILE *rom;
	int ret, cd_state;

	printf("emu_ReloadRom(%s)\n", romFileName);

	get_ext(romFileName, ext);

	// detect wrong extensions
	if(!strcmp(ext, ".srm") || !strcmp(ext, "s.gz") || !strcmp(ext, ".mds")) { // s.gz ~ .mds.gz
		sprintf(menuErrorMsg, "Not a ROM selected.");
		return 0;
	}

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

	// check for MegaCD image
	cd_state = cd_check(ext, &used_rom_name);
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

	rom = fopen(used_rom_name, "rb");
	if(!rom) {
		sprintf(menuErrorMsg, "Failed to open rom.");
		return 0;
	}

	if(rom_data) {
		free(rom_data);
		rom_data = 0;
		rom_size = 0;
	}

	// zipfile support
	if(!strcasecmp(ext, ".zip")) {
		fclose(rom);
		ret = CartLoadZip(used_rom_name, &rom_data, &rom_size);
		if(ret) {
			if (ret == 4) strcpy(menuErrorMsg, "No ROMs found in zip.");
			else sprintf(menuErrorMsg, "Unzip failed with code %i", ret);
			printf("%s\n", menuErrorMsg);
			return 0;
		}
	} else {
		if( (ret = PicoCartLoad(rom, &rom_data, &rom_size)) ) {
			sprintf(menuErrorMsg, "PicoCartLoad() failed.");
			printf("%s\n", menuErrorMsg);
			fclose(rom);
			return 0;
		}
		fclose(rom);
	}

	// detect wrong files (Pico crashes on very small files), also see if ROM EP is good
	if(rom_size <= 0x200 || strncmp((char *)rom_data, "Pico", 4) == 0 ||
	  ((*(unsigned short *)(rom_data+4)<<16)|(*(unsigned short *)(rom_data+6))) >= (int)rom_size) {
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


void emu_Init(void)
{
	// make temp buffer for alt renderer
	framebuff = malloc((8+320)*(8+240+8));
	if (!framebuff)
	{
		printf("framebuff == 0\n");
	}

	PicoInit();

//	logf = fopen("log.txt", "w");
}


static void romfname_ext(char *dst, char *ext)
{
	char *p;

	// make save filename
	for (p = romFileName+strlen(romFileName)-1; p >= romFileName && *p != '/'; p--); p++;
	strncpy(dst, p, 511);
	dst[511-8] = 0;
	if(dst[strlen(dst)-4] == '.') dst[strlen(dst)-4] = 0;
	strcat(dst, ext);
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


int emu_ReadConfig(int game)
{
	FILE *f;
	char cfg[512];
	int bread = 0;

	if (!game)
	{
		// set default config
		memset(&currentConfig, 0, sizeof(currentConfig));
		currentConfig.lastRomFile[0] = 0;
		currentConfig.EmuOpt  = 0x1f | 0x400; // | cd_leds
		currentConfig.PicoOpt = 0x0f | 0xe00; // | use_940 | cd_pcm | cd_cdda
		currentConfig.PsndRate = 44100;
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
		strncpy(cfg, PicoConfigFile, 511);
		cfg[511] = 0;
	} else {
		romfname_ext(cfg, ".pbcfg");
	}

	printf("emu_ReadConfig: %s ", cfg);
	f = fopen(cfg, "rb");
	if (f) {
		bread = fread(&currentConfig, 1, sizeof(currentConfig), f);
		fclose(f);
	}
	printf((bread == sizeof(currentConfig)) ? "(ok)\n" : "(failed)\n");

	PicoOpt = currentConfig.PicoOpt;
	PsndRate = currentConfig.PsndRate;
	PicoRegionOverride = currentConfig.PicoRegion;
	PicoAutoRgnOrder = currentConfig.PicoAutoRgnOrder;
	if (PicoOpt & 0x20) {
		actionNames[ 8] = "Z"; actionNames[ 9] = "Y";
		actionNames[10] = "X"; actionNames[11] = "MODE";
	}
	// some sanity checks
	if (currentConfig.CPUclock < 1 || currentConfig.CPUclock > 4096) currentConfig.CPUclock = 200;
	if (currentConfig.gamma < 10 || currentConfig.gamma > 300) currentConfig.gamma = 100;
	// if volume keys are unbound, bind them to volume control
	if (!currentConfig.KeyBinds[23] && !currentConfig.KeyBinds[22]) {
		currentConfig.KeyBinds[23] = 1<<29; // vol up
		currentConfig.KeyBinds[22] = 1<<30; // vol down
	}

	return (bread == sizeof(currentConfig));
}


int emu_WriteConfig(int game)
{
	FILE *f;
	char cfg[512];
	int bwrite = 0;

	if (!game)
	{
		strncpy(cfg, PicoConfigFile, 511);
		cfg[511] = 0;
	} else {
		romfname_ext(cfg, ".pbcfg");
	}

	printf("emu_WriteConfig: %s ", cfg);
	f = fopen(cfg, "wb");
	if (f) {
		currentConfig.PicoOpt = PicoOpt;
		currentConfig.PsndRate = PsndRate;
		currentConfig.PicoRegion = PicoRegionOverride;
		currentConfig.PicoAutoRgnOrder = PicoAutoRgnOrder;
		bwrite = fwrite(&currentConfig, 1, sizeof(currentConfig), f);
		fflush(f);
		fclose(f);
		sync();
	}
	printf((bwrite == sizeof(currentConfig)) ? "(ok)\n" : "(failed)\n");

	return (bwrite == sizeof(currentConfig));
}


void emu_Deinit(void)
{
	// save SRAM
	if((currentConfig.EmuOpt & 1) && SRam.changed) {
		emu_SaveLoadGame(0, 1);
		SRam.changed = 0;
	}

	if (!(currentConfig.EmuOpt & 0x20))
		emu_WriteConfig(0);
	free(framebuff);

	PicoExit();
//	fclose(logf);

	// restore gamma
	if (gp2x_old_gamma != 100)
		set_gamma(100);
}


void osd_text(int x, int y, char *text)
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

static int localPal[0x100];
static void (*vidCpyM2)(void *dest, void *src) = NULL;

static void blit(char *fps, char *notice)
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

	if (notice) osd_text(4, 232, notice);
	if (emu_opt & 2)
		osd_text(osd_fps_x, 232, fps);
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
	gp2x_video_RGB_setscaling((PicoOpt&0x100)&&!(Pico.video.reg[12]&1) ? 256 : 320, 240);
}


static int check_save_file(void)
{
	char saveFname[512];
	char ext[16];
	FILE *f;

	ext[0] = 0;
	if(state_slot > 0 && state_slot < 10) sprintf(ext, ".%i", state_slot);
	strcat(ext, ".mds");
	if(currentConfig.EmuOpt & 8) strcat(ext, ".gz");

	romfname_ext(saveFname, ext);
	if ((f = fopen(saveFname, "rb"))) {
		fclose(f);
		return 1;
	}
	return 0;
}

static void RunEvents(unsigned int which)
{
	if(which & 0x1800) { // save or load (but not both)
		int do_it = 1;
		if (!(which & 0x1000) && (currentConfig.EmuOpt & 0x200) && check_save_file()) {
			unsigned long keys;
			blit("", "OVERWRITE SAVE? (Y=yes, X=no)");
			while( !((keys = gp2x_joystick_read(1)) & (GP2X_X|GP2X_Y)) )
				usleep(50*1024);
			if (keys & GP2X_X) do_it = 0;
			clearArea(0);
		}
		if (do_it) {
			blit("", (which & 0x1000) ? "LOADING GAME" : "SAVING GAME");
			emu_SaveLoadGame(which & 0x1000, 0);
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
		sprintf(noticeMsg, "SAVE SLOT %i [%s]", state_slot, check_save_file() ? "USED" : "FREE");
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
			if (vol < 90) vol++;
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

static int snd_excess_add = 0, snd_excess_cnt = 0; // hack

static void updateSound(void)
{
	int len = (PicoOpt&8)?PsndLen*2:PsndLen;

	snd_excess_cnt += snd_excess_add;
	if (snd_excess_cnt >= 0x10000) {
		snd_excess_cnt -= 0x10000;
		if (PicoOpt&8) {
			PsndOut[len]   = PsndOut[len-2];
			PsndOut[len+1] = PsndOut[len-1];
			len+=2;
		} else {
			PsndOut[len]   = PsndOut[len-1];
			len++;
		}
	}

	gp2x_sound_write(PsndOut, len<<1);
}


static void SkipFrame(int do_sound)
{
	void *sndbuff_tmp = 0;
	if (PsndOut && !do_sound) {
		sndbuff_tmp = PsndOut;
		PsndOut = 0;
	}

	PicoSkipFrame=1;
	PicoFrame();
	PicoSkipFrame=0;

	if (sndbuff_tmp && !do_sound) {
		PsndOut = sndbuff_tmp;
	}
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
	static int PsndRate_old = 0, PicoOpt_old = 0, PsndLen_real = 0, pal_old = 0;
	char fpsbuff[24]; // fps count c string
	struct timeval tval; // timing
	int thissec = 0, frames_done = 0, frames_shown = 0, oldmodes = 0;
	int target_fps, target_frametime, lim_time, i;
	char *notice = 0;

	printf("entered emu_Loop()\n");

	if (gp2x_old_clock != currentConfig.CPUclock) {
		printf("changing clock to %i...", currentConfig.CPUclock); fflush(stdout);
		set_FCLK(currentConfig.CPUclock);
		gp2x_old_clock = currentConfig.CPUclock;
		printf(" done\n");
	}

	if (gp2x_old_gamma != currentConfig.gamma) {
		set_gamma(currentConfig.gamma);
		gp2x_old_gamma = currentConfig.gamma;
		printf("updated gamma to %i\n", currentConfig.gamma);
	}

	fpsbuff[0] = 0;

	// make sure we are in correct mode
	vidResetMode();
	oldmodes = ((Pico.video.reg[12]&1)<<2) ^ 0xc;
	find_combos();

	// pal/ntsc might have changed, reset related stuff
	target_fps = Pico.m.pal ? 50 : 60;
	target_frametime = 1000000/target_fps;
	reset_timing = 1;

	// prepare sound stuff
	if(currentConfig.EmuOpt & 4) {
		if(PsndRate != PsndRate_old || (PicoOpt&0x20b) != (PicoOpt_old&0x20b) || Pico.m.pal != pal_old || crashed_940) {
			/* if 940 is turned off, we need it to be put back to sleep */
			if (!(PicoOpt&0x200) && ((PicoOpt^PicoOpt_old)&0x200)) {
				Reset940(1, 2);
				Pause940(1);
			}
			sound_rerate();
		}
		//excess_samples = PsndRate - PsndLen*target_fps;
		snd_excess_cnt = 0;
		snd_excess_add = ((PsndRate - PsndLen*target_fps)<<16) / target_fps;
		printf("starting audio: %i len: %i (ex: %04x) stereo: %i, pal: %i\n", PsndRate, PsndLen, snd_excess_add, (PicoOpt&8)>>3, Pico.m.pal);
		gp2x_start_sound(PsndRate, 16, (PicoOpt&8)>>3);
		gp2x_sound_volume(currentConfig.volume, currentConfig.volume);
		PicoWriteSound = updateSound;
		PsndOut = calloc((PicoOpt&8) ? (PsndLen*4+4) : (PsndLen*2+2), 1);
		PsndRate_old = PsndRate;
		PsndLen_real = PsndLen;
		PicoOpt_old  = PicoOpt;
		pal_old = Pico.m.pal;
	} else {
		PsndOut = 0;
	}

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
			gp2x_video_RGB_setscaling(scalex, 240);
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

		lim_time = (frames_done+1) * target_frametime;
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
			updateKeys();
			SkipFrame(tval.tv_usec < lim_time+target_frametime); frames_done++;
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
		if(thissec != tval.tv_sec) tval.tv_usec+=1000000;

		// sleep if we are still too fast
		if(PsndOut != 0 || currentConfig.Frameskip < 0)
		{
			// usleep sleeps for ~20ms minimum, so it is not a solution here
			gettimeofday(&tval, 0);
			if(thissec != tval.tv_sec) tval.tv_usec+=1000000;
			if(tval.tv_usec < lim_time)
			{
				// we are too fast
				simpleWait(thissec, lim_time);
			}
		}

		blit(fpsbuff, notice);

		frames_done++; frames_shown++;
	}

	// save SRAM
	if((currentConfig.EmuOpt & 1) && SRam.changed) {
		blit("", "Writing SRAM/BRAM..");
		emu_SaveLoadGame(0, 1);
		SRam.changed = 0;
	}

	if (PsndOut != 0) {
		free(PsndOut);
		PsndOut = 0;
	}
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


int emu_SaveLoadGame(int load, int sram)
{
	int ret = 0;
	char saveFname[512];

	// make save filename
	romfname_ext(saveFname, "");
	if(sram) strcat(saveFname, (PicoMCD&1) ? ".brm" : ".srm");
	else {
		if(state_slot > 0 && state_slot < 10) sprintf(saveFname, "%s.%i", saveFname, state_slot);
		strcat(saveFname, ".mds");
	}

	printf("saveLoad (%i, %i): %s\n", load, sram, saveFname);

	if(sram) {
		FILE *sramFile;
		int sram_size;
		unsigned char *sram_data;
		if (PicoMCD&1) {
			sram_size = 0x2000;
			sram_data = Pico_mcd->bram;
		} else {
			sram_size = SRam.end-SRam.start+1;
			if(SRam.reg_back & 4) sram_size=0x2000;
			sram_data = SRam.data;
		}
		if(!sram_data) return 0; // SRam forcefully disabled for this game

		if(load) {
			sramFile = fopen(saveFname, "rb");
			if(!sramFile) return -1;
			fread(sram_data, 1, sram_size, sramFile);
			fclose(sramFile);
		} else {
			// sram save needs some special processing
			// see if we have anything to save
			for(; sram_size > 0; sram_size--)
				if(sram_data[sram_size-1]) break;

			if(sram_size) {
				sramFile = fopen(saveFname, "wb");
				ret = fwrite(sram_data, 1, sram_size, sramFile);
				ret = (ret != sram_size) ? -1 : 0;
				fclose(sramFile);
				sync();
			}
		}
		return ret;
	}
	else
	{
		void *PmovFile = NULL;
		// try gzip first
		if(currentConfig.EmuOpt & 8) {
			strcat(saveFname, ".gz");
			if( (PmovFile = gzopen(saveFname, load ? "rb" : "wb")) ) {
				areaRead  = gzRead2;
				areaWrite = gzWrite2;
				areaEof   = (areaeof *) gzeof;
				areaSeek  = (areaseek *) gzseek;
				if(!load) gzsetparams(PmovFile, 9, Z_DEFAULT_STRATEGY);
			} else
				saveFname[strlen(saveFname)-3] = 0;
		}
		if(!PmovFile) { // gzip failed or was disabled
			if( (PmovFile = fopen(saveFname, load ? "rb" : "wb")) ) {
				areaRead  = (arearw *) fread;
				areaWrite = (arearw *) fwrite;
				areaEof   = (areaeof *) feof;
				areaSeek  = (areaseek *) fseek;
			}
		}
		if(PmovFile) {
			ret = PmovState(load ? 6 : 5, PmovFile);
			if(areaRead == gzRead2)
				 gzclose(PmovFile);
			else fclose ((FILE *) PmovFile);
			PmovFile = 0;
			if (!load) sync();
			else Pico.m.dirtyPal=1;
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
