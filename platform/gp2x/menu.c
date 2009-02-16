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
#include "../linux/usbjoy.h"
#include "../common/emu.h"
#include "../common/menu.h"
#include "../common/arm_utils.h"
#include "../common/readpng.h"
#include "../common/common.h"
#include "../common/input.h"
#include "version.h"

#include <pico/pico_int.h>
#include <pico/patch.h>
#include <zlib/zlib.h>

#ifndef _DIRENT_HAVE_D_TYPE
#error "need d_type for file browser"
#endif

extern int  mmuhack_status;

void menu_darken_bg(void *dst, int pixels, int darker);
static void menu_prepare_bg(int use_game_bg);

void menu_flip(void)
{
	gp2x_video_flush_cache();
	gp2x_video_flip2();
}


// --------- loading ROM screen ----------

static int cdload_called = 0;

static void load_progress_cb(int percent)
{
	int ln, len = percent * 320 / 100;
	unsigned short *dst = (unsigned short *)gp2x_screen + 320*20;

	if (len > 320) len = 320;
	for (ln = 8; ln > 0; ln--, dst += 320)
		memset(dst, 0xff, len*2);
	menu_flip();
}

static void cdload_progress_cb(int percent)
{
	int ln, len = percent * 320 / 100;
	unsigned short *dst = (unsigned short *)gp2x_screen + 320*20;

	memset(dst, 0xff, 320*2*8);

	smalltext_out16(1, 3*10, "Processing CD image / MP3s", 0xffff);
	smalltext_out16_lim(1, 4*10, romFileName, 0xffff, 80);
	dst += 320*30;

	if (len > 320) len = 320;
	for (ln = 8; ln > 0; ln--, dst += 320)
		memset(dst, 0xff, len*2);
	menu_flip();
	cdload_called = 1;
}

void menu_romload_prepare(const char *rom_name)
{
	const char *p = rom_name + strlen(rom_name);
	while (p > rom_name && *p != '/') p--;

	if (rom_loaded) gp2x_pd_clone_buffer2();
	else memset(gp2x_screen, 0, 320*240*2);

	smalltext_out16(1, 1, "Loading", 0xffff);
	smalltext_out16_lim(1, 10, p, 0xffff, 53);
	gp2x_memcpy_buffers(3, gp2x_screen, 0, 320*240*2);
	menu_flip();
	PicoCartLoadProgressCB = load_progress_cb;
	PicoCDLoadProgressCB = cdload_progress_cb;
	cdload_called = 0;
}

void menu_romload_end(void)
{
	PicoCartLoadProgressCB = PicoCDLoadProgressCB = NULL;
	smalltext_out16(1, cdload_called ? 60 : 30, "Starting emulation...", 0xffff);
	menu_flip();
}

// -------------- ROM selector --------------

// rrrr rggg gggb bbbb
static unsigned short file2color(const char *fname)
{
	const char *ext = fname + strlen(fname) - 3;
	static const char *rom_exts[]   = { "zip", "bin", "smd", "gen", "iso", "cso", "cue" };
	static const char *other_exts[] = { "gmv", "pat" };
	int i;

	if (ext < fname) ext = fname;
	for (i = 0; i < sizeof(rom_exts)/sizeof(rom_exts[0]); i++)
		if (strcasecmp(ext, rom_exts[i]) == 0) return 0xbdff;
	for (i = 0; i < sizeof(other_exts)/sizeof(other_exts[0]); i++)
		if (strcasecmp(ext, other_exts[i]) == 0) return 0xaff5;
	return 0xffff;
}

static void draw_dirlist(char *curdir, struct dirent **namelist, int n, int sel)
{
	int start, i, pos;

	start = 12 - sel;
	n--; // exclude current dir (".")

	gp2x_pd_clone_buffer2();

	if (!rom_loaded) {
		menu_darken_bg(gp2x_screen, 320*240, 0);
	}

	menu_darken_bg((char *)gp2x_screen + 320*120*2, 320*8, 0);

	if(start - 2 >= 0)
		smalltext_out16_lim(14, (start - 2)*10, curdir, 0xffff, 53-2);
	for (i = 0; i < n; i++) {
		pos = start + i;
		if (pos < 0)  continue;
		if (pos > 23) break;
		if (namelist[i+1]->d_type == DT_DIR) {
			smalltext_out16_lim(14,   pos*10, "/", 0xfff6, 1);
			smalltext_out16_lim(14+6, pos*10, namelist[i+1]->d_name, 0xfff6, 53-3);
		} else {
			unsigned short color = file2color(namelist[i+1]->d_name);
			smalltext_out16_lim(14,   pos*10, namelist[i+1]->d_name, color, 53-2);
		}
	}
	text_out16(5, 120, ">");
	menu_flip();
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
	".jpg", ".gpe"
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

static void do_delete(const char *fpath, const char *fname)
{
	int len, inp;

	gp2x_pd_clone_buffer2();

	if (!rom_loaded)
		menu_darken_bg(gp2x_screen, 320*240, 0);

	len = strlen(fname);
	if (len > 320/6) len = 320/6;

	text_out16(320/2 - 15*8/2,  80, "About to delete");
	smalltext_out16_lim(320/2 - len*6/2, 95, fname, 0xbdff, len);
	text_out16(320/2 - 13*8/2, 110, "Are you sure?");
	text_out16(320/2 - 25*8/2, 120, "(Y - confirm, X - cancel)");
	menu_flip();


	while (in_menu_wait_any(50) & (PBTN_WEST|PBTN_MENU));
	inp = in_menu_wait(GP2X_Y|PBTN_MBACK);	/* FIXME */
	if (inp & GP2X_Y)
		remove(fpath);
}

static char *romsel_loop(char *curr_path)
{
	struct dirent **namelist;
	DIR *dir;
	int n, sel = 0;
	unsigned long inp = 0;
	char *ret = NULL, *fname = NULL;

rescan:
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
			printf("dir: %s\n", curr_path);
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
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_L|PBTN_R|PBTN_WEST|PBTN_MOK|PBTN_MBACK|PBTN_MENU);
		if(inp & PBTN_UP  )  { sel--;   if (sel < 0)   sel = n-2; }
		if(inp & PBTN_DOWN)  { sel++;   if (sel > n-2) sel = 0; }
		if(inp & PBTN_LEFT)  { sel-=10; if (sel < 0)   sel = 0; }
		if(inp & PBTN_L)     { sel-=24; if (sel < 0)   sel = 0; }
		if(inp & PBTN_RIGHT) { sel+=10; if (sel > n-2) sel = n-2; }
		if(inp & PBTN_R)     { sel+=24; if (sel > n-2) sel = n-2; }
		if ((inp & PBTN_MOK) || (inp & (PBTN_MENU|PBTN_WEST)) == (PBTN_MENU|PBTN_WEST)) // enter dir/select || delete
		{
			again:
			if (namelist[sel+1]->d_type == DT_REG)
			{
				strcpy(romFileName, curr_path);
				strcat(romFileName, "/");
				strcat(romFileName, namelist[sel+1]->d_name);
				if (inp & PBTN_MOK) { // return sel
					ret = romFileName;
					break;
				}
				do_delete(romFileName, namelist[sel+1]->d_name);
				if (n > 0) {
					while (n--) free(namelist[n]);
					free(namelist);
				}
				goto rescan;
			}
			else if (namelist[sel+1]->d_type == DT_DIR)
			{
				int newlen;
				char *p, *newdir;
				if (!(inp & PBTN_MOK)) continue;
				newlen = strlen(curr_path) + strlen(namelist[sel+1]->d_name) + 2;
				newdir = malloc(newlen);
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
			}
			else
			{
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
		if(inp & PBTN_MBACK) break; // cancel
	}

	if (n > 0) {
		while (n--) free(namelist[n]);
		free(namelist);
	}

	return ret;
}

// ------------ patch/gg menu ------------

static void draw_patchlist(int sel)
{
	int start, i, pos, active;

	start = 12 - sel;

	gp2x_pd_clone_buffer2();

	for (i = 0; i < PicoPatchCount; i++) {
		pos = start + i;
		if (pos < 0)  continue;
		if (pos > 23) break;
		active = PicoPatches[i].active;
		smalltext_out16_lim(14,     pos*10, active ? "ON " : "OFF", active ? 0xfff6 : 0xffff, 3);
		smalltext_out16_lim(14+6*4, pos*10, PicoPatches[i].name, active ? 0xfff6 : 0xffff, 53-6);
	}
	pos = start + i;
	if (pos < 24) smalltext_out16_lim(14, pos*10, "done", 0xffff, 4);

	text_out16(5, 120, ">");
	menu_flip();
}


static void patches_menu_loop(void)
{
	int menu_sel = 0;
	unsigned long inp = 0;

	for(;;)
	{
		draw_patchlist(menu_sel);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_L|PBTN_R|PBTN_MOK|PBTN_MBACK);
		if(inp & PBTN_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = PicoPatchCount; }
		if(inp & PBTN_DOWN) { menu_sel++; if (menu_sel > PicoPatchCount) menu_sel = 0; }
		if(inp &(PBTN_LEFT|PBTN_L))  { menu_sel-=10; if (menu_sel < 0) menu_sel = 0; }
		if(inp &(PBTN_RIGHT|PBTN_R)) { menu_sel+=10; if (menu_sel > PicoPatchCount) menu_sel = PicoPatchCount; }
		if(inp & PBTN_MOK) { // action
			if (menu_sel < PicoPatchCount)
				PicoPatches[menu_sel].active = !PicoPatches[menu_sel].active;
			else 	return;
		}
		if(inp & PBTN_MBACK) return;
	}

}

// ------------ savestate loader ------------

static int state_slot_flags = 0;

static void state_check_slots(void)
{
	int slot;

	state_slot_flags = 0;

	for (slot = 0; slot < 10; slot++)
	{
		if (emu_checkSaveFile(slot))
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
		emu_setSaveStateCbs(1);
	} else {
		file = fopen(fname, "rb");
		emu_setSaveStateCbs(0);
	}

	if (file) {
		if (PicoAHW & PAHW_MCD) {
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

	emu_forcedFrame(POPT_EN_SOFTSCALE);
	menu_prepare_bg(1);

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

	text_out16(tl_x, 30, is_loading ? "Load state" : "Save state");

	menu_draw_selection(tl_x - 16, tl_y + menu_sel*10, 108);

	/* draw all 10 slots */
	y = tl_y;
	for (i = 0; i < 10; i++, y+=10)
	{
		text_out16(tl_x, y, "SLOT %i (%s)", i, (state_slot_flags & (1 << i)) ? "USED" : "free");
	}
	text_out16(tl_x, y, "back");

	menu_flip();
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
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_MOK|PBTN_MBACK);
		if(inp & PBTN_UP  ) {
			do {
				menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max;
			} while (!(state_slot_flags & (1 << menu_sel)) && menu_sel != menu_sel_max && is_loading);
		}
		if(inp & PBTN_DOWN) {
			do {
				menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0;
			} while (!(state_slot_flags & (1 << menu_sel)) && menu_sel != menu_sel_max && is_loading);
		}
		if(inp & PBTN_MOK) { // save/load
			if (menu_sel < 10) {
				state_slot = menu_sel;
				if (emu_SaveLoadGame(is_loading, 0)) {
					strcpy(menuErrorMsg, is_loading ? "Load failed" : "Save failed");
					return 1;
				}
				return 0;
			} else	return 1;
		}
		if(inp & PBTN_MBACK) return 1;
	}
}

// -------------- key config --------------

static char *action_binds(int player_idx, int action_mask)
{
	static char strkeys[32];
	int d, k, d_prev = -1;

	strkeys[0] = 0;

	for (d = 0; d < IN_MAX_DEVS; d++)
	{
		const int *binds;
		int count;

		binds = in_get_dev_binds(d);
		if (binds == NULL)
			continue;

		count = in_get_dev_bind_count(d);
		for (k = 0; k < count; k++)
		{
			const char *xname;
			char prefix[16];
			if (!(binds[k] & action_mask))
				continue;

			if (player_idx >= 0 && ((binds[k] >> 16) & 3) != player_idx)
				continue;

			xname = in_get_key_name(d, k);
			if (strkeys[0])
				strncat(strkeys, d == d_prev ? " + " : ", ", sizeof(strkeys));
			if (d) sprintf(prefix, "%d: ", d);
			if (d) strncat(strkeys, prefix, sizeof(strkeys));
			strncat(strkeys, xname, sizeof(strkeys));
			d_prev = d;
		}
	}

	// limit..
	strkeys[20] = 0;

	return strkeys;
}

static void unbind_action(int action, int pl_idx, int joy)
{
	int i, u;

	if (joy <= 0)
	{
		for (i = 0; i < 32; i++) {
			if (pl_idx >= 0 && (currentConfig.KeyBinds[i]&0x30000) != (pl_idx<<16)) continue;
			currentConfig.KeyBinds[i] &= ~action;
		}
	}
	if (joy < 0)
	{
		for (u = 0; u < 4; u++)
			for (i = 0; i < 32; i++) {
				if (pl_idx >= 0 && (currentConfig.JoyBinds[u][i]&0x30000) != (pl_idx<<16)) continue;
				currentConfig.JoyBinds[u][i] &= ~action;
			}
	}
	else if (joy > 0)
	{
		for (i = 0; i < 32; i++) {
			if (pl_idx >= 0 && (currentConfig.JoyBinds[joy-1][i]&0x30000) != (pl_idx<<16)) continue;
			currentConfig.JoyBinds[joy-1][i] &= ~action;
		}
	}
}

static int count_bound_keys(int dev_id, int action_mask, int player_idx)
{
	const int *binds;
	int k, keys = 0;
	int count;

	binds = in_get_dev_binds(dev_id);
	if (binds == NULL)
		return 0;

	count = in_get_dev_bind_count(dev_id);
	for (k = 0; k < count; k++)
	{
		if (!(binds[k] & action_mask))
			continue;

		if (player_idx >= 0 && ((binds[k] >> 16) & 3) != player_idx)
			continue;

		keys++;
	}

	return keys;
}

static void draw_key_config(const me_bind_action *opts, int opt_cnt, int player_idx,
		int sel, int is_bind)
{
	int x, y, tl_y = 30, i;

	gp2x_pd_clone_buffer2();
	if (player_idx >= 0) {
		text_out16(80, 10, "Player %i controls", player_idx + 1);
		x = 80;
	} else {
		text_out16(80, 10, "Emulator controls");
		x = 40;
	}

	menu_draw_selection(x - 16, tl_y + sel*10, (player_idx >= 0) ? 66 : 140);

	y = tl_y;
	for (i = 0; i < opt_cnt; i++, y+=10)
		text_out16(x, y, "%s : %s", opts[i].name, action_binds(player_idx, opts[i].mask));

	text_out16(x, y, "Done");

	if (sel < opt_cnt) {
		text_out16(30, 205, is_bind ? "Press a button to bind/unbind" : "Press B to define");
		text_out16(30, 225, "Select \"Done\" to exit");
	} else {
		text_out16(30, 205, "Use Options -> Save cfg");
		text_out16(30, 215, "to save controls");
		text_out16(30, 225, "Press B or X to exit");
	}
	menu_flip();
}

static void key_config_loop(const me_bind_action *opts, int opt_cnt, int player_idx)
{
	int sel = 0, menu_sel_max = opt_cnt;
	int kc, dev_id, is_down, mkey, unbind;

	for (;;)
	{
		draw_key_config(opts, opt_cnt, player_idx, sel, 0);
		mkey = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_MBACK|PBTN_MOK);
		switch (mkey) {
			case PBTN_UP:   sel--; if (sel < 0) sel = menu_sel_max; continue;
			case PBTN_DOWN: sel++; if (sel > menu_sel_max) sel = 0; continue;
			case PBTN_MBACK: return;
			case PBTN_MOK:
				if (sel >= opt_cnt)
					return;
				while (in_menu_wait_any(30) & PBTN_MOK);
				break;
			default:continue;
		}

		draw_key_config(opts, opt_cnt, player_idx, sel, 1);

		/* wait for some up event */
		for (is_down = 1; is_down; ) {
			kc = in_update_keycode(&dev_id, &is_down, -1);
		}

		unbind = count_bound_keys(dev_id, opts[sel].mask, player_idx) >= 2;

		in_bind_key(dev_id, kc, opts[sel].mask, unbind);
		if (player_idx >= 0) {
			/* FIXME */
			in_bind_key(dev_id, kc, 3 << 16, 1);
			in_bind_key(dev_id, kc, player_idx << 16, 0);
		}
	}
}


menu_entry ctrlopt_entries[] =
{
	{ "Player 1",                  MB_NONE,  MA_CTRL_PLAYER1,       NULL, 0, 0, 0, 1, 0 },
	{ "Player 2",                  MB_NONE,  MA_CTRL_PLAYER2,       NULL, 0, 0, 0, 1, 0 },
	{ "Emulator controls",         MB_NONE,  MA_CTRL_EMU,           NULL, 0, 0, 0, 1, 0 },
	{ "6 button pad",              MB_ONOFF, MA_OPT_6BUTTON_PAD,   &PicoOpt, 0x020, 0, 0, 1, 1 },
	{ "Turbo rate",                MB_RANGE, MA_CTRL_TURBO_RATE,   &currentConfig.turbo_rate, 0, 1, 30, 1, 1 },
	{ "Done",                      MB_NONE,  MA_CTRL_DONE,          NULL, 0, 0, 0, 1, 0 },
};

#define CTRLOPT_ENTRY_COUNT (sizeof(ctrlopt_entries) / sizeof(ctrlopt_entries[0]))
const int ctrlopt_entry_count = CTRLOPT_ENTRY_COUNT;


static void draw_kc_sel(int menu_sel)
{
	int tl_x = 25+40, tl_y = 60, y, i;
	char joyname[36];

	y = tl_y;
	gp2x_pd_clone_buffer2();
	menu_draw_selection(tl_x - 16, tl_y + menu_sel*10, 138);

	me_draw(ctrlopt_entries, ctrlopt_entry_count, tl_x, tl_y, NULL, NULL);

	tl_x = 25;
	text_out16(tl_x, (y=130), "Input devices:");
	for (i = 0; i < IN_MAX_DEVS && y < 230; i++) {
		const char *tmp, *name = in_get_dev_name(i, 1);
		if (name == NULL)
			continue;
		tmp = strchr(name, ':');
		if (tmp != NULL)
			name = tmp + 1;
		strncpy(joyname, name, 33); joyname[33] = 0;
		text_out16(tl_x, (y+=10), "%i: %s", i, joyname);
	}

	menu_flip();
}


// player2_flag, reserved, ?, ?,
// ?, ?, fast forward, menu
// "NEXT SAVE SLOT", "PREV SAVE SLOT", "SWITCH RENDERER", "SAVE STATE",
// "LOAD STATE", "VOLUME UP", "VOLUME DOWN", "DONE"
me_bind_action emuctrl_actions[] =
{
	{ "Load State       ", 1<<28 },
	{ "Save State       ", 1<<27 },
	{ "Prev Save Slot   ", 1<<25 },
	{ "Next Save Slot   ", 1<<24 },
	{ "Switch Renderer  ", 1<<26 },
	{ "Volume Down      ", 1<<30 },
	{ "Volume Up        ", 1<<29 },
	{ "Fast forward     ", 1<<22 },
	{ "Enter Menu       ", 1<<23 },
	{ "Pico Next page   ", 1<<21 },
	{ "Pico Prev page   ", 1<<20 },
	{ "Pico Switch input", 1<<19 },
	{ NULL,                0     }
};

static void kc_sel_loop(void)
{
	int menu_sel = 5, menu_sel_max = 5;
	unsigned long inp = 0;
	menu_id selected_id;

	while (1)
	{
		draw_kc_sel(menu_sel);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_RIGHT|PBTN_LEFT|PBTN_MOK|PBTN_MBACK);
		selected_id = me_index2id(ctrlopt_entries, CTRLOPT_ENTRY_COUNT, menu_sel);
		if (inp & (PBTN_LEFT|PBTN_RIGHT)) // multi choise
			me_process(ctrlopt_entries, CTRLOPT_ENTRY_COUNT, selected_id, (inp&PBTN_RIGHT) ? 1 : 0);
		if (inp & PBTN_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if (inp & PBTN_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		if (inp & PBTN_MOK) {
			int is_6button = PicoOpt & POPT_6BTN_PAD;
			switch (selected_id) {
				case MA_CTRL_PLAYER1: key_config_loop(me_ctrl_actions, is_6button ? 15 : 11, 0); return;
				case MA_CTRL_PLAYER2: key_config_loop(me_ctrl_actions, is_6button ? 15 : 11, 1); return;
				case MA_CTRL_EMU:     key_config_loop(emuctrl_actions,
							sizeof(emuctrl_actions)/sizeof(emuctrl_actions[0]) - 1, -1); return;
				case MA_CTRL_DONE:    if (!rom_loaded) emu_WriteConfig(0); return;
				default: return;
			}
		}
		if (inp & PBTN_MBACK) return;
	}
}


// --------- sega/mega cd options ----------

menu_entry cdopt_entries[] =
{
	{ NULL,                        MB_NONE,  MA_CDOPT_TESTBIOS_USA, NULL, 0, 0, 0, 1, 0 },
	{ NULL,                        MB_NONE,  MA_CDOPT_TESTBIOS_EUR, NULL, 0, 0, 0, 1, 0 },
	{ NULL,                        MB_NONE,  MA_CDOPT_TESTBIOS_JAP, NULL, 0, 0, 0, 1, 0 },
	{ "CD LEDs",                   MB_ONOFF, MA_CDOPT_LEDS,         &currentConfig.EmuOpt, 0x0400, 0, 0, 1, 1 },
	{ "CDDA audio",                MB_ONOFF, MA_CDOPT_CDDA,         &PicoOpt, 0x0800, 0, 0, 1, 1 },
	{ "PCM audio",                 MB_ONOFF, MA_CDOPT_PCM,          &PicoOpt, 0x0400, 0, 0, 1, 1 },
	{ NULL,                        MB_NONE,  MA_CDOPT_READAHEAD,    NULL, 0, 0, 0, 1, 1 },
	{ "SaveRAM cart",              MB_ONOFF, MA_CDOPT_SAVERAM,      &PicoOpt, 0x8000, 0, 0, 1, 1 },
	{ "Scale/Rot. fx (slow)",      MB_ONOFF, MA_CDOPT_SCALEROT_CHIP,&PicoOpt, 0x1000, 0, 0, 1, 1 },
	{ "Better sync (slow)",        MB_ONOFF, MA_CDOPT_BETTER_SYNC,  &PicoOpt, 0x2000, 0, 0, 1, 1 },
	{ "done",                      MB_NONE,  MA_CDOPT_DONE,         NULL, 0, 0, 0, 1, 0 },
};

#define CDOPT_ENTRY_COUNT (sizeof(cdopt_entries) / sizeof(cdopt_entries[0]))
const int cdopt_entry_count = CDOPT_ENTRY_COUNT;


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
		case MA_CDOPT_TESTBIOS_USA: text_out16(x, y, "USA BIOS:     %s", bios_names->us); break;
		case MA_CDOPT_TESTBIOS_EUR: text_out16(x, y, "EUR BIOS:     %s", bios_names->eu); break;
		case MA_CDOPT_TESTBIOS_JAP: text_out16(x, y, "JAP BIOS:     %s", bios_names->jp); break;
		case MA_CDOPT_READAHEAD:
			if (PicoCDBuffers > 1) sprintf(ra_buff, "%5iK", PicoCDBuffers * 2);
			else strcpy(ra_buff, "     OFF");
			text_out16(x, y, "ReadAhead buffer      %s", ra_buff);
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

	menu_draw_selection(tl_x - 16, tl_y + menu_sel*10, 246);

	me_draw(cdopt_entries, CDOPT_ENTRY_COUNT, tl_x, tl_y, menu_cdopt_cust_draw, bios_names);

/* FIXME
	selected_id = me_index2id(cdopt_entries, CDOPT_ENTRY_COUNT, menu_sel);
	if ((selected_id == MA_CDOPT_TESTBIOS_USA && strcmp(bios_names->us, "NOT FOUND")) ||
		(selected_id == MA_CDOPT_TESTBIOS_EUR && strcmp(bios_names->eu, "NOT FOUND")) ||
		(selected_id == MA_CDOPT_TESTBIOS_JAP && strcmp(bios_names->jp, "NOT FOUND")))
			text_out16(tl_x, 210, "Press start to test selected BIOS");
*/

	menu_flip();
}

static void cd_menu_loop_options(void)
{
	static int menu_sel = 0;
	int menu_sel_max = 10;
	unsigned long inp = 0;
	struct bios_names_t bios_names;
	menu_id selected_id;
	char *bios, *p;

	if (emu_findBios(4, &bios)) { // US
		for (p = bios+strlen(bios)-1; p > bios && *p != '/'; p--); p++;
		strncpy(bios_names.us, p, sizeof(bios_names.us)); bios_names.us[sizeof(bios_names.us)-1] = 0;
	} else	strcpy(bios_names.us, "NOT FOUND");

	if (emu_findBios(8, &bios)) { // EU
		for (p = bios+strlen(bios)-1; p > bios && *p != '/'; p--); p++;
		strncpy(bios_names.eu, p, sizeof(bios_names.eu)); bios_names.eu[sizeof(bios_names.eu)-1] = 0;
	} else	strcpy(bios_names.eu, "NOT FOUND");

	if (emu_findBios(1, &bios)) { // JP
		for (p = bios+strlen(bios)-1; p > bios && *p != '/'; p--); p++;
		strncpy(bios_names.jp, p, sizeof(bios_names.jp)); bios_names.jp[sizeof(bios_names.jp)-1] = 0;
	} else	strcpy(bios_names.jp, "NOT FOUND");

	for(;;)
	{
		draw_cd_menu_options(menu_sel, &bios_names);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_MOK|PBTN_MBACK);
		if (inp & PBTN_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if (inp & PBTN_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		selected_id = me_index2id(cdopt_entries, CDOPT_ENTRY_COUNT, menu_sel);
		if (inp & (PBTN_LEFT|PBTN_RIGHT)) { // multi choise
			if (!me_process(cdopt_entries, CDOPT_ENTRY_COUNT, selected_id, (inp&PBTN_RIGHT) ? 1 : 0) &&
			    selected_id == MA_CDOPT_READAHEAD) {
				if (inp & PBTN_LEFT) {
					PicoCDBuffers >>= 1;
					if (PicoCDBuffers < 2) PicoCDBuffers = 0;
				} else {
					if (PicoCDBuffers < 2) PicoCDBuffers = 2;
					else PicoCDBuffers <<= 1;
					if (PicoCDBuffers > 8*1024) PicoCDBuffers = 8*1024; // 16M
				}
			}
		}
		if (inp & PBTN_MOK) {
			// toggleable options
			if (!me_process(cdopt_entries, CDOPT_ENTRY_COUNT, selected_id, 1) &&
			    selected_id == MA_CDOPT_DONE) {
				return;
			}
			// BIOS testers
			switch (selected_id) {
				case MA_CDOPT_TESTBIOS_USA:
					if (emu_findBios(4, &bios)) { // test US
						strcpy(romFileName, bios);
						engineState = PGS_ReloadRom;
						return;
					}
					break;
				case MA_CDOPT_TESTBIOS_EUR:
					if (emu_findBios(8, &bios)) { // test EU
						strcpy(romFileName, bios);
						engineState = PGS_ReloadRom;
						return;
					}
					break;
				case MA_CDOPT_TESTBIOS_JAP:
					if (emu_findBios(1, &bios)) { // test JP
						strcpy(romFileName, bios);
						engineState = PGS_ReloadRom;
						return;
					}
					break;
				default:
					break;
			}
		}
		if (inp & PBTN_MBACK) return;
	}
}


// --------- advanced options ----------

menu_entry opt2_entries[] =
{
	{ NULL,                        MB_NONE,  MA_OPT2_GAMMA,         NULL, 0, 0, 0, 1, 1 },
	{ "A_SN's gamma curve",        MB_ONOFF, MA_OPT2_A_SN_GAMMA,    &currentConfig.EmuOpt, 0x1000, 0, 0, 1, 1 },
	{ "Perfect vsync",             MB_ONOFF, MA_OPT2_VSYNC,         &currentConfig.EmuOpt, 0x2000, 0, 0, 1, 1 },
	{ "Disable sprite limit",      MB_ONOFF, MA_OPT2_NO_SPRITE_LIM, &PicoOpt, 0x40000, 0, 0, 1, 1 },
	{ "Emulate Z80",               MB_ONOFF, MA_OPT2_ENABLE_Z80,    &PicoOpt, 0x00004, 0, 0, 1, 1 },
	{ "Emulate YM2612 (FM)",       MB_ONOFF, MA_OPT2_ENABLE_YM2612, &PicoOpt, 0x00001, 0, 0, 1, 1 },
	{ "Emulate SN76496 (PSG)",     MB_ONOFF, MA_OPT2_ENABLE_SN76496,&PicoOpt, 0x00002, 0, 0, 1, 1 },
	{ "gzip savestates",           MB_ONOFF, MA_OPT2_GZIP_STATES,   &currentConfig.EmuOpt, 0x0008, 0, 0, 1, 1 },
	{ "Don't save last used ROM",  MB_ONOFF, MA_OPT2_NO_LAST_ROM,   &currentConfig.EmuOpt, 0x0020, 0, 0, 1, 1 },
	{ "needs restart:",            MB_NONE,  MA_NONE,               NULL, 0, 0, 0, 1, 0 },
	{ "craigix's RAM timings",     MB_ONOFF, MA_OPT2_RAMTIMINGS,    &currentConfig.EmuOpt, 0x0100, 0, 0, 1, 1 },
	{ NULL,                        MB_ONOFF, MA_OPT2_SQUIDGEHACK,   &currentConfig.EmuOpt, 0x0010, 0, 0, 1, 1 },
	{ "SVP dynarec",               MB_ONOFF, MA_OPT2_SVP_DYNAREC,   &PicoOpt, 0x20000, 0, 0, 1, 1 },
	{ "Disable idle loop patching",MB_ONOFF, MA_OPT2_NO_IDLE_LOOPS, &PicoOpt, 0x80000, 0, 0, 1, 1 },
	{ "done",                      MB_NONE,  MA_OPT2_DONE,          NULL, 0, 0, 0, 1, 0 },
};

#define OPT2_ENTRY_COUNT (sizeof(opt2_entries) / sizeof(opt2_entries[0]))
const int opt2_entry_count = OPT2_ENTRY_COUNT;

static void menu_opt2_cust_draw(const menu_entry *entry, int x, int y, void *param)
{
	if (entry->id == MA_OPT2_GAMMA)
		text_out16(x, y, "Gamma correction           %i.%02i", currentConfig.gamma / 100, currentConfig.gamma%100);
	else if (entry->id == MA_OPT2_SQUIDGEHACK)
		text_out16(x, y, "Squidgehack (now %s %s", mmuhack_status ? "active)  " : "inactive)",
			(currentConfig.EmuOpt&0x0010)?"ON":"OFF");
}


static void draw_amenu_options(int menu_sel)
{
	int tl_x = 25, tl_y = 50;

	gp2x_pd_clone_buffer2();

	menu_draw_selection(tl_x - 16, tl_y + menu_sel*10, 252);

	me_draw(opt2_entries, OPT2_ENTRY_COUNT, tl_x, tl_y, menu_opt2_cust_draw, NULL);

	menu_flip();
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
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_MOK|PBTN_MBACK);
		if (inp & PBTN_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if (inp & PBTN_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		selected_id = me_index2id(opt2_entries, OPT2_ENTRY_COUNT, menu_sel);
		if (inp & (PBTN_LEFT|PBTN_RIGHT)) { // multi choise
			if (!me_process(opt2_entries, OPT2_ENTRY_COUNT, selected_id, (inp&PBTN_RIGHT) ? 1 : 0) &&
			    selected_id == MA_OPT2_GAMMA) {
				while ((inp = in_menu_wait_any(20)) & (PBTN_LEFT|PBTN_RIGHT)) {
					currentConfig.gamma += (inp & PBTN_LEFT) ? -1 : 1;
					if (currentConfig.gamma <   1) currentConfig.gamma =   1;
					if (currentConfig.gamma > 300) currentConfig.gamma = 300;
					draw_amenu_options(menu_sel);
				}
			}
		}
		if (inp & PBTN_MOK) { // toggleable options
			if (!me_process(opt2_entries, OPT2_ENTRY_COUNT, selected_id, 1) &&
			    selected_id == MA_OPT2_DONE) {
				return;
			}
		}
		if (inp & PBTN_MBACK) return;
	}
}

// -------------- options --------------


menu_entry opt_entries[] =
{
	{ NULL,                        MB_NONE,  MA_OPT_RENDERER,      NULL, 0, 0, 0, 1, 1 },
	{ NULL,                        MB_RANGE, MA_OPT_SCALING,       &currentConfig.scaling, 0, 0, 3, 1, 1 },
	{ "Accurate sprites",          MB_ONOFF, MA_OPT_ACC_SPRITES,   &PicoOpt, 0x080, 0, 0, 0, 1 },
	{ "Show FPS",                  MB_ONOFF, MA_OPT_SHOW_FPS,      &currentConfig.EmuOpt,  0x002, 0, 0, 1, 1 },
	{ NULL,                        MB_RANGE, MA_OPT_FRAMESKIP,     &currentConfig.Frameskip, 0, -1, 16, 1, 1 },
	{ "Enable sound",              MB_ONOFF, MA_OPT_ENABLE_SOUND,  &currentConfig.EmuOpt,  0x004, 0, 0, 1, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_SOUND_QUALITY, NULL, 0, 0, 0, 1, 1 },
	{ "Use ARM940 core for sound", MB_ONOFF, MA_OPT_ARM940_SOUND,  &PicoOpt, 0x200, 0, 0, 1, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_REGION,        NULL, 0, 0, 0, 1, 1 },
	{ "Use SRAM/BRAM savestates",  MB_ONOFF, MA_OPT_SRAM_STATES,   &currentConfig.EmuOpt,  0x001, 0, 0, 1, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_CONFIRM_STATES,NULL, 0, 0, 0, 1, 1 },
	{ "Save slot",                 MB_RANGE, MA_OPT_SAVE_SLOT,     &state_slot, 0, 0, 9, 1, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_CPU_CLOCKS,    NULL, 0, 0, 0, 1, 1 },
	{ "[Sega/Mega CD options]",    MB_NONE,  MA_OPT_SCD_OPTS,      NULL, 0, 0, 0, 1, 0 },
	{ "[advanced options]",        MB_NONE,  MA_OPT_ADV_OPTS,      NULL, 0, 0, 0, 1, 0 },
	{ NULL,                        MB_NONE,  MA_OPT_SAVECFG,       NULL, 0, 0, 0, 1, 0 },
	{ "Save cfg for current game only",MB_NONE,MA_OPT_SAVECFG_GAME,NULL, 0, 0, 0, 1, 0 },
	{ NULL,                        MB_NONE,  MA_OPT_LOADCFG,       NULL, 0, 0, 0, 1, 0 },
};

#define OPT_ENTRY_COUNT (sizeof(opt_entries) / sizeof(opt_entries[0]))
const int opt_entry_count = OPT_ENTRY_COUNT;


static void menu_opt_cust_draw(const menu_entry *entry, int x, int y, void *param)
{
	char *str, str24[24];

	switch (entry->id)
	{
		case MA_OPT_RENDERER:
			if (PicoOpt & POPT_ALT_RENDERER)
				str = " 8bit fast";
			else if (currentConfig.EmuOpt&0x80)
				str = "16bit accurate";
			else
				str = " 8bit accurate";
			text_out16(x, y, "Renderer:            %s", str);
			break;
		case MA_OPT_SCALING:
			switch (currentConfig.scaling) {
				default: str = "            OFF";   break;
				case 1:  str = "hw horizontal";     break;
				case 2:  str = "hw horiz. + vert."; break;
				case 3:  str = "sw horizontal";     break;
			}
			text_out16(x, y, "Scaling:       %s", str);
			break;
		case MA_OPT_FRAMESKIP:
			if (currentConfig.Frameskip < 0)
			     strcpy(str24, "Auto");
			else sprintf(str24, "%i", currentConfig.Frameskip);
			text_out16(x, y, "Frameskip                  %s", str24);
			break;
		case MA_OPT_SOUND_QUALITY:
			str = (PicoOpt & POPT_EN_STEREO) ? "stereo" : "mono";
			text_out16(x, y, "Sound Quality:     %5iHz %s", PsndRate, str);
			break;
		case MA_OPT_REGION:
			text_out16(x, y, "Region:              %s", me_region_name(PicoRegionOverride, PicoAutoRgnOrder));
			break;
		case MA_OPT_CONFIRM_STATES:
			switch ((currentConfig.EmuOpt >> 9) & 5) {
				default: str = "OFF";    break;
				case 1:  str = "writes"; break;
				case 4:  str = "loads";  break;
				case 5:  str = "both";   break;
			}
			text_out16(x, y, "Confirm savestate          %s", str);
			break;
		case MA_OPT_CPU_CLOCKS:
			text_out16(x, y, "GP2X CPU clocks            %iMhz", currentConfig.CPUclock);
			break;
		case MA_OPT_SAVECFG:
			str24[0] = 0;
			if (config_slot != 0) sprintf(str24, " (profile: %i)", config_slot);
			text_out16(x, y, "Save cfg as default%s", str24);
			break;
		case MA_OPT_LOADCFG:
			text_out16(x, y, "Load cfg from profile %i", config_slot);
			break;
		default:
			printf("%s: unimplemented (%i)\n", __FUNCTION__, entry->id);
			break;
	}
}



static void draw_menu_options(int menu_sel)
{
	int tl_x = 25, tl_y = 24;

	gp2x_pd_clone_buffer2();

	menu_draw_selection(tl_x - 16, tl_y + menu_sel*10, 284);

	me_draw(opt_entries, OPT_ENTRY_COUNT, tl_x, tl_y, menu_opt_cust_draw, NULL);

	menu_flip();
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
		if (!PicoRegionOverride) {
			for (i = 0; i < 6; i++)
				if (rgn_orders[i] == PicoAutoRgnOrder) break;
			if (i < 5) PicoAutoRgnOrder = rgn_orders[i+1];
			else PicoRegionOverride=1;
		}
		else PicoRegionOverride<<=1;
		if (PicoRegionOverride > 8) PicoRegionOverride = 8;
	} else {
		if (!PicoRegionOverride) {
			for (i = 0; i < 6; i++)
				if (rgn_orders[i] == PicoAutoRgnOrder) break;
			if (i > 0) PicoAutoRgnOrder = rgn_orders[i-1];
		}
		else PicoRegionOverride>>=1;
	}
}

static void menu_options_save(void)
{
	if (PicoRegionOverride) {
		// force setting possibly changed..
		Pico.m.pal = (PicoRegionOverride == 2 || PicoRegionOverride == 8) ? 1 : 0;
	}
	if (!(PicoOpt & POPT_6BTN_PAD)) {
		// unbind XYZ MODE, just in case
		unbind_action(0xf00, -1, -1);
	}
}

static int menu_loop_options(void)
{
	static int menu_sel = 0;
	int menu_sel_max, ret;
	unsigned long inp = 0;
	menu_id selected_id;

	me_enable(opt_entries, OPT_ENTRY_COUNT, MA_OPT_SAVECFG_GAME, rom_loaded);
	me_enable(opt_entries, OPT_ENTRY_COUNT, MA_OPT_LOADCFG, config_slot != config_slot_current);
	menu_sel_max = me_count_enabled(opt_entries, OPT_ENTRY_COUNT) - 1;
	if (menu_sel > menu_sel_max) menu_sel = menu_sel_max;

	while (1)
	{
		draw_menu_options(menu_sel);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_MOK|PBTN_MBACK);
		if (inp & PBTN_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if (inp & PBTN_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		selected_id = me_index2id(opt_entries, OPT_ENTRY_COUNT, menu_sel);
		if (inp & (PBTN_LEFT|PBTN_RIGHT)) { // multi choice
			if (!me_process(opt_entries, OPT_ENTRY_COUNT, selected_id, (inp&PBTN_RIGHT) ? 1 : 0)) {
				switch (selected_id) {
					case MA_OPT_RENDERER:
						if (inp & PBTN_LEFT) {
							if      (PicoOpt&0x10) PicoOpt&= ~0x10;
							else if (!(currentConfig.EmuOpt &0x80))currentConfig.EmuOpt |=  0x80;
							else if (  currentConfig.EmuOpt &0x80) break;
						} else {
							if      (PicoOpt&0x10) break;
							else if (!(currentConfig.EmuOpt &0x80))PicoOpt|=  0x10;
							else if (  currentConfig.EmuOpt &0x80) currentConfig.EmuOpt &= ~0x80;
						}
						break;
					case MA_OPT_SOUND_QUALITY:
						if ((inp & PBTN_RIGHT) && PsndRate == 44100 && !(PicoOpt&0x08)) {
							PsndRate = 8000; PicoOpt|= 0x08;
						} else if ((inp & PBTN_LEFT) && PsndRate == 8000 && (PicoOpt&0x08)) {
							PsndRate = 44100; PicoOpt&=~0x08;
						} else  PsndRate = sndrate_prevnext(PsndRate, inp & PBTN_RIGHT);
						break;
					case MA_OPT_REGION:
						region_prevnext(inp & PBTN_RIGHT);
						break;
					case MA_OPT_CONFIRM_STATES: {
							 int n = ((currentConfig.EmuOpt>>9)&1) | ((currentConfig.EmuOpt>>10)&2);
							 n += (inp & PBTN_LEFT) ? -1 : 1;
							 if (n < 0) n = 0; else if (n > 3) n = 3;
							 n |= n << 1; n &= ~2;
							 currentConfig.EmuOpt &= ~0xa00;
							 currentConfig.EmuOpt |= n << 9;
							 break;
						 }
					case MA_OPT_SAVE_SLOT:
						 if (inp & PBTN_RIGHT) {
							 state_slot++; if (state_slot > 9) state_slot = 0;
						 } else {state_slot--; if (state_slot < 0) state_slot = 9;
						 }
						 break;
					case MA_OPT_CPU_CLOCKS:
						 while ((inp = in_menu_wait_any(50)) & (PBTN_LEFT|PBTN_RIGHT)) {
							 currentConfig.CPUclock += (inp & PBTN_LEFT) ? -1 : 1;
							 if (currentConfig.CPUclock < 1) currentConfig.CPUclock = 1;
							 draw_menu_options(menu_sel);
						 }
						 break;
					case MA_OPT_SAVECFG:
					case MA_OPT_SAVECFG_GAME:
					case MA_OPT_LOADCFG:
						 config_slot += (inp&PBTN_RIGHT) ? 1 : -1;
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
		if (inp & PBTN_MOK) {
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
		if(inp & PBTN_MBACK) {
			menu_options_save();
			return 0;  // done (update, no write)
		}
	}
}

// -------------- credits --------------

static void draw_menu_credits(void)
{
	int tl_x = 15, tl_y = 56, y;
	gp2x_pd_clone_buffer2();

	text_out16(tl_x, 20, "PicoDrive v" VERSION " (c) notaz, 2006-2008");
	y = tl_y;
	text_out16(tl_x, y, "Credits:");
	text_out16(tl_x, (y+=10), "fDave: Cyclone 68000 core,");
	text_out16(tl_x, (y+=10), "      base code of PicoDrive");
	text_out16(tl_x, (y+=10), "Reesy & FluBBa: DrZ80 core");
	text_out16(tl_x, (y+=10), "MAME devs: YM2612 and SN76496 cores");
	text_out16(tl_x, (y+=10), "rlyeh and others: minimal SDK");
	text_out16(tl_x, (y+=10), "Squidge: squidgehack");
	text_out16(tl_x, (y+=10), "Dzz: ARM940 sample");
	text_out16(tl_x, (y+=10), "GnoStiC / Puck2099: USB joy code");
	text_out16(tl_x, (y+=10), "craigix: GP2X hardware");
	text_out16(tl_x, (y+=10), "ketchupgun: skin design");

	text_out16(tl_x, (y+=20), "special thanks (for docs, ideas):");
	text_out16(tl_x, (y+=10), " Charles MacDonald, Haze,");
	text_out16(tl_x, (y+=10), " Stephane Dallongeville,");
	text_out16(tl_x, (y+=10), " Lordus, Exophase, Rokas,");
	text_out16(tl_x, (y+=10), " Nemesis, Tasco Deluxe");

	menu_flip();
}


// -------------- root menu --------------

menu_entry main_entries[] =
{
	{ "Resume game",        MB_NONE, MA_MAIN_RESUME_GAME, NULL, 0, 0, 0, 0, 0 },
	{ "Save State",         MB_NONE, MA_MAIN_SAVE_STATE,  NULL, 0, 0, 0, 0, 0 },
	{ "Load State",         MB_NONE, MA_MAIN_LOAD_STATE,  NULL, 0, 0, 0, 0, 0 },
	{ "Reset game",         MB_NONE, MA_MAIN_RESET_GAME,  NULL, 0, 0, 0, 0, 0 },
	{ "Load new ROM/ISO",   MB_NONE, MA_MAIN_LOAD_ROM,    NULL, 0, 0, 0, 1, 0 },
	{ "Change options",     MB_NONE, MA_MAIN_OPTIONS,     NULL, 0, 0, 0, 1, 0 },
	{ "Configure controls", MB_NONE, MA_MAIN_CONTROLS,    NULL, 0, 0, 0, 1, 0 },
	{ "Credits",            MB_NONE, MA_MAIN_CREDITS,     NULL, 0, 0, 0, 1, 0 },
	{ "Patches / GameGenie",MB_NONE, MA_MAIN_PATCHES,     NULL, 0, 0, 0, 0, 0 },
	{ "Exit",               MB_NONE, MA_MAIN_EXIT,        NULL, 0, 0, 0, 1, 0 }
};

#define MAIN_ENTRY_COUNT (sizeof(main_entries) / sizeof(main_entries[0]))

static void draw_menu_root(int menu_sel)
{
	const int tl_x = 70, tl_y = 70;

	gp2x_pd_clone_buffer2();

	text_out16(tl_x, 20, "PicoDrive v" VERSION);

	menu_draw_selection(tl_x - 16, tl_y + menu_sel*10, 146);

	me_draw(main_entries, MAIN_ENTRY_COUNT, tl_x, tl_y, NULL, NULL);

	// error
	if (menuErrorMsg[0]) {
		memset((char *)gp2x_screen + 320*224*2, 0, 320*16*2);
		text_out16(5, 226, menuErrorMsg);
	}
	menu_flip();
}


static void menu_loop_root(void)
{
	static int menu_sel = 0;
	int ret, menu_sel_max;
	unsigned long inp = 0;

	me_enable(main_entries, MAIN_ENTRY_COUNT, MA_MAIN_RESUME_GAME, rom_loaded);
	me_enable(main_entries, MAIN_ENTRY_COUNT, MA_MAIN_SAVE_STATE,  rom_loaded);
	me_enable(main_entries, MAIN_ENTRY_COUNT, MA_MAIN_LOAD_STATE,  rom_loaded);
	me_enable(main_entries, MAIN_ENTRY_COUNT, MA_MAIN_RESET_GAME,  rom_loaded);
	me_enable(main_entries, MAIN_ENTRY_COUNT, MA_MAIN_PATCHES,     PicoPatches != NULL);

	menu_sel_max = me_count_enabled(main_entries, MAIN_ENTRY_COUNT) - 1;
	if (menu_sel > menu_sel_max) menu_sel = menu_sel_max;

	/* make sure action buttons are not pressed on entering menu */
	draw_menu_root(menu_sel);
	while (in_menu_wait_any(50) & (PBTN_MOK|PBTN_MBACK|PBTN_MENU));

	for (;;)
	{
		draw_menu_root(menu_sel);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_MOK|PBTN_MBACK|PBTN_MENU|PBTN_L|PBTN_R);
		if(inp & PBTN_UP  )  { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if(inp & PBTN_DOWN)  { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		if((inp & (PBTN_L|PBTN_R)) == (PBTN_L|PBTN_R)) debug_menu_loop();
		if(inp &(PBTN_MENU|PBTN_MBACK)){
			if (rom_loaded) {
				while (in_menu_wait_any(50) & (PBTN_MENU|PBTN_MBACK)); // wait until select is released
				engineState = PGS_Running;
				break;
			}
		}
		if(inp & PBTN_MOK)  {
			switch (me_index2id(main_entries, MAIN_ENTRY_COUNT, menu_sel))
			{
				case MA_MAIN_RESUME_GAME:
					if (rom_loaded) {
						while (in_menu_wait_any(50) & PBTN_MOK);
						engineState = PGS_Running;
						return;
					}
					break;
				case MA_MAIN_SAVE_STATE:
					if (rom_loaded) {
						if(savestate_menu_loop(0))
							continue;
						engineState = PGS_Running;
						return;
					}
					break;
				case MA_MAIN_LOAD_STATE:
					if (rom_loaded) {
						if(savestate_menu_loop(1))
							continue;
						while (in_menu_wait_any(50) & PBTN_MOK);
						engineState = PGS_Running;
						return;
					}
					break;
				case MA_MAIN_RESET_GAME:
					if (rom_loaded) {
						emu_ResetGame();
						while (in_menu_wait_any(50) & PBTN_MOK);
						engineState = PGS_Running;
						return;
					}
					break;
				case MA_MAIN_LOAD_ROM:
				{
					char curr_path[PATH_MAX], *selfname;
					FILE *tstf;
					if ( (tstf = fopen(loadedRomFName, "rb")) )
					{
						fclose(tstf);
						strcpy(curr_path, loadedRomFName);
					}
					else
						getcwd(curr_path, PATH_MAX);
					selfname = romsel_loop(curr_path);
					if (selfname) {
						printf("selected file: %s\n", selfname);
						engineState = PGS_ReloadRom;
						return;
					}
					break;
				}
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
					inp = in_menu_wait(PBTN_MOK|PBTN_MBACK);
					break;
				case MA_MAIN_EXIT:
					engineState = PGS_Quit;
					return;
				case MA_MAIN_PATCHES:
					if (rom_loaded && PicoPatches) {
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

void menu_darken_bg(void *dst, int pixels, int darker)
{
	unsigned int *screen = dst;
	pixels /= 2;
	if (darker)
	{
		while (pixels--)
		{
			unsigned int p = *screen;
			*screen++ = ((p&0xf79ef79e)>>1) - ((p&0xc618c618)>>3);
		}
	}
	else
	{
		while (pixels--)
		{
			unsigned int p = *screen;
			*screen++ = (p&0xf79ef79e)>>1;
		}
	}
}

static void menu_prepare_bg(int use_game_bg)
{
	if (use_game_bg)
	{
		// darken the active framebuffer
		memset(gp2x_screen, 0, 320*8*2);
		menu_darken_bg((char *)gp2x_screen + 320*8*2, 320*224, 1);
		memset((char *)gp2x_screen + 320*232*2, 0, 320*8*2);
	}
	else
	{
		// should really only happen once, on startup..
		readpng(gp2x_screen, "skin/background.png", READPNG_BG);
	}

	// copy to buffer2
	gp2x_memcpy_buffers((1<<2), gp2x_screen, 0, 320*240*2);
}

static void menu_gfx_prepare(void)
{
	menu_prepare_bg(rom_loaded);

	// switch to 16bpp
	gp2x_video_changemode2(16);
	gp2x_video_RGB_setscaling(0, 320, 240);
	menu_flip();
}


void menu_loop(void)
{
	in_set_blocking(1);
	menu_gfx_prepare();

	menu_loop_root();

	in_set_blocking(0);
	menuErrorMsg[0] = 0;
}


// --------- CD tray close menu ----------

static void draw_menu_tray(int menu_sel)
{
	int tl_x = 70, tl_y = 90, y;
	memset(gp2x_screen, 0, 320*240*2);

	text_out16(tl_x, 20, "The unit is about to");
	text_out16(tl_x, 30, "close the CD tray.");

	y = tl_y;
	text_out16(tl_x, y,       "Load new CD image");
	text_out16(tl_x, (y+=10), "Insert nothing");

	// draw cursor
	text_out16(tl_x - 16, tl_y + menu_sel*10, ">");
	// error
	if (menuErrorMsg[0]) text_out16(5, 226, menuErrorMsg);
	menu_flip();
}


int menu_loop_tray(void)
{
	int menu_sel = 0, menu_sel_max = 1;
	unsigned long inp = 0;
	char curr_path[PATH_MAX], *selfname;
	FILE *tstf;

	gp2x_memset_all_buffers(0, 0, 320*240*2);
	menu_gfx_prepare();

	if ( (tstf = fopen(loadedRomFName, "rb")) )
	{
		fclose(tstf);
		strcpy(curr_path, loadedRomFName);
	}
	else
	{
		getcwd(curr_path, PATH_MAX);
	}

	/* make sure action buttons are not pressed on entering menu */
	draw_menu_tray(menu_sel);
	while (in_menu_wait_any(50) & PBTN_MOK);

	for (;;)
	{
		draw_menu_tray(menu_sel);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_MOK);
		if(inp & PBTN_UP  )  { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if(inp & PBTN_DOWN)  { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		if(inp & PBTN_MOK   )  {
			switch (menu_sel) {
				case 0: // select image
					selfname = romsel_loop(curr_path);
					if (selfname) {
						int ret = -1;
						cd_img_type cd_type;
						cd_type = emu_cdCheck(NULL, romFileName);
						if (cd_type != CIT_NOT_CD)
							ret = Insert_CD(romFileName, cd_type);
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


