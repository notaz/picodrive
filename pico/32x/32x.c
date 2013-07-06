/*
 * PicoDrive
 * (C) notaz, 2009,2010
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */
#include "../pico_int.h"
#include "../sound/ym2612.h"

struct Pico32x Pico32x;
SH2 sh2s[2];

static int REGPARM(2) sh2_irq_cb(SH2 *sh2, int level)
{
  if (sh2->pending_irl > sh2->pending_int_irq) {
    elprintf(EL_32X, "%csh2 ack/irl %d @ %08x",
      sh2->is_slave ? 's' : 'm', level, sh2->pc);
    return 64 + sh2->pending_irl / 2;
  } else {
    elprintf(EL_32X, "%csh2 ack/int %d/%d @ %08x",
      sh2->is_slave ? 's' : 'm', level, sh2->pending_int_vector, sh2->pc);
    sh2->pending_int_irq = 0; // auto-clear
    sh2->pending_level = sh2->pending_irl;
    return sh2->pending_int_vector;
  }
}

void p32x_update_irls(int nested_call)
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
  sh2_irl_irq(&msh2, mlvl, nested_call);
  sh2_irl_irq(&ssh2, slvl, nested_call);
  mlvl = mlvl ? 1 : 0;
  slvl = slvl ? 1 : 0;
  p32x_poll_event(mlvl | (slvl << 1), 0);
}

void Pico32xStartup(void)
{
  elprintf(EL_STATUS|EL_32X, "32X startup");

  // TODO: OOM handling
  PicoAHW |= PAHW_32X;
  sh2_init(&msh2, 0);
  msh2.irq_callback = sh2_irq_cb;
  sh2_init(&ssh2, 1);
  ssh2.irq_callback = sh2_irq_cb;

  PicoMemSetup32x();

  if (!Pico.m.pal)
    Pico32x.vdp_regs[0] |= P32XV_nPAL;

  PREG8(Pico32xMem->sh2_peri_regs[0], 4) =
  PREG8(Pico32xMem->sh2_peri_regs[1], 4) = 0x84; // SCI SSR

  rendstatus_old = -1;

  emu_32x_startup();
}

#define HWSWAP(x) (((x) << 16) | ((x) >> 16))
void p32x_reset_sh2s(void)
{
  elprintf(EL_32X, "sh2 reset");

  sh2_reset(&msh2);
  sh2_reset(&ssh2);

  // if we don't have BIOS set, perform it's work here.
  // MSH2
  if (p32x_bios_m == NULL) {
    unsigned int idl_src, idl_dst, idl_size; // initial data load
    unsigned int vbr;

    // initial data
    idl_src = HWSWAP(*(unsigned int *)(Pico.rom + 0x3d4)) & ~0xf0000000;
    idl_dst = HWSWAP(*(unsigned int *)(Pico.rom + 0x3d8)) & ~0xf0000000;
    idl_size= HWSWAP(*(unsigned int *)(Pico.rom + 0x3dc));
    if (idl_size > Pico.romsize || idl_src + idl_size > Pico.romsize ||
        idl_size > 0x40000 || idl_dst + idl_size > 0x40000 || (idl_src & 3) || (idl_dst & 3)) {
      elprintf(EL_STATUS|EL_ANOMALY, "32x: invalid initial data ptrs: %06x -> %06x, %06x",
        idl_src, idl_dst, idl_size);
    }
    else
      memcpy(Pico32xMem->sdram + idl_dst, Pico.rom + idl_src, idl_size);

    // GBR/VBR
    vbr = HWSWAP(*(unsigned int *)(Pico.rom + 0x3e8));
    sh2_set_gbr(0, 0x20004000);
    sh2_set_vbr(0, vbr);

    // checksum and M_OK
    Pico32x.regs[0x28 / 2] = *(unsigned short *)(Pico.rom + 0x18e);
    // program will set M_OK
  }

  // SSH2
  if (p32x_bios_s == NULL) {
    unsigned int vbr;

    // GBR/VBR
    vbr = HWSWAP(*(unsigned int *)(Pico.rom + 0x3ec));
    sh2_set_gbr(1, 0x20004000);
    sh2_set_vbr(1, vbr);
    // program will set S_OK
  }

  msh2.m68krcycles_done = ssh2.m68krcycles_done = SekCyclesDoneT();
}

void Pico32xInit(void)
{
  if (msh2.mult_m68k_to_sh2 == 0 || msh2.mult_sh2_to_m68k == 0)
    Pico32xSetClocks(PICO_MSH2_HZ, 0);
  if (ssh2.mult_m68k_to_sh2 == 0 || ssh2.mult_sh2_to_m68k == 0)
    Pico32xSetClocks(0, PICO_MSH2_HZ);
}

void PicoPower32x(void)
{
  memset(&Pico32x, 0, sizeof(Pico32x));

  Pico32x.regs[0] = P32XS_REN|P32XS_nRES; // verified
  Pico32x.vdp_regs[0x0a/2] = P32XV_VBLK|P32XV_HBLK|P32XV_PEN;
  Pico32x.sh2_regs[0] = P32XS2_ADEN;
}

void PicoUnload32x(void)
{
  if (Pico32xMem != NULL)
    plat_munmap(Pico32xMem, sizeof(*Pico32xMem));
  Pico32xMem = NULL;
  sh2_finish(&msh2);
  sh2_finish(&ssh2);

  PicoAHW &= ~PAHW_32X;
}

void PicoReset32x(void)
{
  if (PicoAHW & PAHW_32X) {
    Pico32x.sh2irqs |= P32XI_VRES;
    p32x_update_irls(0);
    p32x_poll_event(3, 0);
  }
}

static void p32x_start_blank(void)
{
  if (Pico32xDrawMode != PDM32X_OFF && !PicoSkipFrame) {
    int offs, lines;

    pprof_start(draw);

    offs = 8; lines = 224;
    if ((Pico.video.reg[1] & 8) && !(PicoOpt & POPT_ALT_RENDERER)) {
      offs = 0;
      lines = 240;
    }

    // XXX: no proper handling of 32col mode..
    if ((Pico32x.vdp_regs[0] & P32XV_Mx) != 0 && // 32x not blanking
        (Pico.video.reg[12] & 1) && // 40col mode
        (PicoDrawMask & PDRAW_32X_ON))
    {
      int md_bg = Pico.video.reg[7] & 0x3f;

      // we draw full layer (not line-by-line)
      PicoDraw32xLayer(offs, lines, md_bg);
    }
    else if (Pico32xDrawMode != PDM32X_32X_ONLY)
      PicoDraw32xLayerMdOnly(offs, lines);

    pprof_end(draw);
  }

  // enter vblank
  Pico32x.vdp_regs[0x0a/2] |= P32XV_VBLK|P32XV_PEN;

  // FB swap waits until vblank
  if ((Pico32x.vdp_regs[0x0a/2] ^ Pico32x.pending_fb) & P32XV_FS) {
    Pico32x.vdp_regs[0x0a/2] &= ~P32XV_FS;
    Pico32x.vdp_regs[0x0a/2] |= Pico32x.pending_fb;
    Pico32xSwapDRAM(Pico32x.pending_fb ^ 1);
  }

  Pico32x.sh2irqs |= P32XI_VINT;
  p32x_update_irls(0);
  p32x_poll_event(3, 1);
}

#define sync_sh2s_normal p32x_sync_sh2s
//#define sync_sh2s_lockstep p32x_sync_sh2s

void sync_sh2s_normal(unsigned int m68k_target)
{
  unsigned int target = m68k_target;
  int msh2_cycles, ssh2_cycles;
  int done;

  elprintf(EL_32X, "sh2 sync to %u (%u)", m68k_target, SekCycleCnt);

  if (!(Pico32x.regs[0] & P32XS_nRES))
    return; // rare

  {
    msh2_cycles = C_M68K_TO_SH2(msh2, target - msh2.m68krcycles_done);
    ssh2_cycles = C_M68K_TO_SH2(ssh2, target - ssh2.m68krcycles_done);

    while (msh2_cycles > 0 || ssh2_cycles > 0) {
      elprintf(EL_32X, "sh2 exec %u,%u->%u",
        msh2.m68krcycles_done, ssh2.m68krcycles_done, target);

      if (Pico32x.emu_flags & (P32XF_SSH2POLL|P32XF_SSH2VPOLL)) {
        ssh2.m68krcycles_done = target;
        ssh2_cycles = 0;
      }
      else if (ssh2_cycles > 0) {
        done = sh2_execute(&ssh2, ssh2_cycles);
        ssh2.m68krcycles_done += C_SH2_TO_M68K(ssh2, done);

        ssh2_cycles = C_M68K_TO_SH2(ssh2, target - ssh2.m68krcycles_done);
      }

      if (Pico32x.emu_flags & (P32XF_MSH2POLL|P32XF_MSH2VPOLL)) {
        msh2.m68krcycles_done = target;
        msh2_cycles = 0;
      }
      else if (msh2_cycles > 0) {
        done = sh2_execute(&msh2, msh2_cycles);
        msh2.m68krcycles_done += C_SH2_TO_M68K(msh2, done);

        msh2_cycles = C_M68K_TO_SH2(msh2, target - msh2.m68krcycles_done);
      }
    }
  }
}

#define STEP_68K 24

void sync_sh2s_lockstep(unsigned int m68k_target)
{
  unsigned int mcycles;
  
  mcycles = msh2.m68krcycles_done;
  if (ssh2.m68krcycles_done < mcycles)
    mcycles = ssh2.m68krcycles_done;

  while (mcycles < m68k_target) {
    mcycles += STEP_68K;
    sync_sh2s_normal(mcycles);
  }
}

#define CPUS_RUN(m68k_cycles,s68k_cycles) do { \
  SekRunM68k(m68k_cycles); \
  if (SekIsStoppedM68k()) \
    p32x_sync_sh2s(SekCycleCntT + SekCycleCnt); \
} while (0)

#define PICO_32X
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

// calculate multipliers against 68k clock (7670442)
// normally * 3, but effectively slower due to high latencies everywhere
// however using something lower breaks MK2 animations
void Pico32xSetClocks(int msh2_hz, int ssh2_hz)
{
  float m68k_clk = (float)(OSC_NTSC / 7);
  if (msh2_hz > 0) {
    msh2.mult_m68k_to_sh2 = (int)((float)msh2_hz * (1 << CYCLE_MULT_SHIFT) / m68k_clk);
    msh2.mult_sh2_to_m68k = (int)(m68k_clk * (1 << CYCLE_MULT_SHIFT) / (float)msh2_hz);
  }
  if (ssh2_hz > 0) {
    ssh2.mult_m68k_to_sh2 = (int)((float)ssh2_hz * (1 << CYCLE_MULT_SHIFT) / m68k_clk);
    ssh2.mult_sh2_to_m68k = (int)(m68k_clk * (1 << CYCLE_MULT_SHIFT) / (float)ssh2_hz);
  }
}

// vim:shiftwidth=2:ts=2:expandtab
