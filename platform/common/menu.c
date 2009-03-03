// (c) Copyright 2006,2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "menu.h"
#include "fonts.h"
#include "readpng.h"
#include "lprintf.h"
#include "common.h"
#include "input.h"
#include "emu.h"
#include "plat.h"
#include "posix.h"

#include <pico/pico_int.h>
#include <pico/patch.h>
#include <zlib/zlib.h>

#define array_size(x) (sizeof(x) / sizeof(x[0]))

static char static_buff[64];
char menuErrorMsg[64] = { 0, };


#ifndef UIQ3

static unsigned char menu_font_data[10240];
static int menu_text_color = 0xffff; // default to white
static int menu_sel_color = -1; // disabled

// draws text to current bbp16 screen
static void text_out16_(int x, int y, const char *text, int color)
{
	int i, l, u, tr, tg, tb, len;
	unsigned short *dest = (unsigned short *)SCREEN_BUFFER + x + y*SCREEN_WIDTH;
	tr = (color & 0xf800) >> 8;
	tg = (color & 0x07e0) >> 3;
	tb = (color & 0x001f) << 3;

	if (text == (void *)1)
	{
		// selector symbol
		text = "";
		len = 1;
	}
	else
	{
		const char *p;
		for (p = text; *p != 0 && *p != '\n'; p++)
			;
		len = p - text;
	}

	for (i = 0; i < len; i++)
	{
		unsigned char  *src = menu_font_data + (unsigned int)text[i]*4*10;
		unsigned short *dst = dest;
		for (l = 0; l < 10; l++, dst += SCREEN_WIDTH-8)
		{
			for (u = 8/2; u > 0; u--, src++)
			{
				int c, r, g, b;
				c = *src >> 4;
				r = (*dst & 0xf800) >> 8;
				g = (*dst & 0x07e0) >> 3;
				b = (*dst & 0x001f) << 3;
				r = (c^0xf)*r/15 + c*tr/15;
				g = (c^0xf)*g/15 + c*tg/15;
				b = (c^0xf)*b/15 + c*tb/15;
				*dst++ = ((r<<8)&0xf800) | ((g<<3)&0x07e0) | (b>>3);
				c = *src & 0xf;
				r = (*dst & 0xf800) >> 8;
				g = (*dst & 0x07e0) >> 3;
				b = (*dst & 0x001f) << 3;
				r = (c^0xf)*r/15 + c*tr/15;
				g = (c^0xf)*g/15 + c*tg/15;
				b = (c^0xf)*b/15 + c*tb/15;
				*dst++ = ((r<<8)&0xf800) | ((g<<3)&0x07e0) | (b>>3);
			}
		}
		dest += 8;
	}
}

void text_out16(int x, int y, const char *texto, ...)
{
	va_list args;
	char    buffer[256];
	int     maxw = (SCREEN_WIDTH - x) / 8;

	if (maxw < 0)
		return;

	va_start(args, texto);
	vsnprintf(buffer, sizeof(buffer), texto, args);
	va_end(args);

	if (maxw > sizeof(buffer) - 1)
		maxw = sizeof(buffer) - 1;
	buffer[maxw] = 0;

	text_out16_(x,y,buffer,menu_text_color);
}


static void smalltext_out16_(int x, int y, const char *texto, int color)
{
	int i;
	unsigned char  *src;
	unsigned short *dst;

	for (i = 0;; i++, x += 6)
	{
		unsigned char c = (unsigned char) texto[i];
		int h = 8;

		if (!c) break;

		src = fontdata6x8[c];
		dst = (unsigned short *)SCREEN_BUFFER + x + y*SCREEN_WIDTH;

		while (h--)
		{
			int w = 0x20;
			while (w)
			{
				if( *src & w ) *dst = color;
				dst++;
				w>>=1;
			}
			src++;

			dst += SCREEN_WIDTH-6;
		}
	}
}

void smalltext_out16(int x, int y, const char *texto, int color)
{
	char buffer[SCREEN_WIDTH/6+1];
	int maxw = (SCREEN_WIDTH - x) / 6;

	if (maxw < 0)
		return;

	strncpy(buffer, texto, sizeof(buffer));
	if (maxw > sizeof(buffer) - 1)
		maxw = sizeof(buffer) - 1;
	buffer[maxw] = 0;

	smalltext_out16_(x, y, buffer, color);
}

static void menu_draw_selection(int x, int y, int w)
{
	int i, h;
	unsigned short *dst, *dest;

	text_out16_(x, y, (void *)1, (menu_sel_color < 0) ? menu_text_color : menu_sel_color);

	if (menu_sel_color < 0) return; // no selection hilight

	if (y > 0) y--;
	dest = (unsigned short *)SCREEN_BUFFER + x + y*SCREEN_WIDTH + 14;
	for (h = 11; h > 0; h--)
	{
		dst = dest;
		for (i = w; i > 0; i--)
			*dst++ = menu_sel_color;
		dest += SCREEN_WIDTH;
	}
}

static int parse_hex_color(char *buff)
{
	char *endp = buff;
	int t = (int) strtoul(buff, &endp, 16);
	if (endp != buff)
#ifdef PSP
		return ((t<<8)&0xf800) | ((t>>5)&0x07e0) | ((t>>19)&0x1f);
#else
		return ((t>>8)&0xf800) | ((t>>5)&0x07e0) | ((t>>3)&0x1f);
#endif
	return -1;
}

void menu_init(void)
{
	int c, l;
	unsigned char *fd = menu_font_data;
	char buff[256];
	FILE *f;

	// generate default font from fontdata8x8
	memset(menu_font_data, 0, sizeof(menu_font_data));
	for (c = 0; c < 256; c++)
	{
		for (l = 0; l < 8; l++)
		{
			unsigned char fd8x8 = fontdata8x8[c*8+l];
			if (fd8x8&0x80) *fd |= 0xf0;
			if (fd8x8&0x40) *fd |= 0x0f; fd++;
			if (fd8x8&0x20) *fd |= 0xf0;
			if (fd8x8&0x10) *fd |= 0x0f; fd++;
			if (fd8x8&0x08) *fd |= 0xf0;
			if (fd8x8&0x04) *fd |= 0x0f; fd++;
			if (fd8x8&0x02) *fd |= 0xf0;
			if (fd8x8&0x01) *fd |= 0x0f; fd++;
		}
		fd += 8*2/2; // 2 empty lines
	}

	// load custom font and selector (stored as 1st symbol in font table)
	readpng(menu_font_data, "skin/font.png", READPNG_FONT);
	memcpy(menu_font_data, menu_font_data + ((int)'>')*4*10, 4*10); // default selector symbol is '>'
	readpng(menu_font_data, "skin/selector.png", READPNG_SELECTOR);

	// load custom colors
	f = fopen("skin/skin.txt", "r");
	if (f != NULL)
	{
		lprintf("found skin.txt\n");
		while (!feof(f))
		{
			fgets(buff, sizeof(buff), f);
			if (buff[0] == '#'  || buff[0] == '/')  continue; // comment
			if (buff[0] == '\r' || buff[0] == '\n') continue; // empty line
			if (strncmp(buff, "text_color=", 11) == 0)
			{
				int tmp = parse_hex_color(buff+11);
				if (tmp >= 0) menu_text_color = tmp;
				else lprintf("skin.txt: parse error for text_color\n");
			}
			else if (strncmp(buff, "selection_color=", 16) == 0)
			{
				int tmp = parse_hex_color(buff+16);
				if (tmp >= 0) menu_sel_color = tmp;
				else lprintf("skin.txt: parse error for selection_color\n");
			}
			else
				lprintf("skin.txt: parse error: %s\n", buff);
		}
		fclose(f);
	}
}


static int me_id2offset(const menu_entry *ent, menu_id id)
{
	int i;
	for (i = 0; ent->name; ent++, i++)
		if (ent->id == id) return i;

	lprintf("%s: id %i not found\n", __FUNCTION__, id);
	return 0;
}

static void me_enable(menu_entry *entries, menu_id id, int enable)
{
	int i = me_id2offset(entries, id);
	entries[i].enabled = enable;
}

static int me_count(const menu_entry *ent)
{
	int ret;

	for (ret = 0; ent->name; ent++, ret++)
		;

	return ret;
}

static void me_draw(const menu_entry *entries, int sel)
{
	const menu_entry *ent;
	int x, y, w = 0, h = 0;
	int offs, opt_offs = 27*8;
	const char *name;
	int asel = 0;
	int i, n;

	/* calculate size of menu rect */
	for (ent = entries, i = n = 0; ent->name; ent++, i++)
	{
		int wt;

		if (!ent->enabled)
			continue;

		if (i == sel)
			asel = n;

		name = NULL;
		wt = strlen(ent->name) * 8;	/* FIXME: unhardcode font width */
		if (wt == 0 && ent->generate_name)
			name = ent->generate_name(ent->id, &offs);
		if (name != NULL)
			wt = strlen(name) * 8;

		if (ent->beh != MB_NONE)
		{
			if (wt > opt_offs)
				opt_offs = wt + 8;
			wt = opt_offs;

			switch (ent->beh) {
			case MB_NONE: break;
			case MB_OPT_ONOFF:
			case MB_OPT_RANGE: wt += 8*3; break;
			case MB_OPT_CUSTOM:
			case MB_OPT_CUSTONOFF:
			case MB_OPT_CUSTRANGE:
				name = NULL;
				offs = 0;
				if (ent->generate_name != NULL)
					name = ent->generate_name(ent->id, &offs);
				if (name != NULL)
					wt += (strlen(name) + offs) * 8;
				break;
			}
		}

		if (wt > w)
			w = wt;
		n++;
	}
	h = n * 10;
	w += 16; /* selector */

	if (w > SCREEN_WIDTH) {
		lprintf("width %d > %d\n", w, SCREEN_WIDTH);
		w = SCREEN_WIDTH;
	}
	if (h > SCREEN_HEIGHT) {
		lprintf("height %d > %d\n", w, SCREEN_HEIGHT);
		h = SCREEN_HEIGHT;
	}

	x = SCREEN_WIDTH  / 2 - w / 2;
	y = SCREEN_HEIGHT / 2 - h / 2;

	/* draw */
	plat_video_menu_begin();
	menu_draw_selection(x, y + asel * 10, w);

	for (ent = entries; ent->name; ent++)
	{
		if (!ent->enabled)
			continue;

		name = ent->name;
		if (strlen(name) == 0) {
			if (ent->generate_name)
				name = ent->generate_name(ent->id, &offs);
		}
		if (name != NULL)
			text_out16(x + 16, y, name);

		switch (ent->beh) {
		case MB_NONE:
			break;
		case MB_OPT_ONOFF:
			text_out16(x + 16 + opt_offs, y, (*(int *)ent->var & ent->mask) ? "ON" : "OFF");
			break;
		case MB_OPT_RANGE:
			text_out16(x + 16 + opt_offs, y, "%i", *(int *)ent->var);
			break;
		case MB_OPT_CUSTOM:
		case MB_OPT_CUSTONOFF:
		case MB_OPT_CUSTRANGE:
			name = NULL;
			offs = 0;
			if (ent->generate_name)
				name = ent->generate_name(ent->id, &offs);
			if (name != NULL)
				text_out16(x + 16 + opt_offs + offs * 8, y, "%s", name);
			break;
		}

		y += 10;
	}

	/* display message if we have one */
	if (menuErrorMsg[0] != 0) {
		static int msg_redraws = 0;
		if (SCREEN_HEIGHT - h >= 2*10)
			text_out16(5, 226, menuErrorMsg);
		else
			lprintf("menu msg doesn't fit!\n");

		if (++msg_redraws > 4) {
			menuErrorMsg[0] = 0;
			msg_redraws = 0;
		}
	}

	plat_video_menu_end();
}

static int me_process(menu_entry *entry, int is_next)
{
	switch (entry->beh)
	{
		case MB_OPT_ONOFF:
		case MB_OPT_CUSTONOFF:
			*(int *)entry->var ^= entry->mask;
			return 1;
		case MB_OPT_RANGE:
		case MB_OPT_CUSTRANGE:
			*(int *)entry->var += is_next ? 1 : -1;
			if (*(int *)entry->var < (int)entry->min)
				*(int *)entry->var = (int)entry->max;
			if (*(int *)entry->var > (int)entry->max)
				*(int *)entry->var = (int)entry->min;
			return 1;
		default:
			return 0;
	}
}

static void debug_menu_loop(void);

static void me_loop(menu_entry *menu, int *menu_sel)
{
	int ret, inp, sel = *menu_sel, menu_sel_max;

	menu_sel_max = me_count(menu) - 1;
	if (menu_sel_max < 1) {
		lprintf("no enabled menu entries\n");
		return;
	}

	while (!menu[sel].enabled && sel < menu_sel_max)
		sel++;

	/* make sure action buttons are not pressed on entering menu */
	me_draw(menu, sel);
	while (in_menu_wait_any(50) & (PBTN_MOK|PBTN_MBACK|PBTN_MENU));

	for (;;)
	{
		me_draw(menu, sel);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|
					PBTN_MOK|PBTN_MBACK|PBTN_MENU|PBTN_L|PBTN_R, 70);
		if (inp & (PBTN_MENU|PBTN_MBACK))
			break;

		if (inp & PBTN_UP  ) {
			do {
				sel--;
				if (sel < 0)
					sel = menu_sel_max;
			}
			while (!menu[sel].enabled || !menu[sel].selectable);
		}
		if (inp & PBTN_DOWN) {
			do {
				sel++;
				if (sel > menu_sel_max)
					sel = 0;
			}
			while (!menu[sel].enabled || !menu[sel].selectable);
		}

		/* a bit hacky but oh well */
		if ((inp & (PBTN_L|PBTN_R)) == (PBTN_L|PBTN_R))
			debug_menu_loop();

		if (inp & (PBTN_LEFT|PBTN_RIGHT)) { /* multi choice */
			if (me_process(&menu[sel], (inp & PBTN_RIGHT) ? 1 : 0))
				continue;
		}

		if (inp & (PBTN_MOK|PBTN_LEFT|PBTN_RIGHT))
		{
			if (menu[sel].handler != NULL) {
				ret = menu[sel].handler(menu[sel].id, inp);
				if (ret) break;
				menu_sel_max = me_count(menu) - 1; /* might change */
			}
		}
	}
	*menu_sel = sel;
}

/* ***************************************** */

static void draw_menu_credits(void)
{
	const char *creds, *p;
	int x, y, h, w, wt;

	p = creds = plat_get_credits();

	for (h = 1, w = 0; *p != 0; h++) {
		for (wt = 0; *p != 0 && *p != '\n'; p++)
			wt++;

		if (wt > w)
			w = wt;
		if (*p == 0)
			break;
		p++;
	}

	x = SCREEN_WIDTH  / 2 - w *  8 / 2;
	y = SCREEN_HEIGHT / 2 - h * 10 / 2;
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	plat_video_menu_begin();

	for (p = creds; *p != 0 && y <= SCREEN_HEIGHT - 10; y += 10) {
		text_out16(x, y, p);

		for (; *p != 0 && *p != '\n'; p++)
			;
		if (*p != 0)
			p++;
	}

	plat_video_menu_end();
}

// --------- loading ROM screen ----------

static int cdload_called = 0;

static void load_progress_cb(int percent)
{
	int ln, len = percent * SCREEN_WIDTH / 100;
	unsigned short *dst = (unsigned short *)SCREEN_BUFFER + SCREEN_WIDTH * 10 * 2;

	if (len > SCREEN_WIDTH)
		len = SCREEN_WIDTH;
	for (ln = 10 - 2; ln > 0; ln--, dst += SCREEN_WIDTH)
		memset(dst, 0xff, len * 2);
	plat_video_menu_end();
}

static void cdload_progress_cb(int percent)
{
	int ln, len = percent * SCREEN_WIDTH / 100;
	unsigned short *dst = (unsigned short *)SCREEN_BUFFER + SCREEN_WIDTH * 10 * 2;

	memset(dst, 0xff, SCREEN_WIDTH * (10 - 2) * 2);

	smalltext_out16(1, 3 * 10, "Processing CD image / MP3s", 0xffff);
	smalltext_out16(1, 4 * 10, rom_fname_loaded, 0xffff);
	dst += SCREEN_WIDTH * 30;

	if (len > SCREEN_WIDTH)
		len = SCREEN_WIDTH;
	for (ln = (10 - 2); ln > 0; ln--, dst += SCREEN_WIDTH)
		memset(dst, 0xff, len * 2);

	plat_video_menu_end();
	cdload_called = 1;
}

void menu_romload_prepare(const char *rom_name)
{
	const char *p = rom_name + strlen(rom_name);

	plat_video_menu_begin();

	while (p > rom_name && *p != '/')
		p--;

	/* fill both buffers, callbacks won't update in full */
	smalltext_out16(1, 1, "Loading", 0xffff);
	smalltext_out16(1, 10, p, 0xffff);
	plat_video_menu_end();

	smalltext_out16(1, 1, "Loading", 0xffff);
	smalltext_out16(1, 10, p, 0xffff);
	plat_video_menu_end();

	PicoCartLoadProgressCB = load_progress_cb;
	PicoCDLoadProgressCB = cdload_progress_cb;
	cdload_called = 0;
}

void menu_romload_end(void)
{
	PicoCartLoadProgressCB = PicoCDLoadProgressCB = NULL;
	smalltext_out16(1, (cdload_called ? 6 : 3) * 10,
		"Starting emulation...", 0xffff);
	plat_video_menu_end();
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
	for (i = 0; i < array_size(rom_exts); i++)
		if (strcasecmp(ext, rom_exts[i]) == 0) return 0xbdff; // FIXME: mk defines
	for (i = 0; i < array_size(other_exts); i++)
		if (strcasecmp(ext, other_exts[i]) == 0) return 0xaff5;
	return 0xffff;
}

static void draw_dirlist(char *curdir, struct dirent **namelist, int n, int sel)
{
	int max_cnt, start, i, pos;

	max_cnt = SCREEN_HEIGHT / 10;
	start = max_cnt / 2 - sel;
	n--; // exclude current dir (".")

	plat_video_menu_begin();

//	if (!rom_loaded)
//		menu_darken_bg(gp2x_screen, 320*240, 0);

	menu_darken_bg((short *)SCREEN_BUFFER + SCREEN_WIDTH * max_cnt/2 * 10, SCREEN_WIDTH * 8, 0);

	if (start - 2 >= 0)
		smalltext_out16(14, (start - 2)*10, curdir, 0xffff);
	for (i = 0; i < n; i++) {
		pos = start + i;
		if (pos < 0)  continue;
		if (pos >= max_cnt) break;
		if (namelist[i+1]->d_type == DT_DIR) {
			smalltext_out16(14,   pos*10, "/", 0xfff6);
			smalltext_out16(14+6, pos*10, namelist[i+1]->d_name, 0xfff6);
		} else {
			unsigned short color = file2color(namelist[i+1]->d_name);
			smalltext_out16(14,   pos*10, namelist[i+1]->d_name, color);
		}
	}
	text_out16(5, max_cnt/2 * 10, ">");
	plat_video_menu_end();
}

static int scandir_cmp(const void *p1, const void *p2)
{
	struct dirent **d1 = (struct dirent **)p1, **d2 = (struct dirent **)p2;
	if ((*d1)->d_type == (*d2)->d_type) return alphasort(d1, d2);
	if ((*d1)->d_type == DT_DIR) return -1; // put before
	if ((*d2)->d_type == DT_DIR) return  1;
	return alphasort(d1, d2);
}

static const char *filter_exts[] = {
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

	for (i = 0; i < array_size(filter_exts); i++)
		if (strcmp(p, filter_exts[i]) == 0)
			return 0;

	return 1;
}

static char *menu_loop_romsel(char *curr_path, int len)
{
	struct dirent **namelist;
	int n, inp, sel = 0;
	char *ret = NULL, *fname = NULL;

rescan:
	// is this a dir or a full path?
	if (!plat_is_dir(curr_path)) {
		char *p = curr_path + strlen(curr_path) - 1;
		for (; p > curr_path && *p != '/'; p--)
			;
		*p = 0;
		fname = p+1;
	}

	n = scandir(curr_path, &namelist, scandir_filter, scandir_cmp);
	if (n < 0) {
		lprintf("menu_loop_romsel failed, dir: %s\n", curr_path);

		// try root
		getcwd(curr_path, len);
		n = scandir(curr_path, &namelist, scandir_filter, scandir_cmp);
		if (n < 0) {
			// oops, we failed
			lprintf("menu_loop_romsel failed, dir: %s\n", curr_path);
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
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|
			PBTN_L|PBTN_R|PBTN_WEST|PBTN_MOK|PBTN_MBACK|PBTN_MENU, 33); // TODO L R
		if (inp & PBTN_UP  )  { sel--;   if (sel < 0)   sel = n-2; }
		if (inp & PBTN_DOWN)  { sel++;   if (sel > n-2) sel = 0; }
		if (inp & PBTN_LEFT)  { sel-=10; if (sel < 0)   sel = 0; }
		if (inp & PBTN_L)     { sel-=24; if (sel < 0)   sel = 0; }
		if (inp & PBTN_RIGHT) { sel+=10; if (sel > n-2) sel = n-2; }
		if (inp & PBTN_R)     { sel+=24; if (sel > n-2) sel = n-2; }
		if ((inp & PBTN_MOK) || (inp & (PBTN_MENU|PBTN_WEST)) == (PBTN_MENU|PBTN_WEST)) // enter dir/select || delete
		{
			again:
			if (namelist[sel+1]->d_type == DT_REG)
			{
				strcpy(rom_fname_reload, curr_path);
				strcat(rom_fname_reload, "/");
				strcat(rom_fname_reload, namelist[sel+1]->d_name);
				if (inp & PBTN_MOK) { // return sel
					ret = rom_fname_reload;
					break;
				}
//				do_delete(rom_fname_reload, namelist[sel+1]->d_name); // TODO
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
				ret = menu_loop_romsel(newdir, newlen);
				free(newdir);
				break;
			}
			else
			{
				// unknown file type, happens on NTFS mounts. Try to guess.
				FILE *tstf; int tmp;
				strcpy(rom_fname_reload, curr_path);
				strcat(rom_fname_reload, "/");
				strcat(rom_fname_reload, namelist[sel+1]->d_name);
				tstf = fopen(rom_fname_reload, "rb");
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
		if (inp & PBTN_MBACK)
			break;
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
	int max_cnt, start, i, pos, active;

	max_cnt = SCREEN_HEIGHT / 10;
	start = max_cnt / 2 - sel;

	plat_video_menu_begin();

	for (i = 0; i < PicoPatchCount; i++) {
		pos = start + i;
		if (pos < 0) continue;
		if (pos >= max_cnt) break;
		active = PicoPatches[i].active;
		smalltext_out16(14,     pos*10, active ? "ON " : "OFF", active ? 0xfff6 : 0xffff);
		smalltext_out16(14+6*4, pos*10, PicoPatches[i].name,    active ? 0xfff6 : 0xffff);
	}
	pos = start + i;
	if (pos < max_cnt)
		smalltext_out16(14, pos * 10, "done", 0xffff);

	text_out16(5, max_cnt / 2 * 10, ">");
	plat_video_menu_end();
}

static void menu_loop_patches(void)
{
	static int menu_sel = 0;
	int inp;

	for (;;)
	{
		draw_patchlist(menu_sel);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_L|PBTN_R|PBTN_MOK|PBTN_MBACK, 33);
		if (inp & PBTN_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = PicoPatchCount; }
		if (inp & PBTN_DOWN) { menu_sel++; if (menu_sel > PicoPatchCount) menu_sel = 0; }
		if (inp &(PBTN_LEFT|PBTN_L))  { menu_sel-=10; if (menu_sel < 0) menu_sel = 0; }
		if (inp &(PBTN_RIGHT|PBTN_R)) { menu_sel+=10; if (menu_sel > PicoPatchCount) menu_sel = PicoPatchCount; }
		if (inp & PBTN_MOK) { // action
			if (menu_sel < PicoPatchCount)
				PicoPatches[menu_sel].active = !PicoPatches[menu_sel].active;
			else 	break;
		}
		if (inp & PBTN_MBACK)
			break;
	}
}

// ------------ savestate loader ------------

static int state_slot_flags = 0;

static void state_check_slots(void)
{
	int slot;

	state_slot_flags = 0;

	for (slot = 0; slot < 10; slot++) {
		if (emu_checkSaveFile(slot))
			state_slot_flags |= 1 << slot;
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

	/* do a frame and fetch menu bg */
	emu_forcedFrame(POPT_EN_SOFTSCALE);
	plat_video_menu_enter(1);

	memcpy(Pico.vram, tmp_vram, sizeof(Pico.vram));
	memcpy(Pico.cram, tmp_cram, sizeof(Pico.cram));
	memcpy(Pico.vsram, tmp_vsram, sizeof(Pico.vsram));
	memcpy(&Pico.video, &tmp_pv,  sizeof(Pico.video));
	free(tmp_vram);
}

static void draw_savestate_menu(int menu_sel, int is_loading)
{
	int i, x, y, w, h;

	if (state_slot_flags & (1 << menu_sel))
		draw_savestate_bg(menu_sel);

	w = 13 * 8 + 16;
	h = (1+2+10+1) * 10;
	x = SCREEN_WIDTH / 2 - w / 2;
	if (x < 0) x = 0;
	y = SCREEN_HEIGHT / 2 - h / 2;
	if (y < 0) y = 0;

	plat_video_menu_begin();

	text_out16(x, y, is_loading ? "Load state" : "Save state");
	y += 3*10;

	menu_draw_selection(x - 16, y + menu_sel * 10, 13 * 8 + 4);

	/* draw all 10 slots */
	y += 10;
	for (i = 0; i < 10; i++, y += 10)
	{
		text_out16(x, y, "SLOT %i (%s)", i, (state_slot_flags & (1 << i)) ? "USED" : "free");
	}
	text_out16(x, y, "back");

	plat_video_menu_end();
}

static int menu_loop_savestate(int is_loading)
{
	static int menu_sel = 10;
	int menu_sel_max = 10;
	unsigned long inp = 0;

	state_check_slots();

	for (;;)
	{
		draw_savestate_menu(menu_sel, is_loading);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_MOK|PBTN_MBACK, 100);
		if (inp & PBTN_UP) {
			do {
				menu_sel--;
				if (menu_sel < 0)
					menu_sel = menu_sel_max;
			} while (!(state_slot_flags & (1 << menu_sel)) && menu_sel != menu_sel_max && is_loading);
		}
		if (inp & PBTN_DOWN) {
			do {
				menu_sel++;
				if (menu_sel > menu_sel_max)
					menu_sel = 0;
			} while (!(state_slot_flags & (1 << menu_sel)) && menu_sel != menu_sel_max && is_loading);
		}
		if (inp & PBTN_MOK) { // save/load
			if (menu_sel < 10) {
				state_slot = menu_sel;
				if (emu_SaveLoadGame(is_loading, 0)) {
					strcpy(menuErrorMsg, is_loading ? "Load failed" : "Save failed");
					return 0;
				}
				return 1;
			}
			return 0;
		}
		if (inp & PBTN_MBACK)
			return 0;
	}
}

// -------------- key config --------------

static char *action_binds(int player_idx, int action_mask, int dev_id)
{
	const int *binds;
	int k, count;

	static_buff[0] = 0;

	binds = in_get_dev_binds(dev_id);
	if (binds == NULL)
		return static_buff;

	count = in_get_dev_bind_count(dev_id);
	for (k = 0; k < count; k++)
	{
		const char *xname;
		if (!(binds[k] & action_mask))
			continue;

		if (player_idx >= 0 && ((binds[k] >> 16) & 3) != player_idx)
			continue;

		xname = in_get_key_name(dev_id, k);
		if (static_buff[0])
			strncat(static_buff, " + ", sizeof(static_buff));
		strncat(static_buff, xname, sizeof(static_buff));
	}

	return static_buff;
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
		int sel, int dev_id, int dev_count, int is_bind)
{
	int x, y = 30, w, i;
	const char *dev_name;

	x = SCREEN_WIDTH / 2 - 32*8 / 2;
	if (x < 0) x = 0;

	plat_video_menu_begin();
	if (player_idx >= 0)
		text_out16(x, 10, "Player %i controls", player_idx + 1);
	else
		text_out16(x, 10, "Emulator controls");

	menu_draw_selection(x - 16, y + sel*10, (player_idx >= 0) ? 66 : 140);

	for (i = 0; i < opt_cnt; i++, y+=10)
		text_out16(x, y, "%s : %s", opts[i].name,
			action_binds(player_idx, opts[i].mask, dev_id));

	dev_name = in_get_dev_name(dev_id, 1, 1);
	w = strlen(dev_name) * 8;
	if (w < 30 * 8)
		w = 30 * 8;
	if (w > SCREEN_WIDTH)
		w = SCREEN_WIDTH;

	x = SCREEN_WIDTH / 2 - w / 2;

	if (dev_count > 1) {
		text_out16(x, SCREEN_HEIGHT - 4*10, "Viewing binds for:");
		text_out16(x, SCREEN_HEIGHT - 3*10, dev_name);
	}

	if (is_bind)
		text_out16(x, SCREEN_HEIGHT - 2*10, "Press a button to bind/unbind");
	else if (dev_count > 1)
		text_out16(x, SCREEN_HEIGHT - 2*10, "Press left/right for other devs");

	plat_video_menu_end();
}

static void key_config_loop(const me_bind_action *opts, int opt_cnt, int player_idx)
{
	int i, sel = 0, menu_sel_max = opt_cnt - 1;
	int dev_id, dev_count, kc, is_down, mkey, unbind;

	for (i = 0, dev_id = -1, dev_count = 0; i < IN_MAX_DEVS; i++) {
		if (in_get_dev_name(i, 1, 0) != NULL) {
			dev_count++;
			if (dev_id < 0)
				dev_id = i;
		}
	}

	if (dev_id == -1) {
		lprintf("no devs, can't do config\n");
		return;
	}

	for (;;)
	{
		draw_key_config(opts, opt_cnt, player_idx, sel, dev_id, dev_count, 0);
		mkey = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_MBACK|PBTN_MOK, 100);
		switch (mkey) {
			case PBTN_UP:   sel--; if (sel < 0) sel = menu_sel_max; continue;
			case PBTN_DOWN: sel++; if (sel > menu_sel_max) sel = 0; continue;
			case PBTN_LEFT:
				for (i = 0, dev_id--; i < IN_MAX_DEVS; i++, dev_id--) {
					if (dev_id < 0)
						dev_id = IN_MAX_DEVS - 1;
					if (in_get_dev_name(dev_id, 1, 0) != NULL)
						break;
				}
				continue;
			case PBTN_RIGHT:
				for (i = 0, dev_id++; i < IN_MAX_DEVS; i++, dev_id++) {
					if (dev_id >= IN_MAX_DEVS)
						dev_id = 0;
					if (in_get_dev_name(dev_id, 1, 0) != NULL)
						break;
				}
				continue;
			case PBTN_MBACK: return;
			case PBTN_MOK:
				if (sel >= opt_cnt)
					return;
				while (in_menu_wait_any(30) & PBTN_MOK);
				break;
			default:continue;
		}

		draw_key_config(opts, opt_cnt, player_idx, sel, dev_id, dev_count, 1);

		/* wait for some up event */
		for (is_down = 1; is_down; )
			kc = in_update_keycode(&dev_id, &is_down, -1);

		unbind = count_bound_keys(dev_id, opts[sel].mask, player_idx) >= 2;

		in_bind_key(dev_id, kc, opts[sel].mask, unbind);
		if (player_idx >= 0) {
			/* FIXME */
			in_bind_key(dev_id, kc, 3 << 16, 1);
			in_bind_key(dev_id, kc, player_idx << 16, 0);
		}
	}
}

// PicoPad[] format: MXYZ SACB RLDU
me_bind_action me_ctrl_actions[15] =
{
	{ "UP     ", 0x0001 },
	{ "DOWN   ", 0x0002 },
	{ "LEFT   ", 0x0004 },
	{ "RIGHT  ", 0x0008 },
	{ "A      ", 0x0040 },
	{ "B      ", 0x0010 },
	{ "C      ", 0x0020 },
	{ "A turbo", 0x4000 },
	{ "B turbo", 0x1000 },
	{ "C turbo", 0x2000 },
	{ "START  ", 0x0080 },
	{ "MODE   ", 0x0800 },
	{ "X      ", 0x0400 },
	{ "Y      ", 0x0200 },
	{ "Z      ", 0x0100 }
};

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

static int key_config_loop_wrap(menu_id id, int keys)
{
	switch (id) {
		case MA_CTRL_PLAYER1:
			key_config_loop(me_ctrl_actions, array_size(me_ctrl_actions), 0);
			break;
		case MA_CTRL_PLAYER2:
			key_config_loop(me_ctrl_actions, array_size(me_ctrl_actions), 1);
			break;
		case MA_CTRL_EMU:
			key_config_loop(emuctrl_actions, array_size(emuctrl_actions) - 1, -1);
			break;
		default:
			break;
	}
	return 0;
}

static const char *mgn_dev_name(menu_id id, int *offs)
{
	const char *name = NULL;
	static int it = 0;

	if (id == MA_CTRL_DEV_FIRST)
		it = 0;

	for (; it < IN_MAX_DEVS; it++) {
		name = in_get_dev_name(it, 1, 1);
		if (name != NULL)
			break;
	}

	it++;
	return name;
}

static int mh_saveloadcfg(menu_id id, int keys);
static const char *mgn_savecfg(menu_id id, int *offs);

static menu_entry e_menu_keyconfig[] =
{
	mee_handler_id("Player 1",          MA_CTRL_PLAYER1,    key_config_loop_wrap),
	mee_handler_id("Player 2",          MA_CTRL_PLAYER2,    key_config_loop_wrap),
	mee_handler_id("Emulator controls", MA_CTRL_EMU,        key_config_loop_wrap),
	mee_onoff     ("6 button pad",      MA_OPT_6BUTTON_PAD, PicoOpt, POPT_6BTN_PAD),
	mee_range     ("Turbo rate",        MA_CTRL_TURBO_RATE, currentConfig.turbo_rate, 1, 30),
	mee_handler_mkname_id(MA_OPT_SAVECFG, mh_saveloadcfg, mgn_savecfg),
	mee_handler_id("Save cfg for loaded game", MA_OPT_SAVECFG_GAME, mh_saveloadcfg),
	mee_label     (""),
	mee_label     ("Input devices:"),
	mee_label_mk  (MA_CTRL_DEV_FIRST, mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_label_mk  (MA_CTRL_DEV_NEXT,  mgn_dev_name),
	mee_end,
};

static int menu_loop_keyconfig(menu_id id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_keyconfig, &sel);
	return 0;
}

// ------------ SCD options menu ------------

static const char *mgn_cdopt_ra(menu_id id, int *offs)
{
	*offs = -5;
	if (PicoCDBuffers <= 0)
		return "     OFF";
	sprintf(static_buff, "%5iK", PicoCDBuffers * 2);
	return static_buff;
}

static int mh_cdopt_ra(menu_id id, int keys)
{
	if (keys & PBTN_LEFT) {
		PicoCDBuffers >>= 1;
		if (PicoCDBuffers < 2)
			PicoCDBuffers = 0;
	} else {
		if (PicoCDBuffers <= 0)
			PicoCDBuffers = 1;
		PicoCDBuffers <<= 1;
		if (PicoCDBuffers > 8*1024)
			PicoCDBuffers = 8*1024; // 16M
	}
	return 0;
}

static menu_entry e_menu_cd_options[] =
{
	mee_onoff("CD LEDs",              MA_CDOPT_LEDS,          currentConfig.EmuOpt, 0x0400),
	mee_onoff("CDDA audio",           MA_CDOPT_CDDA,          PicoOpt, POPT_EN_MCD_CDDA),
	mee_onoff("PCM audio",            MA_CDOPT_PCM,           PicoOpt, POPT_EN_MCD_PCM),
	mee_cust ("ReadAhead buffer",     MA_CDOPT_READAHEAD,     mh_cdopt_ra, mgn_cdopt_ra),
	mee_onoff("SaveRAM cart",         MA_CDOPT_SAVERAM,       PicoOpt, POPT_EN_MCD_RAMCART),
	mee_onoff("Scale/Rot. fx (slow)", MA_CDOPT_SCALEROT_CHIP, PicoOpt, POPT_EN_MCD_GFX),
	mee_onoff("Better sync (slow)",   MA_CDOPT_BETTER_SYNC,   PicoOpt, POPT_EN_MCD_PSYNC),
	mee_end,
};

static int menu_loop_cd_options(menu_id id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_cd_options, &sel);
	return 0;
}

// ------------ adv options menu ------------

// TODO FIXME fix if and mv
static const char *mgn_aopt_sqhack(menu_id id, int *offs)
{
	*offs = -10;
	sprintf(static_buff, "%s, %s", 111 ? "  active" : "inactive",
		(currentConfig.EmuOpt & 0x10) ? "ON" : "OFF");
	return static_buff;
}

static menu_entry e_menu_adv_options[] =
{
	mee_onoff     ("SRAM/BRAM saves",          MA_OPT_SRAM_STATES,    currentConfig.EmuOpt, EOPT_USE_SRAM),
	mee_onoff     ("Disable sprite limit",     MA_OPT2_NO_SPRITE_LIM, PicoOpt, POPT_DIS_SPRITE_LIM),
	mee_onoff     ("Use second CPU for sound", MA_OPT_ARM940_SOUND,   PicoOpt, POPT_EXT_FM),
	mee_onoff     ("Emulate Z80",              MA_OPT2_ENABLE_Z80,    PicoOpt, POPT_EN_Z80),
	mee_onoff     ("Emulate YM2612 (FM)",      MA_OPT2_ENABLE_YM2612, PicoOpt, POPT_EN_FM),
	mee_onoff     ("Emulate SN76496 (PSG)",    MA_OPT2_ENABLE_SN76496,PicoOpt, POPT_EN_PSG),
	mee_onoff     ("gzip savestates",          MA_OPT2_GZIP_STATES,   currentConfig.EmuOpt, EOPT_GZIP_SAVES),
	mee_onoff     ("Don't save last used ROM", MA_OPT2_NO_LAST_ROM,   currentConfig.EmuOpt, EOPT_NO_AUTOSVCFG),
	mee_label     ("- needs restart -"),
	mee_onoff     ("craigix's RAM timings",    MA_OPT2_RAMTIMINGS,    currentConfig.EmuOpt, 0x0100),
	mee_onoff_cust("Squidgehack",              MA_OPT2_SQUIDGEHACK,   currentConfig.EmuOpt, 0x0010, mgn_aopt_sqhack),
	mee_onoff     ("SVP dynarec",              MA_OPT2_SVP_DYNAREC,   PicoOpt, POPT_EN_SVP_DRC),
	mee_onoff     ("Disable idle loop patching",MA_OPT2_NO_IDLE_LOOPS,PicoOpt, POPT_DIS_IDLE_DET),
	mee_end,
};

static int menu_loop_adv_options(menu_id id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_adv_options, &sel);
	return 0;
}

// ------------ gfx options menu ------------

static const char *mgn_opt_scaling(menu_id id, int *offs)
{
	*offs = -12;
	switch (currentConfig.scaling) {
		default: return "            OFF";
		case 1:  return "hw horizontal";
		case 2:  return "hw horiz. + vert.";
		case 3:  return "sw horizontal";
	}
}

static const char *mgn_aopt_gamma(menu_id id, int *offs)
{
	sprintf(static_buff, "%i.%02i", currentConfig.gamma / 100, currentConfig.gamma%100);
	return static_buff;
}

static menu_entry e_menu_gfx_options[] =
{
	mee_range_cust("Scaling",                  MA_OPT_SCALING,        currentConfig.scaling, 0, 3, mgn_opt_scaling),
	mee_range_cust("Gamma correction",         MA_OPT2_GAMMA,         currentConfig.gamma, 1, 300, mgn_aopt_gamma),
	mee_onoff     ("A_SN's gamma curve",       MA_OPT2_A_SN_GAMMA,    currentConfig.EmuOpt, 0x1000),
	mee_onoff     ("Perfect vsync",            MA_OPT2_VSYNC,         currentConfig.EmuOpt, 0x2000),
	mee_end,
};

static int menu_loop_gfx_options(menu_id id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_gfx_options, &sel);
	return 0;
}

// ------------ options menu ------------

static menu_entry e_menu_options[];

/* TODO: move to plat */
static int mh_opt_render(menu_id id, int keys)
{
	if (keys & PBTN_LEFT) {
		if      (PicoOpt&0x10) PicoOpt&= ~0x10;
		else if (!(currentConfig.EmuOpt &0x80))currentConfig.EmuOpt |=  0x80;
	} else {
		if      (PicoOpt&0x10) return 0;
		else if (!(currentConfig.EmuOpt &0x80))PicoOpt|=  0x10;
		else if (  currentConfig.EmuOpt &0x80) currentConfig.EmuOpt &= ~0x80;
	}
	return 0;
}

static int sndrate_prevnext(int rate, int dir)
{
	static const int rates[] = { 8000, 11025, 16000, 22050, 44100 };
	int i;

	for (i = 0; i < 5; i++)
		if (rates[i] == rate) break;

	i += dir ? 1 : -1;
	if (i > 4) {
		if (!(PicoOpt & POPT_EN_STEREO)) {
			PicoOpt |= POPT_EN_STEREO;
			return rates[0];
		}
		return rates[4];
	}
	if (i < 0) {
		if (PicoOpt & POPT_EN_STEREO) {
			PicoOpt &= ~POPT_EN_STEREO;
			return rates[4];
		}
		return rates[0];
	}
	return rates[i];
}

static void region_prevnext(int right)
{
	// jp_ntsc=1, jp_pal=2, usa=4, eu=8
	static const int rgn_orders[] = { 0x148, 0x184, 0x814, 0x418, 0x841, 0x481 };
	int i;

	if (right) {
		if (!PicoRegionOverride) {
			for (i = 0; i < 6; i++)
				if (rgn_orders[i] == PicoAutoRgnOrder) break;
			if (i < 5) PicoAutoRgnOrder = rgn_orders[i+1];
			else PicoRegionOverride=1;
		}
		else
			PicoRegionOverride <<= 1;
		if (PicoRegionOverride > 8)
			PicoRegionOverride = 8;
	} else {
		if (!PicoRegionOverride) {
			for (i = 0; i < 6; i++)
				if (rgn_orders[i] == PicoAutoRgnOrder) break;
			if (i > 0) PicoAutoRgnOrder = rgn_orders[i-1];
		}
		else
			PicoRegionOverride >>= 1;
	}
}

static int mh_opt_misc(menu_id id, int keys)
{
	int i;

	switch (id) {
	case MA_OPT_SOUND_QUALITY:
		PsndRate = sndrate_prevnext(PsndRate, keys & PBTN_RIGHT);
		break;
	case MA_OPT_REGION:
		region_prevnext(keys & PBTN_RIGHT);
		break;
	case MA_OPT_CONFIRM_STATES:
		i = ((currentConfig.EmuOpt>>9)&1) | ((currentConfig.EmuOpt>>10)&2);
		i += (keys & PBTN_LEFT) ? -1 : 1;
		if (i < 0) i = 0; else if (i > 3) i = 3;
		i |= i << 1; i &= ~2;
		currentConfig.EmuOpt &= ~0xa00;
		currentConfig.EmuOpt |= i << 9;
		break;
	default:
		break;
	}
	return 0;
}

static int mh_saveloadcfg(menu_id id, int keys)
{
	int ret;

	if (keys & (PBTN_LEFT|PBTN_RIGHT)) { // multi choice
		config_slot += (keys & PBTN_LEFT) ? -1 : 1;
		if (config_slot < 0) config_slot = 9;
		else if (config_slot > 9) config_slot = 0;
		me_enable(e_menu_options, MA_OPT_LOADCFG, config_slot != config_slot_current);
		return 0;
	}

	switch (id) {
	case MA_OPT_SAVECFG:
	case MA_OPT_SAVECFG_GAME:
		if (emu_WriteConfig(id == MA_OPT_SAVECFG_GAME ? 1 : 0))
			strcpy(menuErrorMsg, "config saved");
		else
			strcpy(menuErrorMsg, "failed to write config");
		break;
	case MA_OPT_LOADCFG:
		ret = emu_ReadConfig(1, 1);
		if (!ret) ret = emu_ReadConfig(0, 1);
		if (ret)  strcpy(menuErrorMsg, "config loaded");
		else      strcpy(menuErrorMsg, "failed to load config");
		break;
	default:
		return 0;
	}

	return 1;
}

static const char *mgn_opt_renderer(menu_id id, int *offs)
{
	*offs = -6;
	if (PicoOpt & POPT_ALT_RENDERER)
		return " 8bit fast";
	else if (currentConfig.EmuOpt & 0x80)
		return "16bit accurate";
	else
		return " 8bit accurate";
}

static const char *mgn_opt_fskip(menu_id id, int *offs)
{
	if (currentConfig.Frameskip < 0)
		return "Auto";
	sprintf(static_buff, "%d", currentConfig.Frameskip);
	return static_buff;
}

static const char *mgn_opt_sound(menu_id id, int *offs)
{
	const char *str2;
	*offs = -8;
	str2 = (PicoOpt & POPT_EN_STEREO) ? "stereo" : "mono";
	sprintf(static_buff, "%5iHz %s", PsndRate, str2);
	return static_buff;
}

static const char *mgn_opt_region(menu_id id, int *offs)
{
	static const char *names[] = { "Auto", "      Japan NTSC", "      Japan PAL", "      USA", "      Europe" };
	static const char *names_short[] = { "", " JP", " JP", " US", " EU" };
	int code = PicoRegionOverride;
	int u, i = 0;

	*offs = -6;
	if (code) {
		code <<= 1;
		while ((code >>= 1)) i++;
		if (i > 4)
			return "unknown";
		return names[i];
	} else {
		strcpy(static_buff, "Auto:");
		for (u = 0; u < 3; u++) {
			code = (PicoAutoRgnOrder >> u*4) & 0xf;
			for (i = 0; code; code >>= 1, i++)
				;
			strcat(static_buff, names_short[i]);
		}
		return static_buff;
	}
}

static const char *mgn_opt_c_saves(menu_id id, int *offs)
{
	switch ((currentConfig.EmuOpt >> 9) & 5) {
		default: return "OFF";
		case 1:  return "writes";
		case 4:  return "loads";
		case 5:  return "both";
	}
}

static const char *mgn_savecfg(menu_id id, int *offs)
{
	strcpy(static_buff, "Save global config");
	if (config_slot != 0)
		sprintf(static_buff + strlen(static_buff), " (profile: %i)", config_slot);
	return static_buff;
}

static const char *mgn_loadcfg(menu_id id, int *offs)
{
	sprintf(static_buff, "Load cfg from profile %i", config_slot);
	return static_buff;
}

static menu_entry e_menu_options[] =
{
	mee_range     ("Save slot",                MA_OPT_SAVE_SLOT,     state_slot, 0, 9),
	mee_range_cust("Frameskip",                MA_OPT_FRAMESKIP,     currentConfig.Frameskip, -1, 16, mgn_opt_fskip),
	mee_cust      ("Region",                   MA_OPT_REGION,        mh_opt_misc, mgn_opt_region),
	mee_cust      ("Renderer",                 MA_OPT_RENDERER,      mh_opt_render, mgn_opt_renderer),
	mee_onoff     ("Show FPS",                 MA_OPT_SHOW_FPS,      currentConfig.EmuOpt, 0x002),
	mee_onoff     ("Enable sound",             MA_OPT_ENABLE_SOUND,  currentConfig.EmuOpt, 0x004),
	mee_cust      ("Sound Quality",            MA_OPT_SOUND_QUALITY, mh_opt_misc, mgn_opt_sound),
	mee_cust      ("Confirm savestate",        MA_OPT_CONFIRM_STATES,mh_opt_misc, mgn_opt_c_saves),
#if   defined(__GP2X__)
	mee_range     ("GP2X CPU clocks",          MA_OPT_CPU_CLOCKS,    currentConfig.CPUclock, 20, 400),
#elif defined(PSP)
	mee_range     ("PSP CPU clock",            MA_OPT_CPU_CLOCKS,    currentConfig.CPUclock, )
#endif
	mee_handler   ("[Display options]",        menu_loop_gfx_options),
	mee_handler   ("[Advanced options]",       menu_loop_adv_options),
	mee_handler   ("[Sega/Mega CD options]",   menu_loop_cd_options),
	mee_handler_mkname_id(MA_OPT_SAVECFG, mh_saveloadcfg, mgn_savecfg),
	mee_handler_id("Save cfg for current game only", MA_OPT_SAVECFG_GAME, mh_saveloadcfg),
	mee_handler_mkname_id(MA_OPT_LOADCFG, mh_saveloadcfg, mgn_loadcfg),
	mee_end,
};

static int menu_loop_options(menu_id id, int keys)
{
	static int sel = 0;

	me_enable(e_menu_options, MA_OPT_SAVECFG_GAME, rom_loaded);
	me_enable(e_menu_options, MA_OPT_LOADCFG, config_slot != config_slot_current);

	me_loop(e_menu_options, &sel);

	if (PicoRegionOverride)
		// force setting possibly changed..
		Pico.m.pal = (PicoRegionOverride == 2 || PicoRegionOverride == 8) ? 1 : 0;

	return 0;
}

// ------------ debug menu ------------

#include <sys/stat.h>
#include <sys/types.h>

#include <pico/debug.h>

extern void SekStepM68k(void);

static void mplayer_loop(void)
{
	emu_startSound();

	while (1)
	{
		PDebugZ80Frame();
		if (in_menu_wait_any(0) & PBTN_NORTH) break;
		emu_waitSound();
	}

	emu_endSound();
}

static void draw_text_debug(const char *str, int skip, int from)
{
	const char *p;
	int line;

	p = str;
	while (skip-- > 0)
	{
		while (*p && *p != '\n') p++;
		if (*p == 0 || p[1] == 0) return;
		p++;
	}

	str = p;
	for (line = from; line < SCREEN_HEIGHT/10; line++)
	{
		while (*p && *p != '\n') p++;
		smalltext_out16(1, line*10, str, 0xffff);
		if (*p == 0) break;
		p++; str = p;
	}
}

static void draw_frame_debug(void)
{
	char layer_str[48] = "layers:             ";
	if (PicoDrawMask & PDRAW_LAYERB_ON)      memcpy(layer_str +  8, "B", 1);
	if (PicoDrawMask & PDRAW_LAYERA_ON)      memcpy(layer_str + 10, "A", 1);
	if (PicoDrawMask & PDRAW_SPRITES_LOW_ON) memcpy(layer_str + 12, "spr_lo", 6);
	if (PicoDrawMask & PDRAW_SPRITES_HI_ON)  memcpy(layer_str + 19, "spr_hi", 6);

	clear_screen();
	emu_forcedFrame(0);
	smalltext_out16(4, SCREEN_HEIGHT-8, layer_str, 0xffff);
}

static void debug_menu_loop(void)
{
	int inp, mode = 0;
	int spr_offs = 0, dumped = 0;
	char *tmp;

	while (1)
	{
		switch (mode)
		{
			case 0: plat_video_menu_begin();
				tmp = PDebugMain();
				emu_platformDebugCat(tmp);
				draw_text_debug(tmp, 0, 0);
				if (dumped) {
					smalltext_out16(SCREEN_WIDTH-6*10, SCREEN_HEIGHT-8, "dumped", 0xffff);
					dumped = 0;
				}
				break;
			case 1: draw_frame_debug(); break;
			case 2: clear_screen();
				emu_forcedFrame(0);
				darken_screen();
				PDebugShowSpriteStats((unsigned short *)SCREEN_BUFFER + (SCREEN_HEIGHT/2 - 240/2)*SCREEN_WIDTH +
					SCREEN_WIDTH/2 - 320/2, SCREEN_WIDTH); break;
			case 3: clear_screen();
				PDebugShowPalette(SCREEN_BUFFER, SCREEN_WIDTH);
				PDebugShowSprite((unsigned short *)SCREEN_BUFFER + SCREEN_WIDTH*120+SCREEN_WIDTH/2+16,
					SCREEN_WIDTH, spr_offs);
				draw_text_debug(PDebugSpriteList(), spr_offs, 6);
				break;
		}
		plat_video_menu_end();

		inp = in_menu_wait(PBTN_EAST|PBTN_MBACK|PBTN_WEST|PBTN_NORTH|PBTN_L|PBTN_R |
					PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT, 70);
		if (inp & PBTN_MBACK) return;
		if (inp & PBTN_L) { mode--; if (mode < 0) mode = 3; }
		if (inp & PBTN_R) { mode++; if (mode > 3) mode = 0; }
		switch (mode)
		{
			case 0:
				if (inp & PBTN_EAST) SekStepM68k();
				if (inp & PBTN_NORTH) {
					while (inp & PBTN_NORTH) inp = in_menu_wait_any(-1);
					mplayer_loop();
				}
				if ((inp & (PBTN_WEST|PBTN_LEFT)) == (PBTN_WEST|PBTN_LEFT)) {
					mkdir("dumps", 0777);
					PDebugDumpMem();
					while (inp & PBTN_WEST) inp = in_menu_wait_any(-1);
					dumped = 1;
				}
				break;
			case 1:
				if (inp & PBTN_LEFT)  PicoDrawMask ^= PDRAW_LAYERB_ON;
				if (inp & PBTN_RIGHT) PicoDrawMask ^= PDRAW_LAYERA_ON;
				if (inp & PBTN_DOWN)  PicoDrawMask ^= PDRAW_SPRITES_LOW_ON;
				if (inp & PBTN_UP)    PicoDrawMask ^= PDRAW_SPRITES_HI_ON;
				if (inp & PBTN_EAST) {
					PsndOut = NULL; // just in case
					PicoSkipFrame = 1;
					PicoFrame();
					PicoSkipFrame = 0;
					while (inp & PBTN_EAST) inp = in_menu_wait_any(-1);
				}
				break;
			case 3:
				if (inp & PBTN_DOWN)  spr_offs++;
				if (inp & PBTN_UP)    spr_offs--;
				if (spr_offs < 0) spr_offs = 0;
				break;
		}
	}
}

// ------------ main menu ------------

static char *romsel_run(void)
{
	char *ret, *sel_name;

	sel_name = malloc(sizeof(rom_fname_loaded));
	if (sel_name == NULL)
		return NULL;
	strcpy(sel_name, rom_fname_loaded);

	ret = menu_loop_romsel(sel_name, sizeof(rom_fname_loaded));
	free(sel_name);
	return ret;
}

static int main_menu_handler(menu_id id, int keys)
{
	char *ret_name;

	switch (id)
	{
	case MA_MAIN_RESUME_GAME:
		if (rom_loaded)
			return 1;
		break;
	case MA_MAIN_SAVE_STATE:
		if (rom_loaded)
			return menu_loop_savestate(0);
		break;
	case MA_MAIN_LOAD_STATE:
		if (rom_loaded)
			return menu_loop_savestate(1);
		break;
	case MA_MAIN_RESET_GAME:
		if (rom_loaded) {
			emu_ResetGame();
			return 1;
		}
		break;
	case MA_MAIN_LOAD_ROM:
		ret_name = romsel_run();
		if (ret_name != NULL) {
			lprintf("selected file: %s\n", ret_name);
			engineState = PGS_ReloadRom;
			return 1;
		}
		break;
	case MA_MAIN_CREDITS:
		draw_menu_credits();
		in_menu_wait(PBTN_MOK|PBTN_MBACK, 70);
		break;
	case MA_MAIN_EXIT:
		engineState = PGS_Quit;
		return 1;
	case MA_MAIN_PATCHES:
		if (rom_loaded && PicoPatches) {
			menu_loop_patches();
			PicoPatchApply();
			strcpy(menuErrorMsg, "Patches applied");
		}
		break;
	default:
		lprintf("%s: something unknown selected\n", __FUNCTION__);
		break;
	}

	return 0;
}

static menu_entry e_menu_main[] =
{
	mee_handler_id("Resume game",        MA_MAIN_RESUME_GAME, main_menu_handler),
	mee_handler_id("Save State",         MA_MAIN_SAVE_STATE,  main_menu_handler),
	mee_handler_id("Load State",         MA_MAIN_LOAD_STATE,  main_menu_handler),
	mee_handler_id("Reset game",         MA_MAIN_RESET_GAME,  main_menu_handler),
	mee_handler_id("Load new ROM/ISO",   MA_MAIN_LOAD_ROM,    main_menu_handler),
	mee_handler_id("Change options",     MA_MAIN_OPTIONS,     menu_loop_options),
	mee_handler_id("Configure controls", MA_MAIN_OPTIONS,     menu_loop_keyconfig),
	mee_handler_id("Credits",            MA_MAIN_CREDITS,     main_menu_handler),
	mee_handler_id("Patches / GameGenie",MA_MAIN_PATCHES,     main_menu_handler),
	mee_handler_id("Exit",               MA_MAIN_EXIT,        main_menu_handler),
	mee_end,
};

void menu_loop(void)
{
	static int sel = 0;

	me_enable(e_menu_main, MA_MAIN_RESUME_GAME, rom_loaded);
	me_enable(e_menu_main, MA_MAIN_SAVE_STATE,  rom_loaded);
	me_enable(e_menu_main, MA_MAIN_LOAD_STATE,  rom_loaded);
	me_enable(e_menu_main, MA_MAIN_RESET_GAME,  rom_loaded);
	me_enable(e_menu_main, MA_MAIN_PATCHES, PicoPatches != NULL);

	plat_video_menu_enter(rom_loaded);
	in_set_blocking(1);
	me_loop(e_menu_main, &sel);
	in_set_blocking(0);

	if (rom_loaded && engineState == PGS_Menu) {
		/* wait until menu, ok, back is released */
		while (in_menu_wait_any(50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK));
		engineState = PGS_Running;
	}
}

// --------- CD tray close menu ----------

static int mh_tray_load_cd(menu_id id, int keys)
{
	cd_img_type cd_type;
	char *ret_name;
	int ret = -1;

	ret_name = romsel_run();
	if (ret_name == NULL)
		return 0;

	cd_type = emu_cdCheck(NULL, ret_name);
	if (cd_type != CIT_NOT_CD)
		ret = Insert_CD(ret_name, cd_type);
	if (ret != 0) {
		sprintf(menuErrorMsg, "Load failed, invalid CD image?");
		lprintf("%s\n", menuErrorMsg);
		return 0;
	}

	engineState = PGS_RestartRun;
	return 1;
}

static int mh_tray_nothing(menu_id id, int keys)
{
	return 1;
}

static menu_entry e_menu_tray[] =
{
	mee_label  ("The unit is about to"),
	mee_label  ("close the CD tray."),
	mee_label  (""),
	mee_label  (""),
	mee_handler("Load CD image",  mh_tray_load_cd),
	mee_handler("Insert nothing", mh_tray_nothing),
};

int menu_loop_tray(void)
{
	int ret = 1, sel = 0;

	plat_video_menu_enter(rom_loaded);

	in_set_blocking(1);
	me_loop(e_menu_tray, &sel);
	in_set_blocking(0);

	if (engineState != PGS_RestartRun) {
		engineState = PGS_RestartRun;
		ret = 0; /* no CD inserted */
	}

	while (in_menu_wait_any(50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK));

	return ret;
}

#endif // !UIQ3

// ------------ util ------------

/* TODO: rename */
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

/* hidden options for config engine only */
static menu_entry e_menu_hidden[] =
{
	mee_onoff("Accurate sprites", MA_OPT_ACC_SPRITES, PicoOpt, 0x080),
	mee_end,
};

static menu_entry *e_menu_table[] =
{
	e_menu_options,
	e_menu_gfx_options,
	e_menu_adv_options,
	e_menu_cd_options,
	e_menu_keyconfig,
	e_menu_hidden,
};

static menu_entry *me_list_table = NULL;
static menu_entry *me_list_i = NULL;

menu_entry *me_list_get_first(void)
{
	me_list_table = me_list_i = e_menu_table[0];
	return me_list_i;
}

menu_entry *me_list_get_next(void)
{
	int i;

	me_list_i++;
	if (me_list_i->name != NULL)
		return me_list_i;

	for (i = 0; i < array_size(e_menu_table); i++)
		if (me_list_table == e_menu_table[i])
			break;

	if (i + 1 < array_size(e_menu_table))
		me_list_table = me_list_i = e_menu_table[i + 1];
	else
		me_list_table = me_list_i = NULL;

	return me_list_i;
}

