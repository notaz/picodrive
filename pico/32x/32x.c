#include "../pico_int.h"
#include "../sound/ym2612.h"

struct Pico32x Pico32x;

static void sh2_irq_cb(int level)
{
  // diagnostic for now
  elprintf(EL_32X, "sh2 ack %d @ %08x", level, ash2_pc());
}

void p32x_update_irls(void)
{
  int irqs, mlvl = 0, slvl = 0;

  // msh2
  irqs = (Pico32x.sh2irqs | Pico32x.sh2irqi[0]) & ((Pico32x.sh2irq_mask[0] << 3) | P32XI_VRES);
  while ((irqs >>= 1))
    mlvl++;
  mlvl *= 2;

  // ssh2
  irqs = (Pico32x.sh2irqs | Pico32x.sh2irqi[1]) & ((Pico32x.sh2irq_mask[1] << 3) | P32XI_VRES);
  while ((irqs >>= 1))
    slvl++;
  slvl *= 2;

  elprintf(EL_32X, "update_irls: m %d, s %d", mlvl, slvl);
  sh2_irl_irq(&msh2, mlvl);
  if (mlvl)
    p32x_poll_event(0);
  sh2_irl_irq(&ssh2, slvl);
}

void Pico32xStartup(void)
{
  elprintf(EL_STATUS|EL_32X, "32X startup");

  PicoAHW |= PAHW_32X;
  PicoMemSetup32x();

  sh2_init(&msh2);
  msh2.irq_callback = sh2_irq_cb;
  sh2_reset(&msh2);

  sh2_init(&ssh2);
  ssh2.irq_callback = sh2_irq_cb;
  sh2_reset(&ssh2);

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

  // FB swap waits until vblank
  if ((Pico32x.vdp_regs[0x0a/2] ^ Pico32x.pending_fb) & P32XV_FS) {
    Pico32x.vdp_regs[0x0a/2] &= ~P32XV_FS;
    Pico32x.vdp_regs[0x0a/2] |= Pico32x.pending_fb;
    Pico32xSwapDRAM(Pico32x.pending_fb ^ 1);
  }

  p32x_poll_event(1);
}

// FIXME..
static __inline void SekRunM68k(int cyc)
{
  int cyc_do;
  SekCycleAim += cyc;
  if (Pico32x.emu_flags & P32XF_68KPOLL) {
    SekCycleCnt = SekCycleAim;
    return;
  }
  if ((cyc_do = SekCycleAim - SekCycleCnt) <= 0)
    return;
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

// ~1463.8, but due to cache misses and slow mem
// it's much lower than that
#define SH2_LINE_CYCLES 700

#define PICO_32X
#define RUN_SH2S \
  if (!(Pico32x.emu_flags & (P32XF_MSH2POLL|P32XF_MSH2VPOLL))) \
    sh2_execute(&msh2, SH2_LINE_CYCLES);

#include "../pico_cmn.c"

void PicoFrame32x(void)
{
  Pico32x.vdp_regs[0x0a/2] &= ~P32XV_VBLK; // get out of vblank
  if ((Pico32x.vdp_regs[0] & 3 ) != 0) // no forced blanking
    Pico32x.vdp_regs[0x0a/2] &= ~P32XV_PEN; // no pal access

  p32x_poll_event(1);

  PicoFrameStart();
  PicoFrameHints();

  // hack
  if (Pico.m.frame_count == 83) {
    Pico32xMem->sdram[0x3610 ^ 1] = 'R';
    Pico32xMem->sdram[0x3611 ^ 1] = 'E';
    Pico32xMem->sdram[0x3612 ^ 1] = 'D';
    Pico32xMem->sdram[0x3613 ^ 1] = 'Y';
  }
}
