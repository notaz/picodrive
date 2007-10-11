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

#if   defined(__GP2X__)
 #include "../gp2x/gp2x.h"
 #define SCREEN_WIDTH 320
 #define SCREEN_BUFFER gp2x_screen
#elif defined(__GIZ__)
 //#include "../gizmondo/giz.h"
 #define SCREEN_WIDTH 321
 #define SCREEN_BUFFER menu_screen
 extern unsigned char *menu_screen;
#elif defined(PSP)
 #include "../psp/psp.h"
 #define SCREEN_WIDTH 512
 #define SCREEN_BUFFER psp_screen
#endif

char menuErrorMsg[64] = { 0, };

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
	if (endp != buff) return ((t>>8)&0xf800) | ((t>>5)&0x07e0) | ((t>>3)&0x1f);
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
				lprintf("sel color: %04x\n", menu_sel_color);
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



