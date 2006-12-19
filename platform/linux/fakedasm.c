// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "../../Pico/PicoInt.h"
#undef blockcpy

extern unsigned short DefOutBuff[320*2];
extern unsigned char  HighCol[8+320+8];
extern char HighSprZ[320+8+8]; // Z-buffer for accurate sprites and shadow/hilight mode
                        // (if bit 7 == 0, sh caused by tile; if bit 6 == 0 pixel must be shadowed, else hilighted, if bit5 == 1)
// lsb->msb: moved sprites, all window tiles don't use same priority, accurate sprites (copied from PicoOpt), interlace
//           dirty sprites, sonic mode
extern int rendstatus;
extern int Scanline; // Scanline


struct TileStrip
{
  int nametab; // Position in VRAM of name table (for this tile line)
  int line;    // Line number in pixels 0x000-0x3ff within the virtual tilemap 
  int hscroll; // Horizontal scroll value in pixels for the line
  int xmask;   // X-Mask (0x1f - 0x7f) for horizontal wraparound in the tilemap
  int *hc;     // cache for high tile codes and their positions
  int cells;   // cells (tiles) to draw (32 col mode doesn't need to update whole 320)
};

// utility
void *blockcpy(void *dst, const void *src, size_t n)
{
	return memcpy(dst, src, n);
}

void blockcpy_or(void *dst, void *src, size_t n, int pat)
{
  unsigned char *pd = dst, *ps = src;
  for (; n; n--)
    *pd++ = (unsigned char) (*ps++ | pat);
}


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


// tile renderers for hacky operator sprite support
#define sh_pix(x) \
  if(!t); \
  else if(t==0xe) pd[x]=(unsigned char)((pd[x]&0x3f)|0x80); /* hilight */ \
  else if(t==0xf) pd[x]=(unsigned char)((pd[x]&0x3f)|0xc0); /* shadow  */ \
  else pd[x]=(unsigned char)(pal|t);

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


// --------------------------------------------

static void DrawStrip(struct TileStrip *ts, int sh)
{
  int tilex=0,dx=0,ty=0,code=0,addr=0,cells;
  int oldcode=-1,blank=-1; // The tile we know is blank
  int pal=0;

  // Draw tiles across screen:
  tilex=(-ts->hscroll)>>3;
  ty=(ts->line&7)<<1; // Y-Offset into tile
  dx=((ts->hscroll-1)&7)+1;
  cells = ts->cells;
  if(dx != 8) cells++; // have hscroll, need to draw 1 cell more

  for (; cells; dx+=8,tilex++,cells--)
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

//      pal=Pico.cram+((code>>9)&0x30);
      pal=((code>>9)&0x30)|(sh<<6);
    }

    if (code&0x0800) zero=TileFlip(dx,addr,pal);
    else             zero=TileNorm(dx,addr,pal);

    if (zero) blank=code; // We know this tile is blank now
  }

  // terminate the cache list
  *ts->hc = 0;
}

static void DrawStripVSRam(struct TileStrip *ts, int plane)
{
  int tilex=0,dx=0,ty=0,code=0,addr=0,cell=0,nametabadd=0;
  int oldcode=-1,blank=-1; // The tile we know is blank
  int pal=0,scan=Scanline;

  // Draw tiles across screen:
  tilex=(-ts->hscroll)>>3;
  dx=((ts->hscroll-1)&7)+1;
  if(dx != 8) {
    int vscroll, line;
    cell--; // have hscroll, start with negative cell
    // also calculate intial VS stuff
    vscroll=Pico.vsram[plane];

    // Find the line in the name table
    line=(vscroll+scan)&ts->line&0xffff; // ts->line is really ymask ..
    nametabadd=(line>>3)<<(ts->line>>24);    // .. and shift[width]
    ty=(line&7)<<1; // Y-Offset into tile
  }

  for (; cell < ts->cells; dx+=8,tilex++,cell++)
  {
    int zero=0;

    if((cell&1)==0) {
      int line,vscroll;
      vscroll=Pico.vsram[plane+(cell&~1)];

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

static void DrawStripInterlace(struct TileStrip *ts)
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

void DrawLayer(int plane, int *hcache, int maxcells, int sh)
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
  if (plane==0) ts.nametab=(pvid->reg[2]&0x38)<< 9; // A
  else          ts.nametab=(pvid->reg[4]&0x07)<<12; // B

  htab=pvid->reg[13]<<9; // Horizontal scroll table address
  if ( pvid->reg[11]&2)     htab+=Scanline<<1; // Offset by line
  if ((pvid->reg[11]&1)==0) htab&=~0xf; // Offset by tile
  htab+=plane; // A or B

  // Get horizontal scroll value, will be masked later
  ts.hscroll=Pico.vram[htab&0x7fff];

  if((pvid->reg[12]&6) == 6) {
    // interlace mode 2
    vscroll=Pico.vsram[plane]; // Get vertical scroll value

    // Find the line in the name table
    ts.line=(vscroll+(Scanline<<1))&((ymask<<1)|1);
    ts.nametab+=(ts.line>>4)<<shift[width];

    DrawStripInterlace(&ts);
  } else if( pvid->reg[11]&4) {
    // shit, we have 2-cell column based vscroll
    // luckily this doesn't happen too often
    ts.line=ymask|(shift[width]<<24); // save some stuff instead of line
    DrawStripVSRam(&ts, plane);
  } else {
    vscroll=Pico.vsram[plane]; // Get vertical scroll value

    // Find the line in the name table
    ts.line=(vscroll+Scanline)&ymask;
    ts.nametab+=(ts.line>>3)<<shift[width];

    DrawStrip(&ts, sh);
  }
}


// --------------------------------------------

// tstart & tend are tile pair numbers
void DrawWindow(int tstart, int tend, int prio, int sh) // int *hcache
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

    if(sh) {
      int tmp, *zb = (int *)(HighCol+8+(tilex<<3));
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
    }

    // Get tile address/2:
    addr=(code&0x7ff)<<4;
    if (code&0x1000) addr+=14-ty; else addr+=ty; // Y-flip

    if (code&0x0800) zero=TileFlip(8+(tilex<<3),addr,pal);
    else             zero=TileNorm(8+(tilex<<3),addr,pal);

    if (zero) blank=code; // We know this tile is blank now
  }

  // terminate the cache list
  //*hcache = 0;
}

// --------------------------------------------

void DrawTilesFromCache(int *hc, int sh)
{
  int code, addr, zero, dx;
  int pal;
  short blank=-1; // The tile we know is blank

  // *ts->hc++ = code | (dx<<16) | (ty<<25); // cache it

  while((code=*hc++)) {
    if(!sh && (short)code == blank) continue;

    // Get tile address/2:
    addr=(code&0x7ff)<<4;
    addr+=(unsigned int)code>>25; // y offset into tile
    dx=(code>>16)&0x1ff;
    if(sh) {
      unsigned char *zb = HighCol+dx;
      if(!(*zb&0x80)) *zb&=0x3f; zb++; if(!(*zb&0x80)) *zb&=0x3f; zb++;
      if(!(*zb&0x80)) *zb&=0x3f; zb++; if(!(*zb&0x80)) *zb&=0x3f; zb++;
      if(!(*zb&0x80)) *zb&=0x3f; zb++; if(!(*zb&0x80)) *zb&=0x3f; zb++;
      if(!(*zb&0x80)) *zb&=0x3f; zb++; if(!(*zb&0x80)) *zb&=0x3f; zb++;
    }

    pal=((code>>9)&0x30);

    if (code&0x0800) zero=TileFlip(dx,addr,pal);
    else             zero=TileNorm(dx,addr,pal);

    if(zero) blank=(short)code;
  }
}

// --------------------------------------------

// Index + 0  :    hhhhvvvv ab--hhvv yyyyyyyy yyyyyyyy // a: offscreen h, b: offs. v, h: horiz. size
// Index + 4  :    xxxxxxxx xxxxxxxx pccvhnnn nnnnnnnn // x: x coord + 8

void DrawSprite(int *sprite, int **hc, int sh)
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


void DrawSpritesFromCache(int *hc, int sh)
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


void BackFill(int reg7, int sh)
{
  unsigned int back=0;
  unsigned int *pd=NULL,*end=NULL;

  // Start with a blank scanline (background colour):
  back=reg7&0x3f;
  back|=sh<<6;
  back|=back<<8;
  back|=back<<16;

  pd= (unsigned int *)(HighCol+8);
  end=(unsigned int *)(HighCol+8+320);

  do { pd[0]=pd[1]=pd[2]=pd[3]=back; pd+=4; } while (pd<end);
}

// --------------------------------------------

extern unsigned short HighPal[0x100];

void FinalizeLineBGR444(int sh)
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


void FinalizeLineRGB555(int sh)
{
  unsigned short *pd=DrawLineDest;
  unsigned char  *ps=HighCol+8;
  unsigned short *pal=HighPal;
  int len, i, t, dirtyPal = Pico.m.dirtyPal;

  if(dirtyPal) {
    unsigned short *ppal=Pico.cram;
    for(i = 0x3f; i >= 0; i--)
      pal[i] = (unsigned short) (((ppal[i]&0x00f)<<12)|((ppal[i]&0x0f0)<<3)|((ppal[i]&0xf00)>>7));
    Pico.m.dirtyPal = 0;
  }

  if (Pico.video.reg[12]&1) {
    len = 320;
  } else {
    if(!(PicoOpt&0x100)) pd+=32;
    len = 256;
  }

  if(sh) {
    if(dirtyPal) {
      // shadowed pixels
      for(i = 0x3f; i >= 0; i--)
        pal[0x40|i] = pal[0xc0|i] = (unsigned short)((pal[i]>>1)&0x738e);
      // hilighted pixels
      for(i = 0x3f; i >= 0; i--) {
        t=pal[i]&0xe71c;t+=0x4208;if(t&0x20)t|=0x1c;if(t&0x800)t|=0x700;if(t&0x10000)t|=0xe000;t&=0xe71c;
        pal[0x80|i]=(unsigned short)t;
      }
    }
  }

  for(i = 0; i < len; i++)
    pd[i] = pal[ps[i]];
}



