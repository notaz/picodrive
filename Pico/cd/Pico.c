// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "../PicoInt.h"
#include "../sound/sound.h"


int counter75hz = 0;


int PicoInitMCD(void)
{
  SekInitS68k();
  Init_CD_Driver();

  return 0;
}


void PicoExitMCD(void)
{
  End_CD_Driver();
}

int PicoResetMCD(int hard)
{
  // clear everything except BIOS
  memset(Pico_mcd->prg_ram, 0, sizeof(mcd_state) - sizeof(Pico_mcd->bios));
  PicoMCD |= 2; // s68k reset pending
  counter75hz = 0;

  LC89510_Reset();
  Reset_CD();

  return 0;
}

static __inline void SekRun(int cyc)
{
  int cyc_do;
  SekCycleAim+=cyc;
  if((cyc_do=SekCycleAim-SekCycleCnt) < 0) return;
#if defined(EMU_M68K)
  m68k_set_context(&PicoM68kCPU);
  SekCycleCnt+=m68k_execute(cyc_do);
#endif
}

static __inline void SekRunS68k(int cyc)
{
  int cyc_do;
  SekCycleAimS68k+=cyc;
  if((cyc_do=SekCycleAimS68k-SekCycleCntS68k) < 0) return;
#if defined(EMU_M68K)
  m68k_set_context(&PicoS68kCPU);
  SekCycleCntS68k+=m68k_execute(cyc_do);
#endif
}

// TODO: tidy
extern unsigned char m68k_regs[0x40];
extern unsigned char s68k_regs[0x200];

// Accurate but slower frame which does hints
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
      SekRun(128); SekCycleAim-=128; // there must be a gap between H and V ints, also after vblank bit set (Mazin Saga, Bram Stoker's Dracula)
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
    if(y == 32 && PsndOut)
      emustatus &= ~1;
    else if((y == 224 || y == line_sample) && PsndOut)
      ;//getSamples(y);

    // Run scanline:
      //dprintf("m68k starting exec @ %06x", SekPc);
    SekRun(cycles_68k);
    if ((Pico_mcd->m68k_regs[1]&3) == 1) { // no busreq/no reset
#if 0
	    int i;
	    FILE *f = fopen("prg_ram.bin", "wb");
	    for (i = 0; i < 0x80000; i+=2)
	    {
		    int tmp = Pico_mcd->prg_ram[i];
		    Pico_mcd->prg_ram[i] = Pico_mcd->prg_ram[i+1];
		    Pico_mcd->prg_ram[i+1] = tmp;
	    }
	    fwrite(Pico_mcd->prg_ram, 1, 0x80000, f);
	    fclose(f);
	    exit(1);
#endif
      //dprintf("s68k starting exec @ %06x", SekPcS68k);
      SekRunS68k(cycles_s68k);
    }

    if((PicoOpt&4) && Pico.m.z80Run) {
      Pico.m.z80Run|=2;
      z80CycleAim+=cycles_z80;
      total_z80+=z80_run(z80CycleAim-total_z80);
    }

    // if cdd is on, counter elapsed and irq4 is not masked, do irq4
    if ((Pico_mcd->s68k_regs[0x37]&4) && ++counter75hz > 209 && (Pico_mcd->s68k_regs[0x33]&(1<<4))) {
      counter75hz = 0;
      Check_CD_Command();
    }
  }

  // draw a frame just after vblank in alternative render mode
  if(!PicoSkipFrame && (PicoOpt&0x10))
    PicoFrameFull();

  return 0;
}


int PicoFrameMCD(void)
{
  if(!(PicoOpt&0x10))
    PicoFrameStart();

  PicoFrameHintsMCD();

  return 0;
}


