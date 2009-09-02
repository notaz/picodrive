// (c) Copyright 2007 notaz, All rights reserved.


#include "../pico_int.h"
#include "../sound/ym2612.h"

extern unsigned char formatted_bram[4*0x10];
extern unsigned int s68k_poll_adclk;

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
  PicoMemRemapCD(1);
}

PICO_INTERNAL int PicoResetMCD(void)
{
  memset(Pico_mcd->s68k_regs, 0, sizeof(Pico_mcd->s68k_regs));
  memset(&Pico_mcd->pcm, 0, sizeof(Pico_mcd->pcm));
  memset(&Pico_mcd->m, 0, sizeof(Pico_mcd->m));

  *(unsigned int *)(Pico_mcd->bios + 0x70) = 0xffffffff; // reset hint vector (simplest way to implement reg6)
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

  return 0;
}

static __inline void SekRunM68k(int cyc)
{
  int cyc_do;
  SekCycleAim+=cyc;
  if ((cyc_do=SekCycleAim-SekCycleCnt) <= 0) return;
#if defined(EMU_CORE_DEBUG)
  SekCycleCnt+=CM_compareRun(cyc_do, 0);
#elif defined(EMU_C68K)
  PicoCpuCM68k.cycles=cyc_do;
  CycloneRun(&PicoCpuCM68k);
  SekCycleCnt+=cyc_do-PicoCpuCM68k.cycles;
#elif defined(EMU_M68K)
  m68k_set_context(&PicoCpuMM68k);
  SekCycleCnt+=m68k_execute(cyc_do);
#elif defined(EMU_F68K)
  g_m68kcontext=&PicoCpuFM68k;
  SekCycleCnt+=fm68k_emulate(cyc_do, 0, 0);
#endif
}

static __inline void SekRunS68k(int cyc)
{
  int cyc_do;
  SekCycleAimS68k+=cyc;
  if ((cyc_do=SekCycleAimS68k-SekCycleCntS68k) <= 0) return;
#if defined(EMU_CORE_DEBUG)
  SekCycleCntS68k+=CM_compareRun(cyc_do, 1);
#elif defined(EMU_C68K)
  PicoCpuCS68k.cycles=cyc_do;
  CycloneRun(&PicoCpuCS68k);
  SekCycleCntS68k+=cyc_do-PicoCpuCS68k.cycles;
#elif defined(EMU_M68K)
  m68k_set_context(&PicoCpuMS68k);
  SekCycleCntS68k+=m68k_execute(cyc_do);
#elif defined(EMU_F68K)
  g_m68kcontext=&PicoCpuFS68k;
  SekCycleCntS68k+=fm68k_emulate(cyc_do, 0, 0);
#endif
}

#define PS_STEP_M68K ((488<<16)/20) // ~24
//#define PS_STEP_S68K 13

#if defined(_ASM_CD_PICO_C)
extern void SekRunPS(int cyc_m68k, int cyc_s68k);
#elif defined(EMU_F68K)
static __inline void SekRunPS(int cyc_m68k, int cyc_s68k)
{
  SekCycleAim+=cyc_m68k;
  SekCycleAimS68k+=cyc_s68k;
  fm68k_emulate(0, 1, 0);
}
#else
static __inline void SekRunPS(int cyc_m68k, int cyc_s68k)
{
  int cycn, cycn_s68k, cyc_do;
  SekCycleAim+=cyc_m68k;
  SekCycleAimS68k+=cyc_s68k;

//  fprintf(stderr, "=== start %3i/%3i [%3i/%3i] {%05i.%i} ===\n", cyc_m68k, cyc_s68k,
//  		SekCycleAim-SekCycleCnt, SekCycleAimS68k-SekCycleCntS68k, Pico.m.frame_count, Pico.m.scanline);

  /* loop 488 downto 0 in steps of PS_STEP */
  for (cycn = (488<<16)-PS_STEP_M68K; cycn >= 0; cycn -= PS_STEP_M68K)
  {
    cycn_s68k = (cycn + cycn/2 + cycn/8) >> 16;
    if ((cyc_do = SekCycleAim-SekCycleCnt-(cycn>>16)) > 0) {
#if defined(EMU_C68K)
      PicoCpuCM68k.cycles = cyc_do;
      CycloneRun(&PicoCpuCM68k);
      SekCycleCnt += cyc_do - PicoCpuCM68k.cycles;
#elif defined(EMU_M68K)
      m68k_set_context(&PicoCpuMM68k);
      SekCycleCnt += m68k_execute(cyc_do);
#elif defined(EMU_F68K)
      g_m68kcontext = &PicoCpuFM68k;
      SekCycleCnt += fm68k_emulate(cyc_do, 0, 0);
#endif
    }
    if ((cyc_do = SekCycleAimS68k-SekCycleCntS68k-cycn_s68k) > 0) {
#if defined(EMU_C68K)
      PicoCpuCS68k.cycles = cyc_do;
      CycloneRun(&PicoCpuCS68k);
      SekCycleCntS68k += cyc_do - PicoCpuCS68k.cycles;
#elif defined(EMU_M68K)
      m68k_set_context(&PicoCpuMS68k);
      SekCycleCntS68k += m68k_execute(cyc_do);
#elif defined(EMU_F68K)
      g_m68kcontext = &PicoCpuFS68k;
      SekCycleCntS68k += fm68k_emulate(cyc_do, 0, 0);
#endif
    }
  }
}
#endif


static __inline void check_cd_dma(void)
{
	int ddx;

	if (!(Pico_mcd->scd.Status_CDC & 0x08)) return;

	ddx = Pico_mcd->s68k_regs[4] & 7;
	if (ddx <  2) return; // invalid
	if (ddx <  4) {
		Pico_mcd->s68k_regs[4] |= 0x40; // Data set ready in host port
		return;
	}
	if (ddx == 6) return; // invalid

	Update_CDC_TRansfer(ddx); // now go and do the actual transfer
}

static __inline void update_chips(void)
{
	int counter_timer, int3_set;
	int counter75hz_lim = Pico.m.pal ? 2080 : 2096;

	// 75Hz CDC update
	if ((Pico_mcd->m.counter75hz+=10) >= counter75hz_lim) {
		Pico_mcd->m.counter75hz -= counter75hz_lim;
		Check_CD_Command();
	}

	// update timers
	counter_timer = Pico.m.pal ? 0x21630 : 0x2121c; // 136752 : 135708;
	Pico_mcd->m.timer_stopwatch += counter_timer;
	if ((int3_set = Pico_mcd->s68k_regs[0x31])) {
		Pico_mcd->m.timer_int3 -= counter_timer;
		if (Pico_mcd->m.timer_int3 < 0) {
			if (Pico_mcd->s68k_regs[0x33] & (1<<3)) {
				elprintf(EL_INTS, "s68k: timer irq 3");
				SekInterruptS68k(3);
				Pico_mcd->m.timer_int3 += int3_set << 16;
			}
			// is this really what happens if irq3 is masked out?
			Pico_mcd->m.timer_int3 &= 0xffffff;
		}
	}

	// update gfx chip
	if (Pico_mcd->rot_comp.Reg_58 & 0x8000)
		gfx_cd_update();
}


#define PICO_CD
#include "../pico_cmn.c"


PICO_INTERNAL void PicoFrameMCD(void)
{
  if (!(PicoOpt&POPT_ALT_RENDERER))
    PicoFrameStart();

  PicoFrameHints();
}


