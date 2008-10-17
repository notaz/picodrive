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
#include "emu.h"


char menuErrorMsg[64] = { 0, };

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
		len = strlen(text);

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
	char    buffer[512];

	va_start(args,texto);
	vsprintf(buffer,texto,args);
	va_end(args);

	text_out16_(x,y,buffer,menu_text_color);
}


void smalltext_out16(int x, int y, const char *texto, int color)
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

void smalltext_out16_lim(int x, int y, const char *texto, int color, int max)
{
	char    buffer[SCREEN_WIDTH/6+1];

	strncpy(buffer, texto, SCREEN_WIDTH/6);
	if (max > SCREEN_WIDTH/6) max = SCREEN_WIDTH/6;
	if (max < 0) max = 0;
	buffer[max] = 0;

	smalltext_out16(x, y, buffer, color);
}

void menu_draw_selection(int x, int y, int w)
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


int me_id2offset(const menu_entry *entries, int count, menu_id id)
{
	int i;
	for (i = 0; i < count; i++)
	{
		if (entries[i].id == id) return i;
	}

	lprintf("%s: id %i not found\n", __FUNCTION__, id);
	return 0;
}

void me_enable(menu_entry *entries, int count, menu_id id, int enable)
{
	int i = me_id2offset(entries, count, id);
	entries[i].enabled = enable;
}

int me_count_enabled(const menu_entry *entries, int count)
{
	int i, ret = 0;

	for (i = 0; i < count; i++)
	{
		if (entries[i].enabled) ret++;
	}

	return ret;
}

menu_id me_index2id(const menu_entry *entries, int count, int index)
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

void me_draw(const menu_entry *entries, int count, int x, int y, me_draw_custom_f *cust_draw, void *param)
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
		text_out16(x, y1, entries[i].name);
		if (entries[i].beh == MB_ONOFF)
			text_out16(x + 27*8, y1, (*(int *)entries[i].var & entries[i].mask) ? "ON" : "OFF");
		else if (entries[i].beh == MB_RANGE)
			text_out16(x + 27*8, y1, "%i", *(int *)entries[i].var);
		y1 += 10;
	}
}

int me_process(menu_entry *entries, int count, menu_id id, int is_next)
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

// ------------ debug menu ------------

#include <sys/stat.h>
#include <sys/types.h>

#include <pico/pico.h>
#include <pico/debug.h>

void SekStepM68k(void);

static void mplayer_loop(void)
{
	emu_startSound();

	while (1)
	{
		PDebugZ80Frame();
		if (read_buttons_async(PBTN_NORTH)) break;
		emu_waitSound();
	}

	emu_endSound();
}

static void draw_text_debug(const char *str, int skip, int from)
{
	const char *p;
	int len, line;

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
		len = p - str;
		if (len > 55) len = 55;
		smalltext_out16_lim(1, line*10, str, 0xffff, len);
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

void debug_menu_loop(void)
{
	int inp, mode = 0;
	int spr_offs = 0, dumped = 0;
	char *tmp;

	while (1)
	{
		switch (mode)
		{
			case 0: menu_draw_begin();
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
		menu_draw_end();

		inp = read_buttons(PBTN_EAST|PBTN_SOUTH|PBTN_WEST|PBTN_NORTH|PBTN_L|PBTN_R|PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT);
		if (inp & PBTN_SOUTH) return;
		if (inp & PBTN_L) { mode--; if (mode < 0) mode = 3; }
		if (inp & PBTN_R) { mode++; if (mode > 3) mode = 0; }
		switch (mode)
		{
			case 0:
				if (inp & PBTN_EAST) SekStepM68k();
				if (inp & PBTN_NORTH) {
					while (inp & PBTN_NORTH) inp = read_buttons_async(PBTN_NORTH);
					mplayer_loop();
				}
				if ((inp & (PBTN_WEST|PBTN_LEFT)) == (PBTN_WEST|PBTN_LEFT)) {
					mkdir("dumps", 0777);
					PDebugDumpMem();
					while (inp & PBTN_WEST) inp = read_buttons_async(PBTN_WEST);
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
					while (inp & PBTN_EAST) inp = read_buttons_async(PBTN_EAST);
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

#endif // !UIQ3

// ------------ util ------------

const char *me_region_name(unsigned int code, int auto_order)
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
			i = 0; code = ((auto_order >> u*4) & 0xf) << 1;
			while((code >>= 1)) i++;
			strcat(name, names_short[i]);
		}
		return name;
	}
}


