// EmuScan routines for Pico, also simple text and shape drawing routines.

// (c) Copyright 2006, notaz
// All Rights Reserved

#include "vid.h"
#include "ClientServer.h"
#include "SimpleServer.h"
#include "pico\picoInt.h"
#include "blit.h"
#include "debug.h"

// global stuff
extern TPicoConfig currentConfig;
extern TPicoKeyConfigEntry *keyConfig;
extern TPicoAreaConfigEntry areaConfig[];
extern const char * actionNames[];

// main framebuffer
static void *screenbuff = 0; // pointer to real device video memory
//static
unsigned short *framebuff = 0;  // temporary buffer in sega native BGR format
const  int framebuffsize  = (8+320)*(8+208+8)*2; // actual framebuffer size (in bytes+to support new rendering mode)
static int framebuff_offs = 0; // where to start copying (in pixels)
static int framebuff_len  = 0; // how much of the framebuffer actually needs to be copied (in pixels)
static int fc_lines, fc_inc; // lines, inc for "0 fit2" mode

// drawer function pointers
void (*drawTextFps)(const char *text) = 0;
void (*drawTextNotice)(const char *text) = 0;

// blitter
void (*vidBlit)(int full) = 0;
void (*vidBlitKeyCfg)(int full) = 0;

// stuff for rendermode2
static unsigned short cram_high[0x40];
static unsigned short dt_dmask=0x0333;
unsigned short color_redM2 = 0x022F;

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
// Cram functions

inline int EmuCramNull(int cram)
{
  User::Panic(_L("Cram called!!"), 0);
  return cram;
}


////////////////////////////////
// PicoScan functions in "center" mode

int EmuScanCenter0(unsigned int num, unsigned short *sdata)
{
  //unsigned short *vidmem=framebuff;

  //unsigned short *sp, *sto;
  //sp=sdata+56; sto=sdata+264; vidmem += num*208;

  //do { *vidmem++ = *sp++; } while(sp < sto);
  memcpy(framebuff + num*208, sdata+56, 208*2); // memcpy gives ~1 fps (~2 with optimized memcpy)

  return 0;
}


int EmuScanCenter90(unsigned int num, unsigned short *sdata)
{
  // ignore top and bottom lines
  if(num < 8)   return 7-num;     // skip first 8 lines
  if(num > 215) return 223+8-num; // this should not happen, just in case

  num -= 8;
  if(!num) {
	if(Pico.video.reg[12]&1) { // copy less in 32-col mode
	  framebuff_offs= 0;
      framebuff_len = 208*320;
	} else {
	  framebuff_offs= 208*32;
      framebuff_len = 208*256;
	}
  }

  unsigned short *vidmem=framebuff;
  vidmem += 207-num; // adjust x

  // do less copy in 32-column mode
  unsigned short *sp, *sto;
  int pixels;
  if(!(Pico.video.reg[12]&1))
       { sp=sdata+32; sto=sdata+288; pixels = 288; vidmem += 32*208; }
  else { sp=sdata;    sto=sdata+320; pixels = 320; }

  do { *vidmem = *sp++; vidmem+=208; } while(sp < sto);

  if(num == 207) return 16; // skip bottom of this frame and top of next

  return 0;
}


int EmuScanCenter180(unsigned int num, unsigned short *sdata)
{
  unsigned short *vidmem=framebuff;

  unsigned short *sp, *sto;
  sp=sdata+56; sto=sdata+264; vidmem += (224-num)*208;

  do { *(--vidmem) = *sp++; } while(sp < sto); // reversed

  return 0;
}


int EmuScanCenter270(unsigned int num, unsigned short *sdata)
{
  // ignore top and bottom lines
  if(num < 8)   return 7-num;     // skip first 8 lines
  if(num > 215) return 223-num+8;

  num -= 8;
  if(num > 207) return 0;
  if(!num) {
	if(Pico.video.reg[12]&1) {
	  framebuff_offs= 0;
      framebuff_len = 208*320;
	} else {
	  framebuff_offs= 208*32;
      framebuff_len = 208*256;
	}
  }

  unsigned short *vidmem=framebuff+320*208;
  vidmem -= 208-num; // adjust x

  // do less copy in 32-column mode
  unsigned short *sp, *sto;
  if(!(Pico.video.reg[12]&1))
       { sp=sdata+32; sto=sdata+288; vidmem -= 32*208; }
  else { sp=sdata;    sto=sdata+320; }

  do { *vidmem = *sp++; vidmem-=208; } while(sp < sto);

  if(num == 207) return 16; // skip bottom of this frame and top of next

  return 0;
}



////////////////////////////////
// PicoScan functions in "fit" mode

static int EmuScanFit0(unsigned int num, unsigned short *sdata)
{
  // 0.65, 145 lines in normal mode; 0.8125, 182 lines in 32-column mode

  // draw this line? (ARM4s don't support division, so do some tricks here)
  static int u = 0, num2 = 0;
  if(!num) {
    u = num2 = 0;
	if(currentConfig.iScreenMode == TPicoConfig::PMFit) {
      if(Pico.video.reg[12]&1) { // 32 col mode? This can change on any frame
        fc_inc = 6500;
	    txtheight_fit = 138;
        framebuff_len = 208*145;
	    memset(framebuff+208*145, 0, 208*37*2);
      } else {
        fc_inc = 8125;
	    txtheight_fit = 175;
        framebuff_len = 208*182;
      }
	}
  }
  u += fc_inc;
  if(u < 10000) return 0;
  u -= 10000;

  unsigned short *vidmem=framebuff;

  int slen;
  unsigned short *sp;
  if(!(Pico.video.reg[12]&1))
       { sp=sdata+32; slen=256; }
  else { sp=sdata;    slen=320; }

  vidmem += num2*208;
/*
  int i=0;
  while(sp < sto) {
	i += inc;
    if(i >= 10000) {
	  *vidmem++ = *sp;
	  i -= 10000;
	}
	sp++;
  }
*/
  PicuShrink(vidmem, 208, sp, slen);

  num2++;

  // calculate how many lines pico engine should skip,
  // making sure this func will be called on scanline 0
  int skip = 0;
  while(u+fc_inc < 10000 && num+skip != 223) { u+=fc_inc; skip++; }

  return skip;
}

int EmuScanFit90(unsigned int num, unsigned short *sdata)
{
  // 0.9285

  // draw this line?
  static int u = 0, num2 = 0;
  if(!num) {
    u = num2 = 0;
	if(Pico.video.reg[12]&1) {
	  framebuff_offs= 0;
      framebuff_len = 208*320;
	} else {
	  framebuff_offs= 208*32;
      framebuff_len = 208*256;
	}
  }
  u += 9285;
  if(u < 10000) return 0;
  u -= 10000;

  unsigned short *vidmem=framebuff;
  vidmem += 207-num2; // adjust x

  // do less copy in 32-column mode
  unsigned short *sp, *sto;
  if(!(Pico.video.reg[12]&1))
       { sp=sdata+32; sto=sdata+288; vidmem += 32*208; }
  else { sp=sdata;    sto=sdata+320; }

  do { *vidmem = *sp++; vidmem+=208; } while(sp < sto);

  num2++;

  // skip next line?
  if(u+9285 < 10000 && num != 223) { u+=9285; return 1; }

  return 0;
}

int EmuScanFit180(unsigned int num, unsigned short *sdata)
{
  // 0.65, 145 lines in normal mode; 0.8125, 182 lines in 32-column mode

  // draw this line? (ARM4s don't support division)
  static int u = 0, num2 = 0;
  if(!num) {
    u = num2 = 0;
	if(currentConfig.iScreenMode == TPicoConfig::PMFit) {
      if(Pico.video.reg[12]&1) { // 32 col mode? This can change on any frame
	    fc_lines = 145;
        fc_inc = 6500;
	    txtheight_fit = 138;
        framebuff_len = 208*145;
	    memset(framebuff+208*145, 0, 208*37*2);
      } else {
        fc_lines = 182;
        fc_inc = 8125;
	    txtheight_fit = 175;
        framebuff_len = 208*182;
      }
    }
  }
  u += fc_inc;
  if(u < 10000) return 0;
  u -= 10000;

  unsigned short *vidmem=framebuff;

  int slen;
  unsigned short *sp;
  if(!(Pico.video.reg[12]&1))
       { sp=sdata+32; slen=256; }
  else { sp=sdata;    slen=320; }

  vidmem += (fc_lines-num2)*208;

  PicuShrinkReverse(vidmem, 208, sp, slen);

  num2++;

  // skip some lines?
  int skip = 0;
  while(u+fc_inc < 10000 && num+skip != 223) { u+=fc_inc; skip++; }

  return skip;
}

int EmuScanFit270(unsigned int num, unsigned short *sdata)
{
  // 0.9285

  // draw this line?
  static int u = 0, num2 = 0;
  if(!num) {
    u = num2 = 0;
	if(Pico.video.reg[12]&1) {
	  framebuff_offs= 0;
      framebuff_len = 208*320;
	} else {
	  framebuff_offs= 208*32;
      framebuff_len = 208*256;
	}
  }
  u += 9285;
  if(u < 10000) return 0;
  u -= 10000;

  unsigned short *vidmem=framebuff+320*208;
  vidmem -= 208-num2; // adjust x

  // do less copy in 32-column mode
  unsigned short *sp, *sto;
  if(!(Pico.video.reg[12]&1))
       { sp=sdata+32; sto=sdata+288; vidmem -= 32*208; }
  else { sp=sdata;    sto=sdata+320; }

  do { *vidmem = *sp++; vidmem-=208; } while(sp < sto);

  num2++;

  // skip next line?
  if(u+9285 < 10000 && num != 223) { u+=9285; return 1; }

  return 0;
}


////////////////////////////////
// text drawers
// warning: text must be at least 1px away from screen borders

void drawTextM2(int x, int y, const char *text, long color)
{
	unsigned short *vidmem=framebuff;
	int charmask, i, cx = x, cy;
	unsigned short *l, *le;

	// darken the background (left border)
	for(l=vidmem+(cx-1)+(y-1)*328, le=vidmem+(cx-1)+(y+7)*328; l < le; l+=328)
		*l = (*l >> 2) & dt_dmask;

	for(const char *p=text; *p; p++) {
		cy = y;
		charmask = *(mask_numbers + (*p - 0x2F));

		for(l = vidmem+cx+(y-1)*328, le = vidmem+cx+(y+7)*328; l < le; l+=328-4) {
			*l = (*l >> 2) & dt_dmask; l++; *l = (*l >> 2) & dt_dmask; l++;
			*l = (*l >> 2) & dt_dmask; l++; *l = (*l >> 2) & dt_dmask; l++;
			*l = (*l >> 2) & dt_dmask;
		}

		for(i=0; i < 24; i++) {
			// draw dot. Is this fast?
			if(charmask&0x80000000) *( vidmem + (cx+(i&3)) + (cy+(i>>2))*328 ) = color;
			charmask <<= 1;
		}
		cx += 5;
	}
}

void drawText0(int x, int y, const char *text, long color)
{
	unsigned short *vidmem=framebuff;
	int charmask, i, cx = x, cy;
	unsigned short *l, *le, dmask=0x0333;

	// darken the background (left border)
	for(l=vidmem+(cx-1)+(y-1)*208, le=vidmem+(cx-1)+(y+7)*208; l < le; l+=208)
		*l = (*l >> 2) & dmask;

	for(const char *p=text; *p; p++) {
		cy = y;
		charmask = *(mask_numbers + (*p - 0x2F));

		for(l = vidmem+cx+(y-1)*208, le = vidmem+cx+(y+7)*208; l < le; l+=208-4) {
			*l = (*l >> 2) & dmask; l++; *l = (*l >> 2) & dmask; l++;
			*l = (*l >> 2) & dmask; l++; *l = (*l >> 2) & dmask; l++;
			*l = (*l >> 2) & dmask;
		}

		for(i=0; i < 24; i++) {
			// draw dot. Is this fast?
			if(charmask&0x80000000) *( vidmem + (cx+(i&3)) + (cy+(i>>2))*208 ) = color;
			charmask <<= 1;
		}
		cx += 5;
	}
}

void drawText90(int x, int y, const char *text, long color)
{
	unsigned short *vidmem=framebuff;
	unsigned short *l, *le, dmask=0x0333;
	int charmask, i, cx, cy = y;

	for(l=vidmem+(x+1)+(cy-1)*208, le=vidmem+(x-7)+(cy-1)*208; l > le; l--)
		*l = (*l >> 2) & dmask;

	for(const char *p=text; *p; p++) {
		cx = x;
		charmask = *(mask_numbers + (*p - 0x2F));

		for(l = vidmem+(x+1)+(cy)*208, le = vidmem+(x+1)+(cy+5)*208; l < le; l+=208+7) {
			*l = (*l >> 2) & dmask; l--; *l = (*l >> 2) & dmask; l--;
			*l = (*l >> 2) & dmask; l--; *l = (*l >> 2) & dmask; l--;
			*l = (*l >> 2) & dmask; l--; *l = (*l >> 2) & dmask; l--;
			*l = (*l >> 2) & dmask; l--; *l = (*l >> 2) & dmask;
		}

		for(i=0; i < 24; i++) {
			if(charmask&0x80000000) *( vidmem + (cy+(i&3))*208 + (cx-(i>>2)) ) = color;
			charmask <<= 1;
		}
		cy += 5;
	}
}

void drawText180(int x, int y, const char *text, long color)
{
	unsigned short *vidmem=framebuff;
	int charmask, i, cx = x, cy;
	unsigned short *l, *le, dmask=0x0333;

	for(l=vidmem+(cx+1)+(y+1)*208, le=vidmem+(cx+1)+(y-7)*208; l > le; l-=208)
		*l = (*l >> 2) & dmask;

	for(const char *p=text; *p; p++) {
		cy = y;
		charmask = *(mask_numbers + (*p - 0x2F));

		for(l = vidmem+cx+(y+1)*208, le = vidmem+cx+(y-8)*208; l > le; l-=208-4) {
			*l = (*l >> 2) & dmask; l--; *l = (*l >> 2) & dmask; l--;
			*l = (*l >> 2) & dmask; l--; *l = (*l >> 2) & dmask; l--;
			*l = (*l >> 2) & dmask;
		}

		for(i=0; i < 24; i++) {
			if(charmask&0x80000000) *( vidmem + (cx-(i&3)) + (cy-(i>>2))*208 ) = color;
			charmask <<= 1;
		}
		cx -= 5;
	}
}

void drawText270(int x, int y, const char *text, long color)
{
	unsigned short *vidmem=framebuff;
	int charmask, i, cx, cy = y;
	unsigned short *l, *le, dmask=0x0333;

	for(l=vidmem+(x-1)+(cy+1)*208, le=vidmem+(x+7)+(cy+1)*208; l < le; l++)
		*l = (*l >> 2) & dmask;

	for(const char *p=text; *p; p++) {
		cx = x;
		charmask = *(mask_numbers + (*p - 0x2F));

		for(l = vidmem+(x-1)+(cy)*208, le = vidmem+(x-1)+(cy-5)*208; l > le; l-=208+7) {
			*l = (*l >> 2) & dmask; l++; *l = (*l >> 2) & dmask; l++;
			*l = (*l >> 2) & dmask; l++; *l = (*l >> 2) & dmask; l++;
			*l = (*l >> 2) & dmask; l++; *l = (*l >> 2) & dmask; l++;
			*l = (*l >> 2) & dmask; l++; *l = (*l >> 2) & dmask;
		}

		for(i=0; i < 24; i++) {
			if(charmask&0x80000000) *( vidmem + (cy-(i&3))*208 + (cx+(i>>2)) ) = color;
			charmask <<= 1;
		}
		cy -= 5;
	}
}

void drawTextFpsM2(const char *text)
{
	if(!text) return;
	drawTextM2((Pico.video.reg[12]&1) ? 256 : 224, 200, text, color_redM2);
}

void drawTextFps0(const char *text)
{
	if(!text) return;
	drawText0(176, 216, text, color_red);
}

void drawTextFpsFit0(const char *text)
{
	if(!text) return;
	drawText0(176, txtheight_fit, text, color_red);
}

void drawTextFps90(const char *text)
{
	if(!text) return;
	drawText90(10, 256, text, color_red);
}

void drawTextFps180(const char *text)
{
	if(!text) return;
	drawText180(32, 8, text, color_red);
}

void drawTextFps270(const char *text)
{
	if(!text) return;
	drawText270(200, 64, text, color_red);
}

void drawTextNoticeM2(const char *text)
{
	if(!text) return;
	drawTextM2(20, 200, text, color_redM2);
}

void drawTextNotice0(const char *text)
{
	if(!text) return;
	drawText0(2, 216, text, color_red);
}

void drawTextNoticeFit0(const char *text)
{
	if(!text) return;
	drawText0(2, txtheight_fit, text, color_red);
}

void drawTextNotice90(const char *text)
{
	if(!text) return;
	drawText90(10, 34, text, color_red);
}

void drawTextNotice180(const char *text)
{
	if(!text) return;
	drawText180(206, 8, text, color_red);
}

void drawTextNotice270(const char *text)
{
	if(!text) return;
	drawText270(200, 286, text, color_red);
}


////////////////////////////////
// misc drawers

// draws rect with width - 1 and height - 1
void drawRect(const TRect &rc, unsigned short color)
{
	if(rc.iTl.iX - rc.iBr.iX && rc.iTl.iY - rc.iBr.iY) {
		int stepX = rc.iTl.iX < rc.iBr.iX ? 1 : -1;
		int stepY = rc.iTl.iY < rc.iBr.iY ? 1 : -1;
		
		for(int x = rc.iTl.iX;; x += stepX) {
			*(framebuff + rc.iTl.iY*208 + x) = *(framebuff + (rc.iBr.iY - stepY)*208 + x) = color;
			if(x == rc.iBr.iX - stepX) break;
		}
		
		for(int y = rc.iTl.iY;; y += stepY) {
			*(framebuff + y*208 + rc.iTl.iX) = *(framebuff + y*208 + rc.iBr.iX - stepX) = color;
			if(y == rc.iBr.iY - stepY) break;
		}
	}
}

// draws fullsize filled rect
void drawRectFilled(const TRect rc, unsigned short color)
{
	if(rc.iTl.iX - rc.iBr.iX && rc.iTl.iY - rc.iBr.iY) {
		int stepX = rc.iTl.iX < rc.iBr.iX ? 1 : -1;
		int stepY = rc.iTl.iY < rc.iBr.iY ? 1 : -1;
		
		for(int y = rc.iTl.iY;; y += stepY) {
			for(int x = rc.iTl.iX;; x += stepX) {
				*(framebuff + y*208 + x) = *(framebuff + y*208 + x) = color;
				if(x == rc.iBr.iX) break;
			}
			if(y == rc.iBr.iY) break;
		}
	}
}

// direction: -1 left, 1 right
void drawArrow0(TPoint p, int direction, unsigned short color)
{
	int width = 11;
	int x = p.iX;
	int y = p.iY;

	for(; width > 0; x+=direction, y++, width -=2)
		for(int i=0; i < width; i++)
			*(framebuff + x + y*208 + i*208) = color;
}

void drawArrow90(TPoint p, int direction, unsigned short color)
{
	int width = 11;
	int x = p.iX - width;
	int y = p.iY;

	for(; width > 0; x++, y+=direction, width -=2)
		for(int i=0; i < width; i++)
			*(framebuff + x + y*208 + i) = color;
}


// copies temporary framebuff to real device framebuffer
void vidBlitRGB444(int full)
{
	unsigned short *ps;
	unsigned short *pd;
	int pixels;
	if(full) {
		ps = framebuff;
		pd = (unsigned short *) screenbuff;
		pixels = 208*320;
	} else {
		ps = framebuff + framebuff_offs;
		pd = (unsigned short *) screenbuff + framebuff_offs;
		pixels = framebuff_len;
	}

	vidConvCpyRGB444(pd, ps, pixels);
	//for(unsigned short *ps_end = ps + pixels; ps < ps_end; ps++)
		// Convert 0000bbb0 ggg0rrr0
		// to      0000rrr0 ggg0bbb0
	//	*pd++ = ((*ps&0x000F)<<8) | (*ps&0x00F0) | ((*ps&0x0F00)>>8);
}

void vidBlitRGB565(int full)
{
	unsigned short *ps;
	unsigned short *pd;
	int pixels;
	if(full) {
		ps = framebuff;
		pd = (unsigned short *) screenbuff;
		pixels = 208*320;
	} else {
		ps = framebuff  + framebuff_offs;
		pd = (unsigned short *) screenbuff + framebuff_offs;
		pixels = framebuff_len;
	}

	vidConvCpyRGB565(pd, ps, pixels);
	//for(; ps < ps_end; ps++)
		// Convert 0000bbb0 ggg0rrr0
		// to      rrr00ggg 000bbb00
	//	*pd++ = ((*ps&0x000F)<<12) | ((*ps&0x00F0)<<3) | ((*ps&0x0F00)>>7);
}

void vidBlitRGB32(int full)
{
	unsigned short *ps;
	unsigned long  *pd;
	int pixels;
	if(full) {
		ps = framebuff;
		pd = (unsigned long *) screenbuff;
		pixels = 208*320;
	} else {
		ps = framebuff  + framebuff_offs;
		pd = (unsigned long *) screenbuff + framebuff_offs;
		pixels = framebuff_len;
	}

	vidConvCpyRGB32(pd, ps, pixels);
	//for(; ps < ps_end; ps++)
		// Convert          0000bbb0 ggg0rrr0
		// to  ..0 rrr00000 ggg00000 bbb00000
	//	*pd++ = ((*ps&0x000F)<<20) | ((*ps&0x00F0)<<8) | ((*ps&0x0F00)>>4);
}

// -------- rendermode 2 ---------

void vidBlit16M2(int full)
{
	unsigned short *ps = framebuff+328*8;
	unsigned short *pd = (unsigned short *) screenbuff;

	if(currentConfig.iScreenRotation == TPicoConfig::PRot90) {
		if(Pico.video.reg[12]&1)
			vidConvCpyM2_16_90(pd, ps, 320/8);
		else {
			if(full) memset(pd, 0, 208*32*2);
			pd += 208*32;
			vidConvCpyM2_16_90(pd, ps, 256/8);
			if(full) memset(pd + 208*256, 0, 208*32*2);
		}
	} else if(currentConfig.iScreenRotation == TPicoConfig::PRot270) {
		if(Pico.video.reg[12]&1)
			vidConvCpyM2_16_270(pd, ps, 320/8);
		else {
			if(full) memset(pd, 0, 208*32*2);
			pd += 208*32;
			ps -= 64;     // the blitter starts copying from the right border, so we need to adjust
			vidConvCpyM2_16_270(pd, ps, 256/8);
			if(full) memset(pd + 208*256, 0, 208*32*2);
		}
	}
/*
    for(int x=0; x < 320; x++)
		for(int y=207; y>=0; y--) {
			*pd++ = *(ps+8+x+y*328);
		}
*/
}

void vidBlitRGB32M2(int full)
{
	unsigned short *ps = framebuff+328*8;
	unsigned long  *pd = (unsigned long *) screenbuff;

	if(currentConfig.iScreenRotation == TPicoConfig::PRot90) {
		if(Pico.video.reg[12]&1)
			vidConvCpyM2_RGB32_90(pd, ps, 320/8);
		else {
			if(full) memset(pd, 0, 208*32*4);
			pd += 208*32;
			vidConvCpyM2_RGB32_90(pd, ps, 256/8);
			if(full) memset(pd + 208*256, 0, 208*32*4);
		}
	} else if(currentConfig.iScreenRotation == TPicoConfig::PRot270) {
		if(Pico.video.reg[12]&1)
			vidConvCpyM2_RGB32_270(pd, ps, 320/8);
		else {
			if(full) memset(pd, 0, 208*32*4);
			pd += 208*32;
			ps -= 64;     // the blitter starts copying from the right border, so we need to adjust
			vidConvCpyM2_RGB32_270(pd, ps, 256/8);
			if(full) memset(pd + 208*256, 0, 208*32*4);
		}
	}
}

void PrepareCramRGB444M2()
{
	vidConvCpyRGB444(cram_high, Pico.cram, 0x40);
}

void PrepareCramRGB565M2()
{
	vidConvCpyRGB565(cram_high, Pico.cram, 0x40);
}


////////////////////////////////
// main functions

int vidInit(int displayMode, void *vidmem, int p800, int reinit)
{
	if(!reinit) {
		// prepare framebuffer
		screenbuff = (unsigned short *) vidmem;
		framebuff =  (unsigned short *) malloc(framebuffsize);

		if(!screenbuff) return KErrNotSupported;
		if(!framebuff)  return KErrNoMemory;

		// Cram function: go and hack Pico so it never gets called
		PicoCram = EmuCramNull;
	}

	// select suitable blitter
	switch(displayMode) {
		case EColor4K:  vidBlit = vidBlitKeyCfg = vidBlitRGB444; break;
		case EColor64K: vidBlit = vidBlitKeyCfg = vidBlitRGB565; break;
		case EColor16M: vidBlit = vidBlitKeyCfg = vidBlitRGB32;  break;
		default: return KErrNotSupported;
	}

	memset(framebuff, 0, framebuffsize);

	// rendermode 2?
	if(PicoOpt&0x10) {
		switch(displayMode) {
			case EColor4K:
				vidBlit = vidBlit16M2;
				PicoPrepareCram = PrepareCramRGB444M2;
				PicoCramHigh = cram_high;
				color_redM2 = 0x0F22;
				dt_dmask = 0x0333;
				break;
			case EColor64K:
				vidBlit = vidBlit16M2;
				PicoPrepareCram = PrepareCramRGB565M2;
				PicoCramHigh = cram_high;
				color_redM2 = 0xF882;
				dt_dmask = 0x39e7;
				break;
			case EColor16M:
				vidBlit = vidBlitRGB32M2;
				break;
		}
		drawTextFps = drawTextFpsM2;
		drawTextNotice = drawTextNoticeM2;
		vidBlit(1);
		return 0;
	}

	framebuff_offs = 0;
	framebuff_len  = 208*320;
	vidBlit(1);

	// setup all orientation related stuff
	if(currentConfig.iScreenRotation == TPicoConfig::PRot0) {
		if(currentConfig.iScreenMode == TPicoConfig::PMCenter) {
			PicoScan = EmuScanCenter0;
			framebuff_len = 208*224;
			drawTextFps = drawTextFps0;
			drawTextNotice = drawTextNotice0;
		} else {
			if(currentConfig.iScreenMode == TPicoConfig::PMFit2) {
				if(p800) {
					fc_inc = 6518; // 0.651786 (144+2)
					txtheight_fit = 139;
					framebuff_len = 208*146;
				} else {
					fc_inc = 9286; // 0.92857
					txtheight_fit = 201;
					framebuff_len = 208*208;
				}
			}
			PicoScan = EmuScanFit0;
			drawTextFps = drawTextFpsFit0;
			drawTextNotice = drawTextNoticeFit0;
		}
	} else if(currentConfig.iScreenRotation == TPicoConfig::PRot90) {
		if(currentConfig.iScreenMode == TPicoConfig::PMFit)
			 PicoScan = EmuScanFit90;
		else PicoScan = EmuScanCenter90;
		drawTextFps = drawTextFps90;
		drawTextNotice = drawTextNotice90;
	} else if(currentConfig.iScreenRotation == TPicoConfig::PRot180) {
		if(currentConfig.iScreenMode == TPicoConfig::PMCenter) {
			PicoScan = EmuScanCenter180;
			framebuff_len = 208*224;
		} else {
			if(currentConfig.iScreenMode == TPicoConfig::PMFit2) {
				if(p800) {
					fc_inc = 6518; // 0.651786
					fc_lines = 146;
					framebuff_len = 208*146;
				} else {
					fc_inc = 9286; // 0.92857
					fc_lines = 208;
					framebuff_len = 208*208;
				}
			}
			PicoScan = EmuScanFit180;
		}
		drawTextFps = drawTextFps180;
		drawTextNotice = drawTextNotice180;
	} else if(currentConfig.iScreenRotation == TPicoConfig::PRot270) {
		if(currentConfig.iScreenMode == TPicoConfig::PMFit)
			 PicoScan = EmuScanFit270;
		else PicoScan = EmuScanCenter270;
		drawTextFps = drawTextFps270;
		drawTextNotice = drawTextNotice270;
	}

	return 0;
}

void vidFree()
{
	free(framebuff);
	framebuff = 0;
}

void vidDrawFrame(char *noticeStr, char *fpsStr, int num)
{
	PicoFrame();
	if(currentConfig.iFlags & 2)
		drawTextFps(fpsStr);
	drawTextNotice(noticeStr);

	vidBlit(!num); // copy full frame once a second
}

void vidKeyConfigFrame(const TUint whichAction, TInt flipClosed)
{
	int i;
	char buttonNames[128];
	buttonNames[0] = 0;
	memset(framebuff, 0, framebuffsize);

	unsigned long currentActCode = 1 << whichAction;

	if(flipClosed) {
		drawRectFilled(TRect(56, 2, 152, 16), color_grey); // 96x14
		drawArrow0(TPoint(64, 3), -1, color_green);
		drawArrow0(TPoint(144, 3), 1, color_green);
		drawText0(64, 20, "USE@JOG@TO@SELECT", color_red);

		drawText0(68, 6, actionNames[whichAction], color_red);
	} else {
		// draw all "buttons" in reverse order
		const TPicoAreaConfigEntry *e = areaConfig + 1; i = 0;
		while(e->rect != TRect(0,0,0,0)) { e++; i++; }
		for(e--, i--; e->rect != TRect(0,0,0,0); e--, i--)
			drawRect(e->rect, (currentConfig.iAreaBinds[i] & currentActCode) ? color_red : color_red_dim);
	
		// draw config controls
		drawRectFilled(TRect(190, 112, 204, 208), color_grey);
		drawArrow90(TPoint(203, 120), -1, color_green);
		drawArrow90(TPoint(203, 200),  1, color_green);

		drawText90(200, 124, actionNames[whichAction], color_red);
	}

	// draw active button names if there are any
	i = 0;
	for(TPicoKeyConfigEntry *e = keyConfig; e->name; e++, i++)
		if(currentConfig.iKeyBinds[i] & currentActCode) {
			if(buttonNames[0]) strcat(buttonNames, ";@");
			strcat(buttonNames, e->name);
		}
	if(buttonNames[0]) {
		if(flipClosed) {
			buttonNames[41] = 0; // only 61 chars fit
			drawText0(2, 138, buttonNames, color_blue);
		} else {
			buttonNames[61] = 0;
			drawText90(12, 10, buttonNames, color_blue);
		}
	}

	vidBlitKeyCfg(1);
}

void vidDrawFCconfigDone()
{
	drawText0(64, 20, "USE@JOG@TO@SELECT", 0); // blank prev text
	drawText0(54, 30, "OPEN@FLIP@TO@CONTINUE", color_red);
	vidBlitKeyCfg(1);
}

void vidDrawNotice(const char *txt)
{
	if(framebuff) {
		drawTextNotice(txt);
		vidBlit(1);
	}
}
