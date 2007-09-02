// (c) Copyright 2007 notaz, All rights reserved.


#include "../PicoInt.h"


extern unsigned char formatted_bram[4*0x10];
extern unsigned int s68k_poll_adclk;

void (*PicoMCDopenTray)(void) = NULL;
int  (*PicoMCDcloseTray)(void) = NULL;

#define dump_ram(ram,fname) \
{ \
  int i, d; \
  FILE *f; \
\
  for (i = 0; i < sizeof(ram); i+=2) { \
    d = (ram[i]<<8) | ram[i+1]; \
    *(unsigned short *)(ram+i) = d; \
  } \
  f = fopen(fname, "wb"); \
  if (f) { \
    fwrite(ram, 1, sizeof(ram), f); \
    fclose(f); \
  } \
  for (i = 0; i < sizeof(ram); i+=2) { \
    d = (ram[i]<<8) | ram[i+1]; \
    *(unsigned short *)(ram+i) = d; \
  } \
}


PICO_INTERNAL int PicoInitMCD(void)
{
  SekInitS68k();
  Init_CD_Driver();

  return 0;
}


PICO_INTERNAL void PicoExitMCD(void)
{
  End_CD_Driver();

  //dump_ram(Pico_mcd->prg_ram, "prg.bin");
  //dump_ram(Pico.ram, "ram.bin");
}

PICO_INTERNAL int PicoResetMCD(int hard)
{
  if (hard) {
    int fmt_size = sizeof(formatted_bram);
    memset(Pico_mcd->prg_ram,    0, sizeof(Pico_mcd->prg_ram));
    memset(Pico_mcd->word_ram2M, 0, sizeof(Pico_mcd->word_ram2M));
    memset(Pico_mcd->pcm_ram,    0, sizeof(Pico_mcd->pcm_ram));
    memset(Pico_mcd->bram, 0, sizeof(Pico_mcd->bram));
    memcpy(Pico_mcd->bram + sizeof(Pico_mcd->bram) - fmt_size, formatted_bram, fmt_size);
  }
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
  PicoMemResetCD(1);
  //PicoMemResetCDdecode(1); // don't have to call this in 2M mode
#endif

  // use SRam.data for RAM cart
  if (SRam.data) free(SRam.data);
  SRam.data = NULL;
  if (PicoOpt&0x8000)
    SRam.data = calloc(1, 0x12000);

  return 0;
}

static __inline void SekRunM68k(int cyc)
{
  int cyc_do;
  SekCycleAim+=cyc;
  if((cyc_do=SekCycleAim-SekCycleCnt) < 0) return;
#if defined(EMU_C68K)
  PicoCpu.cycles=cyc_do;
  CycloneRun(&PicoCpu);
  SekCycleCnt+=cyc_do-PicoCpu.cycles;
#elif defined(EMU_M68K)
  m68k_set_context(&PicoM68kCPU);
  SekCycleCnt+=m68k_execute(cyc_do);
#endif
}

static __inline void SekRunS68k(int cyc)
{
  int cyc_do;
  SekCycleAimS68k+=cyc;
  if((cyc_do=SekCycleAimS68k-SekCycleCntS68k) < 0) return;
#if defined(EMU_C68K)
  PicoCpuS68k.cycles=cyc_do;
  CycloneRun(&PicoCpuS68k);
  SekCycleCntS68k+=cyc_do-PicoCpuS68k.cycles;
#elif defined(EMU_M68K)
  m68k_set_context(&PicoS68kCPU);
  SekCycleCntS68k+=m68k_execute(cyc_do);
#endif
}

#define PS_STEP_M68K ((488<<16)/20) // ~24
//#define PS_STEP_S68K 13

#ifdef _ASM_CD_PICO_C
void SekRunPS(int cyc_m68k, int cyc_s68k);
#else
static __inline void SekRunPS(int cyc_m68k, int cyc_s68k)
{
  int cycn, cycn_s68k, cyc_do;
  int ex;
  SekCycleAim+=cyc_m68k;
  SekCycleAimS68k+=cyc_s68k;

//  fprintf(stderr, "=== start %3i/%3i [%3i/%3i] {%05i.%i} ===\n", cyc_m68k, cyc_s68k,
//  		SekCycleAim-SekCycleCnt, SekCycleAimS68k-SekCycleCntS68k, Pico.m.frame_count, Pico.m.scanline);

  /* loop 488 downto 0 in steps of PS_STEP */
  for (cycn = (488<<16)-PS_STEP_M68K; cycn >= 0; cycn -= PS_STEP_M68K)
  {
    ex = 0;
    cycn_s68k = (cycn + cycn/2 + cycn/8) >> 16;
    if ((cyc_do = SekCycleAim-SekCycleCnt-(cycn>>16)) > 0) {
#if defined(EMU_C68K)
      PicoCpu.cycles = cyc_do;
      CycloneRun(&PicoCpu);
      SekCycleCnt += cyc_do - PicoCpu.cycles;
#elif defined(EMU_M68K)
      m68k_set_context(&PicoM68kCPU);
      SekCycleCnt += (ex = m68k_execute(cyc_do));
#endif
    }
    if ((cyc_do = SekCycleAimS68k-SekCycleCntS68k-cycn_s68k) > 0) {
#if defined(EMU_C68K)
      PicoCpuS68k.cycles = cyc_do;
      CycloneRun(&PicoCpuS68k);
      SekCycleCntS68k += cyc_do - PicoCpuS68k.cycles;
#elif defined(EMU_M68K)
      m68k_set_context(&PicoS68kCPU);
      SekCycleCntS68k += (ex = m68k_execute(cyc_do));
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
				dprintf("s68k: timer irq 3");
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

	// delayed setting of DMNA bit (needed for Silpheed)
	if (Pico_mcd->m.state_flags & 2) {
		Pico_mcd->m.state_flags &= ~2;
		if (!(Pico_mcd->s68k_regs[3] & 4)) {
			Pico_mcd->s68k_regs[3] |=  2;
			Pico_mcd->s68k_regs[3] &= ~1;
#ifdef USE_POLL_DETECT
			if ((s68k_poll_adclk&0xfe) == 2) {
				SekSetStopS68k(0); s68k_poll_adclk = 0;
			}
#endif
		}
	}
}


static int PicoFrameHintsMCD(void)
{
  struct PicoVideo *pv=&Pico.video;
  int total_z80=0,lines,y,lines_vis = 224,z80CycleAim = 0,line_sample;
  const int cycles_68k=488,cycles_z80=228,cycles_s68k=795; // both PAL and NTSC compile to same values
  int skip=PicoSkipFrame || (PicoOpt&0x10);
  int hint; // Hint counter

  if(Pico.m.pal) { //
    //cycles_68k = (int) ((double) OSC_PAL  /  7 / 50 / 312 + 0.4); // should compile to a constant (488)
    //cycles_z80 = (int) ((double) OSC_PAL  / 15 / 50 / 312 + 0.4); // 228
    lines  = 312;    // Steve Snake says there are 313 lines, but this seems to also work well
    line_sample = 68;
    if(pv->reg[1]&8) lines_vis = 240;
  } else {
    //cycles_68k = (int) ((double) OSC_NTSC /  7 / 60 / 262 + 0.4); // 488
    //cycles_z80 = (int) ((double) OSC_NTSC / 15 / 60 / 262 + 0.4); // 228
    lines  = 262;
    line_sample = 93;
  }

  SekCyclesReset();
  SekCyclesResetS68k();
  //z80ExtraCycles = 0;

  if(PicoOpt&4)
    z80CycleAim = 0;
//    z80_resetCycles();

  pv->status&=~0x88; // clear V-Int, come out of vblank

  hint=pv->reg[10]; // Load H-Int counter
  //dprintf("-hint: %i", hint);

  for (y=0;y<lines;y++)
  {
    Pico.m.scanline=(short)y;

    // pad delay (for 6 button pads)
    if(PicoOpt&0x20) {
      if(Pico.m.padDelay[0]++ > 25) Pico.m.padTHPhase[0]=0;
      if(Pico.m.padDelay[1]++ > 25) Pico.m.padTHPhase[1]=0;
    }

    check_cd_dma();

    // H-Interrupts:
    if(y <= lines_vis && --hint < 0) // y <= lines_vis: Comix Zone, Golden Axe
    {
      //dprintf("rhint:old @ %06x", SekPc);
      hint=pv->reg[10]; // Reload H-Int counter
      pv->pending_ints|=0x10;
      if (pv->reg[0]&0x10) SekInterrupt(4);
      //dprintf("rhint: %i @ %06x [%i|%i]", hint, SekPc, y, SekCycleCnt);
      //dprintf("hint_routine: %x", (*(unsigned short*)(Pico.ram+0x0B84)<<16)|*(unsigned short*)(Pico.ram+0x0B86));
    }

    // V-Interrupt:
    if (y == lines_vis)
    {
      //dprintf("vint: @ %06x [%i|%i]", SekPc, y, SekCycleCnt);
      pv->status|=0x88; // V-Int happened, go into vblank
      SekRunM68k(128); SekCycleAim-=128; // there must be a gap between H and V ints, also after vblank bit set (Mazin Saga, Bram Stoker's Dracula)
      /*if(Pico.m.z80Run && (PicoOpt&4)) {
        z80CycleAim+=cycles_z80/2;
        total_z80+=z80_run(z80CycleAim-total_z80);
        z80CycleAim-=cycles_z80/2;
      }*/
      pv->pending_ints|=0x20;
      if(pv->reg[1]&0x20) SekInterrupt(6);
      if(Pico.m.z80Run && (PicoOpt&4)) // ?
        z80_int();
      //dprintf("zint: [%i|%i] zPC=%04x", Pico.m.scanline, SekCyclesDone(), mz80GetRegisterValue(NULL, 0));
    }

    // decide if we draw this line
#if CAN_HANDLE_240_LINES
    if(!skip && ((!(pv->reg[1]&8) && y<224) || ((pv->reg[1]&8) && y<240)) )
#else
    if(!skip && y<224)
#endif
      PicoLine(y);

    if(PicoOpt&1)
      sound_timers_and_dac(y);

    // get samples from sound chips
    if (y == 224 && PsndOut) {
      int len = sound_render(0, PsndLen);
      if (PicoWriteSound) PicoWriteSound(len);
      // clear sound buffer
      sound_clear();
    }

    // Run scanline:
      //dprintf("m68k starting exec @ %06x", SekPc);
    if (Pico.m.dma_bytes) SekCycleCnt+=CheckDMA();
    if ((PicoOpt & 0x2000) && (Pico_mcd->m.busreq&3) == 1) {
      SekRunPS(cycles_68k, cycles_s68k); // "better/perfect sync"
    } else {
      SekRunM68k(cycles_68k);
      if ((Pico_mcd->m.busreq&3) == 1) // no busreq/no reset
        SekRunS68k(cycles_s68k);
    }

    if ((PicoOpt&4) && Pico.m.z80Run) {
      if (Pico.m.z80Run & 2) z80CycleAim+=cycles_z80;
      else {
        int cnt = SekCyclesDone() - z80startCycle;
        cnt = (cnt>>1)-(cnt>>5);
        //if (cnt > cycles_z80) printf("FIXME: z80 cycles: %i\n", cnt);
        if (cnt > cycles_z80) cnt = cycles_z80;
        Pico.m.z80Run |= 2;
        z80CycleAim+=cnt;
      }
      total_z80+=z80_run(z80CycleAim-total_z80);
    }

    update_chips();
  }

  // draw a frame just after vblank in alternative render mode
  if (!PicoSkipFrame && (PicoOpt&0x10))
    PicoFrameFull();

  return 0;
}


PICO_INTERNAL int PicoFrameMCD(void)
{
  if(!(PicoOpt&0x10))
    PicoFrameStart();

  PicoFrameHintsMCD();

  return 0;
}


