// (c) Copyright 2006 notaz, All rights reserved.
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
#include "version.h"

#include "Pico/PicoInt.h"

#ifndef _DIRENT_HAVE_D_TYPE
#error "need d_type for file browser
#endif

extern char *actionNames[];
extern char romFileName[PATH_MAX];
extern char *rom_data;
extern int  mmuhack_status;
extern int  state_slot;

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


static unsigned long inp_prev = 0;
static int inp_prevjoy = 0;

static unsigned long wait_for_input(unsigned long interesting)
{
	unsigned long ret;
	static int repeats = 0, wait = 300*1000;
	int release = 0, i;

	if (repeats == 5 || repeats == 15 || repeats == 30) wait /= 2;

	for (i = 0; i < 6 && inp_prev == gp2x_joystick_read(1); i++) {
		if(i == 0) repeats++;
		usleep(wait/6);
	}

	while ( !((ret = gp2x_joystick_read(1)) & interesting) ) {
		usleep(50000);
		release = 1;
	}

	if (release || ret != inp_prev) {
		repeats = 0;
		wait = 300*1000;
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



// -------------- ROM selector --------------

static void draw_dirlist(char *curdir, struct dirent **namelist, int n, int sel)
{
	int start, i, pos;

	start = 12 - sel;
	n--; // exclude current dir (".")

	//memset(gp2x_screen, 0, 320*240);
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

	n = scandir(curr_path, &namelist, 0, scandir_cmp);
	if (n < 0) {
		// try root
		n = scandir(curr_path, &namelist, 0, scandir_cmp);
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
		if(inp &(GP2X_LEFT|GP2X_L))  { sel-=10; if (sel < 0)   sel = 0; }
		if(inp &(GP2X_RIGHT|GP2X_R)) { sel+=10; if (sel > n-2) sel = n-2; }
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

static void draw_key_config(int curr_act, int is_p2)
{
	char strkeys[32*5];
	int joy, i;

	strkeys[0] = 0;
	for (i = 0; i < 32; i++)
	{
		if (currentConfig.KeyBinds[i] & (1 << curr_act))
		{
			if (curr_act < 16 && (currentConfig.KeyBinds[i] & (1 << 16)) != (is_p2 << 16)) continue;
			if (strkeys[0]) { strcat(strkeys, " + "); strcat(strkeys, gp2xKeyNames[i]); break; }
			else strcpy(strkeys, gp2xKeyNames[i]);
		}
	}
	for (joy = 0; joy < num_of_joys; joy++)
	{
		for (i = 0; i < 32; i++)
		{
			if (currentConfig.JoyBinds[joy][i] & (1 << curr_act))
			{
				if (curr_act < 16 && (currentConfig.JoyBinds[joy][i] & (1 << 16)) != (is_p2 << 16)) continue;
				if (strkeys[0]) {
					strcat(strkeys, ", "); strcat(strkeys, usb_joy_key_name(joy + 1, i));
					break;
				}
				else strcpy(strkeys, usb_joy_key_name(joy + 1, i));
			}
		}
	}

	//memset(gp2x_screen, 0, 320*240);
	gp2x_pd_clone_buffer2();
	gp2x_text_out8(60, 40, "Action: %s", actionNames[curr_act]);
	gp2x_text_out8(60, 60, "Keys: %s", strkeys);

	gp2x_text_out8(30, 180, "Use SELECT to change action");
	gp2x_text_out8(30, 190, "Press a key to bind/unbind");
	gp2x_text_out8(30, 200, "Select \"Done\" action and");
	gp2x_text_out8(30, 210, "  press any key to finish");
	gp2x_video_flip2();
}

static void key_config_loop(int is_p2)
{
	int curr_act = 0, joy = 0, i;
	unsigned long inp = 0;

	for (;;)
	{
		draw_key_config(curr_act, is_p2);
		inp = wait_for_input_usbjoy(CONFIGURABLE_KEYS, &joy);
		// printf("got %08lX from joy %i\n", inp, joy);
		if (joy == 0) {
			if (inp & GP2X_SELECT) {
				curr_act++;
				while (!actionNames[curr_act] && curr_act < 32) curr_act++;
				if (curr_act > 31) curr_act = 0;
			}
			inp &= CONFIGURABLE_KEYS;
			inp &= ~GP2X_SELECT;
		}
		if (curr_act == 31 && inp) break;
		if (joy == 0) {
			for (i = 0; i < 32; i++)
				if (inp & (1 << i)) {
					currentConfig.KeyBinds[i] ^= (1 << curr_act);
					if (is_p2) currentConfig.KeyBinds[i] |=  (1 << 16); // player 2 flag
					else       currentConfig.KeyBinds[i] &= ~(1 << 16);
				}
		} else {
			for (i = 0; i < 32; i++)
				if (inp & (1 << i)) {
					currentConfig.JoyBinds[joy-1][i] ^= (1 << curr_act);
					if (is_p2) currentConfig.JoyBinds[joy-1][i] |=  (1 << 16);
					else       currentConfig.JoyBinds[joy-1][i] &= ~(1 << 16);
				}
		}
	}
}

static void draw_kc_sel(int menu_sel)
{
	int tl_x = 25+40, tl_y = 60, y, i;
	char joyname[36];

	y = tl_y;
	//memset(gp2x_screen, 0, 320*240);
	gp2x_pd_clone_buffer2();
	gp2x_text_out8(tl_x, y,       "Player 1");
	gp2x_text_out8(tl_x, (y+=10), "Player 2");
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

static void kc_sel_loop(void)
{
	int menu_sel = 2, menu_sel_max = 2;
	unsigned long inp = 0;

	for(;;)
	{
		draw_kc_sel(menu_sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_B|GP2X_X);
		if(inp & GP2X_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if(inp & GP2X_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		if(inp & GP2X_B) {
			switch (menu_sel) {
				case 0: key_config_loop(0); return;
				case 1: key_config_loop(1); return;
				default: return;
			}
		}
		if(inp & GP2X_X) return;
	}
}



// --------- sega/mega cd options ----------

static void draw_cd_menu_options(int menu_sel, char *b_us, char *b_eu, char *b_jp)
{
	int tl_x = 25, tl_y = 60, y;

	y = tl_y;
	//memset(gp2x_screen, 0, 320*240);
	gp2x_pd_clone_buffer2();

	gp2x_text_out8(tl_x, y,       "USA BIOS:     %s", b_us); // 0
	gp2x_text_out8(tl_x, (y+=10), "EUR BIOS:     %s", b_eu); // 1
	gp2x_text_out8(tl_x, (y+=10), "JAP BIOS:     %s", b_jp); // 2
	gp2x_text_out8(tl_x, (y+=10), "CD LEDs                    %s", (currentConfig.EmuOpt &0x400)?"ON":"OFF"); // 3
	gp2x_text_out8(tl_x, (y+=10), "CDDA audio (using mp3s)    %s", (currentConfig.PicoOpt&0x800)?"ON":"OFF"); // 4
	gp2x_text_out8(tl_x, (y+=10), "PCM audio                  %s", (currentConfig.PicoOpt&0x400)?"ON":"OFF"); // 5
	gp2x_text_out8(tl_x, (y+=10), "Done");

	// draw cursor
	gp2x_text_out8(tl_x - 16, tl_y + menu_sel*10, ">");

	if ((menu_sel == 0 && strcmp(b_us, "NOT FOUND")) ||
		(menu_sel == 1 && strcmp(b_eu, "NOT FOUND")) ||
		(menu_sel == 2 && strcmp(b_jp, "NOT FOUND")))
			gp2x_text_out8(tl_x, 220, "Press start to test selected BIOS");

	gp2x_video_flip2();
}

static void cd_menu_loop_options(void)
{
	int menu_sel = 0, menu_sel_max = 6;
	unsigned long inp = 0;
	char bios_us[32], bios_eu[32], bios_jp[32], *bios, *p;

	if (find_bios(4, &bios)) { // US
		for (p = bios+strlen(bios)-1; p > bios && *p != '/'; p--); p++;
		strncpy(bios_us, p, 31); bios_us[31] = 0;
	} else	strcpy(bios_us, "NOT FOUND");

	if (find_bios(8, &bios)) { // EU
		for (p = bios+strlen(bios)-1; p > bios && *p != '/'; p--); p++;
		strncpy(bios_eu, p, 31); bios_eu[31] = 0;
	} else	strcpy(bios_eu, "NOT FOUND");

	if (find_bios(1, &bios)) { // JP
		for (p = bios+strlen(bios)-1; p > bios && *p != '/'; p--); p++;
		strncpy(bios_jp, p, 31); bios_jp[31] = 0;
	} else	strcpy(bios_jp, "NOT FOUND");

	for(;;)
	{
		draw_cd_menu_options(menu_sel, bios_us, bios_eu, bios_jp);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_LEFT|GP2X_RIGHT|GP2X_B|GP2X_X|GP2X_A|GP2X_START);
		if(inp & GP2X_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if(inp & GP2X_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		if((inp& GP2X_B)||(inp&GP2X_LEFT)||(inp&GP2X_RIGHT)) { // toggleable options
			switch (menu_sel) {
				case  3: currentConfig.EmuOpt ^=0x400; break;
				case  4: currentConfig.PicoOpt^=0x800; break;
				case  5: currentConfig.PicoOpt^=0x400; break;
				case  6: return;
			}
		}
		if(inp & (GP2X_X|GP2X_A)) return;
		if(inp &  GP2X_START) { // BIOS testers
			switch (menu_sel) {
				case 0:	if (find_bios(4, &bios)) { // test US
						strcpy(romFileName, bios);
						engineState = PGS_ReloadRom;
						return;
					}
					break;
				case 1:	if (find_bios(8, &bios)) { // test EU
						strcpy(romFileName, bios);
						engineState = PGS_ReloadRom;
						return;
					}
					break;
				case 2:	if (find_bios(1, &bios)) { // test JP
						strcpy(romFileName, bios);
						engineState = PGS_ReloadRom;
						return;
					}
					break;
			}
		}
	}
}


// --------- advanced options ----------

static void draw_amenu_options(int menu_sel)
{
	int tl_x = 25, tl_y = 60, y;
	char *mms = mmuhack_status ? "active)  " : "inactive)";

	y = tl_y;
	//memset(gp2x_screen, 0, 320*240);
	gp2x_pd_clone_buffer2();

	gp2x_text_out8(tl_x, y,       "Scale 32 column mode       %s", (currentConfig.PicoOpt&0x100)?"ON":"OFF"); // 0
	gp2x_text_out8(tl_x, (y+=10), "Gamma correction           %i.%02i", currentConfig.gamma / 100, currentConfig.gamma%100); // 1
	gp2x_text_out8(tl_x, (y+=10), "Emulate Z80                %s", (currentConfig.PicoOpt&0x004)?"ON":"OFF"); // 2
	gp2x_text_out8(tl_x, (y+=10), "Emulate YM2612 (FM)        %s", (currentConfig.PicoOpt&0x001)?"ON":"OFF"); // 3
	gp2x_text_out8(tl_x, (y+=10), "Emulate SN76496 (PSG)      %s", (currentConfig.PicoOpt&0x002)?"ON":"OFF"); // 4
	gp2x_text_out8(tl_x, (y+=10), "gzip savestates            %s", (currentConfig.EmuOpt &0x008)?"ON":"OFF"); // 5
	gp2x_text_out8(tl_x, (y+=10), "Don't save config on exit  %s", (currentConfig.EmuOpt &0x020)?"ON":"OFF"); // 6
	gp2x_text_out8(tl_x, (y+=10), "needs restart:");
	gp2x_text_out8(tl_x, (y+=10), "craigix's RAM timings      %s", (currentConfig.EmuOpt &0x100)?"ON":"OFF"); // 8
	gp2x_text_out8(tl_x, (y+=10), "squidgehack (now %s %s",   mms, (currentConfig.EmuOpt &0x010)?"ON":"OFF"); // 9
	gp2x_text_out8(tl_x, (y+=10), "Done");

	// draw cursor
	gp2x_text_out8(tl_x - 16, tl_y + menu_sel*10, ">");

	gp2x_video_flip2();
}

static void amenu_loop_options(void)
{
	int menu_sel = 0, menu_sel_max = 10;
	unsigned long inp = 0;

	for(;;)
	{
		draw_amenu_options(menu_sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_LEFT|GP2X_RIGHT|GP2X_B|GP2X_X|GP2X_A);
		if(inp & GP2X_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if(inp & GP2X_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		if((inp& GP2X_B)||(inp&GP2X_LEFT)||(inp&GP2X_RIGHT)) { // toggleable options
			switch (menu_sel) {
				case  0: currentConfig.PicoOpt^=0x100; break;
				case  2: currentConfig.PicoOpt^=0x004; break;
				case  3: currentConfig.PicoOpt^=0x001; break;
				case  4: currentConfig.PicoOpt^=0x002; break;
				case  5: currentConfig.EmuOpt ^=0x008; break;
				case  6: currentConfig.EmuOpt ^=0x020; break;
				case  8: currentConfig.EmuOpt ^=0x100; break;
				case  9: currentConfig.EmuOpt ^=0x010; break;
				case 10: return;
			}
		}
		if(inp & (GP2X_X|GP2X_A)) return;
		if(inp & (GP2X_LEFT|GP2X_RIGHT)) { // multi choise
			switch (menu_sel) {
				case 1:
					while ((inp = gp2x_joystick_read(1)) & (GP2X_LEFT|GP2X_RIGHT)) {
						currentConfig.gamma += (inp & GP2X_LEFT) ? -1 : 1;
						if (currentConfig.gamma <   1) currentConfig.gamma =   1;
						if (currentConfig.gamma > 300) currentConfig.gamma = 300;
						draw_amenu_options(menu_sel);
						usleep(18*1000);
					}
					break;
			}
		}
	}
}

// -------------- options --------------

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

static void draw_menu_options(int menu_sel)
{
	int tl_x = 25, tl_y = 40, y;
	char monostereo[8], strframeskip[8], *strrend;

	strcpy(monostereo, (currentConfig.PicoOpt&0x08)?"stereo":"mono");
	if (currentConfig.Frameskip < 0)
		 strcpy(strframeskip, "Auto");
	else sprintf(strframeskip, "%i", currentConfig.Frameskip);
	if (currentConfig.PicoOpt&0x10) {
		strrend = " 8bit fast";
	} else if (currentConfig.EmuOpt&0x80) {
		strrend = "16bit accurate";
	} else {
		strrend = " 8bit accurate";
	}

	y = tl_y;
	//memset(gp2x_screen, 0, 320*240);
	gp2x_pd_clone_buffer2();

	gp2x_text_out8(tl_x, y,       "Renderer:            %s", strrend); // 0
	gp2x_text_out8(tl_x, (y+=10), "Accurate timing (slower)   %s", (currentConfig.PicoOpt&0x040)?"ON":"OFF"); // 1
	gp2x_text_out8(tl_x, (y+=10), "Accurate sprites (slower)  %s", (currentConfig.PicoOpt&0x080)?"ON":"OFF"); // 2
	gp2x_text_out8(tl_x, (y+=10), "Show FPS                   %s", (currentConfig.EmuOpt &0x002)?"ON":"OFF"); // 3
	gp2x_text_out8(tl_x, (y+=10), "Frameskip                  %s", strframeskip);
	gp2x_text_out8(tl_x, (y+=10), "Enable sound               %s", (currentConfig.EmuOpt &0x004)?"ON":"OFF"); // 5
	gp2x_text_out8(tl_x, (y+=10), "Sound Quality:     %5iHz %s",   currentConfig.PsndRate, monostereo);
	gp2x_text_out8(tl_x, (y+=10), "Use ARM940 core for sound  %s", (currentConfig.PicoOpt&0x200)?"ON":"OFF"); // 7
	gp2x_text_out8(tl_x, (y+=10), "6 button pad               %s", (currentConfig.PicoOpt&0x020)?"ON":"OFF"); // 8
	gp2x_text_out8(tl_x, (y+=10), "Genesis Region:      %s",       region_name(currentConfig.PicoRegion));
	gp2x_text_out8(tl_x, (y+=10), "Use SRAM/BRAM savestates   %s", (currentConfig.EmuOpt &0x001)?"ON":"OFF"); // 10
	gp2x_text_out8(tl_x, (y+=10), "Confirm save overwrites    %s", (currentConfig.EmuOpt &0x200)?"ON":"OFF"); // 11
	gp2x_text_out8(tl_x, (y+=10), "Save slot                  %i", state_slot); // 12
	gp2x_text_out8(tl_x, (y+=10), "GP2X CPU clocks            %iMhz", currentConfig.CPUclock);
	gp2x_text_out8(tl_x, (y+=10), "[Sega/Mega CD options]");
	gp2x_text_out8(tl_x, (y+=10), "[advanced options]");		// 15
	gp2x_text_out8(tl_x, (y+=10), "Save cfg as default");
	if (rom_data)
		gp2x_text_out8(tl_x, (y+=10), "Save cfg for current game only");

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
	if (PicoOpt & 0x20) {
		actionNames[ 8] = "Z"; actionNames[ 9] = "Y";
		actionNames[10] = "X"; actionNames[11] = "MODE";
	} else {
		actionNames[8] = actionNames[9] = actionNames[10] = actionNames[11] = 0;
	}
}

static int menu_loop_options(void)
{
	int menu_sel = 0, menu_sel_max = 16;
	unsigned long inp = 0;

	if (rom_data) menu_sel_max++;
	currentConfig.PicoOpt = PicoOpt;
	currentConfig.PsndRate = PsndRate;
	currentConfig.PicoRegion = PicoRegionOverride;

	for(;;)
	{
		draw_menu_options(menu_sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_LEFT|GP2X_RIGHT|GP2X_B|GP2X_X|GP2X_A);
		if(inp & GP2X_UP  ) { menu_sel--; if (menu_sel < 0) menu_sel = menu_sel_max; }
		if(inp & GP2X_DOWN) { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = 0; }
		if((inp& GP2X_B)||(inp&GP2X_LEFT)||(inp&GP2X_RIGHT)) { // toggleable options
			switch (menu_sel) {
				case  1: currentConfig.PicoOpt^=0x040; break;
				case  2: currentConfig.PicoOpt^=0x080; break;
				case  3: currentConfig.EmuOpt ^=0x002; break;
				case  5: currentConfig.EmuOpt ^=0x004; break;
				case  7: currentConfig.PicoOpt^=0x200; break;
				case  8: currentConfig.PicoOpt^=0x020; break;
				case 10: currentConfig.EmuOpt ^=0x001; break;
				case 11: currentConfig.EmuOpt ^=0x200; break;
				case 14: cd_menu_loop_options();
					if (engineState == PGS_ReloadRom)
						return 0; // test BIOS
					break;
				case 15: amenu_loop_options();    break;
				case 16: // done (update and write)
					menu_options_save();
					if (emu_WriteConfig(0)) strcpy(menuErrorMsg, "config saved");
					else strcpy(menuErrorMsg, "failed to write config");
					return 1;
				case 17: // done (update and write for current game)
					menu_options_save();
					if (emu_WriteConfig(1)) strcpy(menuErrorMsg, "config saved");
					else strcpy(menuErrorMsg, "failed to write config");
					return 1;
			}
		}
		if(inp & (GP2X_X|GP2X_A)) {
			menu_options_save();
			return 0;  // done (update, no write)
		}
		if(inp & (GP2X_LEFT|GP2X_RIGHT)) { // multi choise
			switch (menu_sel) {
				case  0:
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
				case  4:
					currentConfig.Frameskip += (inp & GP2X_LEFT) ? -1 : 1;
					if (currentConfig.Frameskip < 0)  currentConfig.Frameskip = -1;
					if (currentConfig.Frameskip > 32) currentConfig.Frameskip = 32;
					break;
				case  6:
					if ((inp & GP2X_RIGHT) && currentConfig.PsndRate == 44100 && !(currentConfig.PicoOpt&0x08)) {
						currentConfig.PsndRate = 8000;  currentConfig.PicoOpt|= 0x08;
					} else if ((inp & GP2X_LEFT) && currentConfig.PsndRate == 8000 && (currentConfig.PicoOpt&0x08)) {
						currentConfig.PsndRate = 44100; currentConfig.PicoOpt&=~0x08;
					} else currentConfig.PsndRate = sndrate_prevnext(currentConfig.PsndRate, inp & GP2X_RIGHT);
					break;
				case  9:
					region_prevnext(inp & GP2X_RIGHT);
					break;
				case 12:
					if (inp & GP2X_RIGHT) {
						state_slot++; if (state_slot > 9) state_slot = 0;
					} else {state_slot--; if (state_slot < 0) state_slot = 9;
					}
					break;
				case 13:
					while ((inp = gp2x_joystick_read(1)) & (GP2X_LEFT|GP2X_RIGHT)) {
						currentConfig.CPUclock += (inp & GP2X_LEFT) ? -1 : 1;
						if (currentConfig.CPUclock < 1) currentConfig.CPUclock = 1;
						draw_menu_options(menu_sel);
						usleep(50*1000);
					}
					break;
			}
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

static void draw_menu_root(int menu_sel)
{
	int tl_x = 70, tl_y = 70, y;
	//memset(gp2x_screen, 0, 320*240);
	gp2x_pd_clone_buffer2();

	gp2x_text_out8(tl_x, 20, "PicoDrive v" VERSION);

	y = tl_y;
	if (rom_data) {
		gp2x_text_out8(tl_x, y,       "Resume game");
		gp2x_text_out8(tl_x, (y+=10), "Save State");
		gp2x_text_out8(tl_x, (y+=10), "Load State");
		gp2x_text_out8(tl_x, (y+=10), "Reset game");
	} else {
		y += 30;
	}
	gp2x_text_out8(tl_x, (y+=10), "Load new ROM/ISO");
	gp2x_text_out8(tl_x, (y+=10), "Change options");
	gp2x_text_out8(tl_x, (y+=10), "Configure controls");
	gp2x_text_out8(tl_x, (y+=10), "Credits");
	gp2x_text_out8(tl_x, (y+=10), "Exit");

	// draw cursor
	gp2x_text_out8(tl_x - 16, tl_y + menu_sel*10, ">");
	// error
	if (menuErrorMsg[0]) gp2x_text_out8(5, 226, menuErrorMsg);
	gp2x_video_flip2();
}


static void menu_loop_root(void)
{
	int ret, menu_sel = 4, menu_sel_max = 8, menu_sel_min = 4;
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

	if (rom_data) menu_sel = menu_sel_min = 0;

	for(;;)
	{
		draw_menu_root(menu_sel);
		inp = wait_for_input(GP2X_UP|GP2X_DOWN|GP2X_B|GP2X_X|GP2X_SELECT);
		if(inp & GP2X_UP  )  { menu_sel--; if (menu_sel < menu_sel_min) menu_sel = menu_sel_max; }
		if(inp & GP2X_DOWN)  { menu_sel++; if (menu_sel > menu_sel_max) menu_sel = menu_sel_min; }
		if(inp &(GP2X_SELECT|GP2X_X)){
			if (rom_data) {
				while (gp2x_joystick_read(1) & (GP2X_SELECT|GP2X_X)) usleep(50*1000); // wait until select is released
				engineState = PGS_Running;
				break;
			}
		}
		if(inp & GP2X_B   )  {
			switch (menu_sel) {
				case 0: // resume game
					if (rom_data) { engineState = PGS_Running; return; }
					break;
				case 1: // save state
					if (rom_data) {
						if(emu_SaveLoadGame(0, 0)) {
							strcpy(menuErrorMsg, "save failed");
							continue;
						}
						engineState = PGS_Running;
						return;
					}
					break;
				case 2: // load state
					if (rom_data) {
						if(emu_SaveLoadGame(1, 0)) {
							strcpy(menuErrorMsg, "load failed");
							continue;
						}
						engineState = PGS_Running;
						return;
					}
					break;
				case 3: // reset game
					if (rom_data) {
						emu_ResetGame();
						engineState = PGS_Running;
						return;
					}
					break;
				case 4: // select rom
					selfname = romsel_loop(curr_path);
					if (selfname) {
						printf("selected file: %s\n", selfname);
						engineState = PGS_ReloadRom;
					}
					return;
				case 5: // options
					ret = menu_loop_options();
					if (ret == 1) continue; // status update
					if (engineState == PGS_ReloadRom)
						return; // BIOS test
					break;
				case 6: // controls
					kc_sel_loop();
					break;
				case 7: // credits
					draw_menu_credits();
					usleep(500*1000);
					inp = wait_for_input(GP2X_B|GP2X_X);
					break;
				case 8: // exit
					engineState = PGS_Quit;
					return;
			}
		}
		menuErrorMsg[0] = 0; // clear error msg
	}
}


static void menu_gfx_prepare(void)
{
	extern int localPal[0x100];
	int i;

	// don't clear old palette just for fun (but make it dark)
	for (i = 0x100-1; i >= 0; i--)
		localPal[i] = (localPal[i] >> 2) & 0x003f3f3f;
	localPal[0xe0] = 0x00000000; // reserved pixels for OSD
	localPal[0xf0] = 0x00ffffff;

	// switch to 8bpp
	gp2x_video_changemode2(8);
	gp2x_video_RGB_setscaling(320, 240);
	gp2x_video_setpalette(localPal, 0x100);
	gp2x_video_flip2();
}


void menu_loop(void)
{
	menu_gfx_prepare();

	menu_loop_root();

	menuErrorMsg[0] = 0;
}
