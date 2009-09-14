#include "../pico_int.h"
#include "../sound/ym2612.h"

struct Pico32x Pico32x;

void Pico32xStartup(void)
{
  elprintf(EL_STATUS|EL_32X, "32X startup");

  PicoAHW |= PAHW_32X;
  PicoMemSetup32x();

  if (!Pico.m.pal)
    Pico32x.vdp_regs[0] |= P32XV_nPAL;

  emu_32x_startup();
}

void Pico32xInit(void)
{
}

void PicoPower32x(void)
{
  memset(&Pico32x, 0, sizeof(Pico32x));

  Pico32x.regs[0] = 0x0082; // SH2 reset?
  Pico32x.vdp_regs[0x0a/2] = P32XV_VBLK|P32XV_HBLK|P32XV_PEN;
}

void PicoUnload32x(void)
{
  if (Pico32xMem != NULL)
    free(Pico32xMem);
  Pico32xMem = NULL;

  PicoAHW &= ~PAHW_32X;
}

void PicoReset32x(void)
{
  extern int p32x_csum_faked;
  p32x_csum_faked = 0; // tmp
}

static void p32x_start_blank(void)
{
  // enter vblank
  Pico32x.vdp_regs[0x0a/2] |= P32XV_VBLK|P32XV_PEN;

  // swap waits until vblank
  if ((Pico32x.vdp_regs[0x0a/2] ^ Pico32x.pending_fb) & P32XV_FS) {
    Pico32x.vdp_regs[0x0a/2] &= ~P32XV_FS;
    Pico32x.vdp_regs[0x0a/2] |= Pico32x.pending_fb;
    Pico32xSwapDRAM(Pico32x.pending_fb ^ 1);
  }
}

// FIXME..
static __inline void SekRunM68k(int cyc)
{
  int cyc_do;
  SekCycleAim+=cyc;
  if ((cyc_do=SekCycleAim-SekCycleCnt) <= 0) return;
#if defined(EMU_CORE_DEBUG)
  // this means we do run-compare
  SekCycleCnt+=CM_compareRun(cyc_do, 0);
#elif defined(EMU_C68K)
  PicoCpuCM68k.cycles=cyc_do;
  CycloneRun(&PicoCpuCM68k);
  SekCycleCnt+=cyc_do-PicoCpuCM68k.cycles;
#elif defined(EMU_M68K)
  SekCycleCnt+=m68k_execute(cyc_do);
#elif defined(EMU_F68K)
  SekCycleCnt+=fm68k_emulate(cyc_do+1, 0, 0);
#endif
}

#define PICO_32X
#include "../pico_cmn.c"

void PicoFrame32x(void)
{
  if ((Pico32x.vdp_regs[0] & 3 ) != 0) // no forced blanking
    Pico32x.vdp_regs[0x0a/2] &= ~(P32XV_VBLK|P32XV_PEN); // get out of vblank

  PicoFrameStart();
  PicoFrameHints();
}
