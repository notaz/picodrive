// The SVP chip emulator, mem I/O stuff

// (c) Copyright 2008, Grazvydas "notaz" Ignotas
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "../../PicoInt.h"
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
      elprintf(EL_SVP, text); \
    clearing_ram = 1; \
    return; \
  }

unsigned int PicoSVPRead16(unsigned int a, int realsize)
{
  unsigned int d = 0;
  static int a15004_looping = 0;

  // dram: 300000-31ffff
  if      ((a & 0xfe0000) == 0x300000)
    d = *(u16 *)(svp->dram + (a&0x1fffe));

  // "cell arrange" 1: 390000-39ffff
  else if ((a & 0xff0000) == 0x390000) {
    // this is rewritten 68k code
    unsigned int a1 = a >> 1;
    a1 = (a1 & 0x7001) | ((a1 & 0x3e) << 6) | ((a1 & 0xfc0) >> 5);
    d = ((u16 *)svp->dram)[a1];
  }

  // "cell arrange" 2: 3a0000-3affff
  else if ((a & 0xff0000) == 0x3a0000) {
    // this is rewritten 68k code
    unsigned int a1 = a >> 1;
    a1 = (a1 & 0x7801) | ((a1 & 0x1e) << 6) | ((a1 & 0x7e0) >> 4);
    d = ((u16 *)svp->dram)[a1];
  }

  // regs
  else if ((a & 0xfffff0) == 0xa15000) {
    switch (a & 0xf) {
      case 0:
      case 2:
        d = svp->ssp1601.gr[SSP_XST].h;
        break;

      case 4:
        d = svp->ssp1601.gr[SSP_PM0].h;
        svp->ssp1601.gr[SSP_PM0].h &= ~1;
        if (d&1) a15004_looping = 0;
	break;
    }
  }
  else
    elprintf(EL_UIO|EL_SVP|EL_ANOMALY, "SVP FIXME: unhandled r%i: [%06x] %04x @%06x", realsize, a&0xffffff, d, SekPc);

  if (!a15004_looping)
    elprintf(EL_SVP, "SVP r%i: [%06x] %04x @%06x", realsize, a&0xffffff, d, SekPc);

  if (a == 0xa15004 && !(d&1)) {
    if (!a15004_looping)
      elprintf(EL_SVP, "SVP det TIGHT loop: a15004");
    a15004_looping = 1;
  }
  else a15004_looping = 0;

  //if (a == 0x30fe02 && d == 0)
  //  elprintf(EL_ANOMALY, "SVP lag?");

  return d;
}

void PicoSVPWrite8(unsigned int a, unsigned int d, int realsize)
{
  elprintf(EL_UIO|EL_SVP|EL_ANOMALY, "!!! SVP w%i: [%06x], %08x @%06x", realsize, a&0xffffff, d, SekPc);
}

void PicoSVPWrite16(unsigned int a, unsigned int d, int realsize)
{
  static int clearing_ram = 0;

  // DRAM
  if      ((a & 0xfe0000) == 0x300000)
    *(u16 *)(svp->dram + (a&0x1fffe)) = d;

  // regs
  else if ((a & 0xfffff0) == 0xa15000) {
    if (a == 0xa15000 || a == 0xa15002) {
      // just guessing here
      svp->ssp1601.gr[SSP_XST].h = d;
      svp->ssp1601.gr[SSP_PM0].h |= 2;
      svp->ssp1601.emu_status &= ~SSP_WAIT_PM0;
    }
    //else if (a == 0xa15006) svp->ssp1601.gr[SSP_PM0].h = d | (d << 1);
    // 0xa15006 probably has 'halt'
  }
  else
    elprintf(EL_UIO|EL_SVP|EL_ANOMALY, "SVP FIXME: unhandled w%i: [%06x] %04x @%06x", realsize, a&0xffffff, d, SekPc);


  if (a == 0x30fe06 && d != 0)
    svp->ssp1601.emu_status &= ~SSP_WAIT_30FE06;

  if (a == 0x30fe08 && d != 0)
    svp->ssp1601.emu_status &= ~SSP_WAIT_30FE08;

  // debug: detect RAM clears..
  CLEAR_DETECT(0x0221dc, 0x0221f0, "SVP RAM CLEAR (full) @ 0221C2");
  CLEAR_DETECT(0x02204c, 0x022068, "SVP RAM CLEAR 300000-31ffbf (1) @ 022032");
  CLEAR_DETECT(0x021900, 0x021ff0, "SVP RAM CLEAR 300000-305fff");
  CLEAR_DETECT(0x0220b0, 0x0220cc, "SVP RAM CLEAR 300000-31ffbf (2) @ 022096");
  clearing_ram = 0;

  elprintf(EL_SVP, "SVP w%i: [%06x] %04x @%06x", realsize, a&0xffffff, d, SekPc);
}

