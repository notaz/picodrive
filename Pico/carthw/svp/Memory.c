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
    *(u16 *)(svp->ram + (a&0x1fffe)) = d;

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
    *(u16 *)(svp->ram + (a&0x1fffe)) = d;

  // debug: detect RAM clears..
  CLEAR_DETECT(0x0221dc, 0x0221f0, "SVP RAM CLEAR (1)");
  CLEAR_DETECT(0x02204c, 0x022068, "SVP RAM CLEAR (2)");
  CLEAR_DETECT(0x021900, 0x021ff0, "SVP RAM CLEAR 300000-305fff");
  clearing_ram = 0;

  elprintf(EL_UIO, "SVP w%i: [%06x], %04x @%06x", realsize, a&0xffffff, d, SekPc);
}

