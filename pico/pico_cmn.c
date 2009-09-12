// common code for Pico.c and cd/Pico.c
// (c) Copyright 2007,2008 Grazvydas "notaz" Ignotas

#define CYCLES_M68K_LINE     488 // suitable for both PAL/NTSC
#define CYCLES_M68K_VINT_LAG  68
#define CYCLES_M68K_ASD      148
#define CYCLES_S68K_LINE     795
#define CYCLES_S68K_ASD      241

// pad delay (for 6 button pads)
#define PAD_DELAY \
  if (PicoOpt&POPT_6BTN_PAD) { \
    if(Pico.m.padDelay[0]++ > 25) Pico.m.padTHPhase[0]=0; \
    if(Pico.m.padDelay[1]++ > 25) Pico.m.padTHPhase[1]=0; \
  }

// CPUS_RUN
#ifndef PICO_CD
#define CPUS_RUN(m68k_cycles,s68k_cycles) \
    SekRunM68k(m68k_cycles);
#else
#define CPUS_RUN(m68k_cycles,s68k_cycles) \
{ \
    if ((PicoOpt&POPT_EN_MCD_PSYNC) && (Pico_mcd->m.busreq&3) == 1) { \
      SekRunPS(m68k_cycles, s68k_cycles); /* "better/perfect sync" */ \
    } else { \
      SekRunM68k(m68k_cycles); \
      if ((Pico_mcd->m.busreq&3) == 1) /* no busreq/no reset */ \
        SekRunS68k(s68k_cycles); \
    } \
}
#endif

static int PicoFrameHints(void)
{
  struct PicoVideo *pv=&Pico.video;
  int lines, y, lines_vis = 224, line_sample, skip, vcnt_wrap;
  int hint; // Hint counter

  pv->v_counter = Pico.m.scanline = 0;

  if ((PicoOpt&POPT_ALT_RENDERER) && !PicoSkipFrame && (pv->reg[1]&0x40)) { // fast rend., display enabled
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
    line_sample = 68;
    if (pv->reg[1]&8) lines_vis = 240;
  } else {
    line_sample = 93;
  }

  SekCyclesReset();
  z80_resetCycles();
#ifdef PICO_CD
  SekCyclesResetS68k();
#endif
  PsndDacLine = 0;
  emustatus &= ~1;

  pv->status&=~0x88; // clear V-Int, come out of vblank

  hint=pv->reg[10]; // Load H-Int counter
  //dprintf("-hint: %i", hint);

  // This is to make active scan longer (needed for Double Dragon 2, mainly)
  CPUS_RUN(CYCLES_M68K_ASD, CYCLES_S68K_ASD);

  for (y = 0; y < lines_vis; y++)
  {
    pv->v_counter = Pico.m.scanline = y;
    if ((pv->reg[12]&6) == 6) { // interlace mode 2
      pv->v_counter <<= 1;
      pv->v_counter |= pv->v_counter >> 8;
      pv->v_counter &= 0xff;
    }

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
    if (!skip && (PicoOpt & POPT_ALT_RENDERER))
    {
      // find the right moment for frame renderer, when display is no longer blanked
      if ((pv->reg[1]&0x40) || y > 100) {
        PicoFrameFull();
#ifdef DRAW_FINISH_FUNC
        DRAW_FINISH_FUNC();
#endif
        skip = 1;
      }
    }

    // get samples from sound chips
    if ((y == 224 || y == line_sample) && PsndOut)
    {
      if (Pico.m.z80Run && !Pico.m.z80_reset && (PicoOpt&POPT_EN_Z80))
        PicoSyncZ80(SekCycleCnt);
      if (ym2612.dacen && PsndDacLine <= y)
        PsndDoDAC(y);
      PsndGetSamples(y);
    }

    // Run scanline:
    if (Pico.m.dma_xfers) SekCyclesBurn(CheckDMA());
    CPUS_RUN(CYCLES_M68K_LINE, CYCLES_S68K_LINE);

#ifdef PICO_CD
    update_chips();
#else
    if (PicoLineHook) PicoLineHook();
#endif
  }

  if (!skip)
  {
    if (DrawScanline < y)
      PicoDrawSync(y - 1, 0);
#ifdef DRAW_FINISH_FUNC
    DRAW_FINISH_FUNC();
#endif
  }

  // V-int line (224 or 240)
  Pico.m.scanline = y;
  pv->v_counter = 0xe0; // bad for 240 mode
  if ((pv->reg[12]&6) == 6) pv->v_counter = 0xc1;

  // VDP FIFO
  pv->lwrite_cnt=0;
  Pico.video.status|=0x200;

  memcpy(PicoPadInt, PicoPad, sizeof(PicoPadInt));
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

  pv->status|=0x08; // go into vblank
  pv->pending_ints|=0x20;

#ifdef PICO_32X
  p32x_start_blank();
#endif

  // the following SekRun is there for several reasons:
  // there must be a delay after vblank bit is set and irq is asserted (Mazin Saga)
  // also delay between F bit (bit 7) is set in SR and IRQ happens (Ex-Mutants)
  // also delay between last H-int and V-int (Golden Axe 3)
  SekRunM68k(CYCLES_M68K_VINT_LAG);

  if (pv->reg[1]&0x20) {
    elprintf(EL_INTS, "vint: @ %06x [%i]", SekPc, SekCycleCnt);
    SekInterrupt(6);
  }
  if (Pico.m.z80Run && !Pico.m.z80_reset && (PicoOpt&POPT_EN_Z80)) {
    PicoSyncZ80(SekCycleCnt);
    elprintf(EL_INTS, "zint");
    z80_int();
  }

  // get samples from sound chips
  if (y == 224 && PsndOut)
  {
    if (ym2612.dacen && PsndDacLine <= y)
      PsndDoDAC(y);
    PsndGetSamples(y);
  }

  // Run scanline:
  if (Pico.m.dma_xfers) SekCyclesBurn(CheckDMA());
  CPUS_RUN(CYCLES_M68K_LINE - CYCLES_M68K_VINT_LAG - CYCLES_M68K_ASD,
    CYCLES_S68K_LINE - CYCLES_S68K_ASD);

#ifdef PICO_CD
  update_chips();
#else
  if (PicoLineHook) PicoLineHook();
#endif

  // PAL line count might actually be 313 according to Steve Snake, but that would complicate things.
  lines = Pico.m.pal ? 312 : 262;
  vcnt_wrap = Pico.m.pal ? 0x103 : 0xEB; // based on Gens

  for (y++; y < lines; y++)
  {
    pv->v_counter = Pico.m.scanline = y;
    if (y >= vcnt_wrap)
      pv->v_counter -= Pico.m.pal ? 56 : 6;
    if ((pv->reg[12]&6) == 6)
      pv->v_counter = (pv->v_counter << 1) | 1;
    pv->v_counter &= 0xff;

    PAD_DELAY
#ifdef PICO_CD
    check_cd_dma();
#endif

    // Run scanline:
    if (Pico.m.dma_xfers) SekCyclesBurn(CheckDMA());
    CPUS_RUN(CYCLES_M68K_LINE, CYCLES_S68K_LINE);

#ifdef PICO_CD
    update_chips();
#else
    if (PicoLineHook) PicoLineHook();
#endif
  }

  // sync z80
  if (Pico.m.z80Run && !Pico.m.z80_reset && (PicoOpt&POPT_EN_Z80))
    PicoSyncZ80(Pico.m.pal ? 151809 : 127671); // cycles adjusted for converter
  if (PsndOut && ym2612.dacen && PsndDacLine <= lines-1)
    PsndDoDAC(lines-1);

  timers_cycle();

  return 0;
}

#undef PAD_DELAY
#undef CPUS_RUN

