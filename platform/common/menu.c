// (c) Copyright 2006-2009 notaz, All rights reserved.
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
#include "input.h"
#include "emu.h"
#include "plat.h"
#include "posix.h"
#include <version.h>
#include <revision.h>

#include <pico/pico.h>
#include <pico/patch.h>

static char static_buff[64];
static int  menu_error_time = 0;
char menu_error_msg[64] = { 0, };
void *g_menubg_ptr;

#ifndef UIQ3

static unsigned char *menu_font_data = NULL;
static int menu_text_color = 0xffff; // default to white
static int menu_sel_color = -1; // disabled

/* note: these might become non-constant in future */
#if MENU_X2
static const int me_mfont_w = 16, me_mfont_h = 20;
static const int me_sfont_w = 12, me_sfont_h = 20;
#else
static const int me_mfont_w = 8, me_mfont_h = 10;
static const int me_sfont_w = 6, me_sfont_h = 10;
#endif

// draws text to current bbp16 screen
static void text_out16_(int x, int y, const char *text, int color)
{
	int i, lh, tr, tg, tb, len;
	unsigned short *dest = (unsigned short *)g_screen_ptr + x + y * g_screen_width;
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

	lh = me_mfont_h;
	if (y + lh > g_screen_height)
		lh = g_screen_height - y;

	for (i = 0; i < len; i++)
	{
		unsigned char  *src = menu_font_data + (unsigned int)text[i] * me_mfont_w * me_mfont_h / 2;
		unsigned short *dst = dest;
		int u, l;

		for (l = 0; l < lh; l++, dst += g_screen_width - me_mfont_w)
		{
			for (u = me_mfont_w / 2; u > 0; u--, src++)
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
		dest += me_mfont_w;
	}
}

void text_out16(int x, int y, const char *texto, ...)
{
	va_list args;
	char    buffer[256];
	int     maxw = (g_screen_width - x) / me_mfont_w;

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

/* draws in 6x8 font, might multiply size by integer */
static void smalltext_out16_(int x, int y, const char *texto, int color)
{
	unsigned char  *src;
	unsigned short *dst;
	int multiplier = me_sfont_w / 6;
	int i;

	for (i = 0;; i++, x += me_sfont_w)
	{
		unsigned char c = (unsigned char) texto[i];
		int h = 8;

		if (!c || c == '\n')
			break;

		src = fontdata6x8[c];
		dst = (unsigned short *)g_screen_ptr + x + y * g_screen_width;

		while (h--)
		{
			int m, w2, h2;
			for (h2 = multiplier; h2 > 0; h2--)
			{
				for (m = 0x20; m; m >>= 1) {
					if (*src & m)
						for (w2 = multiplier; w2 > 0; w2--)
							*dst++ = color;
					else
						dst += multiplier;
				}

				dst += g_screen_width - me_sfont_w;
			}
			src++;
		}
	}
}

static void smalltext_out16(int x, int y, const char *texto, int color)
{
	char buffer[128];
	int maxw = (g_screen_width - x) / 6;

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
	dest = (unsigned short *)g_screen_ptr + x + y * g_screen_width + me_mfont_w * 2 - 2;
	for (h = me_mfont_h + 1; h > 0; h--)
	{
		dst = dest;
		for (i = w - (me_mfont_w * 2 - 2); i > 0; i--)
			*dst++ = menu_sel_color;
		dest += g_screen_width;
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
	int i, c, l;
	unsigned char *fd, *fds;
	char buff[256];
	FILE *f;

	if (menu_font_data != NULL)
		free(menu_font_data);

	menu_font_data = calloc((MENU_X2 ? 256 * 320 : 128 * 160) / 2, 1);
	if (menu_font_data == NULL)
		return;

	// generate default 8x10 font from fontdata8x8
	for (c = 0, fd = menu_font_data; c < 256; c++)
	{
		for (l = 0; l < 8; l++)
		{
			unsigned char fd8x8 = fontdata8x8[c*8+l];
			if (fd8x8&0x80) *fd  = 0xf0;
			if (fd8x8&0x40) *fd |= 0x0f; fd++;
			if (fd8x8&0x20) *fd  = 0xf0;
			if (fd8x8&0x10) *fd |= 0x0f; fd++;
			if (fd8x8&0x08) *fd  = 0xf0;
			if (fd8x8&0x04) *fd |= 0x0f; fd++;
			if (fd8x8&0x02) *fd  = 0xf0;
			if (fd8x8&0x01) *fd |= 0x0f; fd++;
		}
		fd += 8*2/2; // 2 empty lines
	}

	if (MENU_X2) {
		// expand default font
		fds = menu_font_data + 128 * 160 / 2 - 4;
		fd  = menu_font_data + 256 * 320 / 2 - 1;
		for (c = 255; c >= 0; c--)
		{
			for (l = 9; l >= 0; l--, fds -= 4)
			{
				for (i = 3; i >= 0; i--) {
					int px = fds[i] & 0x0f;
					*fd-- = px | (px << 4);
					px = (fds[i] >> 4) & 0x0f;
					*fd-- = px | (px << 4);
				}
				for (i = 3; i >= 0; i--) {
					int px = fds[i] & 0x0f;
					*fd-- = px | (px << 4);
					px = (fds[i] >> 4) & 0x0f;
					*fd-- = px | (px << 4);
				}
			}
		}
	}

	// load custom font and selector (stored as 1st symbol in font table)
	emu_make_path(buff, "skin/font.png", sizeof(buff));
	readpng(menu_font_data, buff, READPNG_FONT,
		MENU_X2 ? 256 : 128, MENU_X2 ? 320 : 160);
	// default selector symbol is '>'
	memcpy(menu_font_data, menu_font_data + ((int)'>') * me_mfont_w * me_mfont_h / 2,
		me_mfont_w * me_mfont_h / 2);
	emu_make_path(buff, "skin/selector.png", sizeof(buff));
	readpng(menu_font_data, buff, READPNG_SELECTOR, MENU_X2 ? 16 : 8, MENU_X2 ? 20 : 10);

	// load custom colors
	emu_make_path(buff, "skin/skin.txt", sizeof(buff));
	f = fopen(buff, "r");
	if (f != NULL)
	{
		lprintf("found skin.txt\n");
		while (!feof(f))
		{
			if (fgets(buff, sizeof(buff), f) == NULL)
				break;
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


static void menu_darken_bg(void *dst, void *src, int pixels, int darker)
{
	unsigned int *dest = dst;
	unsigned int *sorc = src;
	pixels /= 2;
	if (darker)
	{
		while (pixels--)
		{
			unsigned int p = *sorc++;
			*dest++ = ((p&0xf79ef79e)>>1) - ((p&0xc618c618)>>3);
		}
	}
	else
	{
		while (pixels--)
		{
			unsigned int p = *sorc++;
			*dest++ = (p&0xf79ef79e)>>1;
		}
	}
}

static void menu_enter(int is_rom_loaded)
{
	if (is_rom_loaded)
	{
		// darken the active framebuffer
		menu_darken_bg(g_menubg_ptr, g_screen_ptr, g_screen_width * g_screen_height, 1);
	}
	else
	{
		char buff[256];

		// should really only happen once, on startup..
		emu_make_path(buff, "skin/background.png", sizeof(buff));
		if (readpng(g_menubg_ptr, buff, READPNG_BG, g_screen_width, g_screen_height) < 0)
			memset(g_menubg_ptr, 0, g_screen_width * g_screen_height * 2);
	}

	plat_video_menu_enter(is_rom_loaded);
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

static void me_draw(const menu_entry *entries, int sel, void (*draw_more)(void))
{
	const menu_entry *ent, *ent_sel = entries;
	int x, y, w = 0, h = 0;
	int offs, col2_offs = 27 * me_mfont_w;
	int vi_sel_ln = 0;
	const char *name;
	int i, n;

	/* calculate size of menu rect */
	for (ent = entries, i = n = 0; ent->name; ent++, i++)
	{
		int wt;

		if (!ent->enabled)
			continue;

		if (i == sel) {
			ent_sel = ent;
			vi_sel_ln = n;
		}

		name = NULL;
		wt = strlen(ent->name) * me_mfont_w;
		if (wt == 0 && ent->generate_name)
			name = ent->generate_name(ent->id, &offs);
		if (name != NULL)
			wt = strlen(name) * me_mfont_w;

		if (ent->beh != MB_NONE)
		{
			if (wt > col2_offs)
				col2_offs = wt + me_mfont_w;
			wt = col2_offs;

			switch (ent->beh) {
			case MB_NONE: break;
			case MB_OPT_ONOFF:
			case MB_OPT_RANGE: wt += me_mfont_w * 3; break;
			case MB_OPT_CUSTOM:
			case MB_OPT_CUSTONOFF:
			case MB_OPT_CUSTRANGE:
				name = NULL;
				offs = 0;
				if (ent->generate_name != NULL)
					name = ent->generate_name(ent->id, &offs);
				if (name != NULL)
					wt += (strlen(name) + offs) * me_mfont_w;
				break;
			case MB_OPT_ENUM:
				wt += 10 * me_mfont_w;
				break;
			}
		}

		if (wt > w)
			w = wt;
		n++;
	}
	h = n * me_mfont_h;
	w += me_mfont_w * 2; /* selector */

	if (w > g_screen_width) {
		lprintf("width %d > %d\n", w, g_screen_width);
		w = g_screen_width;
	}
	if (h > g_screen_height) {
		lprintf("height %d > %d\n", w, g_screen_height);
		h = g_screen_height;
	}

	x = g_screen_width  / 2 - w / 2;
	y = g_screen_height / 2 - h / 2;

	/* draw */
	plat_video_menu_begin();
	menu_draw_selection(x, y + vi_sel_ln * me_mfont_h, w);
	x += me_mfont_w * 2;

	for (ent = entries; ent->name; ent++)
	{
		const char **names;
		int len;

		if (!ent->enabled)
			continue;

		name = ent->name;
		if (strlen(name) == 0) {
			if (ent->generate_name)
				name = ent->generate_name(ent->id, &offs);
		}
		if (name != NULL)
			text_out16(x, y, name);

		switch (ent->beh) {
		case MB_NONE:
			break;
		case MB_OPT_ONOFF:
			text_out16(x + col2_offs, y, (*(int *)ent->var & ent->mask) ? "ON" : "OFF");
			break;
		case MB_OPT_RANGE:
			text_out16(x + col2_offs, y, "%i", *(int *)ent->var);
			break;
		case MB_OPT_CUSTOM:
		case MB_OPT_CUSTONOFF:
		case MB_OPT_CUSTRANGE:
			name = NULL;
			offs = 0;
			if (ent->generate_name)
				name = ent->generate_name(ent->id, &offs);
			if (name != NULL)
				text_out16(x + col2_offs + offs * me_mfont_w, y, "%s", name);
			break;
		case MB_OPT_ENUM:
			names = (const char **)ent->data;
			offs = 0;
			for (i = 0; names[i] != NULL; i++) {
				len = strlen(names[i]);
				if (len > 10)
					offs = 10 - len - 2;
				if (i == *(int *)ent->var) {
					text_out16(x + col2_offs + offs * me_mfont_w, y, "%s", names[i]);
					break;
				}
			}
			break;
		}

		y += me_mfont_h;
	}

	/* display help or message if we have one */
	h = (g_screen_height - h) / 2; // bottom area height
	if (menu_error_msg[0] != 0) {
		if (h >= me_mfont_h + 4)
			text_out16(5, g_screen_height - me_mfont_h - 4, menu_error_msg);
		else
			lprintf("menu msg doesn't fit!\n");

		if (plat_get_ticks_ms() - menu_error_time > 2048)
			menu_error_msg[0] = 0;
	}
	else if (ent_sel->help != NULL) {
		const char *tmp = ent_sel->help;
		int l;
		for (l = 0; tmp != NULL && *tmp != 0; l++)
			tmp = strchr(tmp + 1, '\n');
		if (h >= l * me_sfont_h + 4)
			for (tmp = ent_sel->help; l > 0; l--, tmp = strchr(tmp, '\n') + 1)
				smalltext_out16(5, g_screen_height - (l * me_sfont_h + 4), tmp, 0xffff);
	}

	if (draw_more != NULL)
		draw_more();

	plat_video_menu_end();
}

static int me_process(menu_entry *entry, int is_next, int is_lr)
{
	const char **names;
	int c;
	switch (entry->beh)
	{
		case MB_OPT_ONOFF:
		case MB_OPT_CUSTONOFF:
			*(int *)entry->var ^= entry->mask;
			return 1;
		case MB_OPT_RANGE:
		case MB_OPT_CUSTRANGE:
			c = is_lr ? 10 : 1;
			*(int *)entry->var += is_next ? c : -c;
			if (*(int *)entry->var < (int)entry->min)
				*(int *)entry->var = (int)entry->max;
			if (*(int *)entry->var > (int)entry->max)
				*(int *)entry->var = (int)entry->min;
			return 1;
		case MB_OPT_ENUM:
			names = (const char **)entry->data;
			for (c = 0; names[c] != NULL; c++)
				;
			*(int *)entry->var += is_next ? 1 : -1;
			if (*(int *)entry->var < 0)
				*(int *)entry->var = 0;
			if (*(int *)entry->var >= c)
				*(int *)entry->var = c - 1;
			return 1;
		default:
			return 0;
	}
}

static void debug_menu_loop(void);

static void me_loop(menu_entry *menu, int *menu_sel, void (*draw_more)(void))
{
	int ret, inp, sel = *menu_sel, menu_sel_max;

	menu_sel_max = me_count(menu) - 1;
	if (menu_sel_max < 0) {
		lprintf("no enabled menu entries\n");
		return;
	}

	while ((!menu[sel].enabled || !menu[sel].selectable) && sel < menu_sel_max)
		sel++;

	/* make sure action buttons are not pressed on entering menu */
	me_draw(menu, sel, NULL);
	while (in_menu_wait_any(50) & (PBTN_MOK|PBTN_MBACK|PBTN_MENU));

	for (;;)
	{
		me_draw(menu, sel, draw_more);
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

		if (inp & (PBTN_LEFT|PBTN_RIGHT|PBTN_L|PBTN_R)) { /* multi choice */
			if (me_process(&menu[sel], (inp & (PBTN_RIGHT|PBTN_R)) ? 1 : 0,
						inp & (PBTN_L|PBTN_R)))
				continue;
		}

		if (inp & (PBTN_MOK|PBTN_LEFT|PBTN_RIGHT|PBTN_L|PBTN_R))
		{
			/* require PBTN_MOK for MB_NONE */
			if (menu[sel].handler != NULL && (menu[sel].beh != MB_NONE || (inp & PBTN_MOK))) {
				ret = menu[sel].handler(menu[sel].id, inp);
				if (ret) break;
				menu_sel_max = me_count(menu) - 1; /* might change, so update */
			}
		}
	}
	*menu_sel = sel;
}

/* ***************************************** */

/* platform specific options and handlers */
#if   defined(__GP2X__)
#include "../gp2x/menu.c"
#elif defined(PANDORA)
#include "../pandora/menu.c"
#else
#define MENU_OPTIONS_GFX
#define MENU_OPTIONS_ADV
#define menu_main_plat_draw NULL
#endif

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

	x = g_screen_width  / 2 - w * me_mfont_w / 2;
	y = g_screen_height / 2 - h * me_mfont_h / 2;
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	plat_video_menu_begin();

	for (p = creds; *p != 0 && y <= g_screen_height - me_mfont_h; y += me_mfont_h) {
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
	int ln, len = percent * g_screen_width / 100;
	unsigned short *dst = (unsigned short *)g_screen_ptr + g_screen_width * 10 * 2;

	if (len > g_screen_width)
		len = g_screen_width;
	for (ln = 10 - 2; ln > 0; ln--, dst += g_screen_width)
		memset(dst, 0xff, len * 2);
	plat_video_menu_end();
}

static void cdload_progress_cb(const char *fname, int percent)
{
	int ln, len = percent * g_screen_width / 100;
	unsigned short *dst = (unsigned short *)g_screen_ptr + g_screen_width * 10 * 2;

	memset(dst, 0xff, g_screen_width * (me_sfont_h - 2) * 2);

	smalltext_out16(1, 3 * me_sfont_h, "Processing CD image / MP3s", 0xffff);
	smalltext_out16(1, 4 * me_sfont_h, fname, 0xffff);
	dst += g_screen_width * me_sfont_h * 3;

	if (len > g_screen_width)
		len = g_screen_width;
	for (ln = (me_sfont_h - 2); ln > 0; ln--, dst += g_screen_width)
		memset(dst, 0xff, len * 2);

	plat_video_menu_end();
	cdload_called = 1;
}

void menu_romload_prepare(const char *rom_name)
{
	const char *p = rom_name + strlen(rom_name);
	int i;

	while (p > rom_name && *p != '/')
		p--;

	/* fill all buffers, callbacks won't update in full */
	for (i = 0; i < 3; i++) {
		plat_video_menu_begin();
		smalltext_out16(1, 1, "Loading", 0xffff);
		smalltext_out16(1, me_sfont_h, p, 0xffff);
		plat_video_menu_end();
	}

	PicoCartLoadProgressCB = load_progress_cb;
	PicoCDLoadProgressCB = cdload_progress_cb;
	cdload_called = 0;
}

void menu_romload_end(void)
{
	PicoCartLoadProgressCB = NULL;
	PicoCDLoadProgressCB = NULL;
	smalltext_out16(1, (cdload_called ? 6 : 3) * me_sfont_h,
		"Starting emulation...", 0xffff);
	plat_video_menu_end();
}

// -------------- del confirm ---------------

static void do_delete(const char *fpath, const char *fname)
{
	int len, mid, inp;
	const char *nm;
	char tmp[64];

	plat_video_menu_begin();

	if (!rom_loaded)
		menu_darken_bg(g_screen_ptr, g_screen_ptr, g_screen_width * g_screen_height, 0);

	len = strlen(fname);
	if (len > g_screen_width/6)
		len = g_screen_width/6;

	mid = g_screen_width / 2;
	text_out16(mid - me_mfont_w * 15 / 2,  8 * me_mfont_h, "About to delete");
	smalltext_out16(mid - len * me_sfont_w / 2, 9 * me_mfont_h + 5, fname, 0xbdff);
	text_out16(mid - me_mfont_w * 13 / 2, 11 * me_mfont_h, "Are you sure?");

	nm = in_get_key_name(-1, -PBTN_MA3);
	snprintf(tmp, sizeof(tmp), "(%s - confirm, ", nm);
	len = strlen(tmp);
	nm = in_get_key_name(-1, -PBTN_MBACK);
	snprintf(tmp + len, sizeof(tmp) - len, "%s - cancel)", nm);
	len = strlen(tmp);

	text_out16(mid - me_mfont_w * len / 2, 12 * me_mfont_h, tmp);
	plat_video_menu_end();

	while (in_menu_wait_any(50) & (PBTN_MENU|PBTN_MA2));
	inp = in_menu_wait(PBTN_MA3|PBTN_MBACK, 100);
	if (inp & PBTN_MA3)
		remove(fpath);
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
	int max_cnt, start, i, x, pos;
	void *darken_ptr;

	max_cnt = g_screen_height / me_sfont_h;
	start = max_cnt / 2 - sel;
	n--; // exclude current dir (".")

	plat_video_menu_begin();

//	if (!rom_loaded)
//		menu_darken_bg(gp2x_screen, 320*240, 0);

	darken_ptr = (short *)g_screen_ptr + g_screen_width * max_cnt/2 * me_sfont_h;
	menu_darken_bg(darken_ptr, darken_ptr, g_screen_width * me_sfont_h * 8 / 10, 0);

	x = 5 + me_mfont_w + 1;
	if (start - 2 >= 0)
		smalltext_out16(14, (start - 2) * me_sfont_h, curdir, 0xffff);
	for (i = 0; i < n; i++) {
		pos = start + i;
		if (pos < 0)  continue;
		if (pos >= max_cnt) break;
		if (namelist[i+1]->d_type == DT_DIR) {
			smalltext_out16(x, pos * me_sfont_h, "/", 0xfff6);
			smalltext_out16(x + me_sfont_w, pos * me_sfont_h, namelist[i+1]->d_name, 0xfff6);
		} else {
			unsigned short color = file2color(namelist[i+1]->d_name);
			smalltext_out16(x, pos * me_sfont_h, namelist[i+1]->d_name, color);
		}
	}
	smalltext_out16(5, max_cnt/2 * me_sfont_h, ">", 0xffff);
	plat_video_menu_end();
}

static int scandir_cmp(const void *p1, const void *p2)
{
	const struct dirent **d1 = (const struct dirent **)p1;
	const struct dirent **d2 = (const struct dirent **)p2;
	if ((*d1)->d_type == (*d2)->d_type)
		return alphasort(d1, d2);
	if ((*d1)->d_type == DT_DIR)
		return -1; // put before
	if ((*d2)->d_type == DT_DIR)
		return  1;

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

	n = scandir(curr_path, &namelist, scandir_filter, (void *)scandir_cmp);
	if (n < 0) {
		char *t;
		lprintf("menu_loop_romsel failed, dir: %s\n", curr_path);

		// try root
		t = getcwd(curr_path, len);
		if (t == NULL)
			plat_get_root_dir(curr_path, len);
		n = scandir(curr_path, &namelist, scandir_filter, (void *)scandir_cmp);
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

	/* make sure action buttons are not pressed on entering menu */
	draw_dirlist(curr_path, namelist, n, sel);
	while (in_menu_wait_any(50) & (PBTN_MOK|PBTN_MBACK|PBTN_MENU))
		;

	for (;;)
	{
		draw_dirlist(curr_path, namelist, n, sel);
		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|
			PBTN_L|PBTN_R|PBTN_MA2|PBTN_MOK|PBTN_MBACK|PBTN_MENU, 33);
		if (inp & PBTN_UP  )  { sel--;   if (sel < 0)   sel = n-2; }
		if (inp & PBTN_DOWN)  { sel++;   if (sel > n-2) sel = 0; }
		if (inp & PBTN_LEFT)  { sel-=10; if (sel < 0)   sel = 0; }
		if (inp & PBTN_L)     { sel-=24; if (sel < 0)   sel = 0; }
		if (inp & PBTN_RIGHT) { sel+=10; if (sel > n-2) sel = n-2; }
		if (inp & PBTN_R)     { sel+=24; if (sel > n-2) sel = n-2; }
		if ((inp & PBTN_MOK) || (inp & (PBTN_MENU|PBTN_MA2)) == (PBTN_MENU|PBTN_MA2))
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
				do_delete(rom_fname_reload, namelist[sel+1]->d_name);
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

	max_cnt = g_screen_height / 10;
	start = max_cnt / 2 - sel;

	plat_video_menu_begin();

	for (i = 0; i < PicoPatchCount; i++) {
		pos = start + i;
		if (pos < 0) continue;
		if (pos >= max_cnt) break;
		active = PicoPatches[i].active;
		smalltext_out16(14,     pos * me_sfont_h, active ? "ON " : "OFF", active ? 0xfff6 : 0xffff);
		smalltext_out16(14+6*4, pos * me_sfont_h, PicoPatches[i].name,    active ? 0xfff6 : 0xffff);
	}
	pos = start + i;
	if (pos < max_cnt)
		smalltext_out16(14, pos * me_sfont_h, "done", 0xffff);

	text_out16(5, max_cnt / 2 * me_sfont_h, ">");
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
		if (emu_check_save_file(slot))
			state_slot_flags |= 1 << slot;
	}
}

static void draw_savestate_bg(int slot)
{
	const char *fname;
	void *tmp_state;

	fname = emu_get_save_fname(1, 0, slot);
	if (!fname)
		return;

	tmp_state = PicoTmpStateSave();

	PicoStateLoadGfx(fname);

	/* do a frame and fetch menu bg */
	pemu_forced_frame(POPT_EN_SOFTSCALE);
	menu_enter(1);

	PicoTmpStateRestore(tmp_state);
}

static void draw_savestate_menu(int menu_sel, int is_loading)
{
	int i, x, y, w, h;

	if (state_slot_flags & (1 << menu_sel))
		draw_savestate_bg(menu_sel);

	w = (13 + 2) * me_mfont_w;
	h = (1+2+10+1) * me_mfont_h;
	x = g_screen_width / 2 - w / 2;
	if (x < 0) x = 0;
	y = g_screen_height / 2 - h / 2;
	if (y < 0) y = 0;

	plat_video_menu_begin();

	text_out16(x, y, is_loading ? "Load state" : "Save state");
	y += 3 * me_mfont_h;

	menu_draw_selection(x - me_mfont_w * 2, y + menu_sel * me_mfont_h, (13 + 2) * me_mfont_w + 4);

	/* draw all 10 slots */
	for (i = 0; i < 10; i++, y += me_mfont_h)
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
	int ret = 0;

	state_check_slots();

	if (!(state_slot_flags & (1 << menu_sel)) && is_loading)
		menu_sel = menu_sel_max;

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
				if (emu_save_load_game(is_loading, 0)) {
					me_update_msg(is_loading ? "Load failed" : "Save failed");
					break;
				}
				ret = 1;
				break;
			}
			break;
		}
		if (inp & PBTN_MBACK)
			break;
	}

	return ret;
}

// -------------- key config --------------

static char *action_binds(int player_idx, int action_mask, int dev_id)
{
	int k, count, can_combo, type;
	const int *binds;

	static_buff[0] = 0;

	binds = in_get_dev_binds(dev_id);
	if (binds == NULL)
		return static_buff;

	count = in_get_dev_info(dev_id, IN_INFO_BIND_COUNT);
	can_combo = in_get_dev_info(dev_id, IN_INFO_DOES_COMBOS);

	type = IN_BINDTYPE_EMU;
	if (player_idx >= 0) {
		can_combo = 0;
		type = IN_BINDTYPE_PLAYER12;
	}
	if (player_idx == 1)
		action_mask <<= 16;

	for (k = 0; k < count; k++)
	{
		const char *xname;
		int len;

		if (!(binds[IN_BIND_OFFS(k, type)] & action_mask))
			continue;

		xname = in_get_key_name(dev_id, k);
		len = strlen(static_buff);
		if (len) {
			strncat(static_buff, can_combo ? " + " : ", ",
				sizeof(static_buff) - len - 1);
			len += can_combo ? 3 : 2;
		}
		strncat(static_buff, xname, sizeof(static_buff) - len - 1);
	}

	return static_buff;
}

static int count_bound_keys(int dev_id, int action_mask, int bindtype)
{
	const int *binds;
	int k, keys = 0;
	int count;

	binds = in_get_dev_binds(dev_id);
	if (binds == NULL)
		return 0;

	count = in_get_dev_info(dev_id, IN_INFO_BIND_COUNT);
	for (k = 0; k < count; k++)
	{
		if (binds[IN_BIND_OFFS(k, bindtype)] & action_mask)
			keys++;
	}

	return keys;
}

static void draw_key_config(const me_bind_action *opts, int opt_cnt, int player_idx,
		int sel, int dev_id, int dev_count, int is_bind)
{
	char buff[64], buff2[32];
	const char *dev_name;
	int x, y, w, i;

	w = ((player_idx >= 0) ? 20 : 30) * me_mfont_w;
	x = g_screen_width / 2 - w / 2;
	y = (g_screen_height - 4 * me_mfont_h) / 2 - (2 + opt_cnt) * me_mfont_h / 2;
	if (x < me_mfont_w * 2)
		x = me_mfont_w * 2;

	plat_video_menu_begin();
	if (player_idx >= 0)
		text_out16(x, y, "Player %i controls", player_idx + 1);
	else
		text_out16(x, y, "Emulator controls");

	y += 2 * me_mfont_h;
	menu_draw_selection(x - me_mfont_w * 2, y + sel * me_mfont_h, w + 2 * me_mfont_w);

	for (i = 0; i < opt_cnt; i++, y += me_mfont_h)
		text_out16(x, y, "%s : %s", opts[i].name,
			action_binds(player_idx, opts[i].mask, dev_id));

	dev_name = in_get_dev_name(dev_id, 1, 1);
	w = strlen(dev_name) * me_mfont_w;
	if (w < 30 * me_mfont_w)
		w = 30 * me_mfont_w;
	if (w > g_screen_width)
		w = g_screen_width;

	x = g_screen_width / 2 - w / 2;

	if (!is_bind) {
		snprintf(buff2, sizeof(buff2), "%s", in_get_key_name(-1, -PBTN_MOK));
		snprintf(buff, sizeof(buff), "%s - bind, %s - clear", buff2,
				in_get_key_name(-1, -PBTN_MA2));
		text_out16(x, g_screen_height - 4 * me_mfont_h, buff);
	}
	else
		text_out16(x, g_screen_height - 4 * me_mfont_h, "Press a button to bind/unbind");

	if (dev_count > 1) {
		text_out16(x, g_screen_height - 3 * me_mfont_h, dev_name);
		text_out16(x, g_screen_height - 2 * me_mfont_h, "Press left/right for other devs");
	}

	plat_video_menu_end();
}

static void key_config_loop(const me_bind_action *opts, int opt_cnt, int player_idx)
{
	int i, sel = 0, menu_sel_max = opt_cnt - 1;
	int dev_id, dev_count, kc, is_down, mkey;
	int unbind, bindtype, mask_shift;

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

	mask_shift = 0;
	if (player_idx == 1)
		mask_shift = 16;
	bindtype = player_idx >= 0 ? IN_BINDTYPE_PLAYER12 : IN_BINDTYPE_EMU;

	for (;;)
	{
		draw_key_config(opts, opt_cnt, player_idx, sel, dev_id, dev_count, 0);
		mkey = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_MBACK|PBTN_MOK|PBTN_MA2, 100);
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
			case PBTN_MA2:
				in_unbind_all(dev_id, opts[sel].mask << mask_shift, bindtype);
				continue;
			default:continue;
		}

		draw_key_config(opts, opt_cnt, player_idx, sel, dev_id, dev_count, 1);

		/* wait for some up event */
		for (is_down = 1; is_down; )
			kc = in_update_keycode(&dev_id, &is_down, -1);

		i = count_bound_keys(dev_id, opts[sel].mask << mask_shift, bindtype);
		unbind = (i > 0);

		/* allow combos if device supports them */
		if (i == 1 && bindtype == IN_BINDTYPE_EMU &&
				in_get_dev_info(dev_id, IN_INFO_DOES_COMBOS))
			unbind = 0;

		in_bind_key(dev_id, kc, opts[sel].mask << mask_shift, bindtype, unbind);
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

me_bind_action emuctrl_actions[] =
{
	{ "Load State       ", PEV_STATE_LOAD },
	{ "Save State       ", PEV_STATE_SAVE },
	{ "Prev Save Slot   ", PEV_SSLOT_PREV },
	{ "Next Save Slot   ", PEV_SSLOT_NEXT },
	{ "Switch Renderer  ", PEV_SWITCH_RND },
	{ "Volume Down      ", PEV_VOL_DOWN },
	{ "Volume Up        ", PEV_VOL_UP },
	{ "Fast forward     ", PEV_FF },
	{ "Enter Menu       ", PEV_MENU },
	{ "Pico Next page   ", PEV_PICO_PNEXT },
	{ "Pico Prev page   ", PEV_PICO_PPREV },
	{ "Pico Switch input", PEV_PICO_SWINP },
	{ NULL,                0 }
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
static const char *mgn_saveloadcfg(menu_id id, int *offs);

static menu_entry e_menu_keyconfig[] =
{
	mee_handler_id("Player 1",          MA_CTRL_PLAYER1,    key_config_loop_wrap),
	mee_handler_id("Player 2",          MA_CTRL_PLAYER2,    key_config_loop_wrap),
	mee_handler_id("Emulator controls", MA_CTRL_EMU,        key_config_loop_wrap),
	mee_onoff     ("6 button pad",      MA_OPT_6BUTTON_PAD, PicoOpt, POPT_6BTN_PAD),
	mee_range     ("Turbo rate",        MA_CTRL_TURBO_RATE, currentConfig.turbo_rate, 1, 30),
	mee_cust_nosave("Save global config",       MA_OPT_SAVECFG, mh_saveloadcfg, mgn_saveloadcfg),
	mee_cust_nosave("Save cfg for loaded game", MA_OPT_SAVECFG_GAME, mh_saveloadcfg, mgn_saveloadcfg),
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

	me_enable(e_menu_keyconfig, MA_OPT_SAVECFG_GAME, rom_loaded);
	me_loop(e_menu_keyconfig, &sel, NULL);
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

static const char h_cdleds[] = "Show power/CD LEDs of emulated console";
static const char h_cdda[]   = "Play audio tracks from mp3s/wavs/bins";
static const char h_cdpcm[]  = "Emulate PCM audio chip for effects/voices/music";
static const char h_srcart[] = "Emulate the save RAM cartridge accessory\n"
				"most games don't need this";
static const char h_scfx[]   = "Emulate scale/rotate ASIC chip for graphics effects\n"
				"disable to improve performance";
static const char h_bsync[]  = "More accurate mode for CPUs (needed for some games)\n"
				"disable to improve performance";

static menu_entry e_menu_cd_options[] =
{
	mee_onoff_h("CD LEDs",              MA_CDOPT_LEDS,          currentConfig.EmuOpt, EOPT_EN_CD_LEDS, h_cdleds),
	mee_onoff_h("CDDA audio",           MA_CDOPT_CDDA,          PicoOpt, POPT_EN_MCD_CDDA, h_cdda),
	mee_onoff_h("PCM audio",            MA_CDOPT_PCM,           PicoOpt, POPT_EN_MCD_PCM, h_cdpcm),
	mee_cust   ("ReadAhead buffer",     MA_CDOPT_READAHEAD,     mh_cdopt_ra, mgn_cdopt_ra),
	mee_onoff_h("SaveRAM cart",         MA_CDOPT_SAVERAM,       PicoOpt, POPT_EN_MCD_RAMCART, h_srcart),
	mee_onoff_h("Scale/Rot. fx (slow)", MA_CDOPT_SCALEROT_CHIP, PicoOpt, POPT_EN_MCD_GFX, h_scfx),
	mee_onoff_h("Better sync (slow)",   MA_CDOPT_BETTER_SYNC,   PicoOpt, POPT_EN_MCD_PSYNC, h_bsync),
	mee_end,
};

static int menu_loop_cd_options(menu_id id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_cd_options, &sel, NULL);
	return 0;
}

// ------------ 32X options menu ------------

// convert from multiplier of VClk
static int mh_opt_sh2cycles(menu_id id, int keys)
{
	int *mul = (id == MA_32XOPT_MSH2_CYCLES) ? &p32x_msh2_multiplier : &p32x_ssh2_multiplier;

	if (keys & (PBTN_LEFT|PBTN_RIGHT))
		*mul += (keys & PBTN_LEFT) ? -10 : 10;
	if (keys & (PBTN_L|PBTN_R))
		*mul += (keys & PBTN_L) ? -100 : 100;

	if (*mul < 1)
		*mul = 1;
	else if (*mul > (10 << SH2_MULTI_SHIFT))
		*mul = 10 << SH2_MULTI_SHIFT;

	return 0;
}

static const char *mgn_opt_sh2cycles(menu_id id, int *offs)
{
	int mul = (id == MA_32XOPT_MSH2_CYCLES) ? p32x_msh2_multiplier : p32x_ssh2_multiplier;
	
	sprintf(static_buff, "%d", 7670 * mul >> SH2_MULTI_SHIFT);
	return static_buff;
}

static const char h_32x_enable[] = "Enable emulation of the 32X addon";
static const char h_pwm[]        = "Disabling may improve performance, but break sound";
static const char h_sh2cycles[]  = "Cycles/millisecond (similar to DOSBox)\n"
	"lower values speed up emulation but break games\n"
	"at least 11000 recommended for compatibility";

static menu_entry e_menu_32x_options[] =
{
	mee_onoff_h   ("32X enabled",       MA_32XOPT_ENABLE_32X,  PicoOpt, POPT_EN_32X, h_32x_enable),
	mee_enum      ("32X renderer",      MA_32XOPT_RENDERER,    currentConfig.renderer32x, renderer_names32x),
	mee_onoff_h   ("PWM sound",         MA_32XOPT_PWM,         PicoOpt, POPT_EN_PWM, h_pwm),
	mee_cust_h    ("Master SH2 cycles", MA_32XOPT_MSH2_CYCLES, mh_opt_sh2cycles, mgn_opt_sh2cycles, h_sh2cycles),
	mee_cust_h    ("Slave SH2 cycles",  MA_32XOPT_SSH2_CYCLES, mh_opt_sh2cycles, mgn_opt_sh2cycles, h_sh2cycles),
	mee_end,
};

static int menu_loop_32x_options(menu_id id, int keys)
{
	static int sel = 0;

	me_enable(e_menu_32x_options, MA_32XOPT_RENDERER, renderer_names32x != NULL);
	me_loop(e_menu_32x_options, &sel, NULL);

	return 0;
}

// ------------ adv options menu ------------

static menu_entry e_menu_adv_options[] =
{
	mee_onoff     ("SRAM/BRAM saves",          MA_OPT_SRAM_STATES,    currentConfig.EmuOpt, EOPT_EN_SRAM),
	mee_onoff     ("Disable sprite limit",     MA_OPT2_NO_SPRITE_LIM, PicoOpt, POPT_DIS_SPRITE_LIM),
	mee_onoff     ("Emulate Z80",              MA_OPT2_ENABLE_Z80,    PicoOpt, POPT_EN_Z80),
	mee_onoff     ("Emulate YM2612 (FM)",      MA_OPT2_ENABLE_YM2612, PicoOpt, POPT_EN_FM),
	mee_onoff     ("Emulate SN76496 (PSG)",    MA_OPT2_ENABLE_SN76496,PicoOpt, POPT_EN_PSG),
	mee_onoff     ("gzip savestates",          MA_OPT2_GZIP_STATES,   currentConfig.EmuOpt, EOPT_GZIP_SAVES),
	mee_onoff     ("Don't save last used ROM", MA_OPT2_NO_LAST_ROM,   currentConfig.EmuOpt, EOPT_NO_AUTOSVCFG),
	mee_onoff     ("Disable idle loop patching",MA_OPT2_NO_IDLE_LOOPS,PicoOpt, POPT_DIS_IDLE_DET),
	mee_onoff     ("Disable frame limiter",    MA_OPT2_NO_FRAME_LIMIT,currentConfig.EmuOpt, EOPT_NO_FRMLIMIT),
	MENU_OPTIONS_ADV
	mee_end,
};

static int menu_loop_adv_options(menu_id id, int keys)
{
	static int sel = 0;
	me_loop(e_menu_adv_options, &sel, NULL);
	return 0;
}

// ------------ gfx options menu ------------

static menu_entry e_menu_gfx_options[] =
{
	mee_enum("Renderer", MA_OPT_RENDERER, currentConfig.renderer, renderer_names),
	MENU_OPTIONS_GFX
	mee_end,
};

static int menu_loop_gfx_options(menu_id id, int keys)
{
	static int sel = 0;

	me_enable(e_menu_gfx_options, MA_OPT_RENDERER, renderer_names[0] != NULL);
	me_loop(e_menu_gfx_options, &sel, NULL);

	return 0;
}

// ------------ options menu ------------

static menu_entry e_menu_options[];

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
	switch (id) {
	case MA_OPT_SOUND_QUALITY:
		PsndRate = sndrate_prevnext(PsndRate, keys & PBTN_RIGHT);
		break;
	case MA_OPT_REGION:
		region_prevnext(keys & PBTN_RIGHT);
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
		if (emu_write_config(id == MA_OPT_SAVECFG_GAME ? 1 : 0))
			me_update_msg("config saved");
		else
			me_update_msg("failed to write config");
		break;
	case MA_OPT_LOADCFG:
		ret = emu_read_config(rom_fname_loaded, 1);
		if (!ret) ret = emu_read_config(NULL, 1);
		if (ret)  me_update_msg("config loaded");
		else      me_update_msg("failed to load config");
		break;
	default:
		return 0;
	}

	return 1;
}

static int mh_restore_defaults(menu_id id, int keys)
{
	emu_set_defconfig();
	me_update_msg("defaults restored");
	return 1;
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

static const char *mgn_saveloadcfg(menu_id id, int *offs)
{
	static_buff[0] = 0;
	if (config_slot != 0)
		sprintf(static_buff, "[%i]", config_slot);
	return static_buff;
}

static const char *men_confirm_save[] = { "OFF", "writes", "loads", "both", NULL };
static const char h_confirm_save[]    = "Ask for confirmation when overwriting save,\n"
					"loading state or both";

static menu_entry e_menu_options[] =
{
	mee_range     ("Save slot",                MA_OPT_SAVE_SLOT,     state_slot, 0, 9),
	mee_range_cust("Frameskip",                MA_OPT_FRAMESKIP,     currentConfig.Frameskip, -1, 16, mgn_opt_fskip),
	mee_cust      ("Region",                   MA_OPT_REGION,        mh_opt_misc, mgn_opt_region),
	mee_onoff     ("Show FPS",                 MA_OPT_SHOW_FPS,      currentConfig.EmuOpt, EOPT_SHOW_FPS),
	mee_onoff     ("Enable sound",             MA_OPT_ENABLE_SOUND,  currentConfig.EmuOpt, EOPT_EN_SOUND),
	mee_cust      ("Sound Quality",            MA_OPT_SOUND_QUALITY, mh_opt_misc, mgn_opt_sound),
	mee_enum_h    ("Confirm savestate",        MA_OPT_CONFIRM_STATES,currentConfig.confirm_save, men_confirm_save, h_confirm_save),
	mee_range     (cpu_clk_name,               MA_OPT_CPU_CLOCKS,    currentConfig.CPUclock, 20, 900),
	mee_handler   ("[Display options]",        menu_loop_gfx_options),
	mee_handler   ("[Sega/Mega CD options]",   menu_loop_cd_options),
	mee_handler   ("[32X options]",            menu_loop_32x_options),
	mee_handler   ("[Advanced options]",       menu_loop_adv_options),
	mee_cust_nosave("Save global config",      MA_OPT_SAVECFG, mh_saveloadcfg, mgn_saveloadcfg),
	mee_cust_nosave("Save cfg for loaded game",MA_OPT_SAVECFG_GAME, mh_saveloadcfg, mgn_saveloadcfg),
	mee_cust_nosave("Load cfg from profile",   MA_OPT_LOADCFG, mh_saveloadcfg, mgn_saveloadcfg),
	mee_handler   ("Restore defaults",         mh_restore_defaults),
	mee_end,
};

static int menu_loop_options(menu_id id, int keys)
{
	static int sel = 0;

	me_enable(e_menu_options, MA_OPT_SAVECFG_GAME, rom_loaded);
	me_enable(e_menu_options, MA_OPT_LOADCFG, config_slot != config_slot_current);

	me_loop(e_menu_options, &sel, NULL);

	return 0;
}

// ------------ debug menu ------------

#include <pico/debug.h>

extern void SekStepM68k(void);

static void mplayer_loop(void)
{
	pemu_sound_start();

	while (1)
	{
		PDebugZ80Frame();
		if (in_menu_wait_any(0) & PBTN_MA3)
			break;
		pemu_sound_wait();
	}

	pemu_sound_stop();
}

static void draw_text_debug(const char *str, int skip, int from)
{
	const char *p;
	int line;

	p = str;
	while (skip-- > 0)
	{
		while (*p && *p != '\n')
			p++;
		if (*p == 0 || p[1] == 0)
			return;
		p++;
	}

	str = p;
	for (line = from; line < g_screen_height / me_sfont_h; line++)
	{
		smalltext_out16(1, line * me_sfont_h, str, 0xffff);
		while (*p && *p != '\n')
			p++;
		if (*p == 0)
			break;
		p++; str = p;
	}
}

#ifdef __GNUC__
#define COMPILER "gcc " __VERSION__
#else
#define COMPILER
#endif

static void draw_frame_debug(void)
{
	char layer_str[48] = "layers:                   ";
	if (PicoDrawMask & PDRAW_LAYERB_ON)      memcpy(layer_str +  8, "B", 1);
	if (PicoDrawMask & PDRAW_LAYERA_ON)      memcpy(layer_str + 10, "A", 1);
	if (PicoDrawMask & PDRAW_SPRITES_LOW_ON) memcpy(layer_str + 12, "spr_lo", 6);
	if (PicoDrawMask & PDRAW_SPRITES_HI_ON)  memcpy(layer_str + 19, "spr_hi", 6);
	if (PicoDrawMask & PDRAW_32X_ON)         memcpy(layer_str + 26, "32x", 4);

	memset(g_screen_ptr, 0, g_screen_width * g_screen_height * 2);
	pemu_forced_frame(0);
	smalltext_out16(4, 1, "build: r" REVISION "  "__DATE__ " " __TIME__ " " COMPILER, 0xffff);
	smalltext_out16(4, g_screen_height - me_sfont_h, layer_str, 0xffff);
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
				plat_debug_cat(tmp);
				draw_text_debug(tmp, 0, 0);
				if (dumped) {
					smalltext_out16(g_screen_width - 6 * me_sfont_h,
						g_screen_height - me_mfont_h, "dumped", 0xffff);
					dumped = 0;
				}
				break;
			case 1: draw_frame_debug(); break;
			case 2: memset(g_screen_ptr, 0, g_screen_width * g_screen_height * 2);
				pemu_forced_frame(0);
				menu_darken_bg(g_screen_ptr, g_screen_ptr, g_screen_width * g_screen_height, 0);
				PDebugShowSpriteStats((unsigned short *)g_screen_ptr + (g_screen_height/2 - 240/2)*g_screen_width +
					g_screen_width/2 - 320/2, g_screen_width); break;
			case 3: memset(g_screen_ptr, 0, g_screen_width * g_screen_height * 2);
				PDebugShowPalette(g_screen_ptr, g_screen_width);
				PDebugShowSprite((unsigned short *)g_screen_ptr + g_screen_width*120 + g_screen_width/2 + 16,
					g_screen_width, spr_offs);
				draw_text_debug(PDebugSpriteList(), spr_offs, 6);
				break;
			case 4: plat_video_menu_begin();
				tmp = PDebug32x();
				draw_text_debug(tmp, 0, 0);
				break;
		}
		plat_video_menu_end();

		inp = in_menu_wait(PBTN_MOK|PBTN_MBACK|PBTN_MA2|PBTN_MA3|PBTN_L|PBTN_R |
					PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT, 70);
		if (inp & PBTN_MBACK) return;
		if (inp & PBTN_L) { mode--; if (mode < 0) mode = 4; }
		if (inp & PBTN_R) { mode++; if (mode > 4) mode = 0; }
		switch (mode)
		{
			case 0:
				if (inp & PBTN_MOK)
					PDebugCPUStep();
				if (inp & PBTN_MA3) {
					while (inp & PBTN_MA3)
						inp = in_menu_wait_any(-1);
					mplayer_loop();
				}
				if ((inp & (PBTN_MA2|PBTN_LEFT)) == (PBTN_MA2|PBTN_LEFT)) {
					mkdir("dumps", 0777);
					PDebugDumpMem();
					while (inp & PBTN_MA2) inp = in_menu_wait_any(-1);
					dumped = 1;
				}
				break;
			case 1:
				if (inp & PBTN_LEFT)  PicoDrawMask ^= PDRAW_LAYERB_ON;
				if (inp & PBTN_RIGHT) PicoDrawMask ^= PDRAW_LAYERA_ON;
				if (inp & PBTN_DOWN)  PicoDrawMask ^= PDRAW_SPRITES_LOW_ON;
				if (inp & PBTN_UP)    PicoDrawMask ^= PDRAW_SPRITES_HI_ON;
				if (inp & PBTN_MA2)   PicoDrawMask ^= PDRAW_32X_ON;
				if (inp & PBTN_MOK) {
					PsndOut = NULL; // just in case
					PicoSkipFrame = 1;
					PicoFrame();
					PicoSkipFrame = 0;
					while (inp & PBTN_MOK) inp = in_menu_wait_any(-1);
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
			emu_reset_game();
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
			me_update_msg("Patches applied");
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
	mee_label     ("PicoDrive " VERSION),
	mee_label     (""),
	mee_label     (""),
	mee_label     (""),
	mee_handler_id("Resume game",        MA_MAIN_RESUME_GAME, main_menu_handler),
	mee_handler_id("Save State",         MA_MAIN_SAVE_STATE,  main_menu_handler),
	mee_handler_id("Load State",         MA_MAIN_LOAD_STATE,  main_menu_handler),
	mee_handler_id("Reset game",         MA_MAIN_RESET_GAME,  main_menu_handler),
	mee_handler_id("Load new ROM/ISO",   MA_MAIN_LOAD_ROM,    main_menu_handler),
	mee_handler   ("Change options",                          menu_loop_options),
	mee_handler   ("Configure controls",                      menu_loop_keyconfig),
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

	menu_enter(rom_loaded);
	in_set_blocking(1);
	me_loop(e_menu_main, &sel, menu_main_plat_draw);

	if (rom_loaded) {
		if (engineState == PGS_Menu)
			engineState = PGS_Running;
		/* wait until menu, ok, back is released */
		while (in_menu_wait_any(50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK))
			;
	}

	in_set_blocking(0);
}

// --------- CD tray close menu ----------

static int mh_tray_load_cd(menu_id id, int keys)
{
	char *ret_name;

	ret_name = romsel_run();
	if (ret_name == NULL)
		return 0;

	engineState = PGS_RestartRun;
	return emu_swap_cd(ret_name);
}

static int mh_tray_nothing(menu_id id, int keys)
{
	return 1;
}

static menu_entry e_menu_tray[] =
{
	mee_label  ("The CD tray has opened."),
	mee_label  (""),
	mee_label  (""),
	mee_handler("Load CD image",  mh_tray_load_cd),
	mee_handler("Insert nothing", mh_tray_nothing),
	mee_end,
};

int menu_loop_tray(void)
{
	int ret = 1, sel = 0;

	menu_enter(rom_loaded);

	in_set_blocking(1);
	me_loop(e_menu_tray, &sel, NULL);

	if (engineState != PGS_RestartRun) {
		engineState = PGS_RestartRun;
		ret = 0; /* no CD inserted */
	}

	while (in_menu_wait_any(50) & (PBTN_MENU|PBTN_MOK|PBTN_MBACK));
	in_set_blocking(0);

	return ret;
}

#endif // !UIQ3

void me_update_msg(const char *msg)
{
	strncpy(menu_error_msg, msg, sizeof(menu_error_msg));
	menu_error_msg[sizeof(menu_error_msg) - 1] = 0;

	menu_error_time = plat_get_ticks_ms();
	lprintf("msg: %s\n", menu_error_msg);
}

// ------------ util ------------

/* GP2X/wiz for now, probably extend later */
void menu_plat_setup(int is_wiz)
{
	int i;

	if (!is_wiz) {
		me_enable(e_menu_gfx_options, MA_OPT_TEARING_FIX, 0);
		i = me_id2offset(e_menu_gfx_options, MA_OPT_TEARING_FIX);
		e_menu_gfx_options[i].need_to_save = 0;
		return;
	}

	me_enable(e_menu_adv_options, MA_OPT_ARM940_SOUND, 0);
	me_enable(e_menu_gfx_options, MA_OPT2_GAMMA, 0);
	me_enable(e_menu_gfx_options, MA_OPT2_A_SN_GAMMA, 0);

	i = me_id2offset(e_menu_gfx_options, MA_OPT_SCALING);
	e_menu_gfx_options[i].max = 1;	/* only off and sw */
	i = me_id2offset(e_menu_gfx_options, MA_OPT_ARM940_SOUND);
	e_menu_gfx_options[i].need_to_save = 0;
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
	e_menu_32x_options,
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

