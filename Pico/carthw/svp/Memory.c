#include "../../PicoInt.h"

#ifndef UTYPES_DEFINED
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
#define UTYPES_DEFINED
#endif

#define CLEAR_DETECT(pc_start,pc_end,text) \
  if (d == 0 && SekPc >= pc_start && SekPc < pc_end) \
  { \
    if (!clearing_ram) \
      elprintf(EL_UIO, text); \
    clearing_ram = 1; \
    return; \
  }

unsigned int PicoSVPRead16(unsigned int a, int realsize)
{
  unsigned int d = 0;

  if ((a & 0xfe0000) == 0x300000)
    d = *(u16 *)(svp->dram + (a&0x1fffe));

  elprintf(EL_UIO, "SVP r%i: [%06x] %04x @%06x", realsize, a&0xffffff, d, SekPc);

  // if (a == 0x30fe02) d = 1;

  return d;
}

void PicoSVPWrite8(unsigned int a, unsigned int d, int realsize)
{
  elprintf(EL_UIO, "!!! SVP w%i: [%06x], %08x @%06x", realsize, a&0xffffff, d, SekPc);
}

void PicoSVPWrite16(unsigned int a, unsigned int d, int realsize)
{
  static int clearing_ram = 0;

  if ((a & 0xfe0000) == 0x300000)
    *(u16 *)(svp->dram + (a&0x1fffe)) = d;

  if (a == 0x30fe06 && d != 0)
    svp->ssp1601.emu_status &= ~SSP_30FE06_WAIT;

  if (a == 0x30fe08 && d != 0)
    svp->ssp1601.emu_status &= ~SSP_30FE08_WAIT;

  // debug: detect RAM clears..
  CLEAR_DETECT(0x0221dc, 0x0221f0, "SVP RAM CLEAR (full) @ 0221C2");
  CLEAR_DETECT(0x02204c, 0x022068, "SVP RAM CLEAR 300000-31ffbf (1) @ 022032");
  CLEAR_DETECT(0x021900, 0x021ff0, "SVP RAM CLEAR 300000-305fff");
  CLEAR_DETECT(0x0220b0, 0x0220cc, "SVP RAM CLEAR 300000-31ffbf (2) @ 022096");
  clearing_ram = 0;

  elprintf(EL_UIO, "SVP w%i: [%06x], %04x @%06x", realsize, a&0xffffff, d, SekPc);

  // just guessing here
       if (a == 0xa15002) svp->ssp1601.gr[SSP_XST].h = d;
  else if (a == 0xa15006) svp->ssp1601.gr[SSP_PM0].h = d | (d << 1);
}

