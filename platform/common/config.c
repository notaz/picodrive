/*
 * Human-readable config file management for PicoDrive
 * (c) notaz, 2008
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef __EPOC32__
#include <unistd.h>
#endif
#include "config.h"
#include "input.h"
#include "lprintf.h"

static char *mystrip(char *str);

#ifndef _MSC_VER

#include "menu.h"
#include "emu.h"
#include <pico/pico.h>

#define NL "\r\n"

static int seek_sect(FILE *f, const char *section)
{
	char line[128], *tmp;
	int len;

	len = strlen(section);
	// seek to the section needed
	while (!feof(f))
	{
		tmp = fgets(line, sizeof(line), f);
		if (tmp == NULL) break;

		if (line[0] != '[') continue; // not section start
		if (strncmp(line + 1, section, len) == 0 && line[len+1] == ']')
			return 1; // found it
	}

	return 0;
}


static void custom_write(FILE *f, const menu_entry *me, int no_def)
{
	char str24[24];

	switch (me->id)
	{
		case MA_OPT2_GAMMA:
			if (no_def && defaultConfig.gamma == currentConfig.gamma) return;
			fprintf(f, "Gamma correction = %.3f", (double)currentConfig.gamma / 100.0);
			break;
		case MA_OPT2_SQUIDGEHACK:
			if (no_def && !((defaultConfig.EmuOpt^currentConfig.EmuOpt)&0x0010)) return;
			fprintf(f, "Squidgehack = %i", (currentConfig.EmuOpt&0x0010)>>4);
			break;
		case MA_CDOPT_READAHEAD:
			if (no_def && defaultConfig.s_PicoCDBuffers == PicoCDBuffers) return;
			sprintf(str24, "%i", PicoCDBuffers * 2);
			fprintf(f, "ReadAhead buffer = %s", str24);
			break;
		/* PSP */
		case MA_OPT3_SCALE:
			if (no_def && defaultConfig.scale == currentConfig.scale) return;
			fprintf(f, "Scale factor = %.2f", currentConfig.scale);
			break;
		case MA_OPT3_HSCALE32:
			if (no_def && defaultConfig.hscale32 == currentConfig.hscale32) return;
			fprintf(f, "Hor. scale (for low res. games) = %.2f", currentConfig.hscale32);
			break;
		case MA_OPT3_HSCALE40:
			if (no_def && defaultConfig.hscale40 == currentConfig.hscale40) return;
			fprintf(f, "Hor. scale (for hi res. games) = %.2f", currentConfig.hscale40);
			break;
		case MA_OPT3_FILTERING:
			if (no_def && defaultConfig.scaling == currentConfig.scaling) return;
			fprintf(f, "Bilinear filtering = %i", currentConfig.scaling);
			break;
		case MA_OPT3_GAMMAA:
			if (no_def && defaultConfig.gamma == currentConfig.gamma) return;
			fprintf(f, "Gamma adjustment = %i", currentConfig.gamma);
			break;
		case MA_OPT3_BLACKLVL:
			if (no_def && defaultConfig.gamma2 == currentConfig.gamma2) return;
			fprintf(f, "Black level = %i", currentConfig.gamma2);
			break;
		case MA_OPT3_VSYNC:
			if (no_def && (defaultConfig.EmuOpt&0x12000) == (currentConfig.gamma2&0x12000)) return;
			strcpy(str24, "never");
			if (currentConfig.EmuOpt & 0x2000)
				strcpy(str24, (currentConfig.EmuOpt & 0x10000) ? "sometimes" : "always");
			fprintf(f, "Wait for vsync = %s", str24);
			break;

		default:
			lprintf("unhandled custom_write: %i\n", me->id);
			return;
	}
	fprintf(f, NL);
}


static void keys_write(FILE *fn, const char *bind_str, int dev_id, const int *binds, int no_defaults)
{
	char act[48];
	int key_count, t, i;
	const int *def_binds;

	key_count = in_get_dev_info(dev_id, IN_INFO_BIND_COUNT);
	def_binds = in_get_dev_def_binds(dev_id);

	for (t = 0; t < key_count; t++)
	{
		const char *name;
		act[0] = act[31] = 0;

		if (no_defaults && binds[t] == def_binds[t])
			continue;

		name = in_get_key_name(dev_id, t);
#ifdef __GP2X__
		if (strcmp(name, "SELECT") == 0) continue;
#endif

		if (binds[t] == 0 && def_binds[t] != 0) {
			fprintf(fn, "%s %s =" NL, bind_str, name);
			continue;
		}

		for (i = 0; i < sizeof(me_ctrl_actions) / sizeof(me_ctrl_actions[0]); i++) {
			if (me_ctrl_actions[i].mask & binds[t]) {
				strncpy(act, me_ctrl_actions[i].name, 31);
				fprintf(fn, "%s %s = player%i %s" NL, bind_str, name,
					((binds[t]>>16)&1)+1, mystrip(act));
			}
		}

		for (i = 0; emuctrl_actions[i].name != NULL; i++) {
			if (emuctrl_actions[i].mask & binds[t]) {
				strncpy(act, emuctrl_actions[i].name, 31);
				fprintf(fn, "%s %s = %s" NL, bind_str, name, mystrip(act));
			}
		}
	}
}


static int default_var(const menu_entry *me)
{
	switch (me->id)
	{
		case MA_OPT_ACC_TIMING:
		case MA_OPT_ACC_SPRITES:
		case MA_OPT_ARM940_SOUND:
		case MA_OPT_6BUTTON_PAD:
		case MA_OPT2_ENABLE_Z80:
		case MA_OPT2_ENABLE_YM2612:
		case MA_OPT2_ENABLE_SN76496:
		case MA_OPT2_SVP_DYNAREC:
		case MA_CDOPT_CDDA:
		case MA_CDOPT_PCM:
		case MA_CDOPT_SAVERAM:
		case MA_CDOPT_SCALEROT_CHIP:
		case MA_CDOPT_BETTER_SYNC:
			return defaultConfig.s_PicoOpt;

		case MA_OPT_SHOW_FPS:
		case MA_OPT_ENABLE_SOUND:
		case MA_OPT_SRAM_STATES:
		case MA_OPT2_A_SN_GAMMA:
		case MA_OPT2_VSYNC:
		case MA_OPT2_GZIP_STATES:
		case MA_OPT2_NO_LAST_ROM:
		case MA_OPT2_RAMTIMINGS:
		case MA_CDOPT_LEDS:
			return defaultConfig.EmuOpt;

		case MA_CTRL_TURBO_RATE: return defaultConfig.turbo_rate;
		case MA_OPT_SCALING:     return defaultConfig.scaling;
		case MA_OPT_ROTATION:    return defaultConfig.rotation;

		case MA_OPT_SAVE_SLOT:
		default:
			return 0;
	}
}

int config_writesect(const char *fname, const char *section)
{
	FILE *fo = NULL, *fn = NULL; // old and new
	int no_defaults = 0; // avoid saving defaults
	menu_entry *me;
	int t, tlen, ret;
	char line[128], *tmp;

	if (section != NULL)
	{
		no_defaults = 1;

		fo = fopen(fname, "r");
		if (fo == NULL) {
			fn = fopen(fname, "w");
			goto write;
		}

		ret = seek_sect(fo, section);
		if (!ret) {
			// sect not found, we can simply append
			fclose(fo); fo = NULL;
			fn = fopen(fname, "a");
			goto write;
		}

		// use 2 files..
		fclose(fo);
		rename(fname, "tmp.cfg");
		fo = fopen("tmp.cfg", "r");
		fn = fopen(fname, "w");
		if (fo == NULL || fn == NULL) goto write;

		// copy everything until sect
		tlen = strlen(section);
		while (!feof(fo))
		{
			tmp = fgets(line, sizeof(line), fo);
			if (tmp == NULL) break;

			if (line[0] == '[' && strncmp(line + 1, section, tlen) == 0 && line[tlen+1] == ']')
				break;
			fputs(line, fn);
		}

		// now skip to next sect
		while (!feof(fo))
		{
			tmp = fgets(line, sizeof(line), fo);
			if (tmp == NULL) break;
			if (line[0] == '[') {
				fseek(fo, -strlen(line), SEEK_CUR);
				break;
			}
		}
		if (feof(fo))
		{
			fclose(fo); fo = NULL;
			remove("tmp.cfg");
		}
	}
	else
	{
		fn = fopen(fname, "w");
	}

write:
	if (fn == NULL) {
		if (fo) fclose(fo);
		return -1;
	}
	if (section != NULL)
		fprintf(fn, "[%s]" NL, section);

	me = me_list_get_first();
	while (me != NULL)
	{
		int dummy;
		if (!me->need_to_save)
			goto next;
		if (me->beh == MB_OPT_ONOFF) {
			if (!no_defaults || ((*(int *)me->var ^ default_var(me)) & me->mask))
				fprintf(fn, "%s = %i" NL, me->name, (*(int *)me->var & me->mask) ? 1 : 0);
		} else if (me->beh == MB_OPT_RANGE) {
			if (!no_defaults || (*(int *)me->var ^ default_var(me)))
				fprintf(fn, "%s = %i" NL, me->name, *(int *)me->var);
		} else if (me->name != NULL && me->generate_name != NULL) {
			strncpy(line, me->generate_name(0, &dummy), sizeof(line));
			line[sizeof(line) - 1] = 0;
			mystrip(line);
			fprintf(fn, "%s = %s" NL, me->name, line);
		} else
			custom_write(fn, me, no_defaults);
next:
		me = me_list_get_next();
	}

	/* input: save device names */
	for (t = 0; t < IN_MAX_DEVS; t++)
	{
		const int  *binds = in_get_dev_binds(t);
		const char *name =  in_get_dev_name(t, 0, 0);
		if (binds == NULL || name == NULL)
			continue;

		fprintf(fn, "input%d = %s" NL, t, name);
	}

	/* input: save binds */
	for (t = 0; t < IN_MAX_DEVS; t++)
	{
		const int *binds = in_get_dev_binds(t);
		const char *name = in_get_dev_name(t, 0, 0);
		char strbind[16];
		int count;

		if (binds == NULL || name == NULL)
			continue;

		sprintf(strbind, "bind%d", t);
		if (t == 0) strbind[4] = 0;

		count = in_get_dev_info(t, IN_INFO_BIND_COUNT);
		keys_write(fn, strbind, t, binds, no_defaults);
	}

#ifndef PSP
	if (section == NULL)
		fprintf(fn, "Sound Volume = %i" NL, currentConfig.volume);
#endif

	fprintf(fn, NL);

	if (fo != NULL)
	{
		// copy whatever is left
		while (!feof(fo))
		{
			tmp = fgets(line, sizeof(line), fo);
			if (tmp == NULL) break;

			fputs(line, fn);
		}
		fclose(fo);
		remove("tmp.cfg");
	}

	fclose(fn);
	return 0;
}


int config_writelrom(const char *fname)
{
	char line[128], *tmp, *optr = NULL;
	char *old_data = NULL;
	int size;
	FILE *f;

	if (strlen(rom_fname_loaded) == 0) return -1;

	f = fopen(fname, "r");
	if (f != NULL)
	{
		fseek(f, 0, SEEK_END);
		size = ftell(f);
		fseek(f, 0, SEEK_SET);
		old_data = malloc(size + size/8);
		if (old_data != NULL)
		{
			optr = old_data;
			while (!feof(f))
			{
				tmp = fgets(line, sizeof(line), f);
				if (tmp == NULL) break;
				mystrip(line);
				if (strncasecmp(line, "LastUsedROM", 11) == 0)
					continue;
				sprintf(optr, "%s", line);
				optr += strlen(optr);
			}
		}
		fclose(f);
	}

	f = fopen(fname, "w");
	if (f == NULL) return -1;

	if (old_data != NULL) {
		fwrite(old_data, 1, optr - old_data, f);
		free(old_data);
	}
	fprintf(f, "LastUsedROM = %s" NL, rom_fname_loaded);
	fclose(f);
	return 0;
}

/* --------------------------------------------------------------------------*/

int config_readlrom(const char *fname)
{
	char line[128], *tmp;
	int i, len, ret = -1;
	FILE *f;

	f = fopen(fname, "r");
	if (f == NULL) return -1;

	// seek to the section needed
	while (!feof(f))
	{
		tmp = fgets(line, sizeof(line), f);
		if (tmp == NULL) break;

		if (strncasecmp(line, "LastUsedROM", 11) != 0) continue;
		len = strlen(line);
		for (i = 0; i < len; i++)
			if (line[i] == '#' || line[i] == '\r' || line[i] == '\n') { line[i] = 0; break; }
		tmp = strchr(line, '=');
		if (tmp == NULL) break;
		tmp++;
		mystrip(tmp);

		len = sizeof(rom_fname_loaded);
		strncpy(rom_fname_loaded, tmp, len);
		rom_fname_loaded[len-1] = 0;
		ret = 0;
		break;
	}
	fclose(f);
	return ret;
}


static int custom_read(menu_entry *me, const char *var, const char *val)
{
	char *tmp;
	int tmpi;

	switch (me->id)
	{
		case MA_OPT_RENDERER:
			if (strcasecmp(var, "Renderer") != 0) return 0;
			if      (strcasecmp(val, "8bit fast") == 0 || strcasecmp(val, "fast") == 0) {
				PicoOpt |=  POPT_ALT_RENDERER;
			}
			else if (strcasecmp(val, "16bit accurate") == 0 || strcasecmp(val, "accurate") == 0) {
				PicoOpt &= ~POPT_ALT_RENDERER;
				currentConfig.EmuOpt |=  0x80;
			}
			else if (strcasecmp(val, "8bit accurate") == 0) {
				PicoOpt &= ~POPT_ALT_RENDERER;
				currentConfig.EmuOpt &= ~0x80;
			}
			else
				return 0;
			return 1;

		case MA_OPT_SCALING:
#ifdef __GP2X__
			if (strcasecmp(var, "Scaling") != 0) return 0;
			if        (strcasecmp(val, "OFF") == 0) {
				currentConfig.scaling = 0;
			} else if (strcasecmp(val, "hw horizontal") == 0) {
				currentConfig.scaling = 1;
			} else if (strcasecmp(val, "hw horiz. + vert.") == 0) {
				currentConfig.scaling = 2;
			} else if (strcasecmp(val, "sw horizontal") == 0) {
				currentConfig.scaling = 3;
			} else
				return 0;
			return 1;
#else
			return 0;
#endif

		case MA_OPT_FRAMESKIP:
			if (strcasecmp(var, "Frameskip") != 0) return 0;
			if (strcasecmp(val, "Auto") == 0)
			     currentConfig.Frameskip = -1;
			else currentConfig.Frameskip = atoi(val);
			return 1;

		case MA_OPT_SOUND_QUALITY:
			if (strcasecmp(var, "Sound Quality") != 0) return 0;
			PsndRate = strtoul(val, &tmp, 10);
			if (PsndRate < 8000 || PsndRate > 44100)
				PsndRate = 22050;
			while (*tmp == ' ') tmp++;
			if        (strcasecmp(tmp, "stereo") == 0) {
				PicoOpt |=  POPT_EN_STEREO;
			} else if (strcasecmp(tmp, "mono") == 0) {
				PicoOpt &= ~POPT_EN_STEREO;
			} else
				return 0;
			return 1;

		case MA_OPT_REGION:
			if (strcasecmp(var, "Region") != 0) return 0;
			if       (strncasecmp(val, "Auto: ", 6) == 0)
			{
				const char *p = val + 5, *end = val + strlen(val);
				int i;
				PicoRegionOverride = PicoAutoRgnOrder = 0;
				for (i = 0; p < end && i < 3; i++)
				{
					while (*p == ' ') p++;
					if        (p[0] == 'J' && p[1] == 'P') {
						PicoAutoRgnOrder |= 1 << (i*4);
					} else if (p[0] == 'U' && p[1] == 'S') {
						PicoAutoRgnOrder |= 4 << (i*4);
					} else if (p[0] == 'E' && p[1] == 'U') {
						PicoAutoRgnOrder |= 8 << (i*4);
					}
					while (*p != ' ' && *p != 0) p++;
					if (*p == 0) break;
				}
			}
			else   if (strcasecmp(val, "Auto") == 0) {
				PicoRegionOverride = 0;
			} else if (strcasecmp(val, "Japan NTSC") == 0) {
				PicoRegionOverride = 1;
			} else if (strcasecmp(val, "Japan PAL") == 0) {
				PicoRegionOverride = 2;
			} else if (strcasecmp(val, "USA") == 0) {
				PicoRegionOverride = 4;
			} else if (strcasecmp(val, "Europe") == 0) {
				PicoRegionOverride = 8;
			} else
				return 0;
			return 1;

		case MA_OPT_CONFIRM_STATES:
			if (strcasecmp(var, "Confirm savestate") != 0) return 0;
			if        (strcasecmp(val, "OFF") == 0) {
				currentConfig.EmuOpt &= ~(5<<9);
			} else if (strcasecmp(val, "writes") == 0) {
				currentConfig.EmuOpt &= ~(5<<9);
				currentConfig.EmuOpt |=   1<<9;
			} else if (strcasecmp(val, "loads") == 0) {
				currentConfig.EmuOpt &= ~(5<<9);
				currentConfig.EmuOpt |=   4<<9;
			} else if (strcasecmp(val, "both") == 0) {
				currentConfig.EmuOpt &= ~(5<<9);
				currentConfig.EmuOpt |=   5<<9;
			} else
				return 0;
			return 1;

#if 0 // TODO rm?
		case MA_OPT_CPU_CLOCKS:
#ifdef __GP2X__
			if (strcasecmp(var, "GP2X CPU clocks") != 0) return 0;
#elif defined(PSP)
			if (strcasecmp(var, "PSP CPU clock") != 0) return 0;
#endif
			currentConfig.CPUclock = atoi(val);
			return 1;
#endif

		case MA_OPT2_GAMMA:
			if (strcasecmp(var, "Gamma correction") != 0) return 0;
			currentConfig.gamma = (int) (atof(val) * 100.0);
			return 1;

		case MA_OPT2_SQUIDGEHACK:
			if (strcasecmp(var, "Squidgehack") != 0) return 0;
			tmpi = atoi(val);
			if (tmpi) *(int *)me->var |=  me->mask;
			else      *(int *)me->var &= ~me->mask;
			return 1;

		case MA_CDOPT_READAHEAD:
			if (strcasecmp(var, "ReadAhead buffer") != 0) return 0;
			PicoCDBuffers = atoi(val) / 2;
			return 1;

		/* PSP */
		case MA_OPT3_SCALE:
			if (strcasecmp(var, "Scale factor") != 0) return 0;
			currentConfig.scale = atof(val);
			return 1;
		case MA_OPT3_HSCALE32:
			if (strcasecmp(var, "Hor. scale (for low res. games)") != 0) return 0;
			currentConfig.hscale32 = atof(val);
			return 1;
		case MA_OPT3_HSCALE40:
			if (strcasecmp(var, "Hor. scale (for hi res. games)") != 0) return 0;
			currentConfig.hscale40 = atof(val);
			return 1;
		case MA_OPT3_FILTERING:
			if (strcasecmp(var, "Bilinear filtering") != 0) return 0;
			currentConfig.scaling = atoi(val);
			return 1;
		case MA_OPT3_GAMMAA:
			if (strcasecmp(var, "Gamma adjustment") != 0) return 0;
			currentConfig.gamma = atoi(val);
			return 1;
		case MA_OPT3_BLACKLVL:
			if (strcasecmp(var, "Black level") != 0) return 0;
			currentConfig.gamma2 = atoi(val);
			return 1;
		case MA_OPT3_VSYNC:
			if (strcasecmp(var, "Wait for vsync") != 0) return 0;
			if        (strcasecmp(val, "never") == 0) {
				currentConfig.EmuOpt &= ~0x12000;
			} else if (strcasecmp(val, "sometimes") == 0) {
				currentConfig.EmuOpt |=  0x12000;
			} else if (strcasecmp(val, "always") == 0) {
				currentConfig.EmuOpt &= ~0x12000;
				currentConfig.EmuOpt |=  0x02000;
			} else
				return 0;
			return 1;

		default:
			lprintf("unhandled custom_read: %i\n", me->id);
			return 0;
	}
}


static unsigned int keys_encountered = 0;

static int parse_bind_val(const char *val)
{
	int i;

	if (val[0] == 0)
		return 0;
	
	if (strncasecmp(val, "player", 6) == 0)
	{
		unsigned int player;
		player = atoi(val + 6) - 1;
		if (player > 1)
			return -1;

		for (i = 0; i < sizeof(me_ctrl_actions) / sizeof(me_ctrl_actions[0]); i++) {
			if (strncasecmp(me_ctrl_actions[i].name, val + 8, strlen(val + 8)) == 0)
				return me_ctrl_actions[i].mask | (player<<16);
		}
	}
	for (i = 0; emuctrl_actions[i].name != NULL; i++) {
		if (strncasecmp(emuctrl_actions[i].name, val, strlen(val)) == 0)
			return emuctrl_actions[i].mask;
	}

	return -1;
}

static void keys_parse(const char *key, const char *val, int dev_id)
{
	int binds;

	binds = parse_bind_val(val);
	if (binds == -1) {
		lprintf("config: unhandled action \"%s\"\n", val);
		return;
	}

	in_config_bind_key(dev_id, key, binds);
}

static int get_numvar_num(const char *var)
{
	char *p = NULL;
	int num;
	
	if (var[0] == ' ')
		return 0;

	num = strtoul(var, &p, 10);
	if (*p == 0 || *p == ' ')
		return num;

	return -1;
}

/* map dev number in confing to input dev number */
static unsigned char input_dev_map[IN_MAX_DEVS];

static void parse(const char *var, const char *val)
{
	menu_entry *me;
	int tmp, ret = 0;

	if (strcasecmp(var, "LastUsedROM") == 0)
		return; /* handled elsewhere */

	if (strcasecmp(var, "Sound Volume") == 0) {
		currentConfig.volume = atoi(val);
		return;
	}

	/* input: device name */
	if (strncasecmp(var, "input", 5) == 0) {
		int num = get_numvar_num(var + 5);
		if (num >= 0 && num < IN_MAX_DEVS)
			input_dev_map[num] = in_config_parse_dev(val);
		else
			lprintf("config: failed to parse: %s\n", var);
		return;
	}

	// key binds
	if (strncasecmp(var, "bind", 4) == 0) {
		const char *p = var + 4;
		int num = get_numvar_num(p);
		if (num < 0 || num >= IN_MAX_DEVS) {
			lprintf("config: failed to parse: %s\n", var);
			return;
		}

		num = input_dev_map[num];
		if (num < 0 || num >= IN_MAX_DEVS) {
			lprintf("config: invalid device id: %s\n", var);
			return;
		}

		while (*p && *p != ' ') p++;
		while (*p && *p == ' ') p++;
		keys_parse(p, val, num);
		return;
	}

	me = me_list_get_first();
	while (me != NULL && ret == 0)
	{
		if (!me->need_to_save)
			goto next;
		if (me->name != NULL && me->name[0] != 0) {
			if (strcasecmp(var, me->name) != 0)
				goto next; /* surely not this one */
			if (me->beh == MB_OPT_ONOFF) {
				tmp = atoi(val);
				if (tmp) *(int *)me->var |=  me->mask;
				else     *(int *)me->var &= ~me->mask;
				return;
			} else if (me->beh == MB_OPT_RANGE) {
				tmp = atoi(val);
				if (tmp < me->min) tmp = me->min;
				if (tmp > me->max) tmp = me->max;
				*(int *)me->var = tmp;
				return;
			}
		}
		ret = custom_read(me, var, val);
next:
		me = me_list_get_next();
	}
	if (!ret) lprintf("config_readsect: unhandled var: \"%s\"\n", var);
}


int config_havesect(const char *fname, const char *section)
{
	FILE *f;
	int ret;

	f = fopen(fname, "r");
	if (f == NULL) return 0;

	ret = seek_sect(f, section);
	fclose(f);
	return ret;
}

int config_readsect(const char *fname, const char *section)
{
	char line[128], *var, *val;
	FILE *f;
	int ret;

	f = fopen(fname, "r");
	if (f == NULL) return -1;

	if (section != NULL)
	{
		ret = seek_sect(f, section);
		if (!ret) {
			lprintf("config_readsect: %s: missing section [%s]\n", fname, section);
			fclose(f);
			return -1;
		}
	}

	keys_encountered = 0;
	memset(input_dev_map, 0xff, sizeof(input_dev_map));

	in_config_start();
	while (!feof(f))
	{
		ret = config_get_var_val(f, line, sizeof(line), &var, &val);
		if (ret ==  0) break;
		if (ret == -1) continue;

		parse(var, val);
	}
	in_config_end();

	fclose(f);
	return 0;
}

#endif // _MSC_VER

static char *mystrip(char *str)
{
	int i, len;

	len = strlen(str);
	for (i = 0; i < len; i++)
		if (str[i] != ' ') break;
	if (i > 0) memmove(str, str + i, len - i + 1);

	len = strlen(str);
	for (i = len - 1; i >= 0; i--)
		if (str[i] != ' ') break;
	str[i+1] = 0;

	return str;
}

/* returns:
 *  0 - EOF, end
 *  1 - parsed ok
 * -1 - failed to parse line
 */
int config_get_var_val(void *file, char *line, int lsize, char **rvar, char **rval)
{
	char *var, *val, *tmp;
	FILE *f = file;
	int len, i;

	tmp = fgets(line, lsize, f);
	if (tmp == NULL) return 0;

	if (line[0] == '[') return 0; // other section

	// strip comments, linefeed, spaces..
	len = strlen(line);
	for (i = 0; i < len; i++)
		if (line[i] == '#' || line[i] == '\r' || line[i] == '\n') { line[i] = 0; break; }
	mystrip(line);
	len = strlen(line);
	if (len <= 0) return -1;;

	// get var and val
	for (i = 0; i < len; i++)
		if (line[i] == '=') break;
	if (i >= len || strchr(&line[i+1], '=') != NULL) {
		lprintf("config_readsect: can't parse: %s\n", line);
		return -1;
	}
	line[i] = 0;
	var = line;
	val = &line[i+1];
	mystrip(var);
	mystrip(val);

#ifndef _MSC_VER
	if (strlen(var) == 0 || (strlen(val) == 0 && strncasecmp(var, "bind", 4) != 0)) {
		lprintf("config_readsect: something's empty: \"%s\" = \"%s\"\n", var, val);
		return -1;;
	}
#endif

	*rvar = var;
	*rval = val;
	return 1;
}

