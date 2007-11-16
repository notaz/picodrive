// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"

int (*PicoScan)(unsigned int num, void *data)=NULL;

#if OVERRIDE_HIGHCOL
static unsigned char DefHighCol[8+320+8];
unsigned char *HighCol=DefHighCol;
#else
unsigned char  HighCol[8+320+8];
#endif
unsigned short DefOutBuff[320*2];
void *DrawLineDest=DefOutBuff; // pointer to dest buffer where to draw this line to

static int  HighCacheA[41+1];   // caches for high layers
static int  HighCacheB[41+1];
static int  HighCacheS[80+1];   // and sprites
static int  HighPreSpr[80*2+1]; // slightly preprocessed sprites
char HighSprZ[320+8+8]; // Z-buffer for accurate sprites and shadow/hilight mode
                        // (if bit 7 == 0, sh caused by tile; if bit 6 == 0 pixel must be shadowed, else hilighted, if bit5 == 1)
// lsb->msb: moved sprites, not all window tiles use same priority, accurate sprites (copied from PicoOpt), interlace
//           dirty sprites, sonic mode, have layer with all hi prio tiles (mk3), layer sh/hi already processed
int rendstatus;
int Scanline=0; // Scanline

static int SpriteBlocks;
//unsigned short ppt[] = { 0x0f11, 0x0ff1, 0x01f1, 0x011f, 0x01ff, 0x0f1f, 0x0f0e, 0x0e7c };

struct TileStrip
{
  int nametab; // Position in VRAM of name table (for this tile line)
  int line;    // Line number in pixels 0x000-0x3ff within the virtual tilemap
  int hscroll; // Horizontal scroll value in pixels for the line
  int xmask;   // X-Mask (0x1f - 0x7f) for horizontal wraparound in the tilemap
  int *hc;     // cache for high tile codes and their positions
  int cells;   // cells (tiles) to draw (32 col mode doesn't need to update whole 320)
};

// stuff available in asm:
#ifdef _ASM_DRAW_C
void DrawWindow(int tstart, int tend, int prio, int sh);
void BackFill(int reg7, int sh);
void DrawSprite(int *sprite, int **hc, int sh);
void DrawTilesFromCache(int *hc, int sh, int rlim);
void DrawSpritesFromCache(int *hc, int sh);
void DrawLayer(int plane_sh, int *hcache, int cellskip, int maxcells);
void FinalizeLineBGR444(int sh);
void FinalizeLineRGB555(int sh);
void blockcpy_or(void *dst, void *src, size_t n, int pat);
#else
// utility
void blockcpy_or(void *dst, void *src, size_t n, int pat)
{
  unsigned char *pd = dst, *ps = src;
  for (; n; n--)
    *pd++ = (unsigned char) (*ps++ | pat);
}
#endif


#ifdef _ASM_DRAW_C_AMIPS
int TileNorm(int sx,int addr,int pal);
int TileFlip(int sx,int addr,int pal);
#else
static int TileNorm(int sx,int addr,int pal)
{
  unsigned char *pd = HighCol+sx;
  unsigned int pack=0; unsigned int t=0;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=pack&0x0000f000; if (t) pd[0]=(unsigned char)(pal|(t>>12));
    t=pack&0x00000f00; if (t) pd[1]=(unsigned char)(pal|(t>> 8));
    t=pack&0x000000f0; if (t) pd[2]=(unsigned char)(pal|(t>> 4));
    t=pack&0x0000000f; if (t) pd[3]=(unsigned char)(pal|(t    ));
    t=pack&0xf0000000; if (t) pd[4]=(unsigned char)(pal|(t>>28));
    t=pack&0x0f000000; if (t) pd[5]=(unsigned char)(pal|(t>>24));
    t=pack&0x00f00000; if (t) pd[6]=(unsigned char)(pal|(t>>20));
    t=pack&0x000f0000; if (t) pd[7]=(unsigned char)(pal|(t>>16));
    return 0;
  }

  return 1; // Tile blank
}

static int TileFlip(int sx,int addr,int pal)
{
  unsigned char *pd = HighCol+sx;
  unsigned int pack=0; unsigned int t=0;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=pack&0x000f0000; if (t) pd[0]=(unsigned char)(pal|(t>>16));
    t=pack&0x00f00000; if (t) pd[1]=(unsigned char)(pal|(t>>20));
    t=pack&0x0f000000; if (t) pd[2]=(unsigned char)(pal|(t>>24));
    t=pack&0xf0000000; if (t) pd[3]=(unsigned char)(pal|(t>>28));
    t=pack&0x0000000f; if (t) pd[4]=(unsigned char)(pal|(t    ));
    t=pack&0x000000f0; if (t) pd[5]=(unsigned char)(pal|(t>> 4));
    t=pack&0x00000f00; if (t) pd[6]=(unsigned char)(pal|(t>> 8));
    t=pack&0x0000f000; if (t) pd[7]=(unsigned char)(pal|(t>>12));
    return 0;
  }
  return 1; // Tile blank
}
#endif

// tile renderers for hacky operator sprite support
#define sh_pix(x) \
  if(!t); \
  else if(t==0xe) pd[x]=(unsigned char)((pd[x]&0x3f)|0x80); /* hilight */ \
  else if(t==0xf) pd[x]=(unsigned char)((pd[x]&0x3f)|0xc0); /* shadow  */ \
  else pd[x]=(unsigned char)(pal|t)

#ifndef _ASM_DRAW_C
static int TileNormSH(int sx,int addr,int pal)
{
  unsigned int pack=0; unsigned int t=0;
  unsigned char *pd = HighCol+sx;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=(pack&0x0000f000)>>12; sh_pix(0);
    t=(pack&0x00000f00)>> 8; sh_pix(1);
    t=(pack&0x000000f0)>> 4; sh_pix(2);
    t=(pack&0x0000000f)    ; sh_pix(3);
    t=(pack&0xf0000000)>>28; sh_pix(4);
    t=(pack&0x0f000000)>>24; sh_pix(5);
    t=(pack&0x00f00000)>>20; sh_pix(6);
    t=(pack&0x000f0000)>>16; sh_pix(7);
    return 0;
  }

  return 1; // Tile blank
}

static int TileFlipSH(int sx,int addr,int pal)
{
  unsigned int pack=0; unsigned int t=0;
  unsigned char *pd = HighCol+sx;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=(pack&0x000f0000)>>16; sh_pix(0);
    t=(pack&0x00f00000)>>20; sh_pix(1);
    t=(pack&0x0f000000)>>24; sh_pix(2);
    t=(pack&0xf0000000)>>28; sh_pix(3);
    t=(pack&0x0000000f)    ; sh_pix(4);
    t=(pack&0x000000f0)>> 4; sh_pix(5);
    t=(pack&0x00000f00)>> 8; sh_pix(6);
    t=(pack&0x0000f000)>>12; sh_pix(7);
    return 0;
  }
  return 1; // Tile blank
}
#endif

static int TileNormZ(int sx,int addr,int pal,int zval)
{
  unsigned int pack=0; unsigned int t=0;
  unsigned char *pd = HighCol+sx;
  char *zb = HighSprZ+sx;
  int collision = 0, zb_s;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=pack&0x0000f000; if(t) { zb_s=zb[0]; if(zb_s) collision=1; if(zval>zb_s) { pd[0]=(unsigned char)(pal|(t>>12)); zb[0]=(char)zval; } }
    t=pack&0x00000f00; if(t) { zb_s=zb[1]; if(zb_s) collision=1; if(zval>zb_s) { pd[1]=(unsigned char)(pal|(t>> 8)); zb[1]=(char)zval; } }
    t=pack&0x000000f0; if(t) { zb_s=zb[2]; if(zb_s) collision=1; if(zval>zb_s) { pd[2]=(unsigned char)(pal|(t>> 4)); zb[2]=(char)zval; } }
    t=pack&0x0000000f; if(t) { zb_s=zb[3]; if(zb_s) collision=1; if(zval>zb_s) { pd[3]=(unsigned char)(pal|(t    )); zb[3]=(char)zval; } }
    t=pack&0xf0000000; if(t) { zb_s=zb[4]; if(zb_s) collision=1; if(zval>zb_s) { pd[4]=(unsigned char)(pal|(t>>28)); zb[4]=(char)zval; } }
    t=pack&0x0f000000; if(t) { zb_s=zb[5]; if(zb_s) collision=1; if(zval>zb_s) { pd[5]=(unsigned char)(pal|(t>>24)); zb[5]=(char)zval; } }
    t=pack&0x00f00000; if(t) { zb_s=zb[6]; if(zb_s) collision=1; if(zval>zb_s) { pd[6]=(unsigned char)(pal|(t>>20)); zb[6]=(char)zval; } }
    t=pack&0x000f0000; if(t) { zb_s=zb[7]; if(zb_s) collision=1; if(zval>zb_s) { pd[7]=(unsigned char)(pal|(t>>16)); zb[7]=(char)zval; } }
    if(collision) Pico.video.status|=0x20;
    return 0;
  }

  return 1; // Tile blank
}

static int TileFlipZ(int sx,int addr,int pal,int zval)
{
  unsigned int pack=0; unsigned int t=0;
  unsigned char *pd = HighCol+sx;
  char *zb = HighSprZ+sx;
  int collision = 0, zb_s;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=pack&0x000f0000; if(t) { zb_s=zb[0]&0x1f; if(zb_s) collision=1; if(zval>zb_s) { pd[0]=(unsigned char)(pal|(t>>16)); zb[0]=(char)zval; } }
    t=pack&0x00f00000; if(t) { zb_s=zb[1]&0x1f; if(zb_s) collision=1; if(zval>zb_s) { pd[1]=(unsigned char)(pal|(t>>20)); zb[1]=(char)zval; } }
    t=pack&0x0f000000; if(t) { zb_s=zb[2]&0x1f; if(zb_s) collision=1; if(zval>zb_s) { pd[2]=(unsigned char)(pal|(t>>24)); zb[2]=(char)zval; } }
    t=pack&0xf0000000; if(t) { zb_s=zb[3]&0x1f; if(zb_s) collision=1; if(zval>zb_s) { pd[3]=(unsigned char)(pal|(t>>28)); zb[3]=(char)zval; } }
    t=pack&0x0000000f; if(t) { zb_s=zb[4]&0x1f; if(zb_s) collision=1; if(zval>zb_s) { pd[4]=(unsigned char)(pal|(t    )); zb[4]=(char)zval; } }
    t=pack&0x000000f0; if(t) { zb_s=zb[5]&0x1f; if(zb_s) collision=1; if(zval>zb_s) { pd[5]=(unsigned char)(pal|(t>> 4)); zb[5]=(char)zval; } }
    t=pack&0x00000f00; if(t) { zb_s=zb[6]&0x1f; if(zb_s) collision=1; if(zval>zb_s) { pd[6]=(unsigned char)(pal|(t>> 8)); zb[6]=(char)zval; } }
    t=pack&0x0000f000; if(t) { zb_s=zb[7]&0x1f; if(zb_s) collision=1; if(zval>zb_s) { pd[7]=(unsigned char)(pal|(t>>12)); zb[7]=(char)zval; } }
    if(collision) Pico.video.status|=0x20;
    return 0;
  }
  return 1; // Tile blank
}


#define sh_pixZ(x) \
  if(t) { \
    if(zb[x]) collision=1; \
    if(zval>zb[x]) { \
      if     (t==0xe) { pd[x]=(unsigned char)((pd[x]&0x3f)|0x80); /* hilight */ } \
      else if(t==0xf) { pd[x]=(unsigned char)((pd[x]&0x3f)|0xc0); /* shadow  */ } \
      else            { zb[x]=(char)zval; pd[x]=(unsigned char)(pal|t); } \
    } \
  }

static int TileNormZSH(int sx,int addr,int pal,int zval)
{
  unsigned int pack=0; unsigned int t=0;
  unsigned char *pd = HighCol+sx;
  char *zb = HighSprZ+sx;
  int collision = 0;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=(pack&0x0000f000)>>12; sh_pixZ(0);
    t=(pack&0x00000f00)>> 8; sh_pixZ(1);
    t=(pack&0x000000f0)>> 4; sh_pixZ(2);
    t=(pack&0x0000000f)    ; sh_pixZ(3);
    t=(pack&0xf0000000)>>28; sh_pixZ(4);
    t=(pack&0x0f000000)>>24; sh_pixZ(5);
    t=(pack&0x00f00000)>>20; sh_pixZ(6);
    t=(pack&0x000f0000)>>16; sh_pixZ(7);
    if(collision) Pico.video.status|=0x20;
    return 0;
  }

  return 1; // Tile blank
}

static int TileFlipZSH(int sx,int addr,int pal,int zval)
{
  unsigned int pack=0; unsigned int t=0;
  unsigned char *pd = HighCol+sx;
  char *zb = HighSprZ+sx;
  int collision = 0;

  pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
  if (pack)
  {
    t=(pack&0x000f0000)>>16; sh_pixZ(0);
    t=(pack&0x00f00000)>>20; sh_pixZ(1);
    t=(pack&0x0f000000)>>24; sh_pixZ(2);
    t=(pack&0xf0000000)>>28; sh_pixZ(3);
    t=(pack&0x0000000f)    ; sh_pixZ(4);
    t=(pack&0x000000f0)>> 4; sh_pixZ(5);
    t=(pack&0x00000f00)>> 8; sh_pixZ(6);
    t=(pack&0x0000f000)>>12; sh_pixZ(7);
    if(collision) Pico.video.status|=0x20;
    return 0;
  }
  return 1; // Tile blank
}

// --------------------------------------------

#ifndef _ASM_DRAW_C
static void DrawStrip(struct TileStrip *ts, int plane_sh, int cellskip)
{
  int tilex,dx,ty,code=0,addr=0,cells;
  int oldcode=-1,blank=-1; // The tile we know is blank
  int pal=0,sh;

  // Draw tiles across screen:
  sh=(plane_sh<<5)&0x40;
  tilex=((-ts->hscroll)>>3)+cellskip;
  ty=(ts->line&7)<<1; // Y-Offset into tile
  dx=((ts->hscroll-1)&7)+1;
  cells = ts->cells - cellskip;
  if(dx != 8) cells++; // have hscroll, need to draw 1 cell more
  dx+=cellskip<<3;

  for (; cells > 0; dx+=8,tilex++,cells--)
  {
    int zero=0;

    code=Pico.vram[ts->nametab+(tilex&ts->xmask)];
    if (code==blank) continue;
    if (code>>15) { // high priority tile
      int cval = code | (dx<<16) | (ty<<25);
      if(code&0x1000) cval^=7<<26;
      *ts->hc++ = cval; // cache it
      continue;
    }

    if (code!=oldcode) {
      oldcode = code;
      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      addr+=ty;
      if (code&0x1000) addr^=0xe; // Y-flip

      pal=((code>>9)&0x30)|sh;
    }

    if (code&0x0800) zero=TileFlip(dx,addr,pal);
    else             zero=TileNorm(dx,addr,pal);

    if (zero) blank=code; // We know this tile is blank now
  }

  // terminate the cache list
  *ts->hc = 0;
  // if oldcode wasn't changed, it means all layer is hi priority
  if (oldcode == -1) rendstatus|=0x40;
}

// this is messy
void DrawStripVSRam(struct TileStrip *ts, int plane_sh, int cellskip)
{
  int tilex,dx,code=0,addr=0,cell=0;
  int oldcode=-1,blank=-1; // The tile we know is blank
  int pal=0,scan=Scanline;

  // Draw tiles across screen:
  tilex=(-ts->hscroll)>>3;
  dx=((ts->hscroll-1)&7)+1;
  if(dx != 8) cell--; // have hscroll, start with negative cell
  cell+=cellskip;
  tilex+=cellskip;
  dx+=cellskip<<3;

  for (; cell < ts->cells; dx+=8,tilex++,cell++)
  {
    int zero=0,nametabadd,ty;

    //if((cell&1)==0)
    {
      int line,vscroll;
      vscroll=Pico.vsram[(plane_sh&1)+(cell&~1)];

      // Find the line in the name table
      line=(vscroll+scan)&ts->line&0xffff; // ts->line is really ymask ..
      nametabadd=(line>>3)<<(ts->line>>24);    // .. and shift[width]
      ty=(line&7)<<1; // Y-Offset into tile
    }

    code=Pico.vram[ts->nametab+nametabadd+(tilex&ts->xmask)];
    if (code==blank) continue;
    if (code>>15) { // high priority tile
      int cval = code | (dx<<16) | (ty<<25);
      if(code&0x1000) cval^=7<<26;
      *ts->hc++ = cval; // cache it
      continue;
    }

    if (code!=oldcode) {
      oldcode = code;
      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

      pal=((code>>9)&0x30)|((plane_sh<<5)&0x40);
    }

    if (code&0x0800) zero=TileFlip(dx,addr,pal);
    else             zero=TileNorm(dx,addr,pal);

    if (zero) blank=code; // We know this tile is blank now
  }

  // terminate the cache list
  *ts->hc = 0;
  if (oldcode == -1) rendstatus|=0x40;
}
#endif

#ifndef _ASM_DRAW_C
static
#endif
void DrawStripInterlace(struct TileStrip *ts)
{
  int tilex=0,dx=0,ty=0,code=0,addr=0,cells;
  int oldcode=-1,blank=-1; // The tile we know is blank
  int pal=0;

  // Draw tiles across screen:
  tilex=(-ts->hscroll)>>3;
  ty=(ts->line&15)<<1; // Y-Offset into tile
  dx=((ts->hscroll-1)&7)+1;
  cells = ts->cells;
  if(dx != 8) cells++; // have hscroll, need to draw 1 cell more

  for (; cells; dx+=8,tilex++,cells--)
  {
    int zero=0;

    code=Pico.vram[ts->nametab+(tilex&ts->xmask)];
    if (code==blank) continue;
    if (code>>15) { // high priority tile
      int cval = (code&0xfc00) | (dx<<16) | (ty<<25);
      cval|=(code&0x3ff)<<1;
      if(code&0x1000) cval^=0xf<<26;
      *ts->hc++ = cval; // cache it
      continue;
    }

    if (code!=oldcode) {
      oldcode = code;
      // Get tile address/2:
      addr=(code&0x7ff)<<5;
      if (code&0x1000) addr+=30-ty; else addr+=ty; // Y-flip

//      pal=Pico.cram+((code>>9)&0x30);
      pal=((code>>9)&0x30);
    }

    if (code&0x0800) zero=TileFlip(dx,addr,pal);
    else             zero=TileNorm(dx,addr,pal);

    if (zero) blank=code; // We know this tile is blank now
  }

  // terminate the cache list
  *ts->hc = 0;
}

// --------------------------------------------

#ifndef _ASM_DRAW_C
static void DrawLayer(int plane_sh, int *hcache, int cellskip, int maxcells)
{
  struct PicoVideo *pvid=&Pico.video;
  const char shift[4]={5,6,5,7}; // 32,64 or 128 sized tilemaps (2 is invalid)
  struct TileStrip ts;
  int width, height, ymask;
  int vscroll, htab;

  ts.hc=hcache;
  ts.cells=maxcells;

  // Work out the TileStrip to draw

  // Work out the name table size: 32 64 or 128 tiles (0-3)
  width=pvid->reg[16];
  height=(width>>4)&3; width&=3;

  ts.xmask=(1<<shift[width])-1; // X Mask in tiles (0x1f-0x7f)
  ymask=(height<<8)|0xff;       // Y Mask in pixels
  if(width == 1)   ymask&=0x1ff;
  else if(width>1) ymask =0x0ff;

  // Find name table:
  if (plane_sh&1) ts.nametab=(pvid->reg[4]&0x07)<<12; // B
  else            ts.nametab=(pvid->reg[2]&0x38)<< 9; // A

  htab=pvid->reg[13]<<9; // Horizontal scroll table address
  if ( pvid->reg[11]&2)     htab+=Scanline<<1; // Offset by line
  if ((pvid->reg[11]&1)==0) htab&=~0xf; // Offset by tile
  htab+=plane_sh&1; // A or B

  // Get horizontal scroll value, will be masked later
  ts.hscroll=Pico.vram[htab&0x7fff];

  if((pvid->reg[12]&6) == 6) {
    // interlace mode 2
    vscroll=Pico.vsram[plane_sh&1]; // Get vertical scroll value

    // Find the line in the name table
    ts.line=(vscroll+(Scanline<<1))&((ymask<<1)|1);
    ts.nametab+=(ts.line>>4)<<shift[width];

    DrawStripInterlace(&ts);
  } else if( pvid->reg[11]&4) {
    // shit, we have 2-cell column based vscroll
    // luckily this doesn't happen too often
    ts.line=ymask|(shift[width]<<24); // save some stuff instead of line
    DrawStripVSRam(&ts, plane_sh, cellskip);
  } else {
    vscroll=Pico.vsram[plane_sh&1]; // Get vertical scroll value

    // Find the line in the name table
    ts.line=(vscroll+Scanline)&ymask;
    ts.nametab+=(ts.line>>3)<<shift[width];

    DrawStrip(&ts, plane_sh, cellskip);
  }
}


// --------------------------------------------

// tstart & tend are tile pair numbers
static void DrawWindow(int tstart, int tend, int prio, int sh) // int *hcache
{
  struct PicoVideo *pvid=&Pico.video;
  int tilex=0,ty=0,nametab,code=0;
  int blank=-1; // The tile we know is blank

  // Find name table line:
  if (pvid->reg[12]&1)
  {
    nametab=(pvid->reg[3]&0x3c)<<9; // 40-cell mode
    nametab+=(Scanline>>3)<<6;
  }
  else
  {
    nametab=(pvid->reg[3]&0x3e)<<9; // 32-cell mode
    nametab+=(Scanline>>3)<<5;
  }

  tilex=tstart<<1;
  tend<<=1;

  ty=(Scanline&7)<<1; // Y-Offset into tile

  if(!(rendstatus&2)) {
    // check the first tile code
    code=Pico.vram[nametab+tilex];
    // if the whole window uses same priority (what is often the case), we may be able to skip this field
    if((code>>15) != prio) return;
  }

  // Draw tiles across screen:
  if (!sh)
  {
    for (; tilex < tend; tilex++)
    {
      int addr=0,zero=0;
      int pal;

      code=Pico.vram[nametab+tilex];
      if(code==blank) continue;
      if((code>>15) != prio) {
        rendstatus|=2;
        continue;
      }

      pal=((code>>9)&0x30);

      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

      if (code&0x0800) zero=TileFlip(8+(tilex<<3),addr,pal);
      else             zero=TileNorm(8+(tilex<<3),addr,pal);

      if (zero) blank=code; // We know this tile is blank now
    }
  }
  else
  {
    for (; tilex < tend; tilex++)
    {
      int addr=0,zero=0;
      int pal, tmp, *zb;

      code=Pico.vram[nametab+tilex];
      if(code==blank) continue;
      if((code>>15) != prio) {
        rendstatus|=2;
        continue;
      }

      pal=((code>>9)&0x30);

      zb = (int *)(HighCol+8+(tilex<<3));
      if(prio) {
        tmp = *zb;
        if(!(tmp&0x00000080)) tmp&=~0x000000c0; if(!(tmp&0x00008000)) tmp&=~0x0000c000;
        if(!(tmp&0x00800000)) tmp&=~0x00c00000; if(!(tmp&0x80000000)) tmp&=~0xc0000000;
        *zb++=tmp; tmp = *zb;
        if(!(tmp&0x00000080)) tmp&=~0x000000c0; if(!(tmp&0x00008000)) tmp&=~0x0000c000;
        if(!(tmp&0x00800000)) tmp&=~0x00c00000; if(!(tmp&0x80000000)) tmp&=~0xc0000000;
        *zb++=tmp;
      } else {
        pal |= 0x40;
      }

      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

      if (code&0x0800) zero=TileFlip(8+(tilex<<3),addr,pal);
      else             zero=TileNorm(8+(tilex<<3),addr,pal);

      if (zero) blank=code; // We know this tile is blank now
    }
  }
}

// --------------------------------------------

static void DrawTilesFromCacheShPrep(void)
{
  if (!(rendstatus&0x80))
  {
    // as some layer has covered whole line with hi priority tiles,
    // we can process whole line and then act as if sh/hi mode was off.
    rendstatus|=0x80;
    int c = 320/4, *zb = (int *)(HighCol+8);
    while (c--)
    {
      int tmp = *zb;
      if (!(tmp & 0x80808080)) *zb=tmp&0x3f3f3f3f;
      else {
        if(!(tmp&0x00000080)) tmp&=~0x000000c0; if(!(tmp&0x00008000)) tmp&=~0x0000c000;
        if(!(tmp&0x00800000)) tmp&=~0x00c00000; if(!(tmp&0x80000000)) tmp&=~0xc0000000;
        *zb=tmp;
      }
      zb++;
    }
  }
}

static void DrawTilesFromCache(int *hc, int sh, int rlim)
{
  int code, addr, dx;
  int pal;

  // *ts->hc++ = code | (dx<<16) | (ty<<25); // cache it

  if (sh && (rendstatus&0xc0))
  {
    DrawTilesFromCacheShPrep();
    sh = 0;
  }

  if (!sh)
  {
    short blank=-1; // The tile we know is blank
    while ((code=*hc++)) {
      int zero;
      if((short)code == blank) continue;
      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      addr+=(unsigned int)code>>25; // y offset into tile
      dx=(code>>16)&0x1ff;

      pal=((code>>9)&0x30);
      if (rlim-dx < 0) goto last_cut_tile;

      if (code&0x0800) zero=TileFlip(dx,addr,pal);
      else             zero=TileNorm(dx,addr,pal);

      if (zero) blank=(short)code;
    }
  }
  else
  {
    while ((code=*hc++)) {
      unsigned char *zb;
      // Get tile address/2:
      addr=(code&0x7ff)<<4;
      addr+=(unsigned int)code>>25; // y offset into tile
      dx=(code>>16)&0x1ff;
      zb = HighCol+dx;
      if(!(*zb&0x80)) *zb&=0x3f; zb++; if(!(*zb&0x80)) *zb&=0x3f; zb++;
      if(!(*zb&0x80)) *zb&=0x3f; zb++; if(!(*zb&0x80)) *zb&=0x3f; zb++;
      if(!(*zb&0x80)) *zb&=0x3f; zb++; if(!(*zb&0x80)) *zb&=0x3f; zb++;
      if(!(*zb&0x80)) *zb&=0x3f; zb++; if(!(*zb&0x80)) *zb&=0x3f; zb++;

      pal=((code>>9)&0x30);
      if (rlim-dx < 0) goto last_cut_tile;

      if (code&0x0800) TileFlip(dx,addr,pal);
      else             TileNorm(dx,addr,pal);
    }
  }
  return;

last_cut_tile:
  {
    unsigned int t, pack=*(unsigned int *)(Pico.vram+addr); // Get 8 pixels
    unsigned char *pd = HighCol+dx;
    if (!pack) return;
    if (code&0x0800)
    {
      switch (rlim-dx+8)
      {
        case 7: t=pack&0x00000f00; if (t) pd[6]=(unsigned char)(pal|(t>> 8)); // "break" is left out intentionally
        case 6: t=pack&0x000000f0; if (t) pd[5]=(unsigned char)(pal|(t>> 4));
        case 5: t=pack&0x0000000f; if (t) pd[4]=(unsigned char)(pal|(t    ));
        case 4: t=pack&0xf0000000; if (t) pd[3]=(unsigned char)(pal|(t>>28));
        case 3: t=pack&0x0f000000; if (t) pd[2]=(unsigned char)(pal|(t>>24));
        case 2: t=pack&0x00f00000; if (t) pd[1]=(unsigned char)(pal|(t>>20));
        case 1: t=pack&0x000f0000; if (t) pd[0]=(unsigned char)(pal|(t>>16));
        default: break;
      }
    }
    else
    {
      switch (rlim-dx+8)
      {
        case 7: t=pack&0x00f00000; if (t) pd[6]=(unsigned char)(pal|(t>>20));
	case 6: t=pack&0x0f000000; if (t) pd[5]=(unsigned char)(pal|(t>>24));
	case 5: t=pack&0xf0000000; if (t) pd[4]=(unsigned char)(pal|(t>>28));
	case 4: t=pack&0x0000000f; if (t) pd[3]=(unsigned char)(pal|(t    ));
	case 3: t=pack&0x000000f0; if (t) pd[2]=(unsigned char)(pal|(t>> 4));
	case 2: t=pack&0x00000f00; if (t) pd[1]=(unsigned char)(pal|(t>> 8));
	case 1: t=pack&0x0000f000; if (t) pd[0]=(unsigned char)(pal|(t>>12));
	default: break;
      }
    }
  }
}

// --------------------------------------------

// Index + 0  :    hhhhvvvv ab--hhvv yyyyyyyy yyyyyyyy // a: offscreen h, b: offs. v, h: horiz. size
// Index + 4  :    xxxxxxxx xxxxxxxx pccvhnnn nnnnnnnn // x: x coord + 8

static void DrawSprite(int *sprite, int **hc, int sh)
{
  int width=0,height=0;
  int row=0,code=0;
  int pal;
  int tile=0,delta=0;
  int sx, sy;
  int (*fTileFunc)(int sx,int addr,int pal);

  // parse the sprite data
  sy=sprite[0];
  code=sprite[1];
  sx=code>>16; // X
  width=sy>>28;
  height=(sy>>24)&7; // Width and height in tiles
  sy=(sy<<16)>>16; // Y

  row=Scanline-sy; // Row of the sprite we are on

  if (code&0x1000) row=(height<<3)-1-row; // Flip Y

  tile=code&0x7ff; // Tile number
  tile+=row>>3; // Tile number increases going down
  delta=height; // Delta to increase tile by going right
  if (code&0x0800) { tile+=delta*(width-1); delta=-delta; } // Flip X

  tile<<=4; tile+=(row&7)<<1; // Tile address

  if(code&0x8000) { // high priority - cache it
    *(*hc)++ = (tile<<16)|((code&0x0800)<<5)|((sx<<6)&0x0000ffc0)|((code>>9)&0x30)|((sprite[0]>>16)&0xf);
  } else {
    delta<<=4; // Delta of address
    pal=((code>>9)&0x30)|(sh<<6);

    if(sh && (code&0x6000) == 0x6000) {
      if(code&0x0800) fTileFunc=TileFlipSH;
      else            fTileFunc=TileNormSH;
    } else {
      if(code&0x0800) fTileFunc=TileFlip;
      else            fTileFunc=TileNorm;
    }

    for (; width; width--,sx+=8,tile+=delta)
    {
      if(sx<=0)   continue;
      if(sx>=328) break; // Offscreen

      tile&=0x7fff; // Clip tile address
      fTileFunc(sx,tile,pal);
    }
  }
}
#endif


// Index + 0  :    hhhhvvvv s---hhvv yyyyyyyy yyyyyyyy // s: skip flag, h: horiz. size
// Index + 4  :    xxxxxxxx xxxxxxxx pccvhnnn nnnnnnnn // x: x coord + 8

static void DrawSpriteZ(int pack, int pack2, int shpri, int sprio)
{
  int width=0,height=0;
  int row=0;
  int pal;
  int tile=0,delta=0;
  int sx, sy;
  int (*fTileFunc)(int sx,int addr,int pal,int zval);

  // parse the sprite data
  sx=pack2>>16; // X
  sy=(pack <<16)>>16; // Y
  width=pack>>28;
  height=(pack>>24)&7; // Width and height in tiles

  row=Scanline-sy; // Row of the sprite we are on

  if (pack2&0x1000) row=(height<<3)-1-row; // Flip Y

  tile=pack2&0x7ff; // Tile number
  tile+=row>>3; // Tile number increases going down
  delta=height; // Delta to increase tile by going right
  if (pack2&0x0800) { tile+=delta*(width-1); delta=-delta; } // Flip X

  tile<<=4; tile+=(row&7)<<1; // Tile address
  delta<<=4; // Delta of address
  pal=((pack2>>9)&0x30);
  if((shpri&1)&&!(shpri&2)) pal|=0x40;

  shpri&=1;
  if((pack2&0x6000) != 0x6000) shpri = 0;
  shpri |= (pack2&0x0800)>>10;
  switch(shpri) {
    default:
    case 0: fTileFunc=TileNormZ;   break;
    case 1: fTileFunc=TileNormZSH; break;
    case 2: fTileFunc=TileFlipZ;   break;
    case 3: fTileFunc=TileFlipZSH; break;
  }

  for (; width; width--,sx+=8,tile+=delta)
  {
    if(sx<=0)   continue;
    if(sx>=328) break; // Offscreen

    tile&=0x7fff; // Clip tile address
    fTileFunc(sx,tile,pal,sprio);
  }
}

static void DrawSpriteInterlace(unsigned int *sprite)
{
  int width=0,height=0;
  int row=0,code=0;
  int pal;
  int tile=0,delta=0;
  int sx, sy;

  // parse the sprite data
  sy=sprite[0];
  height=sy>>24;
  sy=(sy&0x3ff)-0x100; // Y
  width=(height>>2)&3; height&=3;
  width++; height++; // Width and height in tiles

  row=(Scanline<<1)-sy; // Row of the sprite we are on

  code=sprite[1];
  sx=((code>>16)&0x1ff)-0x78; // X

  if (code&0x1000) row^=(16<<height)-1; // Flip Y

  tile=code&0x3ff; // Tile number
  tile+=row>>4; // Tile number increases going down
  delta=height; // Delta to increase tile by going right
  if (code&0x0800) { tile+=delta*(width-1); delta=-delta; } // Flip X

  tile<<=5; tile+=(row&15)<<1; // Tile address

  delta<<=5; // Delta of address
  pal=((code>>9)&0x30); // Get palette pointer

  for (; width; width--,sx+=8,tile+=delta)
  {
    if(sx<=0)   continue;
    if(sx>=328) break; // Offscreen

    tile&=0x7fff; // Clip tile address
    if (code&0x0800) TileFlip(sx,tile,pal);
    else             TileNorm(sx,tile,pal);
  }
}


static void DrawAllSpritesInterlace(int pri, int maxwidth)
{
  struct PicoVideo *pvid=&Pico.video;
  int i,u,table,link=0,sline=Scanline<<1;
  unsigned int *sprites[80]; // Sprite index

  table=pvid->reg[5]&0x7f;
  if (pvid->reg[12]&1) table&=0x7e; // Lowest bit 0 in 40-cell mode
  table<<=8; // Get sprite table address/2

  for (i=u=0; u < 80 && i < 21; u++)
  {
    unsigned int *sprite;
    int code, sx, sy, height;

    sprite=(unsigned int *)(Pico.vram+((table+(link<<2))&0x7ffc)); // Find sprite

    // get sprite info
    code = sprite[0];
    sx = sprite[1];
    if(((sx>>15)&1) != pri) goto nextsprite; // wrong priority sprite

    // check if it is on this line
    sy = (code&0x3ff)-0x100;
    height = (((code>>24)&3)+1)<<4;
    if(sline < sy || sline >= sy+height) goto nextsprite; // no

    // check if sprite is not hidden offscreen
    sx = (sx>>16)&0x1ff;
    sx -= 0x78; // Get X coordinate + 8
    if(sx <= -8*3 || sx >= maxwidth) goto nextsprite;

    // sprite is good, save it's pointer
    sprites[i++]=sprite;

    nextsprite:
    // Find next sprite
    link=(code>>16)&0x7f;
    if(!link) break; // End of sprites
  }

  // Go through sprites backwards:
  for (i-- ;i>=0; i--)
    DrawSpriteInterlace(sprites[i]);
}


#ifndef _ASM_DRAW_C
static void DrawSpritesFromCache(int *hc, int sh)
{
  int code, tile, sx, delta, width;
  int pal;
  int (*fTileFunc)(int sx,int addr,int pal);

  // *(*hc)++ = (tile<<16)|((code&0x0800)<<5)|((sx<<6)&0x0000ffc0)|((code>>9)&0x30)|((sprite[0]>>24)&0xf);

  while((code=*hc++)) {
    pal=(code&0x30);
    delta=code&0xf;
    width=delta>>2; delta&=3;
    width++; delta++; // Width and height in tiles
    if (code&0x10000) delta=-delta; // Flip X
    delta<<=4;
    tile=((unsigned int)code>>17)<<1;
    sx=(code<<16)>>22; // sx can be negative (start offscreen), so sign extend

    if(sh && pal == 0x30) { //
      if(code&0x10000) fTileFunc=TileFlipSH;
      else             fTileFunc=TileNormSH;
    } else {
      if(code&0x10000) fTileFunc=TileFlip;
      else             fTileFunc=TileNorm;
    }

    for (; width; width--,sx+=8,tile+=delta)
    {
      if(sx<=0)   continue;
      if(sx>=328) break; // Offscreen

      tile&=0x7fff; // Clip tile address
      fTileFunc(sx,tile,pal);
    }
  }
}
#endif


// Index + 0  :    ----hhvv -lllllll -------y yyyyyyyy
// Index + 4  :    -------x xxxxxxxx pccvhnnn nnnnnnnn
// v
// Index + 0  :    hhhhvvvv ab--hhvv yyyyyyyy yyyyyyyy // a: offscreen h, b: offs. v, h: horiz. size
// Index + 4  :    xxxxxxxx xxxxxxxx pccvhnnn nnnnnnnn // x: x coord + 8

static void PrepareSprites(int full)
{
  struct PicoVideo *pvid=&Pico.video;
  int u=0,link=0,sblocks=0;
  int table=0;
  int *pd = HighPreSpr;

  table=pvid->reg[5]&0x7f;
  if (pvid->reg[12]&1) table&=0x7e; // Lowest bit 0 in 40-cell mode
  table<<=8; // Get sprite table address/2

  if (!full)
  {
    int pack;
    // updates: tilecode, sx
    for (u=0; u < 80 && (pack = *pd); u++, pd+=2)
    {
      unsigned int *sprite;
      int code, code2, sx, sy, skip=0;

      sprite=(unsigned int *)(Pico.vram+((table+(link<<2))&0x7ffc)); // Find sprite

      // parse sprite info
      code  = sprite[0];
      code2 = sprite[1];
      code2 &= ~0xfe000000;
      code2 -=  0x00780000; // Get X coordinate + 8 in upper 16 bits
      sx = code2>>16;

      if((sx <= 8-((pack>>28)<<3) && sx >= -0x76) || sx >= 328) skip=1<<23;
      else if ((sy = (pack<<16)>>16) < 240 && sy > -32) {
        int sbl = (2<<(pack>>28))-1;
        sblocks |= sbl<<(sy>>3);
      }

      *pd = (pack&~(1<<23))|skip;
      *(pd+1) = code2;

      // Find next sprite
      link=(code>>16)&0x7f;
      if(!link) break; // End of sprites
    }
    SpriteBlocks |= sblocks;
  }
  else
  {
    for (; u < 80; u++)
    {
      unsigned int *sprite;
      int code, code2, sx, sy, hv, height, width, skip=0, sx_min;

      sprite=(unsigned int *)(Pico.vram+((table+(link<<2))&0x7ffc)); // Find sprite

      // parse sprite info
      code = sprite[0];
      sy = (code&0x1ff)-0x80;
      hv = (code>>24)&0xf;
      height = (hv&3)+1;

      if(sy > 240 || sy + (height<<3) <= 0) skip|=1<<22;

      width  = (hv>>2)+1;
      code2 = sprite[1];
      sx = (code2>>16)&0x1ff;
      sx -= 0x78; // Get X coordinate + 8
      sx_min = 8-(width<<3);

      if((sx <= sx_min && sx >= -0x76) || sx >= 328) skip|=1<<23;
      else if (sx > sx_min && !skip) {
        int sbl = (2<<height)-1;
        int shi = sy>>3;
        if(shi < 0) shi=0; // negative sy
        sblocks |= sbl<<shi;
      }

      *pd++ = (width<<28)|(height<<24)|skip|(hv<<16)|((unsigned short)sy);
      *pd++ = (sx<<16)|((unsigned short)code2);

      // Find next sprite
      link=(code>>16)&0x7f;
      if(!link) break; // End of sprites
    }
    SpriteBlocks = sblocks;
    *pd = 0; // terminate
  }
}

static void DrawAllSprites(int *hcache, int maxwidth, int prio, int sh)
{
  int i,u,n;
  int sx1seen=0; // sprite with x coord 1 or 0 seen
  int ntiles = 0; // tile counter for sprite limit emulation
  int *sprites[40]; // Sprites to draw in fast mode
  int *ps, pack, rs = rendstatus, scan=Scanline;

  if(rs&8) {
    DrawAllSpritesInterlace(prio, maxwidth);
    return;
  }
  if(rs&0x11) {
    //dprintf("PrepareSprites(%i) [%i]", (rs>>4)&1, scan);
    PrepareSprites(rs&0x10);
    rendstatus=rs&~0x11;
  }
  if (!(SpriteBlocks & (1<<(scan>>3)))) return;

  if(((rs&4)||sh)&&prio==0)
    memset(HighSprZ, 0, 328);
  if(!(rs&4)&&prio) {
    if(hcache[0]) DrawSpritesFromCache(hcache, sh);
    return;
  }

  ps = HighPreSpr;

  // Index + 0  :    hhhhvvvv ab--hhvv yyyyyyyy yyyyyyyy // a: offscreen h, b: offs. v, h: horiz. size
  // Index + 4  :    xxxxxxxx xxxxxxxx pccvhnnn nnnnnnnn // x: x coord + 8

  for(i=u=n=0; (pack = *ps) && n < 20; ps+=2, u++)
  {
    int sx, sy, row, pack2;

    if(pack & 0x00400000) continue;

    // get sprite info
    pack2 = *(ps+1);
    sx =  pack2>>16;
    sy = (pack <<16)>>16;
    row = scan-sy;

    //dprintf("x: %i y: %i %ix%i", sx, sy, (pack>>28)<<3, (pack>>21)&0x38);

    if(sx == -0x77) sx1seen|=1; // for masking mode 2

    // check if it is on this line
    if(row < 0 || row >= ((pack>>21)&0x38)) continue; // no
    n++; // number of sprites on this line (both visible and hidden, max is 20) [broken]

    // sprite limit
    ntiles += pack>>28;
    if(ntiles > 40) break;

    if(pack & 0x00800000) continue;

    // masking sprite?
    if(sx == -0x78) {
      if(!(sx1seen&1) || sx1seen==3) {
        break; // this sprite is not drawn and remaining sprites are masked
      }
      if((sx1seen>>8) == 0) sx1seen=(i+1)<<8;
      continue;
    }
    else if(sx == -0x77) {
      // masking mode2 (Outrun, Galaxy Force II, Shadow of the beast)
      if(sx1seen>>8) { i=(sx1seen>>8)-1; break; } // seen both 0 and 1
      sx1seen |= 2;
      continue;
    }

    // accurate sprites
    //dprintf("P:%i",((sx>>15)&1));
    if(rs&4) {
      // might need to skip this sprite
      if((pack2&0x8000) ^ (prio<<15)) continue;
      DrawSpriteZ(pack,pack2,sh|(prio<<1),(char)(0x1f-n));
      continue;
    }

    // sprite is good, save it's pointer
    sprites[i++]=ps;
  }

  // Go through sprites backwards:
  if(!(rs&4)) {
    for (i--; i>=0; i--)
      DrawSprite(sprites[i],&hcache,sh);

    // terminate cache list
    *hcache = 0;
  }
}


// --------------------------------------------

#ifndef _ASM_DRAW_C
static void BackFill(int reg7, int sh)
{
  unsigned int back;

  // Start with a blank scanline (background colour):
  back=reg7&0x3f;
  back|=sh<<6;
  back|=back<<8;
  back|=back<<16;

  memset32((int *)(HighCol+8), back, 320/4);
}
#endif

// --------------------------------------------

unsigned short HighPal[0x100];

#ifndef _ASM_DRAW_C
static void FinalizeLineBGR444(int sh)
{
  unsigned short *pd=DrawLineDest;
  unsigned char  *ps=HighCol+8;
  unsigned short *pal=Pico.cram;
  int len, i, t;

  if (Pico.video.reg[12]&1) {
    len = 320;
  } else {
    if(!(PicoOpt&0x100)) pd+=32;
    len = 256;
  }

  if(sh) {
    pal=HighPal;
    if(Pico.m.dirtyPal) {
      blockcpy(pal, Pico.cram, 0x40*2);
      // shadowed pixels
      for(i = 0x3f; i >= 0; i--)
        pal[0x40|i] = pal[0xc0|i] = (unsigned short)((pal[i]>>1)&0x0777);
      // hilighted pixels
      for(i = 0x3f; i >= 0; i--) {
        t=pal[i]&0xeee;t+=0x444;if(t&0x10)t|=0xe;if(t&0x100)t|=0xe0;if(t&0x1000)t|=0xe00;t&=0xeee;
        pal[0x80|i]=(unsigned short)t;
      }
      Pico.m.dirtyPal = 0;
    }
  }

  for(i = 0; i < len; i++)
    pd[i] = pal[ps[i]];
}


static void FinalizeLineRGB555(int sh)
{
  unsigned short *pd=DrawLineDest;
  unsigned char  *ps=HighCol+8;
  unsigned short *pal=HighPal;
  int len, i, t, dirtyPal = Pico.m.dirtyPal;

  if (dirtyPal)
  {
    unsigned int *spal=(void *)Pico.cram;
    unsigned int *dpal=(void *)HighPal;
    for (i = 0x3f/2; i >= 0; i--)
#ifdef USE_BGR555
      dpal[i] = ((spal[i]&0x000f000f)<< 1)|((spal[i]&0x00f000f0)<<3)|((spal[i]&0x0f000f00)<<4);
#else
      dpal[i] = ((spal[i]&0x000f000f)<<12)|((spal[i]&0x00f000f0)<<3)|((spal[i]&0x0f000f00)>>7);
#endif
    Pico.m.dirtyPal = 0;
  }

  if (sh)
  {
    if (dirtyPal) {
      // shadowed pixels
      for (i = 0x3f; i >= 0; i--)
        pal[0x40|i] = pal[0xc0|i] = (unsigned short)((pal[i]>>1)&0x738e);
      // hilighted pixels
      for (i = 0x3f; i >= 0; i--) {
        t=pal[i]&0xe71c;t+=0x4208;if(t&0x20)t|=0x1c;if(t&0x800)t|=0x700;if(t&0x10000)t|=0xe000;t&=0xe71c;
        pal[0x80|i]=(unsigned short)t;
      }
    }
  }

  if (Pico.video.reg[12]&1) {
    len = 320;
  } else {
    if (!(PicoOpt&0x100)) pd+=32;
    len = 256;
  }

#ifndef PSP
  for (i = 0; i < len; i++)
    pd[i] = pal[ps[i]];
#else
  {
    extern void amips_clut(unsigned short *dst, unsigned char *src, unsigned short *pal, int count);
    amips_clut(pd, ps, pal, len);
  }
#endif
}
#endif

static void FinalizeLine8bit(int sh)
{
  unsigned char *pd=DrawLineDest;
  int len, rs = rendstatus;
  static int dirty_count;

  if (!sh && Pico.m.dirtyPal == 1 && Scanline < 222) {
    // a hack for mid-frame palette changes
    if (!(rs & 0x20))
         dirty_count = 1;
    else dirty_count++;
    rs |= 0x20;
    rendstatus = rs;
    if (dirty_count == 3) {
      blockcpy(HighPal, Pico.cram, 0x40*2);
    } else if (dirty_count == 11) {
      blockcpy(HighPal+0x40, Pico.cram, 0x40*2);
    }
  }

  if (Pico.video.reg[12]&1) {
    len = 320;
  } else {
    if(!(PicoOpt&0x100)) pd+=32;
    len = 256;
  }

  if (!sh && rs & 0x20) {
    if (dirty_count >= 11) {
      blockcpy_or(pd, HighCol+8, len, 0x80);
    } else {
      blockcpy_or(pd, HighCol+8, len, 0x40);
    }
  } else {
    blockcpy(pd, HighCol+8, len);
  }
}

static void (*FinalizeLine)(int sh) = FinalizeLineBGR444;

// --------------------------------------------

static int DrawDisplay(int sh)
{
  struct PicoVideo *pvid=&Pico.video;
  int win=0,edge=0,hvwind=0;
  int maxw, maxcells;

  rendstatus&=~0xc0;

  if(pvid->reg[12]&1) {
    maxw = 328; maxcells = 40;
  } else {
    maxw = 264; maxcells = 32;
  }

  // Find out if the window is on this line:
  win=pvid->reg[0x12];
  edge=(win&0x1f)<<3;

  if (win&0x80) { if (Scanline>=edge) hvwind=1; }
  else          { if (Scanline< edge) hvwind=1; }

  if (!hvwind) { // we might have a vertical window here
    win=pvid->reg[0x11];
    edge=win&0x1f;
    if (win&0x80) {
      if (!edge) hvwind=1;
      else if(edge < (maxcells>>1)) hvwind=2;
    } else {
      if (!edge);
      else if(edge < (maxcells>>1)) hvwind=2;
      else hvwind=1;
    }
  }

  DrawLayer(1|(sh<<1), HighCacheB, 0, maxcells);
  if (hvwind == 1)
    DrawWindow(0, maxcells>>1, 0, sh);
  else if (hvwind == 2) {
    // ahh, we have vertical window
    DrawLayer(0|(sh<<1), HighCacheA, (win&0x80) ?    0 : edge<<1, (win&0x80) ?     edge<<1 : maxcells);
    DrawWindow(                      (win&0x80) ? edge :       0, (win&0x80) ? maxcells>>1 : edge, 0, sh);
  } else
    DrawLayer(0|(sh<<1), HighCacheA, 0, maxcells);
  DrawAllSprites(HighCacheS, maxw, 0, sh);

  if (HighCacheB[0]) DrawTilesFromCache(HighCacheB, sh, 328);
  if (hvwind == 1)
    DrawWindow(0, maxcells>>1, 1, sh);
  else if (hvwind == 2) {
    if(HighCacheA[0]) DrawTilesFromCache(HighCacheA, sh, (win&0x80) ? edge<<4 : 328);
    DrawWindow((win&0x80) ? edge : 0, (win&0x80) ? maxcells>>1 : edge, 1, sh);
  } else
    if (HighCacheA[0]) DrawTilesFromCache(HighCacheA, sh, 328);
  DrawAllSprites(HighCacheS, maxw, 1, sh);

#if 0
  {
    int *c, a, b;
    for (a = 0, c = HighCacheA; *c; c++, a++);
    for (b = 0, c = HighCacheB; *c; c++, b++);
    printf("%i:%03i: a=%i, b=%i\n", Pico.m.frame_count, Scanline, a, b);
  }
#endif

  return 0;
}


static int Skip=0;

PICO_INTERNAL void PicoFrameStart(void)
{
  // prepare to do this frame
  rendstatus = (PicoOpt&0x80)>>5;    // accurate sprites
  if(rendstatus)
       Pico.video.status &= ~0x0020;
  else Pico.video.status |=  0x0020; // sprite collision
  if((Pico.video.reg[12]&6) == 6) rendstatus |= 8; // interlace mode
  if(Pico.m.dirtyPal) Pico.m.dirtyPal = 2; // reset dirty if needed

  PrepareSprites(1);
  Skip=0;
}

PICO_INTERNAL int PicoLine(int scan)
{
  int sh;
  if (Skip>0) { Skip--; return 0; } // Skip rendering lines

  Scanline=scan;
  sh=(Pico.video.reg[0xC]&8)>>3; // shadow/hilight?

  // Draw screen:
  BackFill(Pico.video.reg[7], sh);
  if (Pico.video.reg[1]&0x40)
    DrawDisplay(sh);

  if (FinalizeLine != NULL)
    FinalizeLine(sh);

  Skip=PicoScan(Scanline,DrawLineDest);

  return 0;
}


void PicoDrawSetColorFormat(int which)
{
  switch (which)
  {
    case 2: FinalizeLine = FinalizeLine8bit;   break;
    case 1: FinalizeLine = FinalizeLineRGB555; break;
    case 0: FinalizeLine = FinalizeLineBGR444; break;
    default:FinalizeLine = NULL; break;
  }
#if OVERRIDE_HIGHCOL
  if (which) HighCol=DefHighCol;
#endif
}

