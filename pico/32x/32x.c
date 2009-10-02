#include "../pico_int.h"
#include "../sound/ym2612.h"

SH2 sh2s[2];
struct Pico32x Pico32x;

static void sh2_irq_cb(int id, int level)
{
  // diagnostic for now
  elprintf(EL_32X, "%csh2 ack %d @ %08x", id ? 's' : 'm', level, sh2_pc(id));
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
  sh2_irl_irq(&ssh2, slvl);
  mlvl = mlvl ? 1 : 0;
  slvl = slvl ? 1 : 0;
  p32x_poll_event(mlvl | (slvl << 1), 0);
}

void Pico32xStartup(void)
{
  elprintf(EL_STATUS|EL_32X, "32X startup");

  PicoAHW |= PAHW_32X;
  PicoMemSetup32x();

  sh2_init(&msh2, 0);
  msh2.irq_callback = sh2_irq_cb;
  sh2_reset(&msh2);

  sh2_init(&ssh2, 1);
  ssh2.irq_callback = sh2_irq_cb;
  sh2_reset(&ssh2);

  if (!Pico.m.pal)
    Pico32x.vdp_regs[0] |= P32XV_nPAL;

  PREG8(Pico32xMem->sh2_peri_regs[0], 4) =
  PREG8(Pico32xMem->sh2_peri_regs[1], 4) = 0x84; // SCI SSR

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
  Pico32x.sh2_regs[0] = P32XS2_ADEN;
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

  Pico32x.sh2irqs |= P32XI_VINT;
  p32x_update_irls();
  p32x_poll_event(3, 1);
}

static __inline void run_m68k(int cyc)
{
#if defined(EMU_C68K)
  PicoCpuCM68k.cycles = cyc;
  CycloneRun(&PicoCpuCM68k);
  SekCycleCnt += cyc - PicoCpuCM68k.cycles;
#elif defined(EMU_M68K)
  SekCycleCnt += m68k_execute(cyc);
#elif defined(EMU_F68K)
  SekCycleCnt += fm68k_emulate(cyc+1, 0, 0);
#endif
}

// ~1463.8, but due to cache misses and slow mem
// it's much lower than that
//#define SH2_LINE_CYCLES 735
#define CYCLES_M68K2SH2(x) ((x) * 6 / 4)

#define PICO_32X
#define CPUS_RUN_SIMPLE(m68k_cycles,s68k_cycles) \
{ \
  int slice; \
  SekCycleAim += m68k_cycles; \
  while (SekCycleCnt < SekCycleAim) { \
    slice = SekCycleCnt; \
    run_m68k(SekCycleAim - SekCycleCnt); \
    slice = SekCycleCnt - slice; /* real count from 68k */ \
    if (SekCycleCnt < SekCycleAim) \
      elprintf(EL_32X, "slice %d", slice); \
    if (!(Pico32x.emu_flags & (P32XF_SSH2POLL|P32XF_SSH2VPOLL))) \
      sh2_execute(&ssh2, CYCLES_M68K2SH2(slice)); \
    if (!(Pico32x.emu_flags & (P32XF_MSH2POLL|P32XF_MSH2VPOLL))) \
      sh2_execute(&msh2, CYCLES_M68K2SH2(slice)); \
  } \
}

#define STEP_68K 24
#define CPUS_RUN_LOCKSTEP(m68k_cycles,s68k_cycles) \
{ \
  int i; \
  for (i = 0; i <= (m68k_cycles) - STEP_68K; i += STEP_68K) { \
    run_m68k(STEP_68K); \
    if (!(Pico32x.emu_flags & (P32XF_MSH2POLL|P32XF_MSH2VPOLL))) \
      sh2_execute(&msh2, CYCLES_M68K2SH2(STEP_68K)); \
    if (!(Pico32x.emu_flags & (P32XF_SSH2POLL|P32XF_SSH2VPOLL))) \
      sh2_execute(&ssh2, CYCLES_M68K2SH2(STEP_68K)); \
  } \
  /* last step */ \
  i = (m68k_cycles) - i; \
  run_m68k(i); \
  if (!(Pico32x.emu_flags & (P32XF_MSH2POLL|P32XF_MSH2VPOLL))) \
    sh2_execute(&msh2, CYCLES_M68K2SH2(i)); \
  if (!(Pico32x.emu_flags & (P32XF_SSH2POLL|P32XF_SSH2VPOLL))) \
    sh2_execute(&ssh2, CYCLES_M68K2SH2(i)); \
}

#define CPUS_RUN CPUS_RUN_SIMPLE
//#define CPUS_RUN CPUS_RUN_LOCKSTEP

#include "../pico_cmn.c"

void PicoFrame32x(void)
{
  pwm_frame_smp_cnt = 0;

  Pico32x.vdp_regs[0x0a/2] &= ~P32XV_VBLK; // get out of vblank
  if ((Pico32x.vdp_regs[0] & P32XV_Mx) != 0) // no forced blanking
    Pico32x.vdp_regs[0x0a/2] &= ~P32XV_PEN; // no palette access

  p32x_poll_event(3, 1);

  PicoFrameStart();
  PicoFrameHints();
  elprintf(EL_32X, "poll: %02x", Pico32x.emu_flags);
}

