#include "pico_int.h"

static void (*FinalizeLineM4)(void);
static int skip_next_line;

#define PLANAR_PIXEL(x,p) \
  t = pack & (0x80808080 >> p); \
  if (t) { \
    t = ((t >> (7-p)) | (t >> (14-p)) | (t >> (21-p)) | (t >> (28-p))) & 0x0f; \
    pd[x] = pal|t; \
  }

static int TileNormM4(int sx, int addr, int pal)
{
  unsigned char *pd = HighCol + sx;
  unsigned int pack, t;

  pack = *(unsigned int *)(Pico.vram + addr); /* Get 4 bitplanes / 8 pixels */
  if (pack)
  {
    PLANAR_PIXEL(0, 0)
    PLANAR_PIXEL(1, 1)
    PLANAR_PIXEL(2, 2)
    PLANAR_PIXEL(3, 3)
    PLANAR_PIXEL(4, 4)
    PLANAR_PIXEL(5, 5)
    PLANAR_PIXEL(6, 6)
    PLANAR_PIXEL(7, 7)
    return 0;
  }

  return 1; /* Tile blank */
}

static int TileFlipM4(int sx,int addr,int pal)
{
  unsigned char *pd = HighCol + sx;
  unsigned int pack, t;

  pack = *(unsigned int *)(Pico.vram + addr); /* Get 4 bitplanes / 8 pixels */
  if (pack)
  {
    PLANAR_PIXEL(0, 7)
    PLANAR_PIXEL(1, 6)
    PLANAR_PIXEL(2, 5)
    PLANAR_PIXEL(3, 4)
    PLANAR_PIXEL(4, 3)
    PLANAR_PIXEL(5, 2)
    PLANAR_PIXEL(6, 1)
    PLANAR_PIXEL(7, 0)
    return 0;
  }

  return 1; /* Tile blank */
}

struct TileStrip
{
  int nametab; // Position in VRAM of name table (for this tile line)
  int line;    // Line number in pixels 0x000-0x3ff within the virtual tilemap
  int hscroll; // Horizontal scroll value in pixels for the line
  int xmask;   // X-Mask (0x1f - 0x7f) for horizontal wraparound in the tilemap
  int *hc;     // cache for high tile codes and their positions
  int cells;   // cells (tiles) to draw (32 col mode doesn't need to update whole 320)
};

static void DrawStrip(struct TileStrip *ts, int cellskip)
{
  int tilex,dx,ty,code=0,addr=0,cells;
  int oldcode=-1,blank=-1; // The tile we know is blank
  int pal=0;

  // Draw tiles across screen:
  tilex=((-ts->hscroll)>>3)+cellskip;
  ty=(ts->line&7)<<1; // Y-Offset into tile
  dx=((ts->hscroll-1)&7)+1;
  cells = ts->cells - cellskip;
  if (dx != 8) cells++; // have hscroll, need to draw 1 cell more
  dx+=cellskip<<3;

  for (; cells > 0; dx+=8,tilex++,cells--)
  {
    int zero;

    code=Pico.vram[ts->nametab + (tilex & 0x1f)];
    if (code==blank) continue;

    if (code!=oldcode) {
      oldcode = code;
      // Get tile address/2:
      addr=(code&0x1ff)<<4;
      addr+=ty;
      if (code&0x0400) addr^=0xe; // Y-flip

      pal=((code>>7)&0x10);
    }

    if (code&0x0200) zero=TileFlipM4(dx,addr,pal);
    else             zero=TileNormM4(dx,addr,pal);

    if (zero) blank=code; // We know this tile is blank now
  }
}

static void DrawLayer(int cellskip, int maxcells)
{
  struct PicoVideo *pvid=&Pico.video;
  struct TileStrip ts;
  int vscroll;

  ts.cells=maxcells;

  // Find name table:
  ts.nametab=(pvid->reg[2]&0x0e) << (10-1);

  // Get horizontal scroll value, will be masked later
  ts.hscroll=0;//pvid->reg[8];
  vscroll=0;//pvid->reg[9]; // Get vertical scroll value

  // Find the line in the name table
  ts.line=(vscroll+DrawScanline)&0xff;
  ts.nametab+=(ts.line>>3) << (6-1);

  DrawStrip(&ts, cellskip);
}

static void DrawDisplayM4(void)
{
  DrawLayer(0, 32);
}

void PicoFrameStartMode4(void)
{
  DrawScanline = 0;
  skip_next_line = 0;
}

void PicoLineMode4(int line)
{
  if (skip_next_line > 0) {
    skip_next_line--;
    return;
  }

  DrawScanline = line;

  if (PicoScanBegin != NULL)
    skip_next_line = PicoScanBegin(DrawScanline);

  // Draw screen:
  BackFill((Pico.video.reg[7] & 0x0f) | 0x10, 0);
  if (Pico.video.reg[1] & 0x40)
    DrawDisplayM4();

  if (FinalizeLineM4 != NULL)
    FinalizeLineM4();

  if (PicoScanEnd != NULL)
    skip_next_line = PicoScanEnd(DrawScanline);
}

void PicoDoHighPal555M4(void)
{
  unsigned int *spal=(void *)Pico.cram;
  unsigned int *dpal=(void *)HighPal;
  unsigned int t;
  int i;

  Pico.m.dirtyPal = 0;

  /* cram is always stored as shorts, even though real hardware probably uses bytes */
  for (i = 0x20/2; i > 0; i--, spal++, dpal++) {
    t = *spal;
#ifdef USE_BGR555
    t = ((t & 0x00030003)<< 3) | ((t & 0x000c000c)<<7) | ((t & 0x00300030)<<10);
#else
    t = ((t & 0x00030003)<<14) | ((t & 0x000c000c)<<7) | ((t & 0x00300030)>>1);
#endif
    t |= t >> 2;
    t |= (t >> 4) & 0x08610861;
    *dpal = t;
  }
}

static void FinalizeLineRGB555M4(void)
{
  unsigned short *pd=DrawLineDest;
  unsigned char  *ps=HighCol+8;
  unsigned short *pal=HighPal;
  int i;

  if (Pico.m.dirtyPal)
    PicoDoHighPal555M4();

  if (!(PicoOpt & POPT_DIS_32C_BORDER))
    pd += 32;

  for (i = 256/4; i > 0; i--) {
    *pd++ = pal[*ps++];
    *pd++ = pal[*ps++];
    *pd++ = pal[*ps++];
    *pd++ = pal[*ps++];
  }
}

void PicoDrawSetColorFormatMode4(int which)
{
  switch (which)
  {
    case 1: FinalizeLineM4 = FinalizeLineRGB555M4; break;
    default:FinalizeLineM4 = NULL; break;
  }
#if OVERRIDE_HIGHCOL
  if (which)
    HighCol = DefHighCol;
#endif
}

