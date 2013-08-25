/*
 * PicoDrive
 * (C) notaz, 2007,2013
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */

#include "../pico_int.h"
#include "../sound/ym2612.h"

extern unsigned char formatted_bram[4*0x10];

static unsigned int m68k_cycle_mult;

void (*PicoMCDopenTray)(void) = NULL;
void (*PicoMCDcloseTray)(void) = NULL;


PICO_INTERNAL void PicoInitMCD(void)
{
  SekInitS68k();
  Init_CD_Driver();
}

PICO_INTERNAL void PicoExitMCD(void)
{
  End_CD_Driver();
}

PICO_INTERNAL void PicoPowerMCD(void)
{
  int fmt_size = sizeof(formatted_bram);
  memset(Pico_mcd->prg_ram,    0, sizeof(Pico_mcd->prg_ram));
  memset(Pico_mcd->word_ram2M, 0, sizeof(Pico_mcd->word_ram2M));
  memset(Pico_mcd->pcm_ram,    0, sizeof(Pico_mcd->pcm_ram));
  memset(Pico_mcd->bram, 0, sizeof(Pico_mcd->bram));
  memcpy(Pico_mcd->bram + sizeof(Pico_mcd->bram) - fmt_size, formatted_bram, fmt_size);
}

PICO_INTERNAL int PicoResetMCD(void)
{
  memset(Pico_mcd->s68k_regs, 0, sizeof(Pico_mcd->s68k_regs));
  memset(&Pico_mcd->pcm, 0, sizeof(Pico_mcd->pcm));
  memset(&Pico_mcd->m, 0, sizeof(Pico_mcd->m));

  memset(Pico_mcd->bios + 0x70, 0xff, 4); // reset hint vector (simplest way to implement reg6)
  Pico_mcd->m.state_flags |= 1; // s68k reset pending
  Pico_mcd->s68k_regs[3] = 1; // 2M word RAM mode with m68k access after reset

  Reset_CD();
  LC89510_Reset();
  gfx_cd_reset();
#ifdef _ASM_CD_MEMORY_C
  //PicoMemResetCDdecode(1); // don't have to call this in 2M mode
#endif

  // use SRam.data for RAM cart
  if (PicoOpt & POPT_EN_MCD_RAMCART) {
    if (SRam.data == NULL)
      SRam.data = calloc(1, 0x12000);
  }
  else if (SRam.data != NULL) {
    free(SRam.data);
    SRam.data = NULL;
  }
  SRam.start = SRam.end = 0; // unused

  pcd_event_schedule(0, PCD_EVENT_CDC, 12500000/75);

  return 0;
}

static __inline void SekRunS68k(unsigned int to)
{
  int cyc_do;

  SekCycleAimS68k = to;
  if ((cyc_do = SekCycleAimS68k - SekCycleCntS68k) <= 0)
    return;

  SekCycleCntS68k += cyc_do;
#if defined(EMU_C68K)
  PicoCpuCS68k.cycles = cyc_do;
  CycloneRun(&PicoCpuCS68k);
  SekCycleCntS68k -= PicoCpuCS68k.cycles;
#elif defined(EMU_M68K)
  m68k_set_context(&PicoCpuMS68k);
  SekCycleCntS68k += m68k_execute(cyc_do) - cyc_do;
  m68k_set_context(&PicoCpuMM68k);
#elif defined(EMU_F68K)
  g_m68kcontext = &PicoCpuFS68k;
  SekCycleCntS68k += fm68k_emulate(cyc_do, 0, 0) - cyc_do;
  g_m68kcontext = &PicoCpuFM68k;
#endif
}


unsigned int pcd_cycles_m68k_to_s68k(unsigned int c)
{
  return (long long)c * m68k_cycle_mult >> 16;
}

/* events */
static void pcd_cdc_event(unsigned int now)
{
  // 75Hz CDC update
  Check_CD_Command();
  pcd_event_schedule(now, PCD_EVENT_CDC, 12500000/75);
}

static void pcd_int3_timer_event(unsigned int now)
{
  if (Pico_mcd->s68k_regs[0x33] & PCDS_IEN3) {
    elprintf(EL_INTS|EL_CD, "s68k: timer irq 3");
    SekInterruptS68k(3);
  }

  if (Pico_mcd->s68k_regs[0x31] != 0)
    pcd_event_schedule(now, PCD_EVENT_TIMER3,
      Pico_mcd->s68k_regs[0x31] * 384);
}

static void pcd_gfx_event(unsigned int now)
{
  // update gfx chip
  if (Pico_mcd->rot_comp.Reg_58 & 0x8000) {
    Pico_mcd->rot_comp.Reg_58 &= 0x7fff;
    Pico_mcd->rot_comp.Reg_64  = 0;
    if (Pico_mcd->s68k_regs[0x33] & PCDS_IEN1) {
      elprintf(EL_INTS  |EL_CD, "s68k: gfx_cd irq 1");
      SekInterruptS68k(1);
    }
  }
}

static void pcd_dma_event(unsigned int now)
{
  int ddx = Pico_mcd->s68k_regs[4] & 7;
	Update_CDC_TRansfer(ddx);
}

typedef void (event_cb)(unsigned int now);

/* times are in s68k (12.5MHz) cycles */
unsigned int pcd_event_times[PCD_EVENT_COUNT];
static unsigned int event_time_next;
static event_cb *pcd_event_cbs[PCD_EVENT_COUNT] = {
  [PCD_EVENT_CDC]      = pcd_cdc_event,
  [PCD_EVENT_TIMER3]   = pcd_int3_timer_event,
  [PCD_EVENT_GFX]      = pcd_gfx_event,
  [PCD_EVENT_DMA]      = pcd_dma_event,
};

void pcd_event_schedule(unsigned int now, enum pcd_event event, int after)
{
  unsigned int when;

  when = now + after;
  if (when == 0) {
    // event cancelled
    pcd_event_times[event] = 0;
    return;
  }

  when |= 1;

  elprintf(EL_CD, "cd: new event #%u %u->%u", event, now, when);
  pcd_event_times[event] = when;

  if (event_time_next == 0 || CYCLES_GT(event_time_next, when))
    event_time_next = when;
}

void pcd_event_schedule_s68k(enum pcd_event event, int after)
{
  if (SekCyclesLeftS68k > after)
    SekEndRunS68k(after);

  pcd_event_schedule(SekCyclesDoneS68k(), event, after);
}

static void pcd_run_events(unsigned int until)
{
  int oldest, oldest_diff, time;
  int i, diff;

  while (1) {
    oldest = -1, oldest_diff = 0x7fffffff;

    for (i = 0; i < PCD_EVENT_COUNT; i++) {
      if (pcd_event_times[i]) {
        diff = pcd_event_times[i] - until;
        if (diff < oldest_diff) {
          oldest_diff = diff;
          oldest = i;
        }
      }
    }

    if (oldest_diff <= 0) {
      time = pcd_event_times[oldest];
      pcd_event_times[oldest] = 0;
      elprintf(EL_CD, "cd: run event #%d %u", oldest, time);
      pcd_event_cbs[oldest](time);
    }
    else if (oldest_diff < 0x7fffffff) {
      event_time_next = pcd_event_times[oldest];
      break;
    }
    else {
      event_time_next = 0;
      break;
    }
  }

  if (oldest != -1)
    elprintf(EL_CD, "cd: next event #%d at %u",
      oldest, event_time_next);
}

static void pcd_sync_s68k(unsigned int m68k_target)
{
  #define now SekCycleCntS68k
  unsigned int s68k_target =
    (unsigned long long)m68k_target * m68k_cycle_mult >> 16;
  unsigned int target;

  elprintf(EL_CD, "s68k sync to %u/%u", m68k_target, s68k_target);

  if ((Pico_mcd->m.busreq & 3) != 1) { /* busreq/reset */
    SekCycleCntS68k = SekCycleAimS68k = s68k_target;
    pcd_run_events(m68k_target);
    return;
  }

  while (CYCLES_GT(s68k_target, now)) {
    if (event_time_next && CYCLES_GE(now, event_time_next))
      pcd_run_events(now);

    target = s68k_target;
    if (event_time_next && CYCLES_GT(target, event_time_next))
      target = event_time_next;

    SekRunS68k(target);
  }
  #undef now
}

#define PICO_CD
#define CPUS_RUN(m68k_cycles) \
  SekRunM68k(m68k_cycles)

#include "../pico_cmn.c"


PICO_INTERNAL void PicoFrameMCD(void)
{
  if (!(PicoOpt&POPT_ALT_RENDERER))
    PicoFrameStart();

  // ~1.63 for NTSC, ~1.645 for PAL
  if (Pico.m.pal)
    m68k_cycle_mult = ((12500000ull << 16) / (50*312*488));
  else
    m68k_cycle_mult = ((12500000ull << 16) / (60*262*488)) + 1;

  PicoFrameHints();
}

void pcd_state_loaded(void)
{
  unsigned int cycles;
  int diff;

  pcd_state_loaded_mem();

  // old savestates..
  cycles = pcd_cycles_m68k_to_s68k(SekCycleAim);
  diff = cycles - SekCycleAimS68k;
  if (diff < -1000 || diff > 1000) {
    SekCycleCntS68k = SekCycleAimS68k = cycles;
  }
  if (pcd_event_times[PCD_EVENT_CDC] == 0) {
    pcd_event_schedule(SekCycleAimS68k, PCD_EVENT_CDC, 12500000/75);

    if (Pico_mcd->s68k_regs[0x31])
      pcd_event_schedule(SekCycleAimS68k, PCD_EVENT_TIMER3,
        Pico_mcd->s68k_regs[0x31] * 384);

    if (Pico_mcd->rot_comp.Reg_58 & 0x8000) {
      Pico_mcd->rot_comp.Reg_58 &= 0x7fff;
      Pico_mcd->rot_comp.Reg_64  = 0;
      if (Pico_mcd->s68k_regs[0x33] & PCDS_IEN1)
        SekInterruptS68k(1);
    }
    if (Pico_mcd->scd.Status_CDC & 0x08)
	    Update_CDC_TRansfer(Pico_mcd->s68k_regs[4] & 7);
  }
}

// vim:shiftwidth=2:ts=2:expandtab
