// common code for Pico.c and cd/Pico.c
// (c) Copyright 2007, Grazvydas "notaz" Ignotas

#define CYCLES_M68K_LINE     488 // suitable for both PAL/NTSC
#define CYCLES_M68K_VINT_LAG  68
#define CYCLES_M68K_ASD      148
#define CYCLES_Z80_LINE      228
#define CYCLES_Z80_ASD        69
#define CYCLES_S68K_LINE     795
#define CYCLES_S68K_ASD      241

// pad delay (for 6 button pads)
#define PAD_DELAY \
  if (PicoOpt&0x20) { \
    if(Pico.m.padDelay[0]++ > 25) Pico.m.padTHPhase[0]=0; \
    if(Pico.m.padDelay[1]++ > 25) Pico.m.padTHPhase[1]=0; \
  }

#define Z80_RUN(z80_cycles) \
{ \
  if ((PicoOpt&4) && Pico.m.z80Run) \
  { \
    int cnt; \
    if (Pico.m.z80Run & 2) z80CycleAim += z80_cycles; \
    else { \
      cnt = SekCyclesDone() - z80startCycle; \
      cnt = (cnt>>1)-(cnt>>5); \
      if (cnt < 0 || cnt > (z80_cycles)) cnt = z80_cycles; \
      Pico.m.z80Run |= 2; \
      z80CycleAim+=cnt; \
    } \
    cnt=z80CycleAim-total_z80; \
    if (cnt > 0) total_z80+=z80_run(cnt); \
  } \
}

// CPUS_RUN
#ifndef PICO_CD
#define CPUS_RUN(m68k_cycles,z80_cycles,s68k_cycles) \
    SekRunM68k(m68k_cycles); \
    Z80_RUN(z80_cycles);
#else
#define CPUS_RUN(m68k_cycles,z80_cycles,s68k_cycles) \
{ \
    if ((PicoOpt & 0x2000) && (Pico_mcd->m.busreq&3) == 1) { \
      SekRunPS(m68k_cycles, s68k_cycles); /* "better/perfect sync" */ \
    } else { \
      SekRunM68k(m68k_cycles); \
      if ((Pico_mcd->m.busreq&3) == 1) /* no busreq/no reset */ \
        SekRunS68k(s68k_cycles); \
    } \
    Z80_RUN(z80_cycles); \
}
#endif

// Accurate but slower frame which does hints
static int PicoFrameHints(void)
{
  struct PicoVideo *pv=&Pico.video;
  int lines, y, lines_vis = 224, total_z80 = 0, z80CycleAim = 0, line_sample, skip;
  int hint; // Hint counter

  if ((PicoOpt&0x10) && !PicoSkipFrame) {
    // draw a frame just after vblank in alternative render mode
    // yes, this will cause 1 frame lag, but this is inaccurate mode anyway.
    PicoFrameFull();
#ifdef DRAW_FINISH_FUNC
    DRAW_FINISH_FUNC();
#endif
    skip = 1;
  }
  else skip=PicoSkipFrame;

  if (Pico.m.pal) {
    //cycles_68k = (int) ((double) OSC_PAL  /  7 / 50 / 312 + 0.4); // should compile to a constant (488)
    //cycles_z80 = (int) ((double) OSC_PAL  / 15 / 50 / 312 + 0.4); // 228
    line_sample = 68;
    if(pv->reg[1]&8) lines_vis = 240;
  } else {
    //cycles_68k = (int) ((double) OSC_NTSC /  7 / 60 / 262 + 0.4); // 488
    //cycles_z80 = (int) ((double) OSC_NTSC / 15 / 60 / 262 + 0.4); // 228
    line_sample = 93;
  }

  SekCyclesReset();
#ifdef PICO_CD
  SekCyclesResetS68k();
#endif

  pv->status&=~0x88; // clear V-Int, come out of vblank

  hint=pv->reg[10]; // Load H-Int counter
  //dprintf("-hint: %i", hint);

  // This is to make active scan longer (needed for Double Dragon 2, mainly)
  // also trying to adjust for z80 overclock here (due to int line cycle counts)
  z80CycleAim = Pico.m.pal ? -40 : 7;
  CPUS_RUN(CYCLES_M68K_ASD, 0, CYCLES_S68K_ASD);

  for (y=0;y<lines_vis;y++)
  {
    Pico.m.scanline=(short)y;

    // VDP FIFO
    pv->lwrite_cnt -= 12;
    if (pv->lwrite_cnt <= 0) {
      pv->lwrite_cnt=0;
      Pico.video.status|=0x200;
    }

    PAD_DELAY
#ifdef PICO_CD
    check_cd_dma();
#endif

    // H-Interrupts:
    if (--hint < 0) // y <= lines_vis: Comix Zone, Golden Axe
    {
      hint=pv->reg[10]; // Reload H-Int counter
      pv->pending_ints|=0x10;
      if (pv->reg[0]&0x10) {
        elprintf(EL_INTS, "hint: @ %06x [%i]", SekPc, SekCycleCnt);
        SekInterrupt(4);
      }
    }

    // decide if we draw this line
#if CAN_HANDLE_240_LINES
    if(!skip && ((!(pv->reg[1]&8) && y<224) || (pv->reg[1]&8)) )
#else
    if(!skip && y<224)
#endif
      PicoLine(y);

    if(PicoOpt&1)
      Psnd_timers_and_dac(y);

#ifndef PICO_CD
    // get samples from sound chips
    if(y == 32 && PsndOut)
      emustatus &= ~1;
    else if((y == 224 || y == line_sample) && PsndOut)
      getSamples(y);
#endif

    // Run scanline:
    if (Pico.m.dma_xfers) SekCyclesBurn(CheckDMA());
    CPUS_RUN(CYCLES_M68K_LINE, CYCLES_Z80_LINE, CYCLES_S68K_LINE);

#ifdef PICO_CD
    update_chips();
#endif
  }

#ifdef DRAW_FINISH_FUNC
  if (!skip)
    DRAW_FINISH_FUNC();
#endif

  // V-int line (224 or 240)
  Pico.m.scanline=(short)y;

  // VDP FIFO
  pv->lwrite_cnt=0;
  Pico.video.status|=0x200;

  PAD_DELAY
#ifdef PICO_CD
  check_cd_dma();
#endif

  // Last H-Int:
  if (--hint < 0)
  {
    hint=pv->reg[10]; // Reload H-Int counter
    pv->pending_ints|=0x10;
    //printf("rhint: %i @ %06x [%i|%i]\n", hint, SekPc, y, SekCycleCnt);
    if (pv->reg[0]&0x10) SekInterrupt(4);
  }

  // V-Interrupt:
  pv->status|=0x08; // go into vblank
  pv->pending_ints|=0x20;

  // the following SekRun is there for several reasons:
  // there must be a delay after vblank bit is set and irq is asserted (Mazin Saga)
  // also delay between F bit (bit 7) is set in SR and IRQ happens (Ex-Mutants)
  // also delay between last H-int and V-int (Golden Axe 3)
  SekRunM68k(CYCLES_M68K_VINT_LAG);
  if (pv->reg[1]&0x20) {
    elprintf(EL_INTS, "vint: @ %06x [%i]", SekPc, SekCycleCnt);
    SekInterrupt(6);
  }
  if (Pico.m.z80Run && (PicoOpt&4))
    z80_int();

  if (PicoOpt&1)
    Psnd_timers_and_dac(y);

  // get samples from sound chips
#ifndef PICO_CD
  if (y == 224)
#endif
    if (PsndOut)
      getSamples(y);

  // Run scanline:
  if (Pico.m.dma_xfers) SekCyclesBurn(CheckDMA());
  CPUS_RUN(CYCLES_M68K_LINE - CYCLES_M68K_VINT_LAG - CYCLES_M68K_ASD,
    CYCLES_Z80_LINE - CYCLES_Z80_ASD, CYCLES_S68K_LINE - CYCLES_S68K_ASD);

#ifdef PICO_CD
    update_chips();
#endif

  // PAL line count might actually be 313 according to Steve Snake, but that would complicate things.
  lines = Pico.m.pal ? 312 : 262;

  for (y++;y<lines;y++)
  {
    Pico.m.scanline=(short)y;

    PAD_DELAY
#ifdef PICO_CD
    check_cd_dma();
#endif

    if(PicoOpt&1)
      Psnd_timers_and_dac(y);

    // Run scanline:
    if (Pico.m.dma_xfers) SekCyclesBurn(CheckDMA());
    CPUS_RUN(CYCLES_M68K_LINE, CYCLES_Z80_LINE, CYCLES_S68K_LINE);

#ifdef PICO_CD
    update_chips();
#endif
  }

  return 0;
}

#undef PAD_DELAY
#undef Z80_RUN
#undef CPUS_RUN

