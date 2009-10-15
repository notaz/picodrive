// The SVP chip emulator, mem I/O stuff

// (c) Copyright 2008, Grazvydas "notaz" Ignotas
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "../../pico_int.h"
#include "../../memory.h"

// for wait loop det
static void PicoWrite16_dram(u32 a, u32 d)
{
  a &= ~0xfe0000;

  if (d != 0) {
    if      (a == 0xfe06) // 30fe06
      svp->ssp1601.emu_status &= ~SSP_WAIT_30FE06;
    else if (a == 0xfe08)
      svp->ssp1601.emu_status &= ~SSP_WAIT_30FE08;
  }

  ((u16 *)svp->dram)[a / 2] = d;
}

// "cell arrange" 1: 390000-39ffff
static u32 PicoRead16_svpca1(u32 a)
{
  // this is 68k code rewritten
  u32 a1 = a >> 1;
  a1 = (a1 & 0x7001) | ((a1 & 0x3e) << 6) | ((a1 & 0xfc0) >> 5);
  return ((u16 *)svp->dram)[a1];
}

// "cell arrange" 2: 3a0000-3affff
static u32 PicoRead16_svpca2(u32 a)
{
  u32 a1 = a >> 1;
  a1 = (a1 & 0x7801) | ((a1 & 0x1e) << 6) | ((a1 & 0x7e0) >> 4);
  return ((u16 *)svp->dram)[a1];
}

// IO/control area (0xa10000 - 0xa1ffff)
static u32 PicoRead16_svpr(u32 a)
{
  u32 d = 0;

  // regs
  if ((a & ~0x0f) == 0xa15000) {
    switch (a & 0xf) {
      case 0:
      case 2:
        d = svp->ssp1601.gr[SSP_XST].h;
        break;

      case 4:
        d = svp->ssp1601.gr[SSP_PM0].h;
        svp->ssp1601.gr[SSP_PM0].h &= ~1;
	break;
    }

#if EL_LOGMASK & EL_SVP
    {
      static int a15004_looping = 0;
      if (a == 0xa15004 && (d & 1))
        a15004_looping = 0;

      if (!a15004_looping)
        elprintf(EL_SVP, "SVP r%i: [%06x] %04x @%06x", realsize, a, d, SekPc);

      if (a == 0xa15004 && !(d&1)) {
        if (!a15004_looping)
          elprintf(EL_SVP, "SVP det TIGHT loop: a15004");
        a15004_looping = 1;
      }
      else
        a15004_looping = 0;
    }
#endif
    return d;
  }

  //if (a == 0x30fe02 && d == 0)
  //  elprintf(EL_ANOMALY, "SVP lag?");

  return PicoRead16_io(a);
}

// used in VR test mode
static u32 PicoRead8_svpr(u32 a)
{
  u32 d;

  if ((a & ~0x0f) != 0xa15000)
    return PicoRead8_io(a);

  d = PicoRead16_svpr(a & ~1);
  if (!(a & 1))
    d >>= 8;
  return d;
}

static void PicoWrite16_svpr(u32 a, u32 d)
{
  elprintf(EL_SVP, "SVP w16: [%06x] %04x @%06x", a, d, SekPc);

  if ((a & ~0x0f) == 0xa15000) {
    if (a == 0xa15000 || a == 0xa15002) {
      // just guessing here
      svp->ssp1601.gr[SSP_XST].h = d;
      svp->ssp1601.gr[SSP_PM0].h |= 2;
      svp->ssp1601.emu_status &= ~SSP_WAIT_PM0;
    }
    //else if (a == 0xa15006) svp->ssp1601.gr[SSP_PM0].h = d | (d << 1);
    // 0xa15006 probably has 'halt'
    return;
  }

  PicoWrite16_io(a, d);
}

void PicoSVPMemSetup(void)
{
  // 68k memmap:
  // DRAM
  cpu68k_map_set(m68k_read8_map,   0x300000, 0x31ffff, svp->dram, 0);
  cpu68k_map_set(m68k_read16_map,  0x300000, 0x31ffff, svp->dram, 0);
  cpu68k_map_set(m68k_write8_map,  0x300000, 0x31ffff, svp->dram, 0);
  cpu68k_map_set(m68k_write16_map, 0x300000, 0x31ffff, svp->dram, 0);
  cpu68k_map_set(m68k_write16_map, 0x300000, 0x30ffff, PicoWrite16_dram, 1);

  // DRAM (cell arrange)
  cpu68k_map_set(m68k_read16_map,  0x390000, 0x39ffff, PicoRead16_svpca1, 1);
  cpu68k_map_set(m68k_read16_map,  0x3a0000, 0x3affff, PicoRead16_svpca2, 1);

  // regs
  cpu68k_map_set(m68k_read8_map,   0xa10000, 0xa1ffff, PicoRead8_svpr, 1);
  cpu68k_map_set(m68k_read16_map,  0xa10000, 0xa1ffff, PicoRead16_svpr, 1);
  cpu68k_map_set(m68k_write8_map,  0xa10000, 0xa1ffff, PicoWrite8_io, 1); // PicoWrite8_svpr
  cpu68k_map_set(m68k_write16_map, 0xa10000, 0xa1ffff, PicoWrite16_svpr, 1);
}

