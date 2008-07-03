// PicoDrive

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006-2008 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"
#include "sound/ym2612.h"

int PicoVer=0x0133;
struct Pico Pico;
int PicoOpt = 0;
int PicoSkipFrame = 0; // skip rendering frame?
int emustatus = 0;     // rapid_ym2612, multi_ym_updates
int PicoPad[2];        // Joypads, format is SACB RLDU
int PicoAHW = 0;       // active addon hardware: scd_active, 32x_active, svp_active, pico_active
int PicoRegionOverride = 0; // override the region detection 0: Auto, 1: Japan NTSC, 2: Japan PAL, 4: US, 8: Europe
int PicoAutoRgnOrder = 0;
struct PicoSRAM SRam = {0,};

void (*PicoWriteSound)(int len) = NULL; // called at the best time to send sound buffer (PsndOut) to hardware
void (*PicoResetHook)(void) = NULL;
void (*PicoLineHook)(int count) = NULL;

// to be called once on emu init
void PicoInit(void)
{
  // Blank space for state:
  memset(&Pico,0,sizeof(Pico));
  memset(&PicoPad,0,sizeof(PicoPad));

  // Init CPUs:
  SekInit();
  z80_init(); // init even if we aren't going to use it

  PicoInitMCD();
  PicoSVPInit();

  SRam.data=0;
}

// to be called once on emu exit
void PicoExit(void)
{
  if (PicoAHW & PAHW_MCD)
    PicoExitMCD();
  z80_exit();

  if (SRam.data) free(SRam.data); SRam.data=0;
}

void PicoPower(void)
{
  unsigned char sram_reg=Pico.m.sram_reg; // must be preserved

  Pico.m.frame_count = 0;

  // clear all memory of the emulated machine
  memset(&Pico.ram,0,(unsigned int)&Pico.rom-(unsigned int)&Pico.ram);

  memset(&Pico.video,0,sizeof(Pico.video));
  memset(&Pico.m,0,sizeof(Pico.m));

  Pico.video.pending_ints=0;
  z80_reset();

  // default VDP register values (based on Fusion)
  Pico.video.reg[0] = Pico.video.reg[1] = 0x04;
  Pico.video.reg[0xc] = 0x81;
  Pico.video.reg[0xf] = 0x02;

  if (PicoAHW & PAHW_MCD)
    PicoPowerMCD();

  Pico.m.sram_reg=sram_reg;
  PicoReset();
}

PICO_INTERNAL void PicoDetectRegion(void)
{
  int support=0, hw=0, i;
  unsigned char pal=0;

  if (PicoRegionOverride)
  {
    support = PicoRegionOverride;
  }
  else
  {
    // Read cartridge region data:
    int region=PicoRead32(0x1f0);

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
}

int PicoReset(void)
{
  unsigned char sram_reg=Pico.m.sram_reg; // must be preserved

  if (Pico.romsize<=0) return 1;

  /* must call now, so that banking is reset, and correct vectors get fetched */
  if (PicoResetHook) PicoResetHook();

  PicoMemReset();
  SekReset();
  // s68k doesn't have the TAS quirk, so we just globally set normal TAS handler in MCD mode (used by Batman games).
  SekSetRealTAS(PicoAHW & PAHW_MCD);
  SekCycleCntT=0;

  if (PicoAHW & PAHW_MCD)
    // needed for MCD to reset properly, probably some bug hides behind this..
    memset(Pico.ioports,0,sizeof(Pico.ioports));
  emustatus = 0;

  Pico.m.dirtyPal = 1;

  Pico.m.z80_bank68k = 0;
  memset(Pico.zram, 0, sizeof(Pico.zram)); // ??

  PicoDetectRegion();
  Pico.video.status = 0x3428 | Pico.m.pal; // 'always set' bits | vblank | collision | pal

  PsndReset(); // pal must be known here

  // create an empty "dma" to cause 68k exec start at random frame location
  if (Pico.m.dma_xfers == 0 && !(PicoOpt&POPT_DIS_VDP_FIFO))
    Pico.m.dma_xfers = rand() & 0x1fff;

  if (PicoAHW & PAHW_MCD) {
    PicoResetMCD();
    return 0;
  }
  else {
    // reinit, so that checksum checks pass
    SekFinishIdleDet();
    if (!(PicoOpt & POPT_DIS_IDLE_DET))
      SekInitIdleDet();
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
// 96 is VR hack
static const int dma_timings[] = {
  96,  167, 166,  83, // vblank: 32cell: dma2vram dma2[vs|c]ram vram_fill vram_copy
  102, 205, 204, 102, // vblank: 40cell:
  16,   16,  15,   8, // active: 32cell:
  24,   18,  17,   9  // ...
};

static const int dma_bsycles[] = {
  (488<<8)/96,  (488<<8)/167, (488<<8)/166, (488<<8)/83,
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
  if(xfers <= xfers_can)
  {
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
  if ((cyc_do=SekCycleAim-SekCycleCnt) <= 0) return;
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
  SekCycleCnt+=fm68k_emulate(cyc_do+1, 0);
#endif
}


// to be called on 224 or line_sample scanlines only
static __inline void getSamples(int y)
{
#if SIMPLE_WRITE_SOUND
  if (y != 224) return;
  PsndRender(0, PsndLen);
  if (PicoWriteSound) PicoWriteSound(PsndLen);
  PsndClear();
#else
  static int curr_pos = 0;

  if(y == 224) {
    if(emustatus & 2)
         curr_pos += PsndRender(curr_pos, PsndLen-PsndLen/2);
    else curr_pos  = PsndRender(0, PsndLen);
    if (emustatus&1) emustatus|=2; else emustatus&=~2;
    if (PicoWriteSound) PicoWriteSound(curr_pos);
    // clear sound buffer
    PsndClear();
  }
  else if(emustatus & 3) {
    emustatus|= 2;
    emustatus&=~1;
    curr_pos = PsndRender(0, PsndLen/2);
  }
#endif
}


#include "PicoFrameHints.c"


int z80stopCycle;
int z80_cycle_cnt;        /* 'done' z80 cycles before z80_run() */
int z80_cycle_aim;
int z80_scanline;
int z80_scanline_cycles;  /* cycles done until z80_scanline */

/* sync z80 to 68k */
PICO_INTERNAL void PicoSyncZ80(int m68k_cycles_done)
{
  int cnt;
  z80_cycle_aim = cycles_68k_to_z80(m68k_cycles_done);
  cnt = z80_cycle_aim - z80_cycle_cnt;

  elprintf(EL_BUSREQ, "z80 sync %i (%i|%i -> %i|%i)", cnt, z80_cycle_cnt, z80_cycle_cnt / 228,
    z80_cycle_aim, z80_cycle_aim / 228);

  if (cnt > 0)
    z80_cycle_cnt += z80_run(cnt);
}


// TODO: rm from asm too
int idle_hit_counter = 0;

void PicoFrame(void)
{
#if 0
  if ((Pico.m.frame_count&0x3f) == 0) {
    elprintf(EL_STATUS, "ihits: %i", idle_hit_counter);
    idle_hit_counter = 0;
  }
#endif

  Pico.m.frame_count++;

  if (PicoAHW & PAHW_MCD) {
    PicoFrameMCD();
    return;
  }

  //if(Pico.video.reg[12]&0x2) Pico.video.status ^= 0x10; // change odd bit in interlace mode

  if (!(PicoOpt&POPT_ALT_RENDERER))
    PicoFrameStart();

  PicoFrameHints();
}

void PicoFrameDrawOnly(void)
{
  PicoFrameStart();
  PicoDrawSync(223, 0);
}

void PicoGetInternal(pint_t which, pint_ret_t *r)
{
  switch (which)
  {
    case PI_ROM:         r->vptr = Pico.rom; break;
    case PI_ISPAL:       r->vint = Pico.m.pal; break;
    case PI_IS40_CELL:   r->vint = Pico.video.reg[12]&1; break;
    case PI_IS240_LINES: r->vint = Pico.m.pal && (Pico.video.reg[1]&8); break;
  }
}

// callback to output message from emu
void (*PicoMessage)(const char *msg)=NULL;

#if 1 // defined(__DEBUG_PRINT)
#define bit(r, x) ((r>>x)&1)
void z80_debug(char *dstr);
char *debugString(void)
{
#if 1
  static char dstr[1024];
  struct PicoVideo *pv=&Pico.video;
  unsigned char *reg=pv->reg, r;
  extern int HighPreSpr[];
  int i, sprites_lo, sprites_hi;
  char *dstrp;

  sprites_lo = sprites_hi = 0;
  for (i = 0; HighPreSpr[i] != 0; i+=2)
    if (HighPreSpr[i+1] & 0x8000)
         sprites_hi++;
    else sprites_lo++;

  dstrp = dstr;
  sprintf(dstrp, "mode set 1: %02x       spr lo[%c]: %2i, spr hi[%c]: %2i\n", (r=reg[0]),
    rendstatus&PDRAW_HAVE_LO_SPR?'y':'n', sprites_lo, rendstatus&PDRAW_HAVE_HI_SPR?'y':'n', sprites_hi);
  dstrp+=strlen(dstrp);
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
  sprintf(dstrp, "M68k: PC: %06x, st_flg: %x, cycles: %u\n", SekPc, PicoCpuCM68k.state_flags, SekCyclesDoneT());
  dstrp+=strlen(dstrp);
  sprintf(dstrp, "d0=%08x, a0=%08x, osp=%08x, irql=%i\n", PicoCpuCM68k.d[0], PicoCpuCM68k.a[0], PicoCpuCM68k.osp, PicoCpuCM68k.irq); dstrp+=strlen(dstrp);
  sprintf(dstrp, "d1=%08x, a1=%08x,  sr=%04x\n", PicoCpuCM68k.d[1], PicoCpuCM68k.a[1], CycloneGetSr(&PicoCpuCM68k)); dstrp+=strlen(dstrp);
  for(r=2; r < 8; r++) {
    sprintf(dstrp, "d%i=%08x, a%i=%08x\n", r, PicoCpuCM68k.d[r], r, PicoCpuCM68k.a[r]); dstrp+=strlen(dstrp);
  }
#elif defined(EMU_M68K)
  sprintf(dstrp, "M68k: PC: %06x, cycles: %u, irql: %i\n", SekPc, SekCyclesDoneT(), PicoCpuMM68k.int_level>>8); dstrp+=strlen(dstrp);
#elif defined(EMU_F68K)
  sprintf(dstrp, "M68k: PC: %06x, cycles: %u, irql: %i\n", SekPc, SekCyclesDoneT(), PicoCpuFM68k.interrupts[0]); dstrp+=strlen(dstrp);
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

  return dstr;
}
#endif
