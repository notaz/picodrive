// (c) Copyright 2006-2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <stdlib.h>
#ifndef NO_SYNC
#include <unistd.h>
#endif

#include "emu.h"
#include "menu.h"
#include "fonts.h"
#include "lprintf.h"
#include "config.h"
#include "plat.h"
#include "input.h"
#include "posix.h"

#include <pico/pico_int.h>
#include <pico/patch.h>
#include <pico/cd/cue.h>
#include <zlib/zlib.h>


void *g_screen_ptr;

#if !SCREEN_SIZE_FIXED
int g_screen_width  = SCREEN_WIDTH;
int g_screen_height = SCREEN_HEIGHT;
#endif

char *PicoConfigFile = "config.cfg";
currentConfig_t currentConfig, defaultConfig;
int state_slot = 0;
int config_slot = 0, config_slot_current = 0;
int pico_pen_x = 320/2, pico_pen_y = 240/2;
int pico_inp_mode = 0;
int engineState = PGS_Menu;

/* TODO: len checking */
char rom_fname_reload[512] = { 0, };
char rom_fname_loaded[512] = { 0, };
int rom_loaded = 0;
int reset_timing = 0;

unsigned char *movie_data = NULL;
static int movie_size = 0;


/* don't use tolower() for easy old glibc binary compatibility */
static void strlwr_(char *string)
{
	char *p;
	for (p = string; *p; p++)
		if ('A' <= *p && *p <= 'Z')
			*p += 'a' - 'A';
}

static int try_rfn_cut(char *fname)
{
	FILE *tmp;
	char *p;

	p = fname + strlen(fname) - 1;
	for (; p > fname; p--)
		if (*p == '.') break;
	*p = 0;

	if((tmp = fopen(fname, "rb"))) {
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

static const char *biosfiles_us[] = { "us_scd1_9210", "us_scd2_9306", "SegaCDBIOS9303" };
static const char *biosfiles_eu[] = { "eu_mcd1_9210", "eu_mcd2_9306", "eu_mcd2_9303"   };
static const char *biosfiles_jp[] = { "jp_mcd1_9112", "jp_mcd1_9111" };

static int find_bios(int region, char **bios_file)
{
	static char bios_path[1024];
	int i, count;
	const char **files;
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
		emu_make_path(bios_path, files[i], sizeof(bios_path) - 4);
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
		sprintf(bios_path, "no %s BIOS files found, read docs",
			region != 4 ? (region == 8 ? "EU" : "JAP") : "USA");
		me_update_msg(bios_path);
		return 0;
	}
}

/* check if the name begins with BIOS name */
/*
static int emu_isBios(const char *name)
{
	int i;
	for (i = 0; i < sizeof(biosfiles_us)/sizeof(biosfiles_us[0]); i++)
		if (strstr(name, biosfiles_us[i]) != NULL) return 1;
	for (i = 0; i < sizeof(biosfiles_eu)/sizeof(biosfiles_eu[0]); i++)
		if (strstr(name, biosfiles_eu[i]) != NULL) return 1;
	for (i = 0; i < sizeof(biosfiles_jp)/sizeof(biosfiles_jp[0]); i++)
		if (strstr(name, biosfiles_jp[i]) != NULL) return 1;
	return 0;
}
*/

static unsigned char id_header[0x100];

/* checks if fname points to valid MegaCD image
 * if so, checks for suitable BIOS */
int emu_cd_check(int *pregion, char *fname_in)
{
	unsigned char buf[32];
	pm_file *cd_f;
	int region = 4; // 1: Japan, 4: US, 8: Europe
	char ext[5], *fname = fname_in;
	cue_track_type type = CT_UNKNOWN;
	cue_data_t *cue_data = NULL;

	get_ext(fname_in, ext);
	if (strcasecmp(ext, ".cue") == 0) {
		cue_data = cue_parse(fname_in);
		if (cue_data != NULL) {
			fname = cue_data->tracks[1].fname;
			type  = cue_data->tracks[1].type;
		}
		else
			return -1;
	}

	cd_f = pm_open(fname);
	if (cue_data != NULL)
		cue_destroy(cue_data);

	if (cd_f == NULL) return 0; // let the upper level handle this

	if (pm_read(buf, 32, cd_f) != 32) {
		pm_close(cd_f);
		return -1;
	}

	if (!strncasecmp("SEGADISCSYSTEM", (char *)buf+0x00, 14)) {
		if (type && type != CT_ISO)
			elprintf(EL_STATUS, ".cue has wrong type: %i", type);
		type = CT_ISO;       // Sega CD (ISO)
	}
	if (!strncasecmp("SEGADISCSYSTEM", (char *)buf+0x10, 14)) {
		if (type && type != CT_BIN)
			elprintf(EL_STATUS, ".cue has wrong type: %i", type);
		type = CT_BIN;       // Sega CD (BIN)
	}

	if (type == CT_UNKNOWN) {
		pm_close(cd_f);
		return 0;
	}

	pm_seek(cd_f, (type == CT_ISO) ? 0x100 : 0x110, SEEK_SET);
	pm_read(id_header, sizeof(id_header), cd_f);

	/* it seems we have a CD image here. Try to detect region now.. */
	pm_seek(cd_f, (type == CT_ISO) ? 0x100+0x10B : 0x110+0x10B, SEEK_SET);
	pm_read(buf, 1, cd_f);
	pm_close(cd_f);

	if (buf[0] == 0x64) region = 8; // EU
	if (buf[0] == 0xa1) region = 1; // JAP

	lprintf("detected %s Sega/Mega CD image with %s region\n",
		type == CT_BIN ? "BIN" : "ISO", region != 4 ? (region == 8 ? "EU" : "JAP") : "USA");

	if (pregion != NULL) *pregion = region;

	return type;
}

static int extract_text(char *dest, const unsigned char *src, int len, int swab)
{
	char *p = dest;
	int i;

	if (swab) swab = 1;

	for (i = len - 1; i >= 0; i--)
	{
		if (src[i^swab] != ' ') break;
	}
	len = i + 1;

	for (i = 0; i < len; i++)
	{
		unsigned char s = src[i^swab];
		if (s >= 0x20 && s < 0x7f && s != '#' && s != '|' &&
			s != '[' && s != ']' && s != '\\')
		{
			*p++ = s;
		}
		else
		{
			sprintf(p, "\\%02x", s);
			p += 3;
		}
	}

	return p - dest;
}

static char *emu_make_rom_id(void)
{
	static char id_string[3+0xe*3+0x3*3+0x30*3+3];
	int pos, swab = 1;

	if (PicoAHW & PAHW_MCD) {
		strcpy(id_string, "CD|");
		swab = 0;
	}
	else strcpy(id_string, "MD|");
	pos = 3;

	pos += extract_text(id_string + pos, id_header + 0x80, 0x0e, swab); // serial
	id_string[pos] = '|'; pos++;
	pos += extract_text(id_string + pos, id_header + 0xf0, 0x03, swab); // region
	id_string[pos] = '|'; pos++;
	pos += extract_text(id_string + pos, id_header + 0x50, 0x30, swab); // overseas name
	id_string[pos] = 0;

	return id_string;
}

// buffer must be at least 150 byte long
void emu_get_game_name(char *str150)
{
	int ret, swab = (PicoAHW & PAHW_MCD) ? 0 : 1;
	char *s, *d;

	ret = extract_text(str150, id_header + 0x50, 0x30, swab); // overseas name

	for (s = d = str150 + 1; s < str150+ret; s++)
	{
		if (*s == 0) break;
		if (*s != ' ' || d[-1] != ' ')
			*d++ = *s;
	}
	*d = 0;
}

static void shutdown_MCD(void)
{
	if ((PicoAHW & PAHW_MCD) && Pico_mcd != NULL)
		Stop_CD();
	PicoAHW &= ~PAHW_MCD;
}

// note: this function might mangle rom_fname
int emu_reload_rom(char *rom_fname)
{
	unsigned int rom_size = 0;
	char *used_rom_name = rom_fname;
	unsigned char *rom_data = NULL;
	char ext[5];
	pm_file *rom = NULL;
	int ret, cd_state, cd_region, cfg_loaded = 0;

	lprintf("emu_ReloadRom(%s)\n", rom_fname);

	get_ext(rom_fname, ext);

	// detect wrong extensions
	if (!strcmp(ext, ".srm") || !strcmp(ext, "s.gz") || !strcmp(ext, ".mds")) { // s.gz ~ .mds.gz
		me_update_msg("Not a ROM/CD selected.");
		return 0;
	}

	PicoPatchUnload();

	// check for movie file
	if (movie_data) {
		free(movie_data);
		movie_data = 0;
	}
	if (!strcmp(ext, ".gmv"))
	{
		// check for both gmv and rom
		int dummy;
		FILE *movie_file = fopen(rom_fname, "rb");
		if(!movie_file) {
			me_update_msg("Failed to open movie.");
			return 0;
		}
		fseek(movie_file, 0, SEEK_END);
		movie_size = ftell(movie_file);
		fseek(movie_file, 0, SEEK_SET);
		if(movie_size < 64+3) {
			me_update_msg("Invalid GMV file.");
			fclose(movie_file);
			return 0;
		}
		movie_data = malloc(movie_size);
		if(movie_data == NULL) {
			me_update_msg("low memory.");
			fclose(movie_file);
			return 0;
		}
		fread(movie_data, 1, movie_size, movie_file);
		fclose(movie_file);
		if (strncmp((char *)movie_data, "Gens Movie TEST", 15) != 0) {
			me_update_msg("Invalid GMV file.");
			return 0;
		}
		dummy = try_rfn_cut(rom_fname) || try_rfn_cut(rom_fname);
		if (!dummy) {
			me_update_msg("Could't find a ROM for movie.");
			return 0;
		}
		get_ext(rom_fname, ext);
		lprintf("gmv loaded for %s\n", rom_fname);
	}
	else if (!strcmp(ext, ".pat"))
	{
		int dummy;
		PicoPatchLoad(rom_fname);
		dummy = try_rfn_cut(rom_fname) || try_rfn_cut(rom_fname);
		if (!dummy) {
			me_update_msg("Could't find a ROM to patch.");
			return 0;
		}
		get_ext(rom_fname, ext);
	}

	shutdown_MCD();

	// check for MegaCD image
	cd_state = emu_cd_check(&cd_region, rom_fname);
	if (cd_state >= 0 && cd_state != CIT_NOT_CD)
	{
		PicoAHW |= PAHW_MCD;
		// valid CD image, check for BIOS..

		// we need to have config loaded at this point
		ret = emu_read_config(1, 1);
		if (!ret) emu_read_config(0, 1);
		cfg_loaded = 1;

		if (PicoRegionOverride) {
			cd_region = PicoRegionOverride;
			lprintf("overrided region to %s\n", cd_region != 4 ? (cd_region == 8 ? "EU" : "JAP") : "USA");
		}
		if (!find_bios(cd_region, &used_rom_name)) {
			PicoAHW &= ~PAHW_MCD;
			return 0;
		}

		get_ext(used_rom_name, ext);
	}
	else
	{
		if (PicoAHW & PAHW_MCD) Stop_CD();
		PicoAHW &= ~PAHW_MCD;
	}

	rom = pm_open(used_rom_name);
	if (!rom) {
		me_update_msg("Failed to open ROM/CD image");
		goto fail;
	}

	if (cd_state < 0) {
		me_update_msg("Invalid CD image");
		goto fail;
	}

	menu_romload_prepare(used_rom_name); // also CD load

	PicoCartUnload();
	rom_loaded = 0;

	if ( (ret = PicoCartLoad(rom, &rom_data, &rom_size)) ) {
		if      (ret == 2) me_update_msg("Out of memory");
		else if (ret == 3) me_update_msg("Read failed");
		else               me_update_msg("PicoCartLoad() failed.");
		goto fail2;
	}
	pm_close(rom);
	rom = NULL;

	// detect wrong files (Pico crashes on very small files), also see if ROM EP is good
	if (rom_size <= 0x200 || strncmp((char *)rom_data, "Pico", 4) == 0 ||
	  ((*(unsigned char *)(rom_data+4)<<16)|(*(unsigned short *)(rom_data+6))) >= (int)rom_size) {
		if (rom_data) free(rom_data);
		me_update_msg("Not a ROM selected.");
		goto fail2;
	}

	// load config for this ROM (do this before insert to get correct region)
	if (!(PicoAHW & PAHW_MCD))
		memcpy(id_header, rom_data + 0x100, sizeof(id_header));
	if (!cfg_loaded) {
		ret = emu_read_config(1, 1);
		if (!ret) emu_read_config(0, 1);
	}

	lprintf("PicoCartInsert(%p, %d);\n", rom_data, rom_size);
	if (PicoCartInsert(rom_data, rom_size)) {
		me_update_msg("Failed to load ROM.");
		goto fail2;
	}

	// insert CD if it was detected
	if (cd_state != CIT_NOT_CD) {
		ret = Insert_CD(rom_fname, cd_state);
		if (ret != 0) {
			me_update_msg("Insert_CD() failed, invalid CD image?");
			goto fail2;
		}
	}

	menu_romload_end();

	if (PicoPatches) {
		PicoPatchPrepare();
		PicoPatchApply();
	}

	// additional movie stuff
	if (movie_data)
	{
		if (movie_data[0x14] == '6')
		     PicoOpt |=  POPT_6BTN_PAD; // 6 button pad
		else PicoOpt &= ~POPT_6BTN_PAD;
		PicoOpt |= POPT_DIS_VDP_FIFO; // no VDP fifo timing
		if (movie_data[0xF] >= 'A') {
			if (movie_data[0x16] & 0x80) {
				PicoRegionOverride = 8;
			} else {
				PicoRegionOverride = 4;
			}
			PicoReset();
			// TODO: bits 6 & 5
		}
		movie_data[0x18+30] = 0;
		plat_status_msg("MOVIE: %s", (char *) &movie_data[0x18]);
	}
	else
	{
		PicoOpt &= ~POPT_DIS_VDP_FIFO;
		plat_status_msg(Pico.m.pal ? "PAL SYSTEM / 50 FPS" : "NTSC SYSTEM / 60 FPS");
	}

	// load SRAM for this ROM
	if (currentConfig.EmuOpt & EOPT_EN_SRAM)
		emu_save_load_game(1, 1);

	strncpy(rom_fname_loaded, rom_fname, sizeof(rom_fname_loaded)-1);
	rom_fname_loaded[sizeof(rom_fname_loaded)-1] = 0;
	rom_loaded = 1;
	return 1;

fail2:
	menu_romload_end();
fail:
	if (rom != NULL) pm_close(rom);
	return 0;
}

static void romfname_ext(char *dst, const char *prefix, const char *ext)
{
	char *p;
	int prefix_len = 0;

	// make save filename
	p = rom_fname_loaded + strlen(rom_fname_loaded) - 1;
	for (; p >= rom_fname_loaded && *p != PATH_SEP_C; p--); p++;
	*dst = 0;
	if (prefix) {
		int len = plat_get_root_dir(dst, 512);
		strcpy(dst + len, prefix);
		prefix_len = len + strlen(prefix);
	}
#ifdef UIQ3
	else p = rom_fname_loaded; // backward compatibility
#endif
	strncpy(dst + prefix_len, p, 511-prefix_len);
	dst[511-8] = 0;
	if (dst[strlen(dst)-4] == '.') dst[strlen(dst)-4] = 0;
	if (ext) strcat(dst, ext);
}

void emu_make_path(char *buff, const char *end, int size)
{
	int pos, end_len;

	end_len = strlen(end);
	pos = plat_get_root_dir(buff, size);
	strncpy(buff + pos, end, size - pos);
	buff[size - 1] = 0;
	if (pos + end_len > size - 1)
		lprintf("Warning: path truncated: %s\n", buff);
}

static void make_config_cfg(char *cfg_buff_512)
{
	emu_make_path(cfg_buff_512, PicoConfigFile, 512-6);
	if (config_slot != 0)
	{
		char *p = strrchr(cfg_buff_512, '.');
		if (p == NULL)
			p = cfg_buff_512 + strlen(cfg_buff_512);
		sprintf(p, ".%i.cfg", config_slot);
	}
	cfg_buff_512[511] = 0;
}

void emu_set_defconfig(void)
{
	memcpy(&currentConfig, &defaultConfig, sizeof(currentConfig));
	PicoOpt = currentConfig.s_PicoOpt;
	PsndRate = currentConfig.s_PsndRate;
	PicoRegionOverride = currentConfig.s_PicoRegion;
	PicoAutoRgnOrder = currentConfig.s_PicoAutoRgnOrder;
	PicoCDBuffers = currentConfig.s_PicoCDBuffers;
}

int emu_read_config(int game, int no_defaults)
{
	char cfg[512];
	int ret;

	if (!game)
	{
		if (!no_defaults)
			emu_set_defconfig();
		make_config_cfg(cfg);
		ret = config_readsect(cfg, NULL);
	}
	else
	{
		char *sect = emu_make_rom_id();

		// try new .cfg way
		if (config_slot != 0)
		     sprintf(cfg, "game.%i.cfg", config_slot);
		else strcpy(cfg,  "game.cfg");

		ret = -1;
		if (config_havesect(cfg, sect))
		{
			// read user's config
			int vol = currentConfig.volume;
			emu_set_defconfig();
			ret = config_readsect(cfg, sect);
			currentConfig.volume = vol; // make vol global (bah)
		}
		else
		{
			// read global config, and apply game_def.cfg on top
			make_config_cfg(cfg);
			config_readsect(cfg, NULL);
			ret = config_readsect("game_def.cfg", sect);
		}

		if (ret == 0)
		{
			lprintf("loaded cfg from sect \"%s\"\n", sect);
		}
	}

	plat_validate_config();

	// some sanity checks
#ifdef PSP
	/* TODO: mv to plat_validate_config() */
	if (currentConfig.CPUclock < 10 || currentConfig.CPUclock > 4096) currentConfig.CPUclock = 200;
	if (currentConfig.gamma < -4 || currentConfig.gamma >  16) currentConfig.gamma = 0;
	if (currentConfig.gamma2 < 0 || currentConfig.gamma2 > 2)  currentConfig.gamma2 = 0;
#endif
	if (currentConfig.volume < 0 || currentConfig.volume > 99)
		currentConfig.volume = 50;

	if (ret == 0)
		config_slot_current = config_slot;

	return (ret == 0);
}


int emu_write_config(int is_game)
{
	char cfg[512], *game_sect = NULL;
	int ret, write_lrom = 0;

	if (!is_game)
	{
		make_config_cfg(cfg);
		write_lrom = 1;
	} else {
		if (config_slot != 0)
		     sprintf(cfg, "game.%i.cfg", config_slot);
		else strcpy(cfg,  "game.cfg");
		game_sect = emu_make_rom_id();
		lprintf("emu_write_config: sect \"%s\"\n", game_sect);
	}

	lprintf("emu_write_config: %s ", cfg);
	ret = config_writesect(cfg, game_sect);
	if (write_lrom) config_writelrom(cfg);
#ifndef NO_SYNC
	sync();
#endif
	lprintf((ret == 0) ? "(ok)\n" : "(failed)\n");

	if (ret == 0) config_slot_current = config_slot;
	return ret == 0;
}


/* always using built-in font */

#define mk_text_out(name, type, val, topleft, step_x, step_y) \
void name(int x, int y, const char *text)				\
{									\
	int i, l, len = strlen(text);					\
	type *screen = (type *)(topleft) + x * step_x + y * step_y;	\
									\
	for (i = 0; i < len; i++, screen += 8 * step_x)			\
	{								\
		for (l = 0; l < 8; l++)					\
		{							\
			unsigned char fd = fontdata8x8[text[i] * 8 + l];\
			type *s = screen + l * step_y;			\
			if (fd&0x80) s[step_x * 0] = val;		\
			if (fd&0x40) s[step_x * 1] = val;		\
			if (fd&0x20) s[step_x * 2] = val;		\
			if (fd&0x10) s[step_x * 3] = val;		\
			if (fd&0x08) s[step_x * 4] = val;		\
			if (fd&0x04) s[step_x * 5] = val;		\
			if (fd&0x02) s[step_x * 6] = val;		\
			if (fd&0x01) s[step_x * 7] = val;		\
		}							\
	}								\
}

mk_text_out(emu_text_out8,      unsigned char,    0xf0, g_screen_ptr, 1, g_screen_width)
mk_text_out(emu_text_out16,     unsigned short, 0xffff, g_screen_ptr, 1, g_screen_width)
mk_text_out(emu_text_out8_rot,  unsigned char,    0xf0,
	(char *)g_screen_ptr  + (g_screen_width - 1) * g_screen_height, -g_screen_height, 1)
mk_text_out(emu_text_out16_rot, unsigned short, 0xffff,
	(short *)g_screen_ptr + (g_screen_width - 1) * g_screen_height, -g_screen_height, 1)

#undef mk_text_out


void update_movie(void)
{
	int offs = Pico.m.frame_count*3 + 0x40;
	if (offs+3 > movie_size) {
		free(movie_data);
		movie_data = 0;
		plat_status_msg("END OF MOVIE.");
		lprintf("END OF MOVIE.\n");
	} else {
		// MXYZ SACB RLDU
		PicoPad[0] = ~movie_data[offs]   & 0x8f; // ! SCBA RLDU
		if(!(movie_data[offs]   & 0x10)) PicoPad[0] |= 0x40; // C
		if(!(movie_data[offs]   & 0x20)) PicoPad[0] |= 0x10; // A
		if(!(movie_data[offs]   & 0x40)) PicoPad[0] |= 0x20; // B
		PicoPad[1] = ~movie_data[offs+1] & 0x8f; // ! SCBA RLDU
		if(!(movie_data[offs+1] & 0x10)) PicoPad[1] |= 0x40; // C
		if(!(movie_data[offs+1] & 0x20)) PicoPad[1] |= 0x10; // A
		if(!(movie_data[offs+1] & 0x40)) PicoPad[1] |= 0x20; // B
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

char *emu_get_save_fname(int load, int is_sram, int slot)
{
	static char saveFname[512];
	char ext[16];

	if (is_sram)
	{
		romfname_ext(saveFname, (PicoAHW&1) ? "brm"PATH_SEP : "srm"PATH_SEP, (PicoAHW&1) ? ".brm" : ".srm");
		if (load) {
			if (try_ropen_file(saveFname)) return saveFname;
			// try in current dir..
			romfname_ext(saveFname, NULL, (PicoAHW & PAHW_MCD) ? ".brm" : ".srm");
			if (try_ropen_file(saveFname)) return saveFname;
			return NULL; // give up
		}
	}
	else
	{
		ext[0] = 0;
		if(slot > 0 && slot < 10) sprintf(ext, ".%i", slot);
		strcat(ext, (currentConfig.EmuOpt & EOPT_GZIP_SAVES) ? ".mds.gz" : ".mds");

		romfname_ext(saveFname, "mds" PATH_SEP, ext);
		if (load) {
			if (try_ropen_file(saveFname)) return saveFname;
			romfname_ext(saveFname, NULL, ext);
			if (try_ropen_file(saveFname)) return saveFname;
			// no gzipped states, search for non-gzipped
			if (currentConfig.EmuOpt & EOPT_GZIP_SAVES)
			{
				ext[0] = 0;
				if(slot > 0 && slot < 10) sprintf(ext, ".%i", slot);
				strcat(ext, ".mds");

				romfname_ext(saveFname, "mds"PATH_SEP, ext);
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
	return emu_get_save_fname(1, 0, slot) ? 1 : 0;
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

int emu_save_load_game(int load, int sram)
{
	int ret = 0;
	char *saveFname;

	// make save filename
	saveFname = emu_get_save_fname(load, sram, state_slot);
	if (saveFname == NULL) {
		if (!sram)
			plat_status_msg(load ? "LOAD FAILED (missing file)" : "SAVE FAILED");
		return -1;
	}

	lprintf("saveLoad (%i, %i): %s\n", load, sram, saveFname);

	if (sram)
	{
		FILE *sramFile;
		int sram_size;
		unsigned char *sram_data;
		int truncate = 1;
		if (PicoAHW & PAHW_MCD)
		{
			if (PicoOpt&POPT_EN_MCD_RAMCART) {
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

		if (load)
		{
			sramFile = fopen(saveFname, "rb");
			if(!sramFile) return -1;
			fread(sram_data, 1, sram_size, sramFile);
			fclose(sramFile);
			if ((PicoAHW & PAHW_MCD) && (PicoOpt&POPT_EN_MCD_RAMCART))
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
		if (strcmp(saveFname + strlen(saveFname) - 3, ".gz") == 0)
		{
			if( (PmovFile = gzopen(saveFname, load ? "rb" : "wb")) ) {
				emu_setSaveStateCbs(1);
				if (!load) gzsetparams(PmovFile, 9, Z_DEFAULT_STRATEGY);
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
			plat_status_msg(load ? "GAME LOADED" : "GAME SAVED");
		else
		{
			plat_status_msg(load ? "LOAD FAILED" : "SAVE FAILED");
			ret = -1;
		}

		return ret;
	}
}

void emu_set_fastforward(int set_on)
{
	static void *set_PsndOut = NULL;
	static int set_Frameskip, set_EmuOpt, is_on = 0;

	if (set_on && !is_on) {
		set_PsndOut = PsndOut;
		set_Frameskip = currentConfig.Frameskip;
		set_EmuOpt = currentConfig.EmuOpt;
		PsndOut = NULL;
		currentConfig.Frameskip = 8;
		currentConfig.EmuOpt &= ~4;
		currentConfig.EmuOpt |= 0x40000;
		is_on = 1;
		plat_status_msg("FAST FORWARD");
	}
	else if (!set_on && is_on) {
		PsndOut = set_PsndOut;
		currentConfig.Frameskip = set_Frameskip;
		currentConfig.EmuOpt = set_EmuOpt;
		PsndRerate(1);
		is_on = 0;
	}
}

static void emu_msg_tray_open(void)
{
	plat_status_msg("CD tray opened");
}

void emu_reset_game(void)
{
	PicoReset();
	reset_timing = 1;
}

void run_events_pico(unsigned int events)
{
	int lim_x;

	if (events & PEV_PICO_SWINP) {
		pico_inp_mode++;
		if (pico_inp_mode > 2)
			pico_inp_mode = 0;
		switch (pico_inp_mode) {
			case 2: plat_status_msg("Input: Pen on Pad"); break;
			case 1: plat_status_msg("Input: Pen on Storyware"); break;
			case 0: plat_status_msg("Input: Joystick");
				PicoPicohw.pen_pos[0] = PicoPicohw.pen_pos[1] = 0x8000;
				break;
		}
	}
	if (events & PEV_PICO_PPREV) {
		PicoPicohw.page--;
		if (PicoPicohw.page < 0)
			PicoPicohw.page = 0;
		plat_status_msg("Page %i", PicoPicohw.page);
	}
	if (events & PEV_PICO_PNEXT) {
		PicoPicohw.page++;
		if (PicoPicohw.page > 6)
			PicoPicohw.page = 6;
		plat_status_msg("Page %i", PicoPicohw.page);
	}

	if (pico_inp_mode == 0)
		return;

	/* handle other input modes */
	if (PicoPad[0] & 1) pico_pen_y--;
	if (PicoPad[0] & 2) pico_pen_y++;
	if (PicoPad[0] & 4) pico_pen_x--;
	if (PicoPad[0] & 8) pico_pen_x++;
	PicoPad[0] &= ~0x0f; // release UDLR

	lim_x = (Pico.video.reg[12]&1) ? 319 : 255;
	if (pico_pen_y < 8)
		pico_pen_y = 8;
	if (pico_pen_y > 224 - PICO_PEN_ADJUST_Y)
		pico_pen_y = 224 - PICO_PEN_ADJUST_Y;
	if (pico_pen_x < 0)
		pico_pen_x = 0;
	if (pico_pen_x > lim_x - PICO_PEN_ADJUST_X)
		pico_pen_x = lim_x - PICO_PEN_ADJUST_X;

	PicoPicohw.pen_pos[0] = pico_pen_x;
	if (!(Pico.video.reg[12] & 1))
		PicoPicohw.pen_pos[0] += pico_pen_x / 4;
	PicoPicohw.pen_pos[0] += 0x3c;
	PicoPicohw.pen_pos[1] = pico_inp_mode == 1 ? (0x2f8 + pico_pen_y) : (0x1fc + pico_pen_y);
}

static void do_turbo(int *pad, int acts)
{
	static int turbo_pad = 0;
	static unsigned char turbo_cnt[3] = { 0, 0, 0 };
	int inc = currentConfig.turbo_rate * 2;

	if (acts & 0x1000) {
		turbo_cnt[0] += inc;
		if (turbo_cnt[0] >= 60)
			turbo_pad ^= 0x10, turbo_cnt[0] = 0;
	}
	if (acts & 0x2000) {
		turbo_cnt[1] += inc;
		if (turbo_cnt[1] >= 60)
			turbo_pad ^= 0x20, turbo_cnt[1] = 0;
	}
	if (acts & 0x4000) {
		turbo_cnt[2] += inc;
		if (turbo_cnt[2] >= 60)
			turbo_pad ^= 0x40, turbo_cnt[2] = 0;
	}
	*pad |= turbo_pad & (acts >> 8);
}

static void run_events_ui(unsigned int which)
{
	if (which & (PEV_STATE_LOAD|PEV_STATE_SAVE))
	{
		int do_it = 1;
		if ( emu_check_save_file(state_slot) &&
				(((which & PEV_STATE_LOAD) && (currentConfig.EmuOpt & EOPT_CONFIRM_LOAD)) ||
				 ((which & PEV_STATE_SAVE) && (currentConfig.EmuOpt & EOPT_CONFIRM_SAVE))) )
		{
			const char *nm;
			char tmp[64];
			int keys, len;

			strcpy(tmp, (which & PEV_STATE_LOAD) ? "LOAD STATE?" : "OVERWRITE SAVE?");
			len = strlen(tmp);
			nm = in_get_key_name(-1, -PBTN_MA3);
			snprintf(tmp + len, sizeof(tmp) - len, "(%s=yes, ", nm);
			len = strlen(tmp);
			nm = in_get_key_name(-1, -PBTN_MBACK);
			snprintf(tmp + len, sizeof(tmp) - len, "%s=no)", nm);

			plat_status_msg_busy_first(tmp);

			in_set_blocking(1);
			while (in_menu_wait_any(50) & (PBTN_MA3|PBTN_MBACK))
				;
			while ( !((keys = in_menu_wait_any(50)) & (PBTN_MA3|PBTN_MBACK)) )
				;
			if (keys & PBTN_MBACK)
				do_it = 0;
			while (in_menu_wait_any(50) & (PBTN_MA3|PBTN_MBACK))
				;
			in_set_blocking(0);
		}
		if (do_it) {
			plat_status_msg_busy_first((which & PEV_STATE_LOAD) ? "LOADING GAME" : "SAVING GAME");
			PicoStateProgressCB = plat_status_msg_busy_next;
			emu_save_load_game((which & PEV_STATE_LOAD) ? 1 : 0, 0);
			PicoStateProgressCB = NULL;
		}
	}
	if (which & PEV_SWITCH_RND)
	{
		plat_video_toggle_renderer(1, 0);
	}
	if (which & (PEV_SSLOT_PREV|PEV_SSLOT_NEXT))
	{
		if (which & PEV_SSLOT_PREV) {
			state_slot -= 1;
			if (state_slot < 0)
				state_slot = 9;
		} else {
			state_slot += 1;
			if (state_slot > 9)
				state_slot = 0;
		}

		plat_status_msg("SAVE SLOT %i [%s]", state_slot,
			emu_check_save_file(state_slot) ? "USED" : "FREE");
	}
	if (which & PEV_MENU)
		engineState = PGS_Menu;
}

void emu_update_input(void)
{
	static int prev_events = 0;
	int actions[IN_BINDTYPE_COUNT] = { 0, };
	int pl_actions[2];
	int events;

	in_update(actions);

	pl_actions[0] = actions[IN_BINDTYPE_PLAYER12];
	pl_actions[1] = actions[IN_BINDTYPE_PLAYER12] >> 16;

	PicoPad[0] = pl_actions[0] & 0xfff;
	PicoPad[1] = pl_actions[1] & 0xfff;

	if (pl_actions[0] & 0x7000)
		do_turbo(&PicoPad[0], pl_actions[0]);
	if (pl_actions[1] & 0x7000)
		do_turbo(&PicoPad[1], pl_actions[1]);

	events = actions[IN_BINDTYPE_EMU] & PEV_MASK;

	// volume is treated in special way and triggered every frame
	if (events & (PEV_VOL_DOWN|PEV_VOL_UP))
		plat_update_volume(1, events & PEV_VOL_UP);

	if ((events ^ prev_events) & PEV_FF) {
		emu_set_fastforward(events & PEV_FF);
		plat_update_volume(0, 0);
		reset_timing = 1;
	}

	events &= ~prev_events;

	if (PicoAHW == PAHW_PICO)
		run_events_pico(events);
	if (events)
		run_events_ui(events);
	if (movie_data)
		update_movie();

	prev_events = actions[IN_BINDTYPE_EMU] & PEV_MASK;
}

static void mkdir_path(char *path_with_reserve, int pos, const char *name)
{
	strcpy(path_with_reserve + pos, name);
	if (plat_is_dir(path_with_reserve))
		return;
	if (mkdir(path_with_reserve, 0777) < 0)
		lprintf("failed to create: %s\n", path_with_reserve);
}

void emu_init(void)
{
	char path[512];
	int pos;

	/* make dirs for saves */
	pos = plat_get_root_dir(path, sizeof(path) - 4);
	mkdir_path(path, pos, "mds");
	mkdir_path(path, pos, "srm");
	mkdir_path(path, pos, "brm");

	make_config_cfg(path);
	config_readlrom(path);

	PicoInit();
	PicoMessage = plat_status_msg_busy_next;
	PicoMCDopenTray = emu_msg_tray_open;
	PicoMCDcloseTray = menu_loop_tray;
}

void emu_finish(void)
{
	// save SRAM
	if ((currentConfig.EmuOpt & EOPT_EN_SRAM) && SRam.changed) {
		emu_save_load_game(0, 1);
		SRam.changed = 0;
	}

	if (!(currentConfig.EmuOpt & EOPT_NO_AUTOSVCFG)) {
		char cfg[512];
		make_config_cfg(cfg);
		config_writelrom(cfg);
#ifndef NO_SYNC
		sync();
#endif
	}

	PicoExit();
}

