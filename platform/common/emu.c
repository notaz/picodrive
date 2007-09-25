// (c) Copyright 2006-2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> // tolower
#ifndef NO_SYNC
#include <unistd.h>
#endif

#include "emu.h"
#include "menu.h"
#include "fonts.h"
#include "lprintf.h"

#include <Pico/PicoInt.h>
#include <Pico/Patch.h>
#include <zlib/zlib.h>

#if   defined(__GP2X__)
 #include "../gp2x/gp2x.h"
 #define SCREEN_WIDTH 320
 #define SCREEN_BUFFER gp2x_screen
#elif defined(__GIZ__)
 #include "../gizmondo/giz.h"
 #define SCREEN_WIDTH 321
 #define SCREEN_BUFFER giz_screen // ?
#endif

char *PicoConfigFile = "picoconfig.bin";
currentConfig_t currentConfig;
unsigned char *rom_data = NULL;
char noticeMsg[64];
int state_slot = 0;
int config_slot = 0, config_slot_current = 0;

unsigned char *movie_data = NULL;
static int movie_size = 0;

// provided by platform code:
extern char romFileName[];
extern void emu_noticeMsgUpdated(void);
extern void emu_getMainDir(char *dst, int len);
extern void emu_setDefaultConfig(void);
extern void menu_romload_prepare(const char *rom_name);
extern void menu_romload_end(void);


// utilities
static void strlwr_(char* string)
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
	strlwr_(ext);
}

char *biosfiles_us[] = { "us_scd2_9306", "SegaCDBIOS9303", "us_scd1_9210" };
char *biosfiles_eu[] = { "eu_mcd2_9306", "eu_mcd2_9303",   "eu_mcd1_9210" };
char *biosfiles_jp[] = { "jp_mcd1_9112", "jp_mcd1_9111" };

int emu_findBios(int region, char **bios_file)
{
	static char bios_path[1024];
	int i, count;
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
		emu_getMainDir(bios_path, sizeof(bios_path));
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
		lprintf("using bios: %s\n", bios_path);
		fclose(f);
		if (bios_file) *bios_file = bios_path;
		return 1;
	} else {
		sprintf(menuErrorMsg, "no %s BIOS files found, read docs",
			region != 4 ? (region == 8 ? "EU" : "JAP") : "USA");
		lprintf("%s\n", menuErrorMsg);
		return 0;
	}
}

/* checks if romFileName points to valid MegaCD image
 * if so, checks for suitable BIOS */
int emu_cdCheck(int *pregion)
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

	lprintf("detected %s Sega/Mega CD image with %s region\n",
		type == 2 ? "BIN" : "ISO", region != 4 ? (region == 8 ? "EU" : "JAP") : "USA");

	if (pregion != NULL) *pregion = region;

	return type;
}

int emu_ReloadRom(void)
{
	unsigned int rom_size = 0;
	char *used_rom_name = romFileName;
	char ext[5];
	pm_file *rom;
	int ret, cd_state, cd_region, cfg_loaded = 0;

	lprintf("emu_ReloadRom(%s)\n", romFileName);

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
	cd_state = emu_cdCheck(&cd_region);
	if (cd_state > 0)
	{
		// valid CD image, check for BIOS..

		// we need to have config loaded at this point
		ret = emu_ReadConfig(1, 1);
		if (!ret) emu_ReadConfig(0, 1);
		cfg_loaded = 1;

		if (PicoRegionOverride) {
			cd_region = PicoRegionOverride;
			lprintf("overrided region to %s\n", cd_region != 4 ? (cd_region == 8 ? "EU" : "JAP") : "USA");
		}
		if (!emu_findBios(cd_region, &used_rom_name)) {
			// bios_help() ?
			return 0;
		}

		PicoMCD |= 1;
		get_ext(used_rom_name, ext);
	}
	else
	{
		if (PicoMCD & 1) Stop_CD();
		PicoMCD &= ~1;
	}

	rom = pm_open(used_rom_name);
	if(!rom) {
		sprintf(menuErrorMsg, "Failed to open rom.");
		return 0;
	}

	menu_romload_prepare(used_rom_name);

	if(rom_data) {
		free(rom_data);
		rom_data = 0;
		rom_size = 0;
	}

	if( (ret = PicoCartLoad(rom, &rom_data, &rom_size)) ) {
		sprintf(menuErrorMsg, "PicoCartLoad() failed.");
		lprintf("%s\n", menuErrorMsg);
		pm_close(rom);
		menu_romload_end();
		return 0;
	}
	pm_close(rom);
	menu_romload_end();

	// detect wrong files (Pico crashes on very small files), also see if ROM EP is good
	if(rom_size <= 0x200 || strncmp((char *)rom_data, "Pico", 4) == 0 ||
	  ((*(unsigned char *)(rom_data+4)<<16)|(*(unsigned short *)(rom_data+6))) >= (int)rom_size) {
		if (rom_data) free(rom_data);
		rom_data = 0;
		sprintf(menuErrorMsg, "Not a ROM selected.");
		return 0;
	}

	// load config for this ROM (do this before insert to get correct region)
	if (!cfg_loaded) {
		ret = emu_ReadConfig(1, 1);
		if (!ret) emu_ReadConfig(0, 1);
	}

	lprintf("PicoCartInsert(%p, %d);\n", rom_data, rom_size);
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
			lprintf("%s\n", menuErrorMsg);
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
	if (movie_data) {
		if(movie_data[0x14] == '6')
		     PicoOpt |=  0x20; // 6 button pad
		else PicoOpt &= ~0x20;
		PicoOpt |= 0x10040; // accurate timing, no VDP fifo timing
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
		PicoOpt &= ~0x10000;
		if(Pico.m.pal) {
			strcpy(noticeMsg, "PAL SYSTEM / 50 FPS");
		} else {
			strcpy(noticeMsg, "NTSC SYSTEM / 60 FPS");
		}
	}
	emu_noticeMsgUpdated();

	// load SRAM for this ROM
	if(currentConfig.EmuOpt & 1)
		emu_SaveLoadGame(1, 1);

	return 1;
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


int emu_ReadConfig(int game, int no_defaults)
{
	FILE *f;
	char cfg[512], extbuf[16];
	int bread = 0;

	if (!game)
	{
		if (!no_defaults)
		{
			emu_setDefaultConfig();
		}
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

	lprintf("emu_ReadConfig: %s ", cfg);
	f = fopen(cfg, "rb");
	if (f) {
		bread = fread(&currentConfig, 1, sizeof(currentConfig), f);
		fclose(f);
	}
	lprintf(bread > 0 ? "(ok)\n" : "(failed)\n");

	PicoOpt = currentConfig.PicoOpt;
	PsndRate = currentConfig.PsndRate;
	PicoRegionOverride = currentConfig.PicoRegion;
	PicoAutoRgnOrder = currentConfig.PicoAutoRgnOrder;
	PicoCDBuffers = currentConfig.PicoCDBuffers;
	//scaling_update();
	// some sanity checks
	if (currentConfig.CPUclock < 10 || currentConfig.CPUclock > 4096) currentConfig.CPUclock = 200;
	if (currentConfig.gamma < 10 || currentConfig.gamma > 300) currentConfig.gamma = 100;
	if (currentConfig.volume < 0 || currentConfig.volume > 99) currentConfig.volume = 50;
#ifdef __GP2X__
	// if volume keys are unbound, bind them to volume control
	if (!currentConfig.KeyBinds[23] && !currentConfig.KeyBinds[22]) {
		currentConfig.KeyBinds[23] = 1<<29; // vol up
		currentConfig.KeyBinds[22] = 1<<30; // vol down
	}
#endif
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

	lprintf("emu_WriteConfig: %s ", cfg);
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
	lprintf((bwrite == sizeof(currentConfig)) ? "(ok)\n" : "(failed)\n");

	if (bwrite == sizeof(currentConfig)) config_slot_current = config_slot;
	return (bwrite == sizeof(currentConfig));
}


void emu_textOut8(int x, int y, const char *text)
{
	int i,l,len=strlen(text);
	unsigned char *screen = (unsigned char *)SCREEN_BUFFER + x + y*SCREEN_WIDTH;

	/* always using built-in font */
	for (i = 0; i < len; i++)
	{
		for (l=0;l<8;l++)
		{
			unsigned char fd = fontdata8x8[((text[i])*8)+l];
			if (fd&0x80) screen[l*SCREEN_WIDTH+0]=0xf0;
			if (fd&0x40) screen[l*SCREEN_WIDTH+1]=0xf0;
			if (fd&0x20) screen[l*SCREEN_WIDTH+2]=0xf0;
			if (fd&0x10) screen[l*SCREEN_WIDTH+3]=0xf0;
			if (fd&0x08) screen[l*SCREEN_WIDTH+4]=0xf0;
			if (fd&0x04) screen[l*SCREEN_WIDTH+5]=0xf0;
			if (fd&0x02) screen[l*SCREEN_WIDTH+6]=0xf0;
			if (fd&0x01) screen[l*SCREEN_WIDTH+7]=0xf0;
		}
		screen += 8;
	}
}

void emu_textOut16(int x, int y, const char *text)
{
	int i,l,len=strlen(text);
	unsigned short *screen = (unsigned short *)SCREEN_BUFFER + x + y*SCREEN_WIDTH;

	for (i = 0; i < len; i++)
	{
		for (l=0;l<8;l++)
		{
			unsigned char fd = fontdata8x8[((text[i])*8)+l];
			if(fd&0x80) screen[l*SCREEN_WIDTH+0]=0xffff;
			if(fd&0x40) screen[l*SCREEN_WIDTH+1]=0xffff;
			if(fd&0x20) screen[l*SCREEN_WIDTH+2]=0xffff;
			if(fd&0x10) screen[l*SCREEN_WIDTH+3]=0xffff;
			if(fd&0x08) screen[l*SCREEN_WIDTH+4]=0xffff;
			if(fd&0x04) screen[l*SCREEN_WIDTH+5]=0xffff;
			if(fd&0x02) screen[l*SCREEN_WIDTH+6]=0xffff;
			if(fd&0x01) screen[l*SCREEN_WIDTH+7]=0xffff;
		}
		screen += 8;
	}
}


void emu_updateMovie(void)
{
	int offs = Pico.m.frame_count*3 + 0x40;
	if (offs+3 > movie_size) {
		free(movie_data);
		movie_data = 0;
		strcpy(noticeMsg, "END OF MOVIE.");
		lprintf("END OF MOVIE.\n");
		emu_noticeMsgUpdated();
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


static size_t gzRead2(void *p, size_t _size, size_t _n, void *file)
{
	return gzread(file, p, _n);
}


static size_t gzWrite2(void *p, size_t _size, size_t _n, void *file)
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

int emu_checkSaveFile(int slot)
{
	return emu_GetSaveFName(1, 0, slot) ? 1 : 0;
}

void emu_setSaveStateCbs(int gz)
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
			emu_noticeMsgUpdated();
		}
		return -1;
	}

	lprintf("saveLoad (%i, %i): %s\n", load, sram, saveFname);

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
			if(Pico.m.sram_reg & 4) sram_size=0x2000;
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
				emu_setSaveStateCbs(1);
				if(!load) gzsetparams(PmovFile, 9, Z_DEFAULT_STRATEGY);
			}
		}
		else
		{
			if( (PmovFile = fopen(saveFname, load ? "rb" : "wb")) ) {
				emu_setSaveStateCbs(0);
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

		emu_noticeMsgUpdated();
		return ret;
	}
}
