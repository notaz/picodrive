#include "../pico_int.h"

struct Pico32x Pico32x;

void Pico32xStartup(void)
{
  elprintf(EL_STATUS|EL_32X, "32X startup");

  PicoAHW |= PAHW_32X;
  PicoMemSetup32x();

  // probably should only done on power
//  memset(&Pico32x, 0, sizeof(Pico32x));

  if (!Pico.m.pal)
    Pico32x.vdp_regs[0] |= 0x8000;

  // prefill checksum
  Pico32x.regs[0x28/2] = *(unsigned short *)(Pico.rom + 0x18e);
}

void Pico32xInit(void)
{
  // XXX: mv
  Pico32x.regs[0] = 0x0082;
}

void PicoReset32x(void)
{
}

