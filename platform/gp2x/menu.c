// (c) Copyright 2006,2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>

#include "gp2x.h"
#include "emu.h"
#include "menu.h"
#include "fonts.h"
#include "usbjoy.h"
#include "asmutils.h"
#include "version.h"

#include <Pico/PicoInt.h>
#include <Pico/Patch.h>
#include <zlib/zlib.h>

#ifndef _DIRENT_HAVE_D_TYPE
#error "need d_type for file browser
#endif

extern char romFileName[PATH_MAX];
extern char *rom_data;
extern int  mmuhack_status;
extern int  state_slot;
extern int  config_slot, config_slot_current;

static char *gp2xKeyNames[] = {
	"UP",    "???",    "LEFT", "???",  "DOWN", "???", "RIGHT",    "???",
	"START", "SELECT", "L",    "R",    "A",    "B",   "X",        "Y",
	"???",   "???",    "???",  "???",  "???",  "???", "VOL DOWN", "VOL UP",
	"???",   "???",    "???",  "PUSH", "???",  "???", "???",      "???"
};

char menuErrorMsg[40] = {0, };


static void gp2x_text(unsigned char *screen, int x, int y, const char *text, int color)
{
	int i,l;

	screen = screen + x + y*320;

	for (i = 0; i < strlen(text); i++)
	{
		for (l=0;l<8;l++)
		{
			if(fontdata8x8[((text[i])*8)+l]&0x80) screen[l*320+0]=color;
			if(fontdata8x8[((text[i])*8)+l]&0x40) screen[l*320+1]=color;
			if(fontdata8x8[((text[i])*8)+l]&0x20) screen[l*320+2]=color;
			if(fontdata8x8[((text[i])*8)+l]&0x10) screen[l*320+3]=color;
			if(fontdata8x8[((text[i])*8)+l]&0x08) screen[l*320+4]=color;
			if(fontdata8x8[((text[i])*8)+l]&0x04) screen[l*320+5]=color;
			if(fontdata8x8[((text[i])*8)+l]&0x02) screen[l*320+6]=color;
			if(fontdata8x8[((text[i])*8)+l]&0x01) screen[l*320+7]=color;
		}
		screen += 8;
	}
}

// draws white text to current bbp15 screen
void gp2x_text_out15(int x, int y, const char *text)
{
	int i,l;
	unsigned short *screen = gp2x_screen;

	screen = screen + x + y*320;

	for (i = 0; i < strlen(text); i++)
	{
		for (l=0;l<8;l++)
		{
			if(fontdata8x8[((text[i])*8)+l]&0x80) screen[l*320+0]=0xffff;
			if(fontdata8x8[((text[i])*8)+l]&0x40) screen[l*320+1]=0xffff;
			if(fontdata8x8[((text[i])*8)+l]&0x20) screen[l*320+2]=0xffff;
			if(fontdata8x8[((text[i])*8)+l]&0x10) screen[l*320+3]=0xffff;
			if(fontdata8x8[((text[i])*8)+l]&0x08) screen[l*320+4]=0xffff;
			if(fontdata8x8[((text[i])*8)+l]&0x04) screen[l*320+5]=0xffff;
			if(fontdata8x8[((text[i])*8)+l]&0x02) screen[l*320+6]=0xffff;
			if(fontdata8x8[((text[i])*8)+l]&0x01) screen[l*320+7]=0xffff;
		}
		screen += 8;
	}
}


void gp2x_text_out8(int x, int y, const char *texto, ...)
{
	va_list args;
	char    buffer[512];

	va_start(args,texto);
	vsprintf(buffer,texto,args);
	va_end(args);

	gp2x_text(gp2x_screen,x,y,buffer,0xf0);
}


void gp2x_text_out8_2(int x, int y, const char *texto, int color)
{
	gp2x_text(gp2x_screen, x, y, texto, color);
}

void gp2x_text_out8_lim(int x, int y, const char *texto, int max)
{
	char    buffer[320/8+1];

	strncpy(buffer, texto, 320/8);
	if (max > 320/8) max = 320/8;
	if (max < 0) max = 0;
	buffer[max] = 0;

	gp2x_text(gp2x_screen,x,y,buffer,0xf0);
}

static void gp2x_smalltext8(int x, int y, const char *texto)
{
	int i;
	unsigned char *src, *dst;

	for (i = 0;; i++, x += 6)
	{
		unsigned char c = (unsigned char) texto[i];
		int h = 8;

		if (!c) break;

		src = fontdata6x8[c];
		dst = (unsigned char *)gp2x_screen + x + y*320;

		while (h--)
		{
			int w = 0x20;
			while (w)
			{
				if( *src & w ) *dst = 0xf0;
				dst++;
				w>>=1;
			}
			src++;

			dst += 320-6;
		}
	}
}

static void gp2x_smalltext8_lim(int x, int y, const char *texto, int max)
{
	char    buffer[320/6+1];

	strncpy(buffer, texto, 320/6);
	if (max > 320/6) max = 320/6;
	if (max < 0) max = 0;
	buffer[max] = 0;

	gp2x_smalltext8(x, y, buffer);
}


typedef enum
{
	MB_NONE = 1,		/* no auto processing */
	MB_ONOFF,		/* ON/OFF setting */
	MB_RANGE,		/* [min-max] setting */
} menu_behavior;

typedef enum
{
	MA_NONE = 1,
	MA_MAIN_RESUME_GAME,
	MA_MAIN_SAVE_STATE,
	MA_MAIN_LOAD_STATE,
	MA_MAIN_RESET_GAME,
	MA_MAIN_LOAD_ROM,
	MA_MAIN_OPTIONS,
	MA_MAIN_CONTROLS,
	MA_MAIN_CREDITS,
	MA_MAIN_PATCHES,
	MA_MAIN_EXIT,
	MA_OPT_RENDERER,
	MA_OPT_SCALING,
	MA_OPT_ACC_TIMING,
	MA_OPT_ACC_SPRITES,
	MA_OPT_SHOW_FPS,
	MA_OPT_FRAMESKIP,
	MA_OPT_ENABLE_SOUND,
	MA_OPT_SOUND_QUALITY,
	MA_OPT_ARM940_SOUND,
	MA_OPT_6BUTTON_PAD,
	MA_OPT_REGION,
	MA_OPT_SRAM_STATES,
	MA_OPT_CONFIRM_STATES,
	MA_OPT_SAVE_SLOT,
	MA_OPT_CPU_CLOCKS,
	MA_OPT_SCD_OPTS,
	MA_OPT_ADV_OPTS,
	MA_OPT_SAVECFG,
	MA_OPT_SAVECFG_GAME,
	MA_OPT_LOADCFG,
	MA_OPT2_GAMMA,
	MA_OPT2_A_SN_GAMMA,
	MA_OPT2_VSYNC,
	MA_OPT2_ENABLE_Z80,
	MA_OPT2_ENABLE_YM2612,
	MA_OPT2_ENABLE_SN76496,
	MA_OPT2_GZIP_STATES,
	MA_OPT2_NO_LAST_ROM,
	MA_OPT2_RAMTIMINGS,
	MA_OPT2_SQUIDGEHACK,
	MA_OPT2_DONE,
	MA_CDOPT_TESTBIOS_USA,
	MA_CDOPT_TESTBIOS_EUR,
	MA_CDOPT_TESTBIOS_JAP,
	MA_CDOPT_LEDS,
	MA_CDOPT_CDDA,
	MA_CDOPT_PCM,
	MA_CDOPT_READAHEAD,
	MA_CDOPT_SAVERAM,
	MA_CDOPT_SCALEROT_CHIP,
	MA_CDOPT_BETTER_SYNC,
	MA_CDOPT_DONE,
} menu_id;

typedef struct
{
	char *name;
	menu_behavior beh;
	menu_id id;
	void *var;		/* for on-off settings */
	int mask;
	signed char min;	/* for ranged integer settings, to be sign-extended */
	signed char max;
	char enabled;
} menu_entry;

static int me_id2offset(const menu_entry *entries, int count, menu_id id)
{
	int i;
	for (i = 0; i < count; i++)
	{
		if (entries[i].id == id) return i;
	}

	printf("%s: id %i not found\n", __FUNCTION__, id);
	return 0;
}

static void me_enable(menu_entry *entries, int count, menu_id id, int enable)
{
	int i = me_id2offset(entries, count, id);
	entries[i].enabled = enable;
}

static int me_count_enabled(const menu_entry *entries, int count)
{
	int i, ret = 0;

	for (i = 0; i < count; i++)
	{
		if (entries[i].enabled) ret++;
	}

	return ret;
}

static menu_id me_index2id(const menu_entry *entries, int count, int index)
{
	int i;

	for (i = 0; i < count; i++)
	{
		if (entries[i].enabled)
		{
			if (index == 0) break;
			index--;
		}
	}
	if (i >= count) i = count - 1;
	return entries[i].id;
}

typedef void (me_draw_custom_f)(const menu_entry *entry, int x, int y, void *param);

static void me_draw(const menu_entry *entries, int count, int x, int y, me_draw_custom_f *cust_draw, void *param)
{
	int i, y1 = y;

	for (i = 0; i < count; i++)
	{
		if (!entries[i].enabled) continue;
		if (entries[i].name == NULL)
		{
			if (cust_draw != NULL)
				cust_draw(&entries[i], x, y1, param);
			y1 += 10;
			continue;
		}
		gp2x_text_out8(x, y1, entries[i].name);
		if (entries[i].beh == MB_ONOFF)
			gp2x_text_out8(x + 27*8, y1, (*(int *)entries[i].var & entries[i].mask) ? "ON" : "OFF");
		else if (entries[i].beh == MB_RANGE)
			gp2x_text_out8(x + 27*8, y1, "%i", *(int *)entries[i].var);
		y1 += 10;
	}
}

static int me_process(menu_entry *entries, int count, menu_id id, int is_next)
{
	int i = me_id2offset(entries, count, id);
	menu_entry *entry = &entries[i];
	switch (entry->beh)
	{
		case MB_ONOFF:
			*(int *)entry->var ^= entry->mask;
			return 1;
		case MB_RANGE:
			*(int *)entry->var += is_next ? 1 : -1;
			if (*(int *)entry->var < (int)entry->min) *(int *)entry->var = (int)entry->min;
			if (*(int *)entry->var > (int)entry->max) *(int *)entry->var = (int)entry->max;
			return 1;
		default:
			return 0;
	}
}




static unsigned long inp_prev = 0;
static int inp_prevjoy = 0;

static unsigned long wait_for_input(unsigned long interesting)
{
	unsigned long ret;
	static int repeats = 0, wait = 50*1000;
	int release = 0, i;

	if (repeats == 2 || repeats == 4) wait /= 2;
	if (repeats == 6) wait = 15 * 1000;

	for (i = 0; i < 6 && inp_prev == gp2x_joystick_read(1); i++) {
		if (i == 0) repeats++;
		if (wait >= 30*1000) usleep(wait); // usleep sleeps for ~30ms minimum
		else spend_cycles(wait * currentConfig.CPUclock);
	}

	while ( !((ret = gp2x_joystick_read(1)) & interesting) ) {
		usleep(50000);
		release = 1;
	}

	if (release || ret != inp_prev) {
		repeats = 0;
		wait = 50*1000;
	}
	inp_prev = ret;
	inp_prevjoy = 0;

	// we don't need diagonals in menus
	if ((ret&GP2X_UP)   && (ret&GP2X_LEFT))  ret &= ~GP2X_LEFT;
	if ((ret&GP2X_UP)   && (ret&GP2X_RIGHT)) ret &= ~GP2X_RIGHT;
	if ((ret&GP2X_DOWN) && (ret&GP2X_LEFT))  ret &= ~GP2X_LEFT;
	if ((ret&GP2X_DOWN) && (ret&GP2X_RIGHT)) ret &= ~GP2X_RIGHT;

	return ret;
}

static unsigned long input2_read(unsigned long interesting, int *joy)
{
	unsigned long ret;
	int i;

	do
	{
		*joy = 0;
		if ((ret = gp2x_joystick_read(0) & interesting)) break;
		gp2x_usbjoy_update();
		for (i = 0; i < num_of_joys; i++) {
			ret = gp2x_usbjoy_check2(i);
			if (ret) { *joy = i + 1; break; }
		}
		if (ret) break;
	}
	while(0);

	return ret;
}

// similar to wait_for_input(), but returns joy num
static unsigned long wait_for_input_usbjoy(unsigned long interesting, int *joy)
{
	unsigned long ret;
	const int wait = 300*1000;
	int i;

	if (inp_prevjoy == 0) inp_prev &= interesting;
	for (i = 0; i < 6; i++) {
		ret = input2_read(interesting, joy);
		if (*joy != inp_prevjoy || ret != inp_prev) break;
		usleep(wait/6);
	}

	while ( !(ret = input2_read(interesting, joy)) ) {
		usleep(50000);
	}

	inp_prev = ret;
	inp_prevjoy = *joy;

	return ret;
}



// --------- loading ROM screen ----------

static void load_progress_cb(int percent)
{
	int ln, len = percent * 320 / 100;
	unsigned char *dst = (unsigned char *)gp2x_screen + 320*20;

	if (len > 320) len = 320;
	for (ln = 10; ln > 0; ln--, dst += 320)
		memset(dst, 0xf0, len);
	gp2x_video_flip2();
}

void menu_romload_prepare(const char *rom_name)
{
	const char *p = rom_name + strlen(rom_name);
	while (p > rom_name && *p != '/') p--;

	gp2x_pd_clone_buffer2();
	gp2x_smalltext8(1, 1, "Loading");
	gp2x_smalltext8_lim(1, 10, p, 53);
	gp2x_memcpy_buffers(3, gp2x_screen, 0, 320*240);
	gp2x_video_flip2();
	PicoCartLoadProgressCB = load_progress_cb;
}

void menu_romload_end(void)
{
	PicoCartLoadProgressCB = NULL;
	gp2x_smalltext8(1, 30, "Starting emulation...");
	gp2x_video_flip2();
}

// -------------- ROM selector --------------

static void draw_dirlist(char *curdir, struct dirent **namelist, int n, int sel)
{
	int start, i, pos;

	start = 12 - sel;
	n--; // exclude current dir (".")

	gp2x_pd_clone_buffer2();

	if(start - 2 >= 0)
		gp2x_smalltext8_lim(14, (start - 2)*10, curdir, 53-2);
	for (i = 0; i < n; i++) {
		pos = start + i;
		if (pos < 0)  continue;
		if (pos > 23) break;
		if (namelist[i+1]->d_type == DT_DIR) {
			gp2x_smalltext8_lim(14,   pos*10, "/", 1);
			gp2x_smalltext8_lim(14+6, pos*10, namelist[i+1]->d_name, 53-3);
		} else {
			gp2x_smalltext8_lim(14,   pos*10, namelist[i+1]->d_name, 53-2);
		}
	}
	gp2x_text_out8(5, 120, ">");
	gp2x_video_flip2();
}

static int scandir_cmp(const void *p1, const void *p2)
{
	struct dirent **d1 = (struct dirent **)p1, **d2 = (struct dirent **)p2;
	if ((*d1)->d_type == (*d2)->d_type) return alphasort(d1, d2);
	if ((*d1)->d_type == DT_DIR) return -1; // put before
	if ((*d2)->d_type == DT_DIR) return  1;
	return alphasort(d1, d2);
}

static char *filter_exts[] = {
	".mp3", ".MP3", ".srm", ".brm", "s.gz", ".mds",	"bcfg", ".txt", ".htm", "html",
	".jpg", ".gpe", ".cue"
};

static int scandir_filter(const struct dirent *ent)
{
	const char *p;
	int i;

	if (ent == NULL || ent->d_name == NULL) return 0;
	if (strlen(ent->d_name) < 5) return 1;

	p = ent->d_name + strlen(ent->d_name) - 4;

	for (i = 0; i < sizeof(filter_exts)/sizeof(filter_exts[0]); i++)
	{
		if (strcmp(p, filter_exts[i]) == 0) return 0;
	}

	return 1;
}

static char *romsel_loop(char *curr_path)
{
	struct dirent **namelist;
	DIR *dir;
	int n, sel = 0;
	unsigned long inp = 0;
	char *ret = NULL, *fname = NULL;

	// is this a dir or a full path?
	if ((dir = opendir(curr_path))) {
		closedir(dir);
	} else {
		char *p;
		for (p = curr_path + strlen(curr_path) - 1; p > curr_path && *p != '/'; p--);
		*p = 0;
		fname = p+1;
	}

	n = scandir(curr_path, &namelist, scandir_filter, scandir_cmp);
	if (n < 0) {
		// try root
		n = scandir("/", &namelist, scandir_filter, scandir_cmp);
		if (n < 0) {
			// oops, we failed
			printf("dir: "); printf(curr_path); printf("\n");
			perror("scandir");
			return NULL;
		}
	}

	// try to find sel
	if (fname != NULL) {
		int i;
		for (i = 1; i < n; i++) {
			if (strcmp(namelist[i]->d_name, fname) == 0) {
				sel = i - 1;
				break;
			}
		}
	}

	for (;;)
	{
		draw_dirlist(curr_path, namelist, n, sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_LEFT|GP2X_RIGHT|GP2X_L|GP2X_R|GP2X_B|GP2X_X);
		if(inp & GP2X_UP  )  { sel--;   if (sel < 0)   sel = n-2; }
		if(inp & GP2X_DOWN)  { sel++;   if (sel > n-2) sel = 0; }
		if(inp & GP2X_LEFT)  { sel-=10; if (sel < 0)   sel = 0; }
		if(inp & GP2X_L)     { sel-=24; if (sel < 0)   sel = 0; }
		if(inp & GP2X_RIGHT) { sel+=10; if (sel > n-2) sel = n-2; }
		if(inp & GP2X_R)     { sel+=24; if (sel > n-2) sel = n-2; }
		if(inp & GP2X_B)     { // enter dir/select
			again:
			if (namelist[sel+1]->d_type == DT_REG) {
				strcpy(romFileName, curr_path);
				strcat(romFileName, "/");
				strcat(romFileName, namelist[sel+1]->d_name);
				ret = romFileName;
				break;
			} else if (namelist[sel+1]->d_type == DT_DIR) {
				int newlen = strlen(curr_path) + strlen(namelist[sel+1]->d_name) + 2;
				char *p, *newdir = malloc(newlen);
				if (strcmp(namelist[sel+1]->d_name, "..") == 0) {
					char *start = curr_path;
					p = start + strlen(start) - 1;
					while (*p == '/' && p > start) p--;
					while (*p != '/' && p > start) p--;
					if (p <= start) strcpy(newdir, "/");
					else { strncpy(newdir, start, p-start); newdir[p-start] = 0; }
				} else {
					strcpy(newdir, curr_path);
					p = newdir + strlen(newdir) - 1;
					while (*p == '/' && p >= newdir) *p-- = 0;
					strcat(newdir, "/");
					strcat(newdir, namelist[sel+1]->d_name);
				}
				ret = romsel_loop(newdir);
				free(newdir);
				break;
			} else {
				// unknown file type, happens on NTFS mounts. Try to guess.
				FILE *tstf; int tmp;
				strcpy(romFileName, curr_path);
				strcat(romFileName, "/");
				strcat(romFileName, namelist[sel+1]->d_name);
				tstf = fopen(romFileName, "rb");
				if (tstf != NULL)
				{
					if (fread(&tmp, 1, 1, tstf) > 0 || ferror(tstf) == 0)
						namelist[sel+1]->d_type = DT_REG;
					else	namelist[sel+1]->d_type = DT_DIR;
					fclose(tstf);
					goto again;
				}
			}
		}
		if(inp & GP2X_X) break; // cancel
	}

	if (n > 0) {
		while(n--) free(namelist[n]);
		free(namelist);
	}

	return ret;
}

// ------------ debug menu ------------

char *debugString(void);

static void draw_debug(void)
{
	char *p, *str = debugString();
	int len, line;

	gp2x_pd_clone_buffer2();

	p = str;
	for (line = 0; line < 24; line++)
	{
		while (*p && *p != '\n') p++;
		len = p - str;
		if (len > 55) len = 55;
		gp2x_smalltext8_lim(1, line*10, str, len);
		if (*p == 0) break;
		p++; str = p;
	}
	gp2x_video_flip2();
}

static void debug_menu_loop(void)
{
	draw_debug();
	wait_for_input(GP2X_B|GP2X_X);
}

// ------------ patch/gg menu ------------

static void draw_patchlist(int sel)
{
	int start, i, pos;

	start = 12 - sel;

	gp2x_pd_clone_buffer2();

	for (i = 0; i < PicoPatchCount; i++) {
		pos = start + i;
		if (pos < 0)  continue;
		if (pos > 23) break;
		gp2x_smalltext8_lim(14,     pos*10, PicoPatches[i].active ? "ON " : "OFF", 3);
		gp2x_smalltext8_lim(14+6*4, pos*10, PicoPatches[i].name, 53-6);
	}
	pos = start + i;
	if (pos < 24) gp2x_smalltext8_lim(14, pos*10, "done", 4);

	gp2x_text_out8(5, 120, ">");
	gp2x_video_flip2();
}


static void patches_menu_loop(void)
{
	int menu_sel = 0;
	unsigned long inp = 0;

	for(;;)
	{
		draw_patchlist(menu_sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_LEFT|GP2X_RIGHT|GP2X_L|GP2X_R|GP2X_B|GP2X_X);
		if(inp & GP2X_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = PicoPatchCount; }
		if(inp & GP2X_DOWN) { menu_sel++; if (menu_sel > PicoPatchCount) menu_sel = 0; }
		if(inp &(GP2X_LEFT|GP2X_L))  { menu_sel-=10; if (menu_sel < 0) menu_sel = 0; }
		if(inp &(GP2X_RIGHT|GP2X_R)) { menu_sel+=10; if (menu_sel > PicoPatchCount) menu_sel = PicoPatchCount; }
		if(inp & GP2X_B) { // action
			if (menu_sel < PicoPatchCount)
				PicoPatches[menu_sel].active = !PicoPatches[menu_sel].active;
			else 	return;
		}
		if(inp & GP2X_X) return;
	}

}

// ------------ savestate loader ------------

static void menu_prepare_bg(void);

static int state_slot_flags = 0;

static void state_check_slots(void)
{
	int slot;

	state_slot_flags = 0;

	for (slot = 0; slot < 10; slot++)
	{
		if (emu_check_save_file(slot))
		{
			state_slot_flags |= 1 << slot;
		}
	}
}

static void draw_savestate_bg(int slot)
{
	struct PicoVideo tmp_pv;
	unsigned short tmp_cram[0x40];
	unsigned short tmp_vsram[0x40];
	void *tmp_vram, *file;
	char *fname;

	fname = emu_GetSaveFName(1, 0, slot);
	if (!fname) return;

	tmp_vram = malloc(sizeof(Pico.vram));
	if (tmp_vram == NULL) return;

	memcpy(tmp_vram, Pico.vram, sizeof(Pico.vram));
	memcpy(tmp_cram, Pico.cram, sizeof(Pico.cram));
	memcpy(tmp_vsram, Pico.vsram, sizeof(Pico.vsram));
	memcpy(&tmp_pv, &Pico.video, sizeof(Pico.video));

	if (strcmp(fname + strlen(fname) - 3, ".gz") == 0) {
		file = gzopen(fname, "rb");
		emu_set_save_cbs(1);
	} else {
		file = fopen(fname, "rb");
		emu_set_save_cbs(0);
	}

	if (file) {
		if (PicoMCD & 1) {
			PicoCdLoadStateGfx(file);
		} else {
			areaSeek(file, 0x10020, SEEK_SET);  // skip header and RAM in state file
			areaRead(Pico.vram, 1, sizeof(Pico.vram), file);
			areaSeek(file, 0x2000, SEEK_CUR);
			areaRead(Pico.cram, 1, sizeof(Pico.cram), file);
			areaRead(Pico.vsram, 1, sizeof(Pico.vsram), file);
			areaSeek(file, 0x221a0, SEEK_SET);
			areaRead(&Pico.video, 1, sizeof(Pico.video), file);
		}
		areaClose(file);
	}

	emu_forced_frame();
	gp2x_memcpy_buffers((1<<2), gp2x_screen, 0, 320*240*2);
	menu_prepare_bg();

	memcpy(Pico.vram, tmp_vram, sizeof(Pico.vram));
	memcpy(Pico.cram, tmp_cram, sizeof(Pico.cram));
	memcpy(Pico.vsram, tmp_vsram, sizeof(Pico.vsram));
	memcpy(&Pico.video, &tmp_pv,  sizeof(Pico.video));
	free(tmp_vram);
}

static void draw_savestate_menu(int menu_sel, int is_loading)
{
	int tl_x = 25, tl_y = 60, y, i;

	if (state_slot_flags & (1 << menu_sel))
		draw_savestate_bg(menu_sel);
	gp2x_pd_clone_buffer2();

	gp2x_text_out8(tl_x, 30, is_loading ? "Load state" : "Save state");

	/* draw all 10 slots */
	y = tl_y;
	for (i = 0; i < 10; i++, y+=10)
	{
		gp2x_text_out8(tl_x, y, "SLOT %i (%s)", i, (state_slot_flags & (1 << i)) ? "USED" : "free");
	}
	gp2x_text_out8(tl_x, y, "back");

	// draw cursor
	gp2x_text_out8(tl_x - 16, tl_y + menu_sel*10, ">");

	gp2x_video_flip2();
}

static int savestate_menu_loop(int is_loading)
{
	static int menu_sel = 10;
	int menu_sel_max = 10;
	unsigned long inp = 0;

	state_check_slots();

	for(;;)
	{
		draw_savestate_menu(menu_sel, is_loading);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_B|GP2X_X);
		if(inp & GP2X_UP  ) {
			do {
				menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max;
			} while (!(state_slot_flags & (1 << menu_sel)) && menu_sel != menu_sel_max && is_loading);
		}
		if(inp & GP2X_DOWN) {
			do {
				menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0;
			} while (!(state_slot_flags & (1 << menu_sel)) && menu_sel != menu_sel_max && is_loading);
		}
		if(inp & GP2X_B) { // save/load
			if (menu_sel < 10) {
				state_slot = menu_sel;
				if (emu_SaveLoadGame(is_loading, 0)) {
					strcpy(menuErrorMsg, is_loading ? "Load failed" : "Save failed");
					return 1;
				}
				return 0;
			} else	return 1;
		}
		if(inp & GP2X_X) return 1;
	}
}

// -------------- key config --------------

static char *usb_joy_key_name(int joy, int num)
{
	static char name[16];
	switch (num)
	{
		case 0: sprintf(name, "Joy%i UP", joy); break;
		case 1: sprintf(name, "Joy%i DOWN", joy); break;
		case 2: sprintf(name, "Joy%i LEFT", joy); break;
		case 3: sprintf(name, "Joy%i RIGHT", joy); break;
		default:sprintf(name, "Joy%i b%i", joy, num-3); break;
	}
	return name;
}

static char *action_binds(int player_idx, int action_mask)
{
	static char strkeys[32*5];
	int joy, i;

	strkeys[0] = 0;
	for (i = 0; i < 32; i++) // i is key index
	{
		if (currentConfig.KeyBinds[i] & action_mask)
		{
			if (player_idx >= 0 && ((currentConfig.KeyBinds[i] >> 16) & 3) != player_idx) continue;
			if (strkeys[0]) { strcat(strkeys, " + "); strcat(strkeys, gp2xKeyNames[i]); break; }
			else strcpy(strkeys, gp2xKeyNames[i]);
		}
	}
	for (joy = 0; joy < num_of_joys; joy++)
	{
		for (i = 0; i < 32; i++)
		{
			if (currentConfig.JoyBinds[joy][i] & action_mask)
			{
				if (player_idx >= 0 && ((currentConfig.JoyBinds[joy][i] >> 16) & 3) != player_idx) continue;
				if (strkeys[0]) {
					strcat(strkeys, ", "); strcat(strkeys, usb_joy_key_name(joy + 1, i));
					break;
				}
				else strcpy(strkeys, usb_joy_key_name(joy + 1, i));
			}
		}
	}

	return strkeys;
}

static void unbind_action(int action)
{
	int i, u;

	for (i = 0; i < 32; i++)
		currentConfig.KeyBinds[i] &= ~action;
	for (u = 0; u < 4; u++)
		for (i = 0; i < 32; i++)
			currentConfig.JoyBinds[u][i] &= ~action;
}

static int count_bound_keys(int action, int joy)
{
	int i, keys = 0;

	if (joy)
	{
		for (i = 0; i < 32; i++)
			if (currentConfig.JoyBinds[joy-1][i] & action) keys++;
	}
	else
	{
		for (i = 0; i < 32; i++)
			if (currentConfig.KeyBinds[i] & action) keys++;
	}
	return keys;
}

typedef struct { char *name; int mask; } bind_action_t;

static void draw_key_config(const bind_action_t *opts, int opt_cnt, int player_idx, int sel)
{
	int x, y, tl_y = 40, i;

	gp2x_pd_clone_buffer2();
	if (player_idx >= 0) {
		gp2x_text_out8(80, 20, "Player %i controls", player_idx + 1);
		x = 100;
	} else {
		gp2x_text_out8(80, 20, "Emulator controls");
		x = 40;
	}

	y = tl_y;
	for (i = 0; i < opt_cnt; i++, y+=10)
		gp2x_text_out8(x, y, "%s : %s", opts[i].name, action_binds(player_idx, opts[i].mask));

	gp2x_text_out8(x, y, "Done");

	// draw cursor
	gp2x_text_out8(x - 16, tl_y + sel*10, ">");

	if (sel < opt_cnt) {
		gp2x_text_out8(30, 190, "Press a button to bind/unbind");
		gp2x_text_out8(30, 200, "Use SELECT to clear");
		gp2x_text_out8(30, 210, "To bind UP/DOWN, hold SELECT");
		gp2x_text_out8(30, 220, "Select \"Done\" to exit");
	} else {
		gp2x_text_out8(30, 200, "Use Options -> Save cfg");
		gp2x_text_out8(30, 210, "to save controls");
		gp2x_text_out8(30, 220, "Press B or X to exit");
	}
	gp2x_video_flip2();
}

static void key_config_loop(const bind_action_t *opts, int opt_cnt, int player_idx)
{
	int joy = 0, sel = 0, menu_sel_max = opt_cnt, prev_select = 0, i;
	unsigned long inp = 0;

	for (;;)
	{
		draw_key_config(opts, opt_cnt, player_idx, sel);
		inp = wait_for_input_usbjoy(CONFIGURABLE_KEYS, &joy);
		// printf("got %08lX from joy %i\n", inp, joy);
		if (joy == 0) {
			if (!(inp & GP2X_SELECT)) {
				prev_select = 0;
				if(inp & GP2X_UP  ) { sel--; if (sel < 0) sel = menu_sel_max; continue; }
				if(inp & GP2X_DOWN) { sel++; if (sel > menu_sel_max) sel = 0; continue; }
			}
			if (sel >= opt_cnt) {
				if (inp & (GP2X_B|GP2X_X)) break;
				else continue;
			}
			// if we are here, we want to bind/unbind something
			if ((inp & GP2X_SELECT) && !prev_select)
				unbind_action(opts[sel].mask);
			prev_select = inp & GP2X_SELECT;
			inp &= CONFIGURABLE_KEYS;
			inp &= ~GP2X_SELECT;
			for (i = 0; i < 32; i++)
				if (inp & (1 << i)) {
					if (count_bound_keys(opts[sel].mask, 0) >= 2)
					     currentConfig.KeyBinds[i] &= ~opts[sel].mask; // allow to unbind only
					else currentConfig.KeyBinds[i] ^=  opts[sel].mask;
					if (player_idx >= 0) {
						currentConfig.KeyBinds[i] &= ~(3 << 16);
						currentConfig.KeyBinds[i] |= player_idx << 16;
					}
				}
		}
		else if (sel < opt_cnt)
		{
			for (i = 0; i < 32; i++)
				if (inp & (1 << i)) {
					if (count_bound_keys(opts[sel].mask, joy) >= 1) // disallow combos for usbjoy
					     currentConfig.JoyBinds[joy-1][i] &= ~opts[sel].mask;
					else currentConfig.JoyBinds[joy-1][i] ^=  opts[sel].mask;
					if (player_idx >= 0) {
						currentConfig.JoyBinds[joy-1][i] &= ~(3 << 16);
						currentConfig.JoyBinds[joy-1][i] |= player_idx << 16;
					}
				}
		}
	}
}

static void draw_kc_sel(int menu_sel)
{
	int tl_x = 25+40, tl_y = 60, y, i;
	char joyname[36];

	y = tl_y;
	gp2x_pd_clone_buffer2();
	gp2x_text_out8(tl_x, y,       "Player 1");
	gp2x_text_out8(tl_x, (y+=10), "Player 2");
	gp2x_text_out8(tl_x, (y+=10), "Emulator controls");
	gp2x_text_out8(tl_x, (y+=10), "Done");

	// draw cursor
	gp2x_text_out8(tl_x - 16, tl_y + menu_sel*10, ">");

	tl_x = 25;
	gp2x_text_out8(tl_x, (y=110), "USB joys detected:");
	if (num_of_joys > 0) {
		for (i = 0; i < num_of_joys; i++) {
			strncpy(joyname, joy_name(joys[i]), 33); joyname[33] = 0;
			gp2x_text_out8(tl_x, (y+=10), "%i: %s", i+1, joyname);
		}
	} else {
		gp2x_text_out8(tl_x, (y+=10), "none");
	}

	gp2x_video_flip2();
}


// PicoPad[] format: MXYZ SACB RLDU
static bind_action_t ctrl_actions[] =
{
	{ "UP     ", 0x001 },
	{ "DOWN   ", 0x002 },
	{ "LEFT   ", 0x004 },
	{ "RIGHT  ", 0x008 },
	{ "A      ", 0x040 },
	{ "B      ", 0x010 },
	{ "C      ", 0x020 },
	{ "START  ", 0x080 },
	{ "MODE   ", 0x800 },
	{ "X      ", 0x400 },
	{ "Y      ", 0x200 },
	{ "Z      ", 0x100 },
};

// player2_flag, ?, ?, ?, ?, ?, ?, menu
// "NEXT SAVE SLOT", "PREV SAVE SLOT", "SWITCH RENDERER", "SAVE STATE",
// "LOAD STATE", "VOLUME UP", "VOLUME DOWN", "DONE"
static bind_action_t emuctrl_actions[] =
{
	{ "Volume Down    ", 1<<30 },
	{ "Volume Up      ", 1<<29 },
	{ "Load State     ", 1<<28 },
	{ "Save State     ", 1<<27 },
	{ "Switch Renderer", 1<<26 },
	{ "Prev Save Slot ", 1<<25 },
	{ "Next Save Slot ", 1<<24 },
	{ "Enter Menu     ", 1<<23 },
};

static void kc_sel_loop(void)
{
	int menu_sel = 3, menu_sel_max = 3;
	unsigned long inp = 0;
	int is_6button = currentConfig.PicoOpt & 0x020;

	while (1)
	{
		draw_kc_sel(menu_sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_B|GP2X_X);
		if (inp & GP2X_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if (inp & GP2X_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		if (inp & GP2X_B) {
			switch (menu_sel) {
				case 0: key_config_loop(ctrl_actions, is_6button ? 12 : 8, 0); return;
				case 1: key_config_loop(ctrl_actions, is_6button ? 12 : 8, 1); return;
				case 2: key_config_loop(emuctrl_actions,
						sizeof(emuctrl_actions)/sizeof(emuctrl_actions[0]), -1); return;
				case 3: if (rom_data == NULL) emu_WriteConfig(0); return;
				default: return;
			}
		}
		if (inp & GP2X_X) return;
	}
}


// --------- sega/mega cd options ----------

menu_entry cdopt_entries[] =
{
	{ NULL,                        MB_NONE,  MA_CDOPT_TESTBIOS_USA, NULL, 0, 0, 0, 1 },
	{ NULL,                        MB_NONE,  MA_CDOPT_TESTBIOS_EUR, NULL, 0, 0, 0, 1 },
	{ NULL,                        MB_NONE,  MA_CDOPT_TESTBIOS_JAP, NULL, 0, 0, 0, 1 },
	{ "CD LEDs",                   MB_ONOFF, MA_CDOPT_LEDS,         &currentConfig.EmuOpt,  0x0400, 0, 0, 1 },
	{ "CDDA audio (using mp3s)",   MB_ONOFF, MA_CDOPT_CDDA,         &currentConfig.PicoOpt, 0x0800, 0, 0, 1 },
	{ "PCM audio",                 MB_ONOFF, MA_CDOPT_PCM,          &currentConfig.PicoOpt, 0x0400, 0, 0, 1 },
	{ NULL,                        MB_NONE,  MA_CDOPT_READAHEAD,    NULL, 0, 0, 0, 1 },
	{ "SaveRAM cart",              MB_ONOFF, MA_CDOPT_SAVERAM,      &currentConfig.PicoOpt, 0x8000, 0, 0, 1 },
	{ "Scale/Rot. fx (slow)",      MB_ONOFF, MA_CDOPT_SCALEROT_CHIP,&currentConfig.PicoOpt, 0x1000, 0, 0, 1 },
	{ "Better sync (slow)",        MB_ONOFF, MA_CDOPT_BETTER_SYNC,  &currentConfig.PicoOpt, 0x2000, 0, 0, 1 },
	{ "done",                      MB_NONE,  MA_CDOPT_DONE,         NULL, 0, 0, 0, 1 },
};

#define CDOPT_ENTRY_COUNT (sizeof(cdopt_entries) / sizeof(cdopt_entries[0]))


struct bios_names_t
{
	char us[32], eu[32], jp[32];
};

static void menu_cdopt_cust_draw(const menu_entry *entry, int x, int y, void *param)
{
	struct bios_names_t *bios_names = param;
	char ra_buff[16];

	switch (entry->id)
	{
		case MA_CDOPT_TESTBIOS_USA: gp2x_text_out8(x, y, "USA BIOS:     %s", bios_names->us); break;
		case MA_CDOPT_TESTBIOS_EUR: gp2x_text_out8(x, y, "EUR BIOS:     %s", bios_names->eu); break;
		case MA_CDOPT_TESTBIOS_JAP: gp2x_text_out8(x, y, "JAP BIOS:     %s", bios_names->jp); break;
		case MA_CDOPT_READAHEAD:
			if (PicoCDBuffers > 1) sprintf(ra_buff, "%5iK", PicoCDBuffers * 2);
			else strcpy(ra_buff, "     OFF");
			gp2x_text_out8(x, y, "ReadAhead buffer      %s", ra_buff);
			break;
		default:break;
	}
}

static void draw_cd_menu_options(int menu_sel, struct bios_names_t *bios_names)
{
	int tl_x = 25, tl_y = 60;
	menu_id selected_id;
	char ra_buff[16];

	if (PicoCDBuffers > 1) sprintf(ra_buff, "%5iK", PicoCDBuffers * 2);
	else strcpy(ra_buff, "     OFF");

	gp2x_pd_clone_buffer2();

	me_draw(cdopt_entries, CDOPT_ENTRY_COUNT, tl_x, tl_y, menu_cdopt_cust_draw, bios_names);

	// draw cursor
	gp2x_text_out8(tl_x - 16, tl_y + menu_sel*10, ">");

	selected_id = me_index2id(cdopt_entries, CDOPT_ENTRY_COUNT, menu_sel);
	if ((selected_id == MA_CDOPT_TESTBIOS_USA && strcmp(bios_names->us, "NOT FOUND")) ||
		(selected_id == MA_CDOPT_TESTBIOS_EUR && strcmp(bios_names->eu, "NOT FOUND")) ||
		(selected_id == MA_CDOPT_TESTBIOS_JAP && strcmp(bios_names->jp, "NOT FOUND")))
			gp2x_text_out8(tl_x, 220, "Press start to test selected BIOS");

	gp2x_video_flip2();
}

static void cd_menu_loop_options(void)
{
	static int menu_sel = 0;
	int menu_sel_max = 10;
	unsigned long inp = 0;
	struct bios_names_t bios_names;
	menu_id selected_id;
	char *bios, *p;

	if (find_bios(4, &bios)) { // US
		for (p = bios+strlen(bios)-1; p > bios && *p != '/'; p--); p++;
		strncpy(bios_names.us, p, sizeof(bios_names.us)); bios_names.us[sizeof(bios_names.us)-1] = 0;
	} else	strcpy(bios_names.us, "NOT FOUND");

	if (find_bios(8, &bios)) { // EU
		for (p = bios+strlen(bios)-1; p > bios && *p != '/'; p--); p++;
		strncpy(bios_names.eu, p, sizeof(bios_names.eu)); bios_names.eu[sizeof(bios_names.eu)-1] = 0;
	} else	strcpy(bios_names.eu, "NOT FOUND");

	if (find_bios(1, &bios)) { // JP
		for (p = bios+strlen(bios)-1; p > bios && *p != '/'; p--); p++;
		strncpy(bios_names.jp, p, sizeof(bios_names.jp)); bios_names.jp[sizeof(bios_names.jp)-1] = 0;
	} else	strcpy(bios_names.jp, "NOT FOUND");

	for(;;)
	{
		draw_cd_menu_options(menu_sel, &bios_names);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_LEFT|GP2X_RIGHT|GP2X_B|GP2X_X|GP2X_A|GP2X_START);
		if (inp & GP2X_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if (inp & GP2X_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		selected_id = me_index2id(cdopt_entries, CDOPT_ENTRY_COUNT, menu_sel);
		if (inp & (GP2X_LEFT|GP2X_RIGHT)) { // multi choise
			if (!me_process(cdopt_entries, CDOPT_ENTRY_COUNT, selected_id, (inp&GP2X_RIGHT) ? 1 : 0) &&
			    selected_id == MA_CDOPT_READAHEAD) {
				if (inp & GP2X_LEFT) {
					PicoCDBuffers >>= 1;
					if (PicoCDBuffers < 64) PicoCDBuffers = 0;
				} else {
					if (PicoCDBuffers < 64) PicoCDBuffers = 64;
					else PicoCDBuffers <<= 1;
					if (PicoCDBuffers > 8*1024) PicoCDBuffers = 8*1024; // 16M
				}
			}
		}
		if (inp & GP2X_B) { // toggleable options
			if (!me_process(cdopt_entries, CDOPT_ENTRY_COUNT, selected_id, 1) &&
			    selected_id == MA_CDOPT_DONE) {
				return;
			}
		}
		if (inp & GP2X_START) { // BIOS testers
			switch (selected_id) {
				case MA_CDOPT_TESTBIOS_USA:
					if (find_bios(4, &bios)) { // test US
						strcpy(romFileName, bios);
						engineState = PGS_ReloadRom;
						return;
					}
					break;
				case MA_CDOPT_TESTBIOS_EUR:
					if (find_bios(8, &bios)) { // test EU
						strcpy(romFileName, bios);
						engineState = PGS_ReloadRom;
						return;
					}
					break;
				case MA_CDOPT_TESTBIOS_JAP:
					if (find_bios(1, &bios)) { // test JP
						strcpy(romFileName, bios);
						engineState = PGS_ReloadRom;
						return;
					}
					break;
				default:
					break;
			}
		}
		if (inp & (GP2X_X|GP2X_A)) return;
	}
}


// --------- advanced options ----------

menu_entry opt2_entries[] =
{
	{ NULL,                        MB_NONE,  MA_OPT2_GAMMA,         NULL, 0, 0, 0, 1 },
	{ "A_SN's gamma curve",        MB_ONOFF, MA_OPT2_A_SN_GAMMA,    &currentConfig.EmuOpt, 0x1000, 0, 0, 1 },
	{ "Perfecf vsync",             MB_ONOFF, MA_OPT2_VSYNC,         &currentConfig.EmuOpt, 0x2000, 0, 0, 1 },
	{ "Emulate Z80",               MB_ONOFF, MA_OPT2_ENABLE_Z80,    &currentConfig.PicoOpt,0x0004, 0, 0, 1 },
	{ "Emulate YM2612 (FM)",       MB_ONOFF, MA_OPT2_ENABLE_YM2612, &currentConfig.PicoOpt,0x0001, 0, 0, 1 },
	{ "Emulate SN76496 (PSG)",     MB_ONOFF, MA_OPT2_ENABLE_SN76496,&currentConfig.PicoOpt,0x0002, 0, 0, 1 },
	{ "gzip savestates",           MB_ONOFF, MA_OPT2_GZIP_STATES,   &currentConfig.EmuOpt, 0x0008, 0, 0, 1 },
	{ "Don't save last used ROM",  MB_ONOFF, MA_OPT2_NO_LAST_ROM,   &currentConfig.EmuOpt, 0x0020, 0, 0, 1 },
	{ "needs restart:",            MB_NONE,  MA_NONE,               NULL, 0, 0, 0, 1 },
	{ "craigix's RAM timings",     MB_ONOFF, MA_OPT2_RAMTIMINGS,    &currentConfig.EmuOpt, 0x0100, 0, 0, 1 },
	{ NULL,                        MB_ONOFF, MA_OPT2_SQUIDGEHACK,   &currentConfig.EmuOpt, 0x0010, 0, 0, 1 },
	{ "done",                      MB_NONE,  MA_OPT2_DONE,          NULL, 0, 0, 0, 1 },
};

#define OPT2_ENTRY_COUNT (sizeof(opt2_entries) / sizeof(opt2_entries[0]))

static void menu_opt2_cust_draw(const menu_entry *entry, int x, int y, void *param)
{
	if (entry->id == MA_OPT2_GAMMA)
		gp2x_text_out8(x, y, "Gamma correction           %i.%02i", currentConfig.gamma / 100, currentConfig.gamma%100);
	else if (entry->id == MA_OPT2_SQUIDGEHACK)
		gp2x_text_out8(x, y, "squidgehack (now %s %s", mmuhack_status ? "active)  " : "inactive)",
			(currentConfig.EmuOpt&0x0010)?"ON":"OFF");
}


static void draw_amenu_options(int menu_sel)
{
	int tl_x = 25, tl_y = 50;

	gp2x_pd_clone_buffer2();

	me_draw(opt2_entries, OPT2_ENTRY_COUNT, tl_x, tl_y, menu_opt2_cust_draw, NULL);

	// draw cursor
	gp2x_text_out8(tl_x - 16, tl_y + menu_sel*10, ">");

	gp2x_video_flip2();
}

static void amenu_loop_options(void)
{
	static int menu_sel = 0;
	int menu_sel_max;
	unsigned long inp = 0;
	menu_id selected_id;

	menu_sel_max = me_count_enabled(opt2_entries, OPT2_ENTRY_COUNT) - 1;

	for(;;)
	{
		draw_amenu_options(menu_sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_LEFT|GP2X_RIGHT|GP2X_B|GP2X_X|GP2X_A);
		if (inp & GP2X_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if (inp & GP2X_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		selected_id = me_index2id(opt2_entries, OPT2_ENTRY_COUNT, menu_sel);
		if (inp & (GP2X_LEFT|GP2X_RIGHT)) { // multi choise
			if (!me_process(opt2_entries, OPT2_ENTRY_COUNT, selected_id, (inp&GP2X_RIGHT) ? 1 : 0) &&
			    selected_id == MA_OPT2_GAMMA) {
				while ((inp = gp2x_joystick_read(1)) & (GP2X_LEFT|GP2X_RIGHT)) {
					currentConfig.gamma += (inp & GP2X_LEFT) ? -1 : 1;
					if (currentConfig.gamma <   1) currentConfig.gamma =   1;
					if (currentConfig.gamma > 300) currentConfig.gamma = 300;
					draw_amenu_options(menu_sel);
					usleep(18*1000);
				}
			}
		}
		if (inp & GP2X_B) { // toggleable options
			if (!me_process(opt2_entries, OPT2_ENTRY_COUNT, selected_id, 1) &&
			    selected_id == MA_OPT2_DONE) {
				return;
			}
		}
		if (inp & (GP2X_X|GP2X_A)) return;
	}
}

// -------------- options --------------


menu_entry opt_entries[] =
{
	{ NULL,                        MB_NONE,  MA_OPT_RENDERER,      NULL, 0, 0, 0, 1 },
	{ NULL,                        MB_RANGE, MA_OPT_SCALING,       &currentConfig.scaling, 0, 0, 3, 1 },
	{ "Accurate timing (slower)",  MB_ONOFF, MA_OPT_ACC_TIMING,    &currentConfig.PicoOpt, 0x040, 0, 0, 1 },
	{ "Accurate sprites (slower)", MB_ONOFF, MA_OPT_ACC_SPRITES,   &currentConfig.PicoOpt, 0x080, 0, 0, 1 },
	{ "Show FPS",                  MB_ONOFF, MA_OPT_SHOW_FPS,      &currentConfig.EmuOpt,  0x002, 0, 0, 1 },
	{ NULL,                        MB_RANGE, MA_OPT_FRAMESKIP,     &currentConfig.Frameskip, 0, -1, 16, 1 },
	{ "Enable sound",              MB_ONOFF, MA_OPT_ENABLE_SOUND,  &currentConfig.EmuOpt,  0x004, 0, 0, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_SOUND_QUALITY, NULL, 0, 0, 0, 1 },
	{ "Use ARM940 core for sound", MB_ONOFF, MA_OPT_ARM940_SOUND,  &currentConfig.PicoOpt, 0x200, 0, 0, 1 },
	{ "6 button pad",              MB_ONOFF, MA_OPT_6BUTTON_PAD,   &currentConfig.PicoOpt, 0x020, 0, 0, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_REGION,        NULL, 0, 0, 0, 1 },
	{ "Use SRAM/BRAM savestates",  MB_ONOFF, MA_OPT_SRAM_STATES,   &currentConfig.EmuOpt,  0x001, 0, 0, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_CONFIRM_STATES,NULL, 0, 0, 0, 1 },
	{ "Save slot",                 MB_RANGE, MA_OPT_SAVE_SLOT,     &state_slot, 0, 0, 9, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_CPU_CLOCKS,    NULL, 0, 0, 0, 1 },
	{ "[Sega/Mega CD options]",    MB_NONE,  MA_OPT_SCD_OPTS,      NULL, 0, 0, 0, 1 },
	{ "[advanced options]",        MB_NONE,  MA_OPT_ADV_OPTS,      NULL, 0, 0, 0, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_SAVECFG,       NULL, 0, 0, 0, 1 },
	{ "Save cfg for current game only",MB_NONE,MA_OPT_SAVECFG_GAME,NULL, 0, 0, 0, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_LOADCFG,       NULL, 0, 0, 0, 1 },
};

#define OPT_ENTRY_COUNT (sizeof(opt_entries) / sizeof(opt_entries[0]))


static const char *region_name(unsigned int code)
{
	static const char *names[] = { "Auto", "      Japan NTSC", "      Japan PAL", "      USA", "      Europe" };
	static const char *names_short[] = { "", " JP", " JP", " US", " EU" };
	int u, i = 0;
	if (code) {
		code <<= 1;
		while((code >>= 1)) i++;
		if (i > 4) return "unknown";
		return names[i];
	} else {
		static char name[24];
		strcpy(name, "Auto:");
		for (u = 0; u < 3; u++) {
			i = 0; code = ((PicoAutoRgnOrder >> u*4) & 0xf) << 1;
			while((code >>= 1)) i++;
			strcat(name, names_short[i]);
		}
		return name;
	}
}


static void menu_opt_cust_draw(const menu_entry *entry, int x, int y, void *param)
{
	char *str, str24[24];

	switch (entry->id)
	{
		case MA_OPT_RENDERER:
			if (currentConfig.PicoOpt&0x10)
				str = " 8bit fast";
			else if (currentConfig.EmuOpt&0x80)
				str = "16bit accurate";
			else
				str = " 8bit accurate";
			gp2x_text_out8(x, y, "Renderer:            %s", str);
			break;
		case MA_OPT_SCALING:
			switch (currentConfig.scaling) {
				default: str = "            OFF";   break;
				case 1:  str = "hw horizontal";     break;
				case 2:  str = "hw horiz. + vert."; break;
				case 3:  str = "sw horizontal";     break;
			}
			gp2x_text_out8(x, y, "Scaling:       %s", str);
			break;
		case MA_OPT_FRAMESKIP:
			if (currentConfig.Frameskip < 0)
			     strcpy(str24, "Auto");
			else sprintf(str24, "%i", currentConfig.Frameskip);
			gp2x_text_out8(x, y, "Frameskip                  %s", str24);
			break;
		case MA_OPT_SOUND_QUALITY:
			str = (currentConfig.PicoOpt&0x08)?"stereo":"mono";
			gp2x_text_out8(x, y, "Sound Quality:     %5iHz %s", currentConfig.PsndRate, str);
			break;
		case MA_OPT_REGION:
			gp2x_text_out8(x, y, "Region:              %s", region_name(currentConfig.PicoRegion));
			break;
		case MA_OPT_CONFIRM_STATES:
			switch ((currentConfig.EmuOpt >> 9) & 5) {
				default: str = "OFF";    break;
				case 1:  str = "writes"; break;
				case 4:  str = "loads";  break;
				case 5:  str = "both";   break;
			}
			gp2x_text_out8(x, y, "Confirm savestate          %s", str);
			break;
		case MA_OPT_CPU_CLOCKS:
			gp2x_text_out8(x, y, "GP2X CPU clocks            %iMhz", currentConfig.CPUclock);
			break;
		case MA_OPT_SAVECFG:
			str24[0] = 0;
			if (config_slot != 0) sprintf(str24, " (profile: %i)", config_slot);
			gp2x_text_out8(x, y, "Save cfg as default%s", str24);
			break;
		case MA_OPT_LOADCFG:
			gp2x_text_out8(x, y, "Load cfg from profile %i", config_slot);
			break;
		default:
			printf("%s: unimplemented (%i)\n", __FUNCTION__, entry->id);
			break;
	}
}



static void draw_menu_options(int menu_sel)
{
	int tl_x = 25, tl_y = 32;

	gp2x_pd_clone_buffer2();

	me_draw(opt_entries, OPT_ENTRY_COUNT, tl_x, tl_y, menu_opt_cust_draw, NULL);

	// draw cursor
	gp2x_text_out8(tl_x - 16, tl_y + menu_sel*10, ">");

	gp2x_video_flip2();
}

static int sndrate_prevnext(int rate, int dir)
{
	int i, rates[] = { 8000, 11025, 16000, 22050, 44100 };

	for (i = 0; i < 5; i++)
		if (rates[i] == rate) break;

	i += dir ? 1 : -1;
	if (i > 4) return dir ? 44100 : 22050;
	if (i < 0) return dir ? 11025 : 8000;
	return rates[i];
}

static void region_prevnext(int right)
{
	// jp_ntsc=1, jp_pal=2, usa=4, eu=8
	static int rgn_orders[] = { 0x148, 0x184, 0x814, 0x418, 0x841, 0x481 };
	int i;
	if (right) {
		if (!currentConfig.PicoRegion) {
			for (i = 0; i < 6; i++)
				if (rgn_orders[i] == PicoAutoRgnOrder) break;
			if (i < 5) PicoAutoRgnOrder = rgn_orders[i+1];
			else currentConfig.PicoRegion=1;
		}
		else currentConfig.PicoRegion<<=1;
		if (currentConfig.PicoRegion > 8) currentConfig.PicoRegion = 8;
	} else {
		if (!currentConfig.PicoRegion) {
			for (i = 0; i < 6; i++)
				if (rgn_orders[i] == PicoAutoRgnOrder) break;
			if (i > 0) PicoAutoRgnOrder = rgn_orders[i-1];
		}
		else currentConfig.PicoRegion>>=1;
	}
}

static void menu_options_save(void)
{
	PicoOpt = currentConfig.PicoOpt;
	PsndRate = currentConfig.PsndRate;
	PicoRegionOverride = currentConfig.PicoRegion;
	if (!(PicoOpt & 0x20)) {
		// unbind XYZ MODE, just in case
		unbind_action(0xf00);
	}
	scaling_update();
}

static int menu_loop_options(void)
{
	static int menu_sel = 0;
	int menu_sel_max, ret;
	unsigned long inp = 0;
	menu_id selected_id;

	currentConfig.PicoOpt = PicoOpt;
	currentConfig.PsndRate = PsndRate;
	currentConfig.PicoRegion = PicoRegionOverride;

	me_enable(opt_entries, OPT_ENTRY_COUNT, MA_OPT_SAVECFG_GAME, rom_data != NULL);
	me_enable(opt_entries, OPT_ENTRY_COUNT, MA_OPT_LOADCFG, config_slot != config_slot_current);
	menu_sel_max = me_count_enabled(opt_entries, OPT_ENTRY_COUNT) - 1;
	if (menu_sel > menu_sel_max) menu_sel = menu_sel_max;

	while (1)
	{
		draw_menu_options(menu_sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_LEFT|GP2X_RIGHT|GP2X_B|GP2X_X|GP2X_A);
		if (inp & GP2X_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if (inp & GP2X_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		selected_id = me_index2id(opt_entries, OPT_ENTRY_COUNT, menu_sel);
		if (inp & (GP2X_LEFT|GP2X_RIGHT)) { // multi choise
			if (!me_process(opt_entries, OPT_ENTRY_COUNT, selected_id, (inp&GP2X_RIGHT) ? 1 : 0)) {
				switch (selected_id) {
					case MA_OPT_RENDERER:
						if (inp & GP2X_LEFT) {
							if      (  currentConfig.PicoOpt&0x10) currentConfig.PicoOpt&= ~0x10;
							else if (!(currentConfig.EmuOpt &0x80))currentConfig.EmuOpt |=  0x80;
							else if (  currentConfig.EmuOpt &0x80) break;
						} else {
							if      (  currentConfig.PicoOpt&0x10) break;
							else if (!(currentConfig.EmuOpt &0x80))currentConfig.PicoOpt|=  0x10;
							else if (  currentConfig.EmuOpt &0x80) currentConfig.EmuOpt &= ~0x80;
						}
						break;
					case MA_OPT_SOUND_QUALITY:
						if ((inp & GP2X_RIGHT) && currentConfig.PsndRate == 44100 && !(currentConfig.PicoOpt&0x08)) {
							currentConfig.PsndRate = 8000;  currentConfig.PicoOpt|= 0x08;
						} else if ((inp & GP2X_LEFT) && currentConfig.PsndRate == 8000 && (currentConfig.PicoOpt&0x08)) {
							currentConfig.PsndRate = 44100; currentConfig.PicoOpt&=~0x08;
						} else currentConfig.PsndRate = sndrate_prevnext(currentConfig.PsndRate, inp & GP2X_RIGHT);
						break;
					case MA_OPT_REGION:
						region_prevnext(inp & GP2X_RIGHT);
						break;
					case MA_OPT_CONFIRM_STATES: {
							 int n = ((currentConfig.EmuOpt>>9)&1) | ((currentConfig.EmuOpt>>10)&2);
							 n += (inp & GP2X_LEFT) ? -1 : 1;
							 if (n < 0) n = 0; else if (n > 3) n = 3;
							 n |= n << 1; n &= ~2;
							 currentConfig.EmuOpt &= ~0xa00;
							 currentConfig.EmuOpt |= n << 9;
							 break;
						 }
					case MA_OPT_SAVE_SLOT:
						 if (inp & GP2X_RIGHT) {
							 state_slot++; if (state_slot > 9) state_slot = 0;
						 } else {state_slot--; if (state_slot < 0) state_slot = 9;
						 }
						 break;
					case MA_OPT_CPU_CLOCKS:
						 while ((inp = gp2x_joystick_read(1)) & (GP2X_LEFT|GP2X_RIGHT)) {
							 currentConfig.CPUclock += (inp & GP2X_LEFT) ? -1 : 1;
							 if (currentConfig.CPUclock < 1) currentConfig.CPUclock = 1;
							 draw_menu_options(menu_sel);
							 usleep(50*1000);
						 }
						 break;
					case MA_OPT_SAVECFG:
					case MA_OPT_SAVECFG_GAME:
					case MA_OPT_LOADCFG:
						 config_slot += (inp&GP2X_RIGHT) ? 1 : -1;
						 if (config_slot > 9) config_slot = 0;
						 if (config_slot < 0) config_slot = 9;
						 me_enable(opt_entries, OPT_ENTRY_COUNT, MA_OPT_LOADCFG, config_slot != config_slot_current);
						 menu_sel_max = me_count_enabled(opt_entries, OPT_ENTRY_COUNT) - 1;
						 if (menu_sel > menu_sel_max) menu_sel = menu_sel_max;
						 break;
					default:
						//printf("%s: something unknown selected (%i)\n", __FUNCTION__, selected_id);
						break;
				}
			}
		}
		if (inp & GP2X_B) {
			if (!me_process(opt_entries, OPT_ENTRY_COUNT, selected_id, 1))
			{
				switch (selected_id)
				{
					case MA_OPT_SCD_OPTS:
						cd_menu_loop_options();
						if (engineState == PGS_ReloadRom)
							return 0; // test BIOS
						break;
					case MA_OPT_ADV_OPTS:
						amenu_loop_options();
						break;
					case MA_OPT_SAVECFG: // done (update and write)
						menu_options_save();
						if (emu_WriteConfig(0)) strcpy(menuErrorMsg, "config saved");
						else strcpy(menuErrorMsg, "failed to write config");
						return 1;
					case MA_OPT_SAVECFG_GAME: // done (update and write for current game)
						menu_options_save();
						if (emu_WriteConfig(1)) strcpy(menuErrorMsg, "config saved");
						else strcpy(menuErrorMsg, "failed to write config");
						return 1;
					case MA_OPT_LOADCFG:
						ret = emu_ReadConfig(1, 1);
						if (!ret) ret = emu_ReadConfig(0, 1);
						if (ret)  strcpy(menuErrorMsg, "config loaded");
						else      strcpy(menuErrorMsg, "failed to load config");
						return 1;
					default:
						//printf("%s: something unknown selected (%i)\n", __FUNCTION__, selected_id);
						break;
				}
			}
		}
		if(inp & (GP2X_X|GP2X_A)) {
			menu_options_save();
			return 0;  // done (update, no write)
		}
	}
}

// -------------- credits --------------

static void draw_menu_credits(void)
{
	int tl_x = 15, tl_y = 70, y;
	//memset(gp2x_screen, 0, 320*240);
	gp2x_pd_clone_buffer2();

	gp2x_text_out8(tl_x, 20, "PicoDrive v" VERSION " (c) notaz, 2006,2007");
	y = tl_y;
	gp2x_text_out8(tl_x, y, "Credits:");
	gp2x_text_out8(tl_x, (y+=10), "Dave: Cyclone 68000 core,");
	gp2x_text_out8(tl_x, (y+=10), "      base code of PicoDrive");
	gp2x_text_out8(tl_x, (y+=10), "Reesy & FluBBa: DrZ80 core");
	gp2x_text_out8(tl_x, (y+=10), "MAME devs: YM2612 and SN76496 cores");
	gp2x_text_out8(tl_x, (y+=10), "Charles MacDonald: Genesis hw docs");
	gp2x_text_out8(tl_x, (y+=10), "Stephane Dallongeville:");
	gp2x_text_out8(tl_x, (y+=10), "      opensource Gens");
	gp2x_text_out8(tl_x, (y+=10), "Haze: Genesis hw info");
	gp2x_text_out8(tl_x, (y+=10), "rlyeh and others: minimal SDK");
	gp2x_text_out8(tl_x, (y+=10), "Squidge: squidgehack");
	gp2x_text_out8(tl_x, (y+=10), "Dzz: ARM940 sample");
	gp2x_text_out8(tl_x, (y+=10), "GnoStiC / Puck2099: USB joystick");
	gp2x_text_out8(tl_x, (y+=10), "craigix: GP2X hardware");

	gp2x_video_flip2();
}


// -------------- root menu --------------

menu_entry main_entries[] =
{
	{ "Resume game",        MB_NONE, MA_MAIN_RESUME_GAME, NULL, 0, 0, 0, 0 },
	{ "Save State",         MB_NONE, MA_MAIN_SAVE_STATE,  NULL, 0, 0, 0, 0 },
	{ "Load State",         MB_NONE, MA_MAIN_LOAD_STATE,  NULL, 0, 0, 0, 0 },
	{ "Reset game",         MB_NONE, MA_MAIN_RESET_GAME,  NULL, 0, 0, 0, 0 },
	{ "Load new ROM/ISO",   MB_NONE, MA_MAIN_LOAD_ROM,    NULL, 0, 0, 0, 1 },
	{ "Change options",     MB_NONE, MA_MAIN_OPTIONS,     NULL, 0, 0, 0, 1 },
	{ "Configure controls", MB_NONE, MA_MAIN_CONTROLS,    NULL, 0, 0, 0, 1 },
	{ "Credits",            MB_NONE, MA_MAIN_CREDITS,     NULL, 0, 0, 0, 1 },
	{ "Patches / GameGenie",MB_NONE, MA_MAIN_PATCHES,     NULL, 0, 0, 0, 0 },
	{ "Exit",               MB_NONE, MA_MAIN_EXIT,        NULL, 0, 0, 0, 1 }
};

#define MAIN_ENTRY_COUNT (sizeof(main_entries) / sizeof(main_entries[0]))

static void draw_menu_root(int menu_sel)
{
	const int tl_x = 70, tl_y = 70;

	gp2x_pd_clone_buffer2();

	gp2x_text_out8(tl_x, 20, "PicoDrive v" VERSION);

	me_draw(main_entries, MAIN_ENTRY_COUNT, tl_x, tl_y, NULL, NULL);

	// draw cursor
	gp2x_text_out8(tl_x - 16, tl_y + menu_sel*10, ">");
	// error
	if (menuErrorMsg[0]) gp2x_text_out8(5, 226, menuErrorMsg);
	gp2x_video_flip2();
}


static void menu_loop_root(void)
{
	static int menu_sel = 0;
	int ret, menu_sel_max;
	unsigned long inp = 0;
	char curr_path[PATH_MAX], *selfname;
	FILE *tstf;

	if ( (tstf = fopen(currentConfig.lastRomFile, "rb")) )
	{
		fclose(tstf);
		strcpy(curr_path, currentConfig.lastRomFile);
	}
	else
	{
		getcwd(curr_path, PATH_MAX);
	}

	me_enable(main_entries, MAIN_ENTRY_COUNT, MA_MAIN_RESUME_GAME, rom_data != NULL);
	me_enable(main_entries, MAIN_ENTRY_COUNT, MA_MAIN_SAVE_STATE,  rom_data != NULL);
	me_enable(main_entries, MAIN_ENTRY_COUNT, MA_MAIN_LOAD_STATE,  rom_data != NULL);
	me_enable(main_entries, MAIN_ENTRY_COUNT, MA_MAIN_RESET_GAME,  rom_data != NULL);
	me_enable(main_entries, MAIN_ENTRY_COUNT, MA_MAIN_PATCHES,     PicoPatches != NULL);

	menu_sel_max = me_count_enabled(main_entries, MAIN_ENTRY_COUNT) - 1;
	if (menu_sel > menu_sel_max) menu_sel = menu_sel_max;

	/* make sure action buttons are not pressed on entering menu */
	draw_menu_root(menu_sel);
	while (gp2x_joystick_read(1) & (GP2X_B|GP2X_X|GP2X_SELECT)) usleep(50*1000);

	for (;;)
	{
		draw_menu_root(menu_sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_B|GP2X_X|GP2X_SELECT|GP2X_L|GP2X_R);
		if(inp & GP2X_UP  )  { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if(inp & GP2X_DOWN)  { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		if((inp & (GP2X_L|GP2X_R)) == (GP2X_L|GP2X_R)) debug_menu_loop();
		if(inp &(GP2X_SELECT|GP2X_X)){
			if (rom_data) {
				while (gp2x_joystick_read(1) & (GP2X_SELECT|GP2X_X)) usleep(50*1000); // wait until select is released
				engineState = PGS_Running;
				break;
			}
		}
		if(inp & GP2X_B)  {
			switch (me_index2id(main_entries, MAIN_ENTRY_COUNT, menu_sel))
			{
				case MA_MAIN_RESUME_GAME:
					if (rom_data) {
						while (gp2x_joystick_read(1) & GP2X_B) usleep(50*1000);
						engineState = PGS_Running;
						return;
					}
					break;
				case MA_MAIN_SAVE_STATE:
					if (rom_data) {
						if(savestate_menu_loop(0))
							continue;
						engineState = PGS_Running;
						return;
					}
					break;
				case MA_MAIN_LOAD_STATE:
					if (rom_data) {
						if(savestate_menu_loop(1))
							continue;
						engineState = PGS_Running;
						return;
					}
					break;
				case MA_MAIN_RESET_GAME:
					if (rom_data) {
						emu_ResetGame();
						engineState = PGS_Running;
						return;
					}
					break;
				case MA_MAIN_LOAD_ROM:
					selfname = romsel_loop(curr_path);
					if (selfname) {
						printf("selected file: %s\n", selfname);
						engineState = PGS_ReloadRom;
					}
					return;
				case MA_MAIN_OPTIONS:
					ret = menu_loop_options();
					if (ret == 1) continue; // status update
					if (engineState == PGS_ReloadRom)
						return; // BIOS test
					break;
				case MA_MAIN_CONTROLS:
					kc_sel_loop();
					break;
				case MA_MAIN_CREDITS:
					draw_menu_credits();
					usleep(500*1000);
					inp = wait_for_input(GP2X_B|GP2X_X);
					break;
				case MA_MAIN_EXIT:
					engineState = PGS_Quit;
					return;
				case MA_MAIN_PATCHES:
					if (rom_data && PicoPatches) {
						patches_menu_loop();
						PicoPatchApply();
						strcpy(menuErrorMsg, "Patches applied");
						continue;
					}
					break;
				default:
					printf("%s: something unknown selected\n", __FUNCTION__);
					break;
			}
		}
		menuErrorMsg[0] = 0; // clear error msg
	}
}


static void menu_prepare_bg(void)
{
	extern int localPal[0x100];
	int c, i;

	// don't clear old palette just for fun (but make it dark)
	for (i = 0x100-1; i >= 0; i--) {
		c = localPal[i];
		localPal[i] = ((c >> 1) & 0x007f7f7f) - ((c >> 3) & 0x001f1f1f);
	}
	localPal[0xe0] = 0x00000000; // reserved pixels for OSD
	localPal[0xf0] = 0x00ffffff;

	gp2x_video_setpalette(localPal, 0x100);
}

static void menu_gfx_prepare(void)
{
	menu_prepare_bg();

	// switch to 8bpp
	gp2x_video_changemode2(8);
	gp2x_video_RGB_setscaling(0, 320, 240);
	gp2x_video_flip2();
}


void menu_loop(void)
{
	menu_gfx_prepare();

	menu_loop_root();

	menuErrorMsg[0] = 0;
}


// --------- CD tray close menu ----------

static void draw_menu_tray(int menu_sel)
{
	int tl_x = 70, tl_y = 90, y;
	memset(gp2x_screen, 0xe0, 320*240);

	gp2x_text_out8(tl_x, 20, "The unit is about to");
	gp2x_text_out8(tl_x, 30, "close the CD tray.");

	y = tl_y;
	gp2x_text_out8(tl_x, y,       "Load new CD image");
	gp2x_text_out8(tl_x, (y+=10), "Insert nothing");

	// draw cursor
	gp2x_text_out8(tl_x - 16, tl_y + menu_sel*10, ">");
	// error
	if (menuErrorMsg[0]) gp2x_text_out8(5, 226, menuErrorMsg);
	gp2x_video_flip2();
}


int menu_loop_tray(void)
{
	int menu_sel = 0, menu_sel_max = 1;
	unsigned long inp = 0;
	char curr_path[PATH_MAX], *selfname;
	FILE *tstf;

	gp2x_memset_all_buffers(0, 0xe0, 320*240);
	menu_gfx_prepare();

	if ( (tstf = fopen(currentConfig.lastRomFile, "rb")) )
	{
		fclose(tstf);
		strcpy(curr_path, currentConfig.lastRomFile);
	}
	else
	{
		getcwd(curr_path, PATH_MAX);
	}

	/* make sure action buttons are not pressed on entering menu */
	draw_menu_tray(menu_sel);
	while (gp2x_joystick_read(1) & GP2X_B) usleep(50*1000);

	for (;;)
	{
		draw_menu_tray(menu_sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_B);
		if(inp & GP2X_UP  )  { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if(inp & GP2X_DOWN)  { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		if(inp & GP2X_B   )  {
			switch (menu_sel) {
				case 0: // select image
					selfname = romsel_loop(curr_path);
					if (selfname) {
						int ret = -1, cd_type;
						cd_type = emu_cd_check(NULL);
						if (cd_type > 0)
							ret = Insert_CD(romFileName, cd_type == 2);
						if (ret != 0) {
							sprintf(menuErrorMsg, "Load failed, invalid CD image?");
							printf("%s\n", menuErrorMsg);
							continue;
						}
						engineState = PGS_RestartRun;
						return 1;
					}
					break;
				case 1: // insert nothing
					engineState = PGS_RestartRun;
					return 0;
			}
		}
		menuErrorMsg[0] = 0; // clear error msg
	}
}


