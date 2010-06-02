// (c) Copyright 2006-2009 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
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


#define STATUS_MSG_TIMEOUT 2000

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

/* tmp buff to reduce stack usage for plats with small stack */
static char static_buff[512];
/* TODO: len checking */
char rom_fname_reload[512];
char rom_fname_loaded[512];
int rom_loaded = 0;
int reset_timing = 0;
static unsigned int notice_msg_time;	/* when started showing */
static char noticeMsg[40];

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

static void get_ext(const char *file, char *ext)
{
	const char *p;

	p = file + strlen(file) - 4;
	if (p < file) p = file;
	strncpy(ext, p, 4);
	ext[4] = 0;
	strlwr_(ext);
}

static void fname_ext(char *dst, int dstlen, const char *prefix, const char *ext, const char *fname)
{
	int prefix_len = 0;
	const char *p;

	*dst = 0;
	if (prefix) {
		int len = plat_get_root_dir(dst, dstlen);
		strcpy(dst + len, prefix);
		prefix_len = len + strlen(prefix);
	}

	p = fname + strlen(fname) - 1;
	for (; p >= fname && *p != PATH_SEP_C; p--)
		;
	p++;
	strncpy(dst + prefix_len, p, dstlen - prefix_len - 1);

	dst[dstlen - 8] = 0;
	if (dst[strlen(dst) - 4] == '.')
		dst[strlen(dst) - 4] = 0;
	if (ext)
		strcat(dst, ext);
}

static void romfname_ext(char *dst, int dstlen, const char *prefix, const char *ext)
{
	fname_ext(dst, dstlen, prefix, ext, rom_fname_loaded);
}

void emu_status_msg(const char *format, ...)
{
	va_list vl;
	int ret;

	va_start(vl, format);
	ret = vsnprintf(noticeMsg, sizeof(noticeMsg), format, vl);
	va_end(vl);

	/* be sure old text gets overwritten */
	for (; ret < 28; ret++)
		noticeMsg[ret] = ' ';
	noticeMsg[ret] = 0;

	notice_msg_time = plat_get_ticks_ms();
}

static const char * const biosfiles_us[] = { "us_scd1_9210", "us_scd2_9306", "SegaCDBIOS9303" };
static const char * const biosfiles_eu[] = { "eu_mcd1_9210", "eu_mcd2_9306", "eu_mcd2_9303"   };
static const char * const biosfiles_jp[] = { "jp_mcd1_9112", "jp_mcd1_9111" };

static int find_bios(int region, char **bios_file)
{
	int i, count;
	const char * const *files;
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
		emu_make_path(static_buff, files[i], sizeof(static_buff) - 4);
		strcat(static_buff, ".bin");
		f = fopen(static_buff, "rb");
		if (f) break;

		static_buff[strlen(static_buff) - 4] = 0;
		strcat(static_buff, ".zip");
		f = fopen(static_buff, "rb");
		if (f) break;
	}

	if (f) {
		lprintf("using bios: %s\n", static_buff);
		fclose(f);
		if (bios_file)
			*bios_file = static_buff;
		return 1;
	} else {
		sprintf(static_buff, "no %s BIOS files found, read docs",
			region != 4 ? (region == 8 ? "EU" : "JAP") : "USA");
		me_update_msg(static_buff);
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

/* checks if fname points to valid MegaCD image */
static int emu_cd_check(int *pregion, const char *fname_in)
{
	const char *fname = fname_in;
	unsigned char buf[32];
	pm_file *cd_f;
	int region = 4; // 1: Japan, 4: US, 8: Europe
	char ext[5];
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

static int detect_media(const char *fname)
{
	static const short sms_offsets[] = { 0x7ff0, 0x3ff0, 0x1ff0 };
	static const char *sms_exts[] = { "sms", "gg", "sg" };
	static const char *md_exts[] = { "gen", "bin", "smd" };
	char buff0[32], buff[32];
	unsigned short *d16;
	pm_file *pmf;
	char ext[5];
	int i;

	get_ext(fname, ext);

	// detect wrong extensions
	if (!strcmp(ext, ".srm") || !strcmp(ext, "s.gz") || !strcmp(ext, ".mds")) // s.gz ~ .mds.gz
		return PM_BAD;

	/* don't believe in extensions, except .cue */
	if (strcasecmp(ext, ".cue") == 0)
		return PM_CD;

	pmf = pm_open(fname);
	if (pmf == NULL)
		return PM_BAD;

	if (pm_read(buff0, 32, pmf) != 32) {
		pm_close(pmf);
		return PM_BAD;
	}

	if (strncasecmp("SEGADISCSYSTEM", buff0 + 0x00, 14) == 0 ||
	    strncasecmp("SEGADISCSYSTEM", buff0 + 0x10, 14) == 0) {
		pm_close(pmf);
		return PM_CD;
	}

	/* check for SMD evil */
	if (pmf->size >= 0x4200 && (pmf->size & 0x3fff) == 0x200) {
		if (pm_seek(pmf, sms_offsets[0] + 0x200, SEEK_SET) == sms_offsets[0] + 0x200 &&
		    pm_read(buff, 16, pmf) == 16 &&
		    strncmp("TMR SEGA", buff, 8) == 0)
			goto looks_like_sms;

		/* could parse further but don't bother */
		goto extension_check;
	}

	/* MD header? Act as TMSS BIOS here */
	if (pm_seek(pmf, 0x100, SEEK_SET) == 0x100 && pm_read(buff, 16, pmf) == 16) {
		if (strncmp(buff, "SEGA", 4) == 0 || strncmp(buff, " SEG", 4) == 0)
			goto looks_like_md;
	}

	for (i = 0; i < array_size(sms_offsets); i++) {
		if (pm_seek(pmf, sms_offsets[i], SEEK_SET) != sms_offsets[i])
			continue;

		if (pm_read(buff, 16, pmf) != 16)
			continue;

		if (strncmp("TMR SEGA", buff, 8) == 0)
			goto looks_like_sms;
	}

extension_check:
	/* probably some headerless thing. Maybe check the extension after all. */
	for (i = 0; i < array_size(md_exts); i++)
		if (strcasecmp(pmf->ext, md_exts[i]) == 0)
			goto looks_like_md;

	for (i = 0; i < array_size(sms_exts); i++)
		if (strcasecmp(pmf->ext, sms_exts[i]) == 0)
			goto looks_like_sms;

	/* If everything else fails, make a guess on the reset vector */
	d16 = (unsigned short *)(buff0 + 4);
	if ((((d16[0] << 16) | d16[1]) & 0xffffff) >= pmf->size) {
		lprintf("bad MD reset vector, assuming SMS\n");
		goto looks_like_sms;
	}

looks_like_md:
	pm_close(pmf);
	return PM_MD_CART;

looks_like_sms:
	pm_close(pmf);
	return PM_MARK3;
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

static char *emu_make_rom_id(const char *fname)
{
	static char id_string[3+0xe*3+0x3*3+0x30*3+3];
	int pos, swab = 1;

	if (PicoAHW & PAHW_MCD) {
		strcpy(id_string, "CD|");
		swab = 0;
	}
	else if (PicoAHW & PAHW_SMS)
		strcpy(id_string, "MS|");
	else	strcpy(id_string, "MD|");
	pos = 3;

	if (!(PicoAHW & PAHW_SMS)) {
		pos += extract_text(id_string + pos, id_header + 0x80, 0x0e, swab); // serial
		id_string[pos] = '|'; pos++;
		pos += extract_text(id_string + pos, id_header + 0xf0, 0x03, swab); // region
		id_string[pos] = '|'; pos++;
		pos += extract_text(id_string + pos, id_header + 0x50, 0x30, swab); // overseas name
		id_string[pos] = 0;
		if (pos > 5)
			return id_string;
		pos = 3;
	}

	// can't find name in ROM, use filename
	fname_ext(id_string + 3, sizeof(id_string) - 3, NULL, NULL, fname);

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

static void system_announce(void)
{
	const char *sys_name, *tv_standard, *extra = "";
	int fps;

	if (PicoAHW & PAHW_SMS) {
		sys_name = "Master System";
#ifdef NO_SMS
		extra = " [no support]";
#endif
	} else if (PicoAHW & PAHW_PICO) {
		sys_name = "Pico";
	} else if (PicoAHW & PAHW_MCD) {
		sys_name = "Mega CD";
		if ((Pico.m.hardware & 0xc0) == 0x80)
			sys_name = "Sega CD";
	} else if (PicoAHW & PAHW_32X) {
		sys_name = "32X";
	} else {
		sys_name = "MegaDrive";
		if ((Pico.m.hardware & 0xc0) == 0x80)
			sys_name = "Genesis";
	}
	tv_standard = Pico.m.pal ? "PAL" : "NTSC";
	fps = Pico.m.pal ? 50 : 60;

	emu_status_msg("%s %s / %dFPS%s", tv_standard, sys_name, fps, extra);
}

// note: this function might mangle rom_fname
// XXX: portions of this code should move to pico/
int emu_reload_rom(char *rom_fname)
{
	unsigned int rom_size = 0;
	char *used_rom_name = rom_fname;
	unsigned char *rom_data = NULL;
	char ext[5];
	pm_file *rom = NULL;
	int cd_state = CIT_NOT_CD;
	int ret, media_type, cd_region;
	int cfg_loaded = 0, bad_rom = 0;

	lprintf("emu_ReloadRom(%s)\n", rom_fname);

	get_ext(rom_fname, ext);

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
		if (!movie_file) {
			me_update_msg("Failed to open movie.");
			return 0;
		}
		fseek(movie_file, 0, SEEK_END);
		movie_size = ftell(movie_file);
		fseek(movie_file, 0, SEEK_SET);
		if (movie_size < 64+3) {
			me_update_msg("Invalid GMV file.");
			fclose(movie_file);
			return 0;
		}
		movie_data = malloc(movie_size);
		if (movie_data == NULL) {
			me_update_msg("low memory.");
			fclose(movie_file);
			return 0;
		}
		dummy = fread(movie_data, 1, movie_size, movie_file);
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

	media_type = detect_media(rom_fname);
	if (media_type == PM_BAD) {
		me_update_msg("Not a ROM/CD img selected.");
		return 0;
	}

	shutdown_MCD();
	PicoPatchUnload();
	PicoCartUnload();
	rom_loaded = 0;

	PicoAHW = 0;

	if (media_type == PM_CD)
	{
		// check for MegaCD image
		cd_state = emu_cd_check(&cd_region, rom_fname);
		if (cd_state >= 0 && cd_state != CIT_NOT_CD)
		{
			// valid CD image, check for BIOS..

			// we need to have config loaded at this point
			ret = emu_read_config(rom_fname, 0);
			if (!ret) emu_read_config(NULL, 0);
			cfg_loaded = 1;

			if (PicoRegionOverride) {
				cd_region = PicoRegionOverride;
				lprintf("override region to %s\n", cd_region != 4 ?
					(cd_region == 8 ? "EU" : "JAP") : "USA");
			}
			if (!find_bios(cd_region, &used_rom_name))
				return 0;

			get_ext(used_rom_name, ext);
			PicoAHW |= PAHW_MCD;
		}
		else {
			me_update_msg("Invalid CD image");
			return 0;
		}
	}
	else if (media_type == PM_MARK3) {
		lprintf("detected SMS ROM\n");
		PicoAHW = PAHW_SMS;
	}

	rom = pm_open(used_rom_name);
	if (rom == NULL) {
		me_update_msg("Failed to open ROM");
		return 0;
	}

	menu_romload_prepare(used_rom_name); // also CD load
	used_rom_name = NULL; // uses static_buff

	ret = PicoCartLoad(rom, &rom_data, &rom_size, (PicoAHW & PAHW_SMS) ? 1 : 0);
	pm_close(rom);
	if (ret != 0) {
		if      (ret == 2) me_update_msg("Out of memory");
		else if (ret == 3) me_update_msg("Read failed");
		else               me_update_msg("PicoCartLoad() failed.");
		goto fail;
	}

	// detect wrong files
	if (strncmp((char *)rom_data, "Pico", 4) == 0)
		bad_rom = 1;
	else if (!(PicoAHW & PAHW_SMS)) {
		unsigned short *d = (unsigned short *)(rom_data + 4);
		if ((((d[0] << 16) | d[1]) & 0xffffff) >= (int)rom_size) {
			lprintf("bad reset vector\n");
			bad_rom = 1;
		}
	}

	if (bad_rom) {
		me_update_msg("Bad ROM detected.");
		goto fail;
	}

	// load config for this ROM (do this before insert to get correct region)
	if (!(PicoAHW & PAHW_MCD))
		memcpy(id_header, rom_data + 0x100, sizeof(id_header));
	if (!cfg_loaded) {
		ret = emu_read_config(rom_fname, 0);
		if (!ret) emu_read_config(NULL, 0);
	}

	emu_make_path(static_buff, "carthw.cfg", sizeof(static_buff));
	if (PicoCartInsert(rom_data, rom_size, static_buff)) {
		me_update_msg("Failed to load ROM.");
		goto fail;
	}

	// insert CD if it was detected
	if (cd_state != CIT_NOT_CD) {
		ret = Insert_CD(rom_fname, cd_state);
		if (ret != 0) {
			PicoCartUnload();
			rom_data = NULL; // freed by unload
			me_update_msg("Insert_CD() failed, invalid CD image?");
			goto fail;
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
		emu_status_msg("MOVIE: %s", (char *) &movie_data[0x18]);
	}
	else
	{
		system_announce();
		PicoOpt &= ~POPT_DIS_VDP_FIFO;
	}

	strncpy(rom_fname_loaded, rom_fname, sizeof(rom_fname_loaded)-1);
	rom_fname_loaded[sizeof(rom_fname_loaded)-1] = 0;
	rom_loaded = 1;

	// load SRAM for this ROM
	if (currentConfig.EmuOpt & EOPT_EN_SRAM)
		emu_save_load_game(1, 1);

	return 1;

fail:
	if (rom_data)
		free(rom_data);
	menu_romload_end();
	return 0;
}

int emu_swap_cd(const char *fname)
{
	cd_img_type cd_type;
	int ret = -1;

	cd_type = emu_cd_check(NULL, fname);
	if (cd_type != CIT_NOT_CD)
		ret = Insert_CD(fname, cd_type);
	if (ret != 0) {
		me_update_msg("Load failed, invalid CD image?");
		return 0;
	}

	strncpy(rom_fname_loaded, fname, sizeof(rom_fname_loaded)-1);
	rom_fname_loaded[sizeof(rom_fname_loaded)-1] = 0;
	return 1;
}

// <base dir><end>
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

void emu_prep_defconfig(void)
{
	memset(&defaultConfig, 0, sizeof(defaultConfig));
	defaultConfig.EmuOpt    = 0x9d | EOPT_RAM_TIMINGS|EOPT_EN_CD_LEDS;
	defaultConfig.s_PicoOpt = POPT_EN_STEREO|POPT_EN_FM|POPT_EN_PSG|POPT_EN_Z80 |
				  POPT_EN_MCD_PCM|POPT_EN_MCD_CDDA|POPT_EN_SVP_DRC|POPT_ACC_SPRITES |
				  POPT_EN_32X|POPT_EN_PWM;
	defaultConfig.s_PsndRate = 44100;
	defaultConfig.s_PicoRegion = 0; // auto
	defaultConfig.s_PicoAutoRgnOrder = 0x184; // US, EU, JP
	defaultConfig.s_PicoCDBuffers = 0;
	defaultConfig.confirm_save = EOPT_CONFIRM_SAVE;
	defaultConfig.Frameskip = -1; // auto
	defaultConfig.volume = 50;
	defaultConfig.gamma = 100;
	defaultConfig.scaling = 0;
	defaultConfig.turbo_rate = 15;

	// platform specific overrides
	pemu_prep_defconfig();
}

void emu_set_defconfig(void)
{
	memcpy(&currentConfig, &defaultConfig, sizeof(currentConfig));
	PicoOpt = currentConfig.s_PicoOpt;
	PsndRate = currentConfig.s_PsndRate;
	PicoRegionOverride = currentConfig.s_PicoRegion;
	PicoAutoRgnOrder = currentConfig.s_PicoAutoRgnOrder;
	PicoCDBuffers = currentConfig.s_PicoCDBuffers;
	p32x_msh2_multiplier = MSH2_MULTI_DEFAULT;
	p32x_ssh2_multiplier = SSH2_MULTI_DEFAULT;
}

int emu_read_config(const char *rom_fname, int no_defaults)
{
	char cfg[512];
	int ret;

	if (!no_defaults)
		emu_set_defconfig();

	if (rom_fname == NULL)
	{
		// global config
		make_config_cfg(cfg);
		ret = config_readsect(cfg, NULL);
	}
	else
	{
		char *sect = emu_make_rom_id(rom_fname);

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
			emu_make_path(cfg, "game_def.cfg", sizeof(cfg));
			ret = config_readsect(cfg, sect);
		}

		if (ret == 0)
		{
			lprintf("loaded cfg from sect \"%s\"\n", sect);
		}
	}

	pemu_validate_config();

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
		game_sect = emu_make_rom_id(rom_fname_loaded);
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
		emu_status_msg("END OF MOVIE.");
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
	char *saveFname = static_buff;
	char ext[16];

	if (is_sram)
	{
		strcpy(ext, (PicoAHW & PAHW_MCD) ? ".brm" : ".srm");
		romfname_ext(saveFname, sizeof(static_buff),
			(PicoAHW & PAHW_MCD) ? "brm"PATH_SEP : "srm"PATH_SEP, ext);
		if (!load)
			return saveFname;

		if (try_ropen_file(saveFname))
			return saveFname;

		romfname_ext(saveFname, sizeof(static_buff), NULL, ext);
		if (try_ropen_file(saveFname))
			return saveFname;
	}
	else
	{
		const char *ext_main = (currentConfig.EmuOpt & EOPT_GZIP_SAVES) ? ".mds.gz" : ".mds";
		const char *ext_othr = (currentConfig.EmuOpt & EOPT_GZIP_SAVES) ? ".mds" : ".mds.gz";
		ext[0] = 0;
		if (slot > 0 && slot < 10)
			sprintf(ext, ".%i", slot);
		strcat(ext, ext_main);

		if (!load) {
			romfname_ext(saveFname, sizeof(static_buff), "mds" PATH_SEP, ext);
			return saveFname;
		}
		else {
			romfname_ext(saveFname, sizeof(static_buff), "mds" PATH_SEP, ext);
			if (try_ropen_file(saveFname))
				return saveFname;

			romfname_ext(saveFname, sizeof(static_buff), NULL, ext);
			if (try_ropen_file(saveFname))
				return saveFname;

			// try the other ext
			ext[0] = 0;
			if (slot > 0 && slot < 10)
				sprintf(ext, ".%i", slot);
			strcat(ext, ext_othr);

			romfname_ext(saveFname, sizeof(static_buff), "mds"PATH_SEP, ext);
			if (try_ropen_file(saveFname))
				return saveFname;
		}
	}

	return NULL;
}

int emu_check_save_file(int slot)
{
	return emu_get_save_fname(1, 0, slot) ? 1 : 0;
}

int emu_save_load_game(int load, int sram)
{
	int ret = 0;
	char *saveFname;

	// make save filename
	saveFname = emu_get_save_fname(load, sram, state_slot);
	if (saveFname == NULL) {
		if (!sram)
			emu_status_msg(load ? "LOAD FAILED (missing file)" : "SAVE FAILED");
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
			if (PicoOpt & POPT_EN_MCD_RAMCART) {
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
			sram_size = SRam.size;
			sram_data = SRam.data;
		}
		if (sram_data == NULL)
			return 0; // SRam forcefully disabled for this game

		if (load)
		{
			sramFile = fopen(saveFname, "rb");
			if (!sramFile)
				return -1;
			ret = fread(sram_data, 1, sram_size, sramFile);
			ret = ret > 0 ? 0 : -1;
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
		ret = PicoState(saveFname, !load);
		if (!ret) {
#ifndef NO_SYNC
			if (!load) sync();
#endif
			emu_status_msg(load ? "STATE LOADED" : "STATE SAVED");
		} else {
			emu_status_msg(load ? "LOAD FAILED" : "SAVE FAILED");
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
		emu_status_msg("FAST FORWARD");
	}
	else if (!set_on && is_on) {
		PsndOut = set_PsndOut;
		currentConfig.Frameskip = set_Frameskip;
		currentConfig.EmuOpt = set_EmuOpt;
		PsndRerate(1);
		is_on = 0;
	}
}

static void emu_tray_open(void)
{
	engineState = PGS_TrayMenu;
}

static void emu_tray_close(void)
{
	emu_status_msg("CD tray closed.");
}

void emu_32x_startup(void)
{
	plat_video_toggle_renderer(0, 0);
	system_announce();
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
			case 2: emu_status_msg("Input: Pen on Pad"); break;
			case 1: emu_status_msg("Input: Pen on Storyware"); break;
			case 0: emu_status_msg("Input: Joystick");
				PicoPicohw.pen_pos[0] = PicoPicohw.pen_pos[1] = 0x8000;
				break;
		}
	}
	if (events & PEV_PICO_PPREV) {
		PicoPicohw.page--;
		if (PicoPicohw.page < 0)
			PicoPicohw.page = 0;
		emu_status_msg("Page %i", PicoPicohw.page);
	}
	if (events & PEV_PICO_PNEXT) {
		PicoPicohw.page++;
		if (PicoPicohw.page > 6)
			PicoPicohw.page = 6;
		emu_status_msg("Page %i", PicoPicohw.page);
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
			(((which & PEV_STATE_LOAD) && (currentConfig.confirm_save & EOPT_CONFIRM_LOAD)) ||
			 ((which & PEV_STATE_SAVE) && (currentConfig.confirm_save & EOPT_CONFIRM_SAVE))) )
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
			plat_status_msg_busy_first((which & PEV_STATE_LOAD) ? "LOADING STATE" : "SAVING STATE");
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

		emu_status_msg("SAVE SLOT %i [%s]", state_slot,
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

#if 0
	// FIXME: handle through menu, etc
	FILE *f;
	f = fopen("32X_M_BIOS.BIN", "rb");
	p32x_bios_m = malloc(2048);
	fread(p32x_bios_m, 1, 2048, f);
	fclose(f);
	f = fopen("32X_S_BIOS.BIN", "rb");
	p32x_bios_s = malloc(1024);
	fread(p32x_bios_s, 1, 1024, f);
	fclose(f);
#endif

	/* make dirs for saves */
	pos = plat_get_root_dir(path, sizeof(path) - 4);
	mkdir_path(path, pos, "mds");
	mkdir_path(path, pos, "srm");
	mkdir_path(path, pos, "brm");

	pprof_init();

	make_config_cfg(path);
	config_readlrom(path);

	PicoInit();
	PicoMessage = plat_status_msg_busy_next;
	PicoMCDopenTray = emu_tray_open;
	PicoMCDcloseTray = emu_tray_close;
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

	pprof_finish();

	PicoExit();
}

static void skip_frame(int do_audio)
{
	PicoSkipFrame = do_audio ? 1 : 2;
	PicoFrame();
	PicoSkipFrame = 0;
}

/* our tick here is 1 us right now */
#define ms_to_ticks(x) (unsigned int)(x * 1000)
#define get_ticks() plat_get_ticks_us()

void emu_loop(void)
{
	int pframes_done;		/* "period" frames, used for sync */
	int frames_done, frames_shown;	/* actual frames for fps counter */
	int target_fps, target_frametime;
	unsigned int timestamp_base = 0, timestamp_fps;
	char *notice_msg = NULL;
	char fpsbuff[24];
	int i;

	fpsbuff[0] = 0;

	/* make sure we are in correct mode */
	Pico.m.dirtyPal = 1;
	rendstatus_old = -1;

	PicoLoopPrepare();

	// prepare CD buffer
	if (PicoAHW & PAHW_MCD)
		PicoCDBufferInit();

	pemu_loop_prep();

	/* number of ticks per frame */
	if (Pico.m.pal) {
		target_fps = 50;
		target_frametime = ms_to_ticks(1000) / 50;
	} else {
		target_fps = 60;
		target_frametime = ms_to_ticks(1000) / 60 + 1;
	}

	timestamp_fps = get_ticks();
	reset_timing = 1;

	frames_done = frames_shown = pframes_done = 0;

	plat_video_wait_vsync();

	/* loop with resync every 1 sec. */
	while (engineState == PGS_Running)
	{
		unsigned int timestamp;
		int diff, diff_lim;

		pprof_start(main);

		timestamp = get_ticks();
		if (reset_timing) {
			reset_timing = 0;
			timestamp_base = timestamp;
			pframes_done = 0;
		}

		// show notice_msg message?
		if (notice_msg_time != 0)
		{
			static int noticeMsgSum;
			if (timestamp - ms_to_ticks(notice_msg_time) > ms_to_ticks(STATUS_MSG_TIMEOUT)) {
				notice_msg_time = 0;
				plat_status_msg_clear();
				notice_msg = NULL;
			} else {
				int sum = noticeMsg[0] + noticeMsg[1] + noticeMsg[2];
				if (sum != noticeMsgSum) {
					plat_status_msg_clear();
					noticeMsgSum = sum;
				}
				notice_msg = noticeMsg;
			}
		}

		// second changed?
		if (timestamp - timestamp_fps >= ms_to_ticks(1000))
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
			printf("%s\n", fpsbuff);
#else
			if (currentConfig.EmuOpt & EOPT_SHOW_FPS) {
				sprintf(fpsbuff, "%02i/%02i", frames_shown, frames_done);
				if (fpsbuff[5] == 0) { fpsbuff[5] = fpsbuff[6] = ' '; fpsbuff[7] = 0; }
			}
#endif
			frames_shown = frames_done = 0;
			timestamp_fps += ms_to_ticks(1000);
		}
#ifdef PFRAMES
		sprintf(fpsbuff, "%i", Pico.m.frame_count);
#endif

		if (timestamp - timestamp_base >= ms_to_ticks(1000))
		{
			if ((currentConfig.EmuOpt & EOPT_NO_FRMLIMIT) && currentConfig.Frameskip >= 0)
				pframes_done = 0;
			else
				pframes_done -= target_fps;
			if (pframes_done < -2) {
				/* don't drag more than 2 frames behind */
				pframes_done = -2;
				timestamp_base = timestamp - 2 * target_frametime;
			}
			else
				timestamp_base += ms_to_ticks(1000);
		}

		diff = timestamp - timestamp_base;
		diff_lim = (pframes_done + 1) * target_frametime;

		if (currentConfig.Frameskip >= 0) // frameskip enabled
		{
			for (i = 0; i < currentConfig.Frameskip; i++) {
				emu_update_input();
				skip_frame(1);
				pframes_done++; frames_done++;
				diff_lim += target_frametime;

				if (!(currentConfig.EmuOpt & (EOPT_NO_FRMLIMIT|EOPT_EXT_FRMLIMIT))) {
					timestamp = get_ticks();
					diff = timestamp - timestamp_base;
					if (!reset_timing && diff < diff_lim) // we are too fast
						plat_wait_till_us(timestamp_base + diff_lim);
				}
			}
		}
		else if (diff > diff_lim)
		{
			/* no time left for this frame - skip */
			/* limit auto frameskip to 8 */
			if (frames_done / 8 <= frames_shown) {
				emu_update_input();
				skip_frame(diff < diff_lim + target_frametime * 16);
				pframes_done++; frames_done++;
				continue;
			}
		}

		emu_update_input();
		PicoFrame();
		pemu_finalize_frame(fpsbuff, notice_msg);

		// plat_video_flip();

		/* frame limiter */
		if (!reset_timing && !(currentConfig.EmuOpt & (EOPT_NO_FRMLIMIT|EOPT_EXT_FRMLIMIT)))
		{
			timestamp = get_ticks();
			diff = timestamp - timestamp_base;

			// sleep or vsync if we are still too fast
			if (diff < diff_lim)
			{
				// we are too fast
				plat_wait_till_us(timestamp_base + diff_lim - target_frametime / 4);
				if (currentConfig.EmuOpt & EOPT_VSYNC)
					plat_video_wait_vsync();
			}
		}

		// XXX: for some plats it might be better to flip before vsync
		// (due to shadow registers in display hw)
		plat_video_flip();

		pframes_done++; frames_done++; frames_shown++;

		pprof_end(main);
	}

	emu_set_fastforward(0);

	// save SRAM
	if ((currentConfig.EmuOpt & EOPT_EN_SRAM) && SRam.changed) {
		plat_status_msg_busy_first("Writing SRAM/BRAM...");
		emu_save_load_game(0, 1);
		SRam.changed = 0;
	}

	pemu_loop_end();

	// pemu_loop_end() might want to do 1 frame for bg image,
	// so free CD buffer here
	if (PicoAHW & PAHW_MCD)
		PicoCDBufferFree();
}

