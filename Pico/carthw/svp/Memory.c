#include "../../PicoInt.h"

unsigned int PicoSVPRead16(unsigned int a, int realsize)
{
  unsigned int d = 0;

  elprintf(EL_UIO, "SVP r%i: [%06x] %04x @%06x", realsize, a&0xffffff, d, SekPc);

  return d;
}

void PicoSVPWrite8(unsigned int a, unsigned int d, int realsize)
{
  elprintf(EL_UIO, "SVP w%i: %06x, %08x @%06x", realsize, a&0xffffff, d, SekPc);
}

