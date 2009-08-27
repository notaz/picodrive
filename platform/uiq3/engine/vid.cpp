// EmuScan routines for Pico, also simple text and shape drawing routines.

// (c) Copyright 2006, notaz
// All Rights Reserved

#include "vid.h"
#include "../Engine.h"
#include <pico/pico_int.h>
#include "../../common/emu.h"
#include "blit.h"
#include "debug.h"


// global stuff
extern TPicoAreaConfigEntry areaConfig[];
extern const char *actionNames[];

// main framebuffer
static void *screenbuff = 0; // pointer to real device video memory
//static
extern "C" { unsigned char *PicoDraw2FB = 0; }  // temporary buffer
const int framebuffsize  = (8+320)*(8+240+8)*2+8*2; // PicoDraw2FB size (in bytes+to support new rendering mode)

// drawer function pointers
static void (*drawTextFps)(const char *text) = 0;
static void (*drawTextNotice)(const char *text) = 0;

// blitter
static void (*vidBlit)(int full) = 0;

// colors
const unsigned short color_red     = 0x022F;
const unsigned short color_red_dim = 0x0004;
const unsigned short color_green   = 0x01F1;
const unsigned short color_blue    = 0x0F11;
const unsigned short color_grey    = 0x0222;

// other
int txtheight_fit = 138;

// bitmasks
static const unsigned long mask_numbers[] = {
	0x12244800, // 47 2F /
	0x69999600, // 48 30 0
	0x26222200, // 49 31 1
	0x69168F00, // 50 32 2
	0x69219600, // 51 33 3
	0x266AF200, // 52 34 4
	0xF8E11E00, // 53 35 5
	0x68E99600, // 54 36 6
	0x71222200, // 55 37 7
	0x69699600, // 56 38 8
	0x69719600, // 57 39 9
	0x04004000, // 58 3A :
	0x04004400, // 59 3B ;
	0x01242100, // 60 3C <
	0x00707000, // 61 3D =
	0x04212400, // 62 3E >
	0x69240400, // 63 3F ?
	0x00000000, // 64 40 @ [used instead of space for now]
	0x22579900, // 65 41 A
	0xE9E99E00, // 66 42 B
	0x69889600, // 67 43 C
	0xE9999E00, // 68 44 D
	0xF8E88F00, // 69 45 E
	0xF8E88800, // 70 46 F
	0x698B9700, // 71 47 G
	0x99F99900, // 72 48 H
	0x44444400, // 73 49 I
	0x11119600, // 74 4A J
	0x9ACCA900, // 75 4B K
	0x88888F00, // 76 4C L
	0x9F999900, // 77 4D M
	0x9DDBB900, // 78 4E N
	0x69999600, // 79 4F O
	0xE99E8800, // 80 50 P
	0x6999A500, // 81 51 Q
	0xE99E9900, // 82 52 R
	0x69429600, // 83 53 S
	0x72222200, // 84 54 T
	0x99999600, // 85 55 U
	0x55552200, // 86 56 V
	0x9999F900, // 87 57 W
	0x55225500, // 88 58 X
	0x55222200, // 89 59 Y
	0xF1248F00, // 90 5A Z
};


////////////////////////////////
// PicoScan functions

static int EmuScanBegin8(unsigned int num)
{
	DrawLineDest = PicoDraw2FB + 328*num + 8;

	return 0;
}


static int EmuScanEndFit0(unsigned int num)
{
	// 0.75, 168 lines

	static int u = 0, num2 = 0;
	if(!num) u = num2 = 0;

	DrawLineDest = PicoDraw2FB + 328*(++num2) + 8;

	u += 6666;

	if(u < 10000) {
//		u += 7500;
		return 1;
	}

	u -= 10000;

	return 0;
}


////////////////////////////////
// text drawers
// warning: text must be at least 1px away from screen borders

static void drawTextM2(int x, int y, const char *text)
{
	unsigned char *vidmem = PicoDraw2FB + 328*8 + 8;
	int charmask, i, cx = x, cy;
	unsigned char *l, *le;

	// darken the background (left border)
	for(l=vidmem+(cx-1)+(y-1)*328, le=l+8*328; l < le; l+=328) *l = 0xE0;

	for(const char *p=text; *p; p++) {
		cy = y;
		charmask = *(mask_numbers + (*p - 0x2F));

		for(l = vidmem+cx+(y-1)*328, le = l+8*328; l < le; l+=328-4) {
			*l = 0xE0; l++; *l = 0xE0; l++;
			*l = 0xE0; l++; *l = 0xE0; l++;
			*l = 0xE0;
		}

		for(i=0; i < 24; i++) {
			if(charmask&0x80000000) *( vidmem + (cx+(i&3)) + (cy+(i>>2))*328 ) = 0xf0;
			charmask <<= 1;
		}
		cx += 5;
	}
}


static void drawTextM2Fat(int x, int y, const char *text)
{
	unsigned char *vidmem = PicoDraw2FB + 328*8 + 8;
	int charmask, i, cx = x&~1, cy;
	unsigned short *l, *le;

	// darken the background (left border)
	for(l=(unsigned short *)(vidmem+(cx-2)+(y-1)*328), le=l+8*328/2; l < le; l+=328/2) *l = 0xE0;

	for(const char *p=text; *p; p++) {
		cy = y;
		for(l = (unsigned short *)(vidmem+cx+(y-1)*328), le = l+8*328/2; l < le; l+=328/2) {
			l += 4;
			*l-- = 0xe0e0; *l-- = 0xe0e0; *l-- = 0xe0e0; *l-- = 0xe0e0; *l = 0xe0e0; 
		}

		charmask = *(mask_numbers + (*p - 0x2F));

		for(i=0; i < 24; i++) {
			if(charmask&0x80000000) *(unsigned short *)( vidmem + cx+(i&3)*2 + (cy+(i>>2))*328 ) = 0xf0f0;
			charmask <<= 1;
		}
		cx += 5*2;
	}
}


static void drawTextFpsCenter0(const char *text)
{
	if(!text) return;
	drawTextM2((Pico.video.reg[12]&1) ? 234 : 214, 216, text);
}

static void drawTextFpsFit0(const char *text)
{
	if(!text) return;
	drawTextM2Fat((Pico.video.reg[12]&1) ? 256-32 : 224-32, 160, text);
}

static void drawTextFpsFit2_0(const char *text)
{
	if(!text) return;
	drawTextM2Fat((Pico.video.reg[12]&1) ? 256-32 : 224-32, 216, text);
}

static void drawTextFps0(const char *text)
{
	if(!text) return;
	drawTextM2((Pico.video.reg[12]&1) ? 256 : 224, 216, text);
}

static void drawTextNoticeCenter0(const char *text)
{
	if(!text) return;
	drawTextM2(42, 216, text);
}

static void drawTextNoticeFit0(const char *text)
{
	if(!text) return;
	drawTextM2Fat(2, 160, text);
}

static void drawTextNoticeFit2_0(const char *text)
{
	if(!text) return;
	drawTextM2Fat(2, 216, text);
}

static void drawTextNotice0(const char *text)
{
	if(!text) return;
	drawTextM2(2, 216, text);
}


// -----------------------------------------------------------------

static int localPal[0x100];

static void fillLocalPal(void)
{
	Pico.m.dirtyPal = 0;

	if (PicoOpt&0x10) {
		// 8bit fast renderer
		vidConvCpyRGB32(localPal, Pico.cram, 0x40);
		return;
	}

	// 8bit accurate renderer
	if(Pico.video.reg[0xC]&8) { // shadow/hilight mode
		vidConvCpyRGB32(localPal, Pico.cram, 0x40);
		vidConvCpyRGB32sh(localPal+0x40, Pico.cram, 0x40);
		vidConvCpyRGB32hi(localPal+0x80, Pico.cram, 0x40);
		memcpy32(localPal+0xc0, localPal+0x40, 0x40);
		localPal[0xe0] = 0x00000000; // reserved pixels for OSD
		localPal[0xf0] = 0x00ee0000;
	}
	else if (rendstatus & PDRAW_SONIC_MODE) { // mid-frame palette changes
		vidConvCpyRGB32(localPal, Pico.cram, 0x40);
		vidConvCpyRGB32(localPal+0x40, HighPal, 0x40);
		vidConvCpyRGB32(localPal+0x80, HighPal+0x40, 0x40);
	} else {
		vidConvCpyRGB32(localPal, Pico.cram, 0x40);
		memcpy32(localPal+0x80, localPal, 0x40); // for sprite prio mess
	}
}


// note: the internal 8 pixel border is taken care by asm code
static void vidBlit_90(int full)
{
	unsigned char *ps = PicoDraw2FB+328*8;
	unsigned long *pd = (unsigned long *) screenbuff;

	if(Pico.video.reg[12]&1)
		vidConvCpy_90(pd, ps, localPal, 320/8);
	else {
		if(full) vidClear(pd, 32);
		pd += (240+VID_BORDER_R)*32;
		vidConvCpy_90(pd, ps, localPal, 256/8);
		if(full) vidClear(pd + (240+VID_BORDER_R)*256, 32);
	}
}


static void vidBlit_270(int full)
{
	unsigned char *ps = PicoDraw2FB+328*8;
	unsigned long *pd = (unsigned long *) screenbuff;

	if(Pico.video.reg[12]&1)
		vidConvCpy_270(pd, ps, localPal, 320/8);
	else {
		if(full) vidClear(pd, 32);
		pd += (240+VID_BORDER_R)*32;
		ps -= 64;     // the blitter starts copying from the right border, so we need to adjust
		vidConvCpy_270(pd, ps, localPal, 256/8);
		if(full) vidClear(pd + (240+VID_BORDER_R)*256, 32);
	}
}


static void vidBlitCenter_0(int full)
{
	unsigned char *ps = PicoDraw2FB+328*8+8;
	unsigned long *pd = (unsigned long *) screenbuff;

	if(Pico.video.reg[12]&1) ps += 32;
	vidConvCpy_center_0(pd, ps, localPal);
	if(full) vidClear(pd + (240+VID_BORDER_R)*224, 96);
}


static void vidBlitCenter_180(int full)
{
	unsigned char *ps = PicoDraw2FB+328*8+8;
	unsigned long *pd = (unsigned long *) screenbuff;

	if(Pico.video.reg[12]&1) ps += 32;
	vidConvCpy_center_180(pd, ps, localPal);
	if(full) vidClear(pd + (240+VID_BORDER_R)*224, 96);
}


static void vidBlitFit_0(int full)
{
	if(Pico.video.reg[12]&1)
	     vidConvCpy_center2_40c_0(screenbuff, PicoDraw2FB+328*8, localPal, 168);
	else vidConvCpy_center2_32c_0(screenbuff, PicoDraw2FB+328*8, localPal, 168);
	if(full) vidClear((unsigned long *)screenbuff + (240+VID_BORDER_R)*168, 320-168);
}


static void vidBlitFit_180(int full)
{
	if(Pico.video.reg[12]&1)
	     vidConvCpy_center2_40c_180(screenbuff, PicoDraw2FB+328*8, localPal, 168);
	else vidConvCpy_center2_32c_180(screenbuff, PicoDraw2FB+328*8-64, localPal, 168);
	if(full) vidClear((unsigned long *)screenbuff + (240+VID_BORDER_R)*168, 320-168);
}


static void vidBlitFit2_0(int full)
{
	if(Pico.video.reg[12]&1)
	     vidConvCpy_center2_40c_0(screenbuff, PicoDraw2FB+328*8, localPal, 224);
	else vidConvCpy_center2_32c_0(screenbuff, PicoDraw2FB+328*8, localPal, 224);
	if(full) vidClear((unsigned long *)screenbuff + (240+VID_BORDER_R)*224, 96);
}


static void vidBlitFit2_180(int full)
{
	if(Pico.video.reg[12]&1)
	     vidConvCpy_center2_40c_180(screenbuff, PicoDraw2FB+328*8, localPal, 224);
	else vidConvCpy_center2_32c_180(screenbuff, PicoDraw2FB+328*8-64, localPal, 224);
	if(full) vidClear((unsigned long *)screenbuff + (240+VID_BORDER_R)*224, 96);
}

static void vidBlitCfg(void)
{
	unsigned short *ps = (unsigned short *) PicoDraw2FB;
	unsigned long *pd = (unsigned long *) screenbuff;
	int i;

	// hangs randomly (due to repeated ldms/stms?)
	//for (int i = 1; i < 320; i++, ps += 240, pd += 256)
	//	vidConvCpyRGB32(pd, ps, 240);

	for (i = 0; i < 320; i++, pd += VID_BORDER_R)
		for (int u = 0; u < 240; u++, ps++, pd++)
			*pd = ((*ps & 0xf) << 20) | ((*ps & 0xf0) << 8) | ((*ps & 0xf00) >> 4);
}


////////////////////////////////
// main functions

int vidInit(void *vidmem, int reinit)
{
	if(!reinit) {
		// prepare framebuffer
		screenbuff = vidmem;
		PicoDraw2FB = (unsigned char *) malloc(framebuffsize);

		if(!screenbuff) return KErrNotSupported;
		if(!PicoDraw2FB)  return KErrNoMemory;

		memset(PicoDraw2FB, 0, framebuffsize);
	}

	// select suitable blitters
	vidBlit = vidBlit_270;
	PicoScanBegin = EmuScanBegin8;
	PicoScanEnd = NULL;
	drawTextFps = drawTextFps0;
	drawTextNotice = drawTextNotice0;

	memset(localPal, 0, 0x100*4);
	localPal[0xe0] = 0x00000000; // reserved pixels for OSD
	localPal[0xf0] = 0x00ee0000;

	// setup all orientation related stuff
	if (currentConfig.rotation == TPicoConfig::PRot0)
	{
		if (currentConfig.scaling == TPicoConfig::PMCenter) {
			vidBlit = vidBlitCenter_0;
			drawTextFps = drawTextFpsCenter0;
			drawTextNotice = drawTextNoticeCenter0;
		} else if (currentConfig.scaling == TPicoConfig::PMFit2) {
			vidBlit = vidBlitFit2_0;
			drawTextFps = drawTextFpsFit2_0;
			drawTextNotice = drawTextNoticeFit2_0;
		} else {
			vidBlit = vidBlitFit_0;
			drawTextFps = drawTextFpsFit0;
			drawTextNotice = drawTextNoticeFit0;
			PicoScanBegin = NULL;
			PicoScanEnd = EmuScanEndFit0;
		}
	} else if (currentConfig.rotation == TPicoConfig::PRot90) {
		vidBlit = vidBlit_90;
	}
	else if (currentConfig.rotation == TPicoConfig::PRot180)
	{
		if (currentConfig.scaling == TPicoConfig::PMCenter)
		{
			vidBlit = vidBlitCenter_180;
			drawTextFps = drawTextFpsCenter0;
			drawTextNotice = drawTextNoticeCenter0;
		}
		else if (currentConfig.scaling == TPicoConfig::PMFit2) {
			vidBlit = vidBlitFit2_180;
			drawTextFps = drawTextFpsFit2_0;
			drawTextNotice = drawTextNoticeFit2_0;
		} else {
			vidBlit = vidBlitFit_180;
			drawTextFps = drawTextFpsFit0;
			drawTextNotice = drawTextNoticeFit0;
			PicoScanBegin = NULL;
			PicoScanEnd = EmuScanEndFit0;
		}
	}
	else if (currentConfig.rotation == TPicoConfig::PRot270) {
		vidBlit = vidBlit_270;
	}

	fillLocalPal();
	vidBlit(1);
	PicoOpt |= 0x100;
	Pico.m.dirtyPal = 1;

	return 0;
}

void vidFree()
{
	free(PicoDraw2FB);
	PicoDraw2FB = 0;
}

void vidDrawFrame(char *noticeStr, char *fpsStr, int num)
{
	DrawLineDest = PicoDraw2FB + 328*8 + 8;

//	PicoFrame(); // moved to main loop
	if (currentConfig.EmuOpt & EOPT_SHOW_FPS)
		drawTextFps(fpsStr);
	drawTextNotice(noticeStr);

	if (Pico.m.dirtyPal) fillLocalPal();
	vidBlit(!num); // copy full frame once a second
}

// -----------------------------------------------------------------

static void drawText0(int x, int y, const char *text, long color)
{
	unsigned short *vidmem=(unsigned short *)PicoDraw2FB;
	int charmask, i, cx = x, cy;
	unsigned short *l, *le, dmask=0x0333;

	// darken the background (left border)
	for(l=vidmem+(cx-1)+(y-1)*240, le=vidmem+(cx-1)+(y+7)*240; l < le; l+=240)
		*l = (*l >> 2) & dmask;

	for(const char *p=text; *p; p++) {
		cy = y;
		charmask = *(mask_numbers + (*p - 0x2F));

		for(l = vidmem+cx+(y-1)*240, le = vidmem+cx+(y+7)*240; l < le; l+=240-4) {
			*l = (*l >> 2) & dmask; l++; *l = (*l >> 2) & dmask; l++;
			*l = (*l >> 2) & dmask; l++; *l = (*l >> 2) & dmask; l++;
			*l = (*l >> 2) & dmask;
		}

		for(i=0; i < 24; i++) {
			// draw dot. Is this fast?
			if(charmask&0x80000000) *( vidmem + (cx+(i&3)) + (cy+(i>>2))*240 ) = color;
			charmask <<= 1;
		}
		cx += 5;
	}
}

// draws rect with width - 1 and height - 1
static void drawRect(const TRect &rc, unsigned short color)
{
	unsigned short *vidmem=(unsigned short *)PicoDraw2FB;

	if(rc.iTl.iX - rc.iBr.iX && rc.iTl.iY - rc.iBr.iY) {
		int stepX = rc.iTl.iX < rc.iBr.iX ? 1 : -1;
		int stepY = rc.iTl.iY < rc.iBr.iY ? 1 : -1;
		
		for(int x = rc.iTl.iX;; x += stepX) {
			*(vidmem + rc.iTl.iY*240 + x) = *(vidmem + (rc.iBr.iY - stepY)*240 + x) = color;
			if(x == rc.iBr.iX - stepX) break;
		}
		
		for(int y = rc.iTl.iY;; y += stepY) {
			*(vidmem + y*240 + rc.iTl.iX) = *(vidmem + y*240 + rc.iBr.iX - stepX) = color;
			if(y == rc.iBr.iY - stepY) break;
		}
	}
}

// draws fullsize filled rect
static void drawRectFilled(const TRect rc, unsigned short color)
{
	unsigned short *vidmem=(unsigned short *)PicoDraw2FB;

	if(rc.iTl.iX - rc.iBr.iX && rc.iTl.iY - rc.iBr.iY) {
		int stepX = rc.iTl.iX < rc.iBr.iX ? 1 : -1;
		int stepY = rc.iTl.iY < rc.iBr.iY ? 1 : -1;
		
		for(int y = rc.iTl.iY;; y += stepY) {
			for(int x = rc.iTl.iX;; x += stepX) {
				*(vidmem + y*240 + x) = *(vidmem + y*240 + x) = color;
				if(x == rc.iBr.iX) break;
			}
			if(y == rc.iBr.iY) break;
		}
	}
}

// direction: -1 left, 1 right
static void drawArrow0(TPoint p, int direction, unsigned short color)
{
	unsigned short *vidmem=(unsigned short *)PicoDraw2FB;
	int width = 15;
	int x = p.iX;
	int y = p.iY;

	for(; width > 0; x+=direction, y++, width -=2)
		for(int i=0; i < width; i++)
			*(vidmem + x + y*240 + i*240) = color;
}

static char *vidGetScanName(int scan)
{
	static char buff[32];

	if((scan >= '0' && scan <= '9') || (scan >= 'A' && scan <= 'Z')) {
		buff[0] = (char) scan; buff[1] = 0;
	} else {
		switch(scan) {
			case 0x01: strcpy(buff, "BSPACE");   break;
			case 0x03: strcpy(buff, "OK");       break;
			case 0x05: strcpy(buff, "SPACE");    break;
			case 0x0e: strcpy(buff, "AST");      break;
			case 0x0f: strcpy(buff, "HASH");     break;
			case 0x12: strcpy(buff, "SHIFT");    break;
			case 0x19: strcpy(buff, "ALT");      break;
			case 0x79: strcpy(buff, "PLUS");     break;
			case 0x7a: strcpy(buff, "DOT");      break;
			case 0xa5: strcpy(buff, "JOG@UP");   break;
			case 0xa6: strcpy(buff, "JOG@DOWN"); break;
			case 0xb5: strcpy(buff, "INET");     break;
			case 0xd4: strcpy(buff, "JOG@PUSH"); break;
			case 0xd5: strcpy(buff, "BACK");     break;
			default:  sprintf(buff, "KEY@%02X", scan); break;
		}
	}

	return buff;
}

void vidKeyConfigFrame(const TUint whichAction)
{
	int i;
	char buttonNames[128];
	buttonNames[0] = 0;
	memset(PicoDraw2FB, 0, framebuffsize);

	unsigned long currentActCode = 1 << whichAction;

	// draw all "buttons" in reverse order
	const TPicoAreaConfigEntry *e = areaConfig + 1; i = 0;
	while(e->rect != TRect(0,0,0,0)) { e++; i++; }
	for(e--, i--; e->rect != TRect(0,0,0,0); e--, i--)
		drawRect(e->rect, (currentConfig.KeyBinds[i+256] & currentActCode) ? color_red : color_red_dim);

	// action name control
	drawRectFilled(TRect(72, 2, 168, 20), color_grey); // 96x14
	drawArrow0(TPoint(80, 3), -1, color_green);
	drawArrow0(TPoint(160, 3), 1, color_green);

	drawText0(86, 9, actionNames[whichAction], color_red);

	// draw active button names if there are any
	for (i = 0; i < 256; i++) {
		if (currentConfig.KeyBinds[i] & currentActCode) {
			if(buttonNames[0]) strcat(buttonNames, ";@");
			strcat(buttonNames, vidGetScanName(i));
		}
	}

	if (buttonNames[0]) {
		buttonNames[61] = 0; // only 60 chars fit
		drawText0(6, 48, buttonNames, color_blue);
	}

	vidBlitCfg();
}

void vidDrawNotice(const char *txt)
{
	if(PicoDraw2FB) {
		drawTextNotice(txt);
		vidBlit(1);
	}
}
