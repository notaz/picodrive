// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"
#include "sound/ym2612.h"

int PicoVer=0x0133;
struct Pico Pico;
int PicoOpt=0; // disable everything by default
int PicoSkipFrame=0; // skip rendering frame?
int PicoRegionOverride = 0; // override the region detection 0: Auto, 1: Japan NTSC, 2: Japan PAL, 4: US, 8: Europe
int PicoAutoRgnOrder = 0;
int emustatus = 0; // rapid_ym2612, multi_ym_updates
void (*PicoWriteSound)(int len) = 0; // called once per frame at the best time to send sound buffer (PsndOut) to hardware

struct PicoSRAM SRam = {0,};
int z80startCycle, z80stopCycle; // in 68k cycles
int PicoPad[2];  // Joypads, format is SACB RLDU
int PicoMCD = 0; // mega CD status: scd_started

// to be called once on emu init
int PicoInit(void)
{
  // Blank space for state:
  memset(&Pico,0,sizeof(Pico));
  memset(&PicoPad,0,sizeof(PicoPad));

  // Init CPUs:
  SekInit();
  z80_init(); // init even if we aren't going to use it

  PicoInitMCD();

  SRam.data=0;

  return 0;
}

// to be called once on emu exit
void PicoExit(void)
{
  if (PicoMCD&1)
    PicoExitMCD();
  z80_exit();

  if(SRam.data) free(SRam.data); SRam.data=0;
}

int PicoReset(int hard)
{
  unsigned int region=0;
  int support=0,hw=0,i=0;
  unsigned char pal=0;
  unsigned char sram_reg=Pico.m.sram_reg; // must be preserved

  if (Pico.romsize<=0) return 1;

  PicoMemReset();
  SekReset();
  // s68k doesn't have the TAS quirk, so we just globally set normal TAS handler in MCD mode (used by Batman games).
  SekSetRealTAS(PicoMCD & 1);
  SekCycleCntT=0;
  z80_reset();

  // reset VDP state, VRAM and PicoMisc
  //memset(&Pico.video,0,sizeof(Pico.video));
  //memset(&Pico.vram,0,sizeof(Pico.vram));
  memset(Pico.ioports,0,sizeof(Pico.ioports)); // needed for MCD to reset properly
  memset(&Pico.m,0,sizeof(Pico.m));
  Pico.video.pending_ints=0;
  emustatus = 0;

  if(hard) {
    // clear all memory of the emulated machine
    memset(&Pico.ram,0,(unsigned int)&Pico.rom-(unsigned int)&Pico.ram);
  }

  // default VDP register values (based on Fusion)
  Pico.video.reg[0] = Pico.video.reg[1] = 0x04;
  Pico.video.reg[0xc] = 0x81;
  Pico.video.reg[0xf] = 0x02;
  Pico.m.dirtyPal = 1;

  if(PicoRegionOverride)
  {
    support = PicoRegionOverride;
  }
  else
  {
    // Read cartridge region data:
    region=PicoRead32(0x1f0);

    for (i=0;i<4;i++)
    {
      int c=0;

      c=region>>(i<<3); c&=0xff;
      if (c<=' ') continue;

           if (c=='J')  support|=1;
      else if (c=='U')  support|=4;
      else if (c=='E')  support|=8;
      else if (c=='j') {support|=1; break; }
      else if (c=='u') {support|=4; break; }
      else if (c=='e') {support|=8; break; }
      else
      {
        // New style code:
        char s[2]={0,0};
        s[0]=(char)c;
        support|=strtol(s,NULL,16);
      }
    }
  }

  // auto detection order override
  if (PicoAutoRgnOrder) {
         if (((PicoAutoRgnOrder>>0)&0xf) & support) support = (PicoAutoRgnOrder>>0)&0xf;
    else if (((PicoAutoRgnOrder>>4)&0xf) & support) support = (PicoAutoRgnOrder>>4)&0xf;
    else if (((PicoAutoRgnOrder>>8)&0xf) & support) support = (PicoAutoRgnOrder>>8)&0xf;
  }

  // Try to pick the best hardware value for English/50hz:
       if (support&8) { hw=0xc0; pal=1; } // Europe
  else if (support&4)   hw=0x80;          // USA
  else if (support&2) { hw=0x40; pal=1; } // Japan PAL
  else if (support&1)   hw=0x00;          // Japan NTSC
  else hw=0x80; // USA

  Pico.m.hardware=(unsigned char)(hw|0x20); // No disk attached
  Pico.m.pal=pal;
  Pico.video.status = 0x3408 | pal; // always set bits | vblank | pal

  sound_reset(); // pal must be known here

  if (PicoMCD & 1) {
    PicoResetMCD(hard);
    return 0;
  }

  // reset sram state; enable sram access by default if it doesn't overlap with ROM
  Pico.m.sram_reg=sram_reg&0x14;
  if (!(Pico.m.sram_reg&4) && Pico.romsize <= SRam.start) Pico.m.sram_reg |= 1;

  elprintf(EL_STATUS, "sram: det: %i; eeprom: %i; start: %06x; end: %06x",
    (Pico.m.sram_reg>>4)&1, (Pico.m.sram_reg>>2)&1, SRam.start, SRam.end);

  return 0;
}


// dma2vram settings are just hacks to unglitch Legend of Galahad (needs <= 104 to work)
// same for Outrunners (92-121, when active is set to 24)
static const int dma_timings[] = {
83,  167, 166,  83, // vblank: 32cell: dma2vram dma2[vs|c]ram vram_fill vram_copy
102, 205, 204, 102, // vblank: 40cell:
16,   16,  15,   8, // active: 32cell:
24,   18,  17,   9  // ...
};

static const int dma_bsycles[] = {
(488<<8)/82,  (488<<8)/167, (488<<8)/166, (488<<8)/83,
(488<<8)/102, (488<<8)/205, (488<<8)/204, (488<<8)/102,
(488<<8)/16,  (488<<8)/16,  (488<<8)/15,  (488<<8)/8,
(488<<8)/24,  (488<<8)/18,  (488<<8)/17,  (488<<8)/9
};

PICO_INTERNAL int CheckDMA(void)
{
  int burn = 0, xfers_can, dma_op = Pico.video.reg[0x17]>>6; // see gens for 00 and 01 modes
  int xfers = Pico.m.dma_xfers;
  int dma_op1;

  if(!(dma_op&2)) dma_op = (Pico.video.type==1) ? 0 : 1; // setting dma_timings offset here according to Gens
  dma_op1 = dma_op;
  if(Pico.video.reg[12] & 1) dma_op |= 4; // 40 cell mode?
  if(!(Pico.video.status&8)&&(Pico.video.reg[1]&0x40)) dma_op|=8; // active display?
  xfers_can = dma_timings[dma_op];
  if(xfers <= xfers_can) {
    if(dma_op&2) Pico.video.status&=~2; // dma no longer busy
    else {
      burn = xfers * dma_bsycles[dma_op] >> 8; // have to be approximate because can't afford division..
    }
    Pico.m.dma_xfers = 0;
  } else {
    if(!(dma_op&2)) burn = 488;
    Pico.m.dma_xfers -= xfers_can;
  }

  elprintf(EL_VDPDMA, "~Dma %i op=%i can=%i burn=%i [%i]", Pico.m.dma_xfers, dma_op1, xfers_can, burn, SekCyclesDone());
  //dprintf("~aim: %i, cnt: %i", SekCycleAim, SekCycleCnt);
  return burn;
}

static __inline void SekRunM68k(int cyc)
{
  int cyc_do;
  SekCycleAim+=cyc;
  //printf("aim: %i, cnt: %i\n", SekCycleAim, SekCycleCnt);
  if((cyc_do=SekCycleAim-SekCycleCnt) <= 0) return;
  //printf("cyc_do: %i\n", cyc_do);
#if   defined(EMU_C68K) && defined(EMU_M68K)
  // this means we do run-compare Cyclone vs Musashi
  SekCycleCnt+=CM_compareRun(cyc_do);
#elif defined(EMU_C68K)
  PicoCpu.cycles=cyc_do;
  CycloneRun(&PicoCpu);
  SekCycleCnt+=cyc_do-PicoCpu.cycles;
#elif defined(EMU_A68K)
  m68k_ICount=cyc_do;
  M68000_RUN();
  SekCycleCnt+=cyc_do-m68k_ICount;
#elif defined(EMU_M68K)
  SekCycleCnt+=m68k_execute(cyc_do);
#endif
}

static __inline void SekStep(void)
{
  // this is required for timing sensitive stuff to work
  int realaim=SekCycleAim; SekCycleAim=SekCycleCnt+1;
#if   defined(EMU_C68K) && defined(EMU_M68K)
  // this means we do run-compare Cyclone vs Musashi
  SekCycleCnt+=CM_compareRun(1);
#elif defined(EMU_C68K)
  PicoCpu.cycles=1;
  CycloneRun(&PicoCpu);
  SekCycleCnt+=1-PicoCpu.cycles;
#elif defined(EMU_A68K)
  m68k_ICount=1;
  M68000_RUN();
  SekCycleCnt+=1-m68k_ICount;
#elif defined(EMU_M68K)
  SekCycleCnt+=m68k_execute(1);
#endif
  SekCycleAim=realaim;
}

static int CheckIdle(void)
{
#if 1
  unsigned char state[0x88];

  memset(state,0,sizeof(state));

  // See if the state is the same after 2 steps:
  SekState(state); SekStep(); SekStep(); SekState(state+0x44);
  if (memcmp(state,state+0x44,0x44)==0) return 1;
#else
  unsigned char state[0x44];
  static unsigned char oldstate[0x44];

  SekState(state);
  if(memcmp(state,oldstate,0x40)==0) return 1;
  memcpy(oldstate, state, 0x40);
#endif

  return 0;
}

void lprintf_al(const char *fmt, ...);

// to be called on 224 or line_sample scanlines only
static __inline void getSamples(int y)
{
  static int curr_pos = 0;

  if(y == 224) {
    if(emustatus & 2)
         curr_pos += sound_render(curr_pos, PsndLen-PsndLen/2);
    else curr_pos  = sound_render(0, PsndLen);
    if (emustatus&1) emustatus|=2; else emustatus&=~2;
    if (PicoWriteSound) PicoWriteSound(curr_pos);
    // clear sound buffer
    sound_clear();
  }
  else if(emustatus & 3) {
    emustatus|= 2;
    emustatus&=~1;
    curr_pos = sound_render(0, PsndLen/2);
  }
}


#if 1*0
int vint_delay = 205/*68*/, as_delay = 18/*148*/;

// Accurate but slower frame which does hints
static int PicoFrameHints(void)
{
  struct PicoVideo *pv=&Pico.video;
  int total_z80=0,lines,y,lines_vis = 224,z80CycleAim = 0,line_sample;
  const int cycles_68k=488,cycles_z80=228; // both PAL and NTSC compile to same values
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
  //z80ExtraCycles = 0;

  if(PicoOpt&4)
    z80CycleAim = 0;
//    z80_resetCycles();

  pv->status&=~0x88; // clear V-Int, come out of vblank

  hint=pv->reg[10]; // Load H-Int counter
  //dprintf("-hint: %i", hint);

  //SekRun(as_delay);
  SekRun(148);

  for (y=0;y<lines;y++)
  {
    Pico.m.scanline=(short)y;

    // VDP FIFO
    pv->lwrite_cnt -= 12;
    if (pv->lwrite_cnt <  0) pv->lwrite_cnt=0;
    if (pv->lwrite_cnt == 0)
      Pico.video.status|=0x200;

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
      if (pv->reg[0]&0x10) {
        elprintf(EL_INTS, "hint: @ %06x [%i]", SekPc, SekCycleCnt);
        SekInterrupt(4);
      }
      //dprintf("hint_routine: %x", (*(unsigned short*)(Pico.ram+0x0B84)<<16)|*(unsigned short*)(Pico.ram+0x0B86));
    }

    // V-Interrupt:
    if (y == lines_vis)
    {
      pv->status|=0x08; // go into vblank
      //pv->status|=0x80; // V-Int happened
      //if(!Pico.m.dma_bytes||(Pico.video.reg[0x17]&0x80)) {
        // there must be a gap between H and V ints, also after vblank bit set (Mazin Saga, Bram Stoker's Dracula)
        SekRun(68); SekCycleAim-=68; // 128; ?
        SekCycleAim-=148;
//       SekRun(vint_delay); SekCycleAim-=vint_delay; // 128; ?
//	SekCycleAim-=as_delay;
      //}
      pv->pending_ints|=0x20;
      if(pv->reg[1]&0x20) {
        elprintf(EL_INTS, "vint: @ %06x [%i]", SekPc, SekCycleCnt);
        SekInterrupt(6);
      }
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
      getSamples(y);

    // Run scanline:
    if (Pico.m.dma_xfers) SekCyclesBurn(CheckDMA());
    SekRun(cycles_68k);
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
  }

  // draw a frame just after vblank in alternative render mode
  if(!PicoSkipFrame && (PicoOpt&0x10))
    PicoFrameFull();

  return 0;
}
#else
#include "PicoFrameHints.c"
#endif

// helper z80 runner. Runs only if z80 is enabled at this point
// (z80WriteBusReq will handle the rest)
static void PicoRunZ80Simple(int line_from, int line_to)
{
  int line_from_r=line_from, line_to_r=line_to, line=0;
  int line_sample = Pico.m.pal ? 68 : 93;

  if (!(PicoOpt&4) || Pico.m.z80Run == 0) line_to_r = 0;
  else {
    extern const unsigned short vcounts[];
    if (z80startCycle) {
      line = vcounts[z80startCycle>>8];
      if (line > line_from)
        line_from_r = line;
    }
    z80startCycle = SekCyclesDone();
  }

  if (PicoOpt&1) {
    // we have ym2612 enabled, so we have to run Z80 in lines, so we could update DAC and timers
    for (line = line_from; line < line_to; line++) {
      sound_timers_and_dac(line);
      if ((line == 224 || line == line_sample) && PsndOut) getSamples(line);
      if (line == 32 && PsndOut) emustatus &= ~1;
      if (line >= line_from_r && line < line_to_r)
        z80_run(228);
    }
  } else if (line_to_r-line_from_r > 0) {
    z80_run(228*(line_to_r-line_from_r));
    // samples will be taken by caller
  }
}

// Simple frame without H-Ints
static int PicoFrameSimple(void)
{
  struct PicoVideo *pv=&Pico.video;
  int y=0,line=0,lines=0,lines_step=0,sects;
  int cycles_68k_vblock,cycles_68k_block;

  // split to 16 run calls for active scan, for vblank split to 2 (ntsc), 3 (pal 240), 4 (pal 224)
  if (Pico.m.pal && (pv->reg[1]&8)) { // 240 lines
    if(pv->reg[1]&8) { // 240 lines
      cycles_68k_block  = 7329;  // (488*240+148)/16.0, -4
      cycles_68k_vblock = 11640; // (72*488-148-68)/3.0, 0
      lines_step = 15;
    } else {
      cycles_68k_block  = 6841;  // (488*224+148)/16.0, -4
      cycles_68k_vblock = 10682; // (88*488-148-68)/4.0, 0
      lines_step = 14;
    }
  } else {
    // M68k cycles/frame: 127840.71
    cycles_68k_block  = 6841; // (488*224+148)/16.0, -4
    cycles_68k_vblock = 9164; // (38*488-148-68)/2.0, 0
    lines_step = 14;
  }

  // we don't emulate DMA timing in this mode
  if (Pico.m.dma_xfers) {
    Pico.m.dma_xfers=0;
    Pico.video.status&=~2;
  }

  // VDP FIFO too
  pv->lwrite_cnt = 0;
  Pico.video.status|=0x200;

  Pico.m.scanline=-1;
  z80startCycle=0;

  SekCyclesReset();

  // 6 button pad: let's just say it timed out now
  Pico.m.padTHPhase[0]=Pico.m.padTHPhase[1]=0;

  // ---- Active Scan ----
  pv->status&=~0x88; // clear V-Int, come out of vblank

  // Run in sections:
  for(sects=16; sects; sects--)
  {
    if (CheckIdle()) break;

    lines += lines_step;
    SekRunM68k(cycles_68k_block);

    PicoRunZ80Simple(line, lines);
    line=lines;
  }

  // run Z80 for remaining sections
  if(sects) {
    int c = sects*cycles_68k_block;

    // this "run" is for approriate line counter, etc
    SekCycleCnt += c;
    SekCycleAim += c;

    lines += sects*lines_step;
    PicoRunZ80Simple(line, lines);
  }

  // here we render sound if ym2612 is disabled
  if (!(PicoOpt&1) && PsndOut) {
    int len = sound_render(0, PsndLen);
    if (PicoWriteSound) PicoWriteSound(len);
    // clear sound buffer
    sound_clear();
  }

  // render screen
  if (!PicoSkipFrame)
  {
    if (!(PicoOpt&0x10))
      // Draw the screen
#if CAN_HANDLE_240_LINES
      if (pv->reg[1]&8) {
        for (y=0;y<240;y++) PicoLine(y);
      } else {
        for (y=0;y<224;y++) PicoLine(y);
      }
#else
      for (y=0;y<224;y++) PicoLine(y);
#endif
    else PicoFrameFull();
  }

  // a gap between flags set and vint
  pv->pending_ints|=0x20;
  pv->status|=8; // go into vblank
  SekRunM68k(68+4);

  // ---- V-Blanking period ----
  // fix line counts
  if(Pico.m.pal) {
    if(pv->reg[1]&8) { // 240 lines
      lines = line = 240;
      sects = 3;
      lines_step = 24;
    } else {
      lines = line = 224;
      sects = 4;
      lines_step = 22;
    }
  } else {
    lines = line = 224;
    sects = 2;
    lines_step = 19;
  }

  if (pv->reg[1]&0x20) SekInterrupt(6); // Set IRQ
  if (Pico.m.z80Run && (PicoOpt&4))
    z80_int();

  while (sects) {
    lines += lines_step;

    SekRunM68k(cycles_68k_vblock);

    PicoRunZ80Simple(line, lines);
    line=lines;

    sects--;
    if (sects && CheckIdle()) break;
  }

  // run Z80 for remaining sections
  if (sects) {
    lines += sects*lines_step;
    PicoRunZ80Simple(line, lines);
  }

  return 0;
}

int PicoFrame(void)
{
  int acc;

  Pico.m.frame_count++;

  if (PicoMCD & 1) {
    PicoFrameMCD();
    return 0;
  }

  // be accurate if we are asked for this
  if(PicoOpt&0x40) acc=1;
  // don't be accurate in alternative render mode, as hint effects will not be rendered anyway
  else if(PicoOpt&0x10) acc = 0;
  else acc=Pico.video.reg[0]&0x10; // be accurate if hints are used

  //if(Pico.video.reg[12]&0x2) Pico.video.status ^= 0x10; // change odd bit in interlace mode

  if(!(PicoOpt&0x10))
    PicoFrameStart();

  if(acc)
       PicoFrameHints();
  else PicoFrameSimple();

  return 0;
}

void PicoFrameDrawOnly(void)
{
  int y;
  PicoFrameStart();
  for (y=0;y<224;y++) PicoLine(y);
}

// callback to output message from emu
void (*PicoMessage)(const char *msg)=NULL;

#if 1 // defined(__DEBUG_PRINT)
// tmp debug: dump some stuff
#define bit(r, x) ((r>>x)&1)
void z80_debug(char *dstr);
char *debugString(void)
{
#if 1
  static char dstr[1024];
  struct PicoVideo *pv=&Pico.video;
  unsigned char *reg=pv->reg, r;
  char *dstrp;

  dstrp = dstr;
  sprintf(dstrp, "mode set 1: %02x\n", (r=reg[0])); dstrp+=strlen(dstrp);
  sprintf(dstrp, "display_disable: %i, M3: %i, palette: %i, ?, hints: %i\n", bit(r,0), bit(r,1), bit(r,2), bit(r,4));
  dstrp+=strlen(dstrp);
  sprintf(dstrp, "mode set 2: %02x\n", (r=reg[1])); dstrp+=strlen(dstrp);
  sprintf(dstrp, "SMS/gen: %i, pal: %i, dma: %i, vints: %i, disp: %i, TMS: %i\n", bit(r,2), bit(r,3), bit(r,4),
  	bit(r,5), bit(r,6), bit(r,7)); dstrp+=strlen(dstrp);
  sprintf(dstrp, "mode set 3: %02x\n", (r=reg[0xB])); dstrp+=strlen(dstrp);
  sprintf(dstrp, "LSCR: %i, HSCR: %i, 2cell vscroll: %i, IE2: %i\n", bit(r,0), bit(r,1), bit(r,2), bit(r,3)); dstrp+=strlen(dstrp);
  sprintf(dstrp, "mode set 4: %02x\n", (r=reg[0xC])); dstrp+=strlen(dstrp);
  sprintf(dstrp, "interlace: %i%i, cells: %i, shadow: %i\n", bit(r,2), bit(r,1), (r&0x80) ? 40 : 32,  bit(r,3));
  dstrp+=strlen(dstrp);
  sprintf(dstrp, "scroll size: w: %i, h: %i  SRAM: %i; eeprom: %i (%i)\n", reg[0x10]&3, (reg[0x10]&0x30)>>4,
  	bit(Pico.m.sram_reg, 4), bit(Pico.m.sram_reg, 2), SRam.eeprom_type); dstrp+=strlen(dstrp);
  sprintf(dstrp, "sram range: %06x-%06x, reg: %02x\n", SRam.start, SRam.end, Pico.m.sram_reg); dstrp+=strlen(dstrp);
  sprintf(dstrp, "pend int: v:%i, h:%i, vdp status: %04x\n", bit(pv->pending_ints,5), bit(pv->pending_ints,4), pv->status);
  dstrp+=strlen(dstrp);
#if defined(EMU_C68K)
  sprintf(dstrp, "M68k: PC: %06x, st_flg: %x, cycles: %u\n", SekPc, PicoCpu.state_flags, SekCyclesDoneT());
  dstrp+=strlen(dstrp);
  sprintf(dstrp, "d0=%08x, a0=%08x, osp=%08x, irql=%i\n", PicoCpu.d[0], PicoCpu.a[0], PicoCpu.osp, PicoCpu.irq); dstrp+=strlen(dstrp);
  sprintf(dstrp, "d1=%08x, a1=%08x,  sr=%04x\n", PicoCpu.d[1], PicoCpu.a[1], CycloneGetSr(&PicoCpu)); dstrp+=strlen(dstrp);
  for(r=2; r < 8; r++) {
    sprintf(dstrp, "d%i=%08x, a%i=%08x\n", r, PicoCpu.d[r], r, PicoCpu.a[r]); dstrp+=strlen(dstrp);
  }
#elif defined(EMU_M68K)
  sprintf(dstrp, "M68k: PC: %06x, cycles: %u, irql: %i\n", SekPc, SekCyclesDoneT(), PicoM68kCPU.int_level>>8); dstrp+=strlen(dstrp);
#endif
  sprintf(dstrp, "z80Run: %i, pal: %i, frame#: %i\n", Pico.m.z80Run, Pico.m.pal, Pico.m.frame_count); dstrp+=strlen(dstrp);
  z80_debug(dstrp); dstrp+=strlen(dstrp);
  if (strlen(dstr) > sizeof(dstr))
    printf("warning: debug buffer overflow (%i/%i)\n", strlen(dstr), sizeof(dstr));

#else
  struct PicoVideo *pvid=&Pico.video;
  int table=0;
  int i,u,n,link=0;
  static char dstr[1024*8];
  dstr[0] = 0;

  table=pvid->reg[5]&0x7f;
  if (pvid->reg[12]&1) table&=0x7e; // Lowest bit 0 in 40-cell mode
  table<<=8; // Get sprite table address/2

  for (i=u=n=0; u < 80 && n < 20; u++)
  {
    unsigned int *sprite;
    int code, code2, sx, sy, height;

    sprite=(unsigned int *)(Pico.vram+((table+(link<<2))&0x7ffc)); // Find sprite

    // get sprite info
    code = sprite[0];

    // check if it is on this line
    sy = (code&0x1ff);//-0x80;
    height = ((code>>24)&3)+1;

    // masking sprite?
    code2 = sprite[1];
    sx = (code2>>16)&0x1ff;

    printf("#%02i x: %03i y: %03i %ix%i\n", u, sx, sy, ((code>>26)&3)+1, height);

    link=(code>>16)&0x7f;
    if(!link) break; // End of sprites
  }
#endif

#if 0
  {
    FILE *f = fopen("zram", "wb");
    fwrite(Pico.zram, 1, 0x2000, f);
    fclose(f);
  }
#endif

  return dstr;
}
#endif
