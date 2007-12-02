// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006,2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"

#include "sound/ym2612.h"
#include "sound/sn76496.h"

#ifndef UTYPES_DEFINED
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
#define UTYPES_DEFINED
#endif

extern unsigned int lastSSRamWrite; // used by serial SRAM code

#ifdef _ASM_MEMORY_C
u32  PicoRead8(u32 a);
u32  PicoRead16(u32 a);
void PicoWrite8(u32 a,u8 d);
void PicoWriteRomHW_SSF2(u32 a,u32 d);
#endif


#ifdef EMU_CORE_DEBUG
u32 lastread_a, lastread_d[16]={0,}, lastwrite_cyc_d[16]={0,}, lastwrite_mus_d[16]={0,};
int lrp_cyc=0, lrp_mus=0, lwp_cyc=0, lwp_mus=0;
extern unsigned int ppop;
#endif

#ifdef IO_STATS
void log_io(unsigned int addr, int bits, int rw);
#else
#define log_io(...)
#endif

#if defined(EMU_C68K)
static __inline int PicoMemBase(u32 pc)
{
  int membase=0;

  if (pc<Pico.romsize+4)
  {
    membase=(int)Pico.rom; // Program Counter in Rom
  }
  else if ((pc&0xe00000)==0xe00000)
  {
    membase=(int)Pico.ram-(pc&0xff0000); // Program Counter in Ram
  }
  else
  {
    // Error - Program Counter is invalid
    membase=(int)Pico.rom;
  }

  return membase;
}
#endif


static u32 PicoCheckPc(u32 pc)
{
  u32 ret=0;
#if defined(EMU_C68K)
  pc-=PicoCpuCM68k.membase; // Get real pc
//  pc&=0xfffffe;
  pc&=~1;
  if ((pc<<8) == 0)
  {
    printf("%i:%03i: game crash detected @ %06x\n", Pico.m.frame_count, Pico.m.scanline, SekPc);
    return (int)Pico.rom + Pico.romsize; // common crash condition, can happen if acc timing is off
  }

  PicoCpuCM68k.membase=PicoMemBase(pc&0x00ffffff);
  PicoCpuCM68k.membase-=pc&0xff000000;

  ret = PicoCpuCM68k.membase+pc;
#endif
  return ret;
}


PICO_INTERNAL int PicoInitPc(u32 pc)
{
  PicoCheckPc(pc);
  return 0;
}

#ifndef _ASM_MEMORY_C
PICO_INTERNAL_ASM void PicoMemReset(void)
{
}
#endif

// -----------------------------------------------------------------

int PadRead(int i)
{
  int pad,value,data_reg;
  pad=~PicoPad[i]; // Get inverse of pad MXYZ SACB RLDU
  data_reg=Pico.ioports[i+1];

  // orr the bits, which are set as output
  value = data_reg&(Pico.ioports[i+4]|0x80);

  if(PicoOpt & 0x20) { // 6 button gamepad enabled
    int phase = Pico.m.padTHPhase[i];

    if(phase == 2 && !(data_reg&0x40)) { // TH
      value|=(pad&0xc0)>>2;              // ?0SA 0000
      return value;
    } else if(phase == 3) {
      if(data_reg&0x40)
        value|=(pad&0x30)|((pad>>8)&0xf);  // ?1CB MXYZ
      else
        value|=((pad&0xc0)>>2)|0x0f;       // ?0SA 1111
      return value;
    }
  }

  if(data_reg&0x40) // TH
       value|=(pad&0x3f);              // ?1CB RLDU
  else value|=((pad&0xc0)>>2)|(pad&3); // ?0SA 00DU

  return value; // will mirror later
}


#ifndef _ASM_MEMORY_C
static
#endif
u32 SRAMRead(u32 a)
{
  unsigned int sreg = Pico.m.sram_reg;
  if (!(sreg & 0x10) && (sreg & 1) && a > 0x200001) { // not yet detected SRAM
    elprintf(EL_SRAMIO, "normal sram detected.");
    Pico.m.sram_reg|=0x10; // should be normal SRAM
  }
  if (sreg & 4) // EEPROM read
    return SRAMReadEEPROM();
  else // if(sreg & 1) // (sreg&5) is one of prerequisites
    return *(u8 *)(SRam.data-SRam.start+a);
}

#ifndef _ASM_MEMORY_C
static
#endif
u32 SRAMRead16(u32 a)
{
  u32 d;
  if (Pico.m.sram_reg & 4) {
    d = SRAMReadEEPROM();
    d |= d << 8;
  } else {
    u8 *pm=(u8 *)(SRam.data-SRam.start+a);
    d =*pm++ << 8;
    d|=*pm++;
  }
  return d;
}

static void SRAMWrite(u32 a, u32 d)
{
  unsigned int sreg = Pico.m.sram_reg;
  if(!(sreg & 0x10)) {
    // not detected SRAM
    if((a&~1)==0x200000) {
      elprintf(EL_SRAMIO, "eeprom detected.");
      sreg|=4; // this should be a game with EEPROM (like NBA Jam)
      SRam.start=0x200000; SRam.end=SRam.start+1;
    } else
      elprintf(EL_SRAMIO, "normal sram detected.");
    sreg|=0x10;
    Pico.m.sram_reg=sreg;
  }
  if(sreg & 4) { // EEPROM write
    // this diff must be at most 16 for NBA Jam to work
    if(SekCyclesDoneT()-lastSSRamWrite < 16) {
      // just update pending state
      elprintf(EL_EEPROM, "eeprom: skip because cycles=%i", SekCyclesDoneT()-lastSSRamWrite);
      SRAMUpdPending(a, d);
    } else {
      int old=sreg;
      SRAMWriteEEPROM(sreg>>6); // execute pending
      SRAMUpdPending(a, d);
      if ((old^Pico.m.sram_reg)&0xc0) // update time only if SDA/SCL changed
        lastSSRamWrite = SekCyclesDoneT();
    }
  } else if(!(sreg & 2)) {
    u8 *pm=(u8 *)(SRam.data-SRam.start+a);
    if(*pm != (u8)d) {
      SRam.changed = 1;
      *pm=(u8)d;
    }
  }
}

// for nonstandard reads
#ifndef _ASM_MEMORY_C
static
#endif
u32 OtherRead16End(u32 a, int realsize)
{
  u32 d=0;

  // for games with simple protection devices, discovered by Haze
  // some dumb detection is used, but that should be enough to make things work
  if ((a>>22) == 1 && Pico.romsize >= 512*1024) {
    if      (*(int *)(Pico.rom+0x123e4) == 0x00550c39 && *(int *)(Pico.rom+0x123e8) == 0x00000040) { // Super Bubble Bobble (Unl) [!]
      if      (a == 0x400000) { d=0x55<<8; goto end; }
      else if (a == 0x400002) { d=0x0f<<8; goto end; }
    }
    else if (*(int *)(Pico.rom+0x008c4) == 0x66240055 && *(int *)(Pico.rom+0x008c8) == 0x00404df9) { // Smart Mouse (Unl)
      if      (a == 0x400000) { d=0x55<<8; goto end; }
      else if (a == 0x400002) { d=0x0f<<8; goto end; }
      else if (a == 0x400004) { d=0xaa<<8; goto end; }
      else if (a == 0x400006) { d=0xf0<<8; goto end; }
    }
    else if (*(int *)(Pico.rom+0x00404) == 0x00a90600 && *(int *)(Pico.rom+0x00408) == 0x6708b013) { // King of Fighters '98, The (Unl) [!]
      if      (a == 0x480000 || a == 0x4800e0 || a == 0x4824a0 || a == 0x488880) { d=0xaa<<8; goto end; }
      else if (a == 0x4a8820) { d=0x0a<<8; goto end; }
      // there is also a read @ 0x4F8820 which needs 0, but that is returned in default case
    }
    else if (*(int *)(Pico.rom+0x01b24) == 0x004013f9 && *(int *)(Pico.rom+0x01b28) == 0x00ff0000) { // Mahjong Lover (Unl) [!]
      if      (a == 0x400000) { d=0x90<<8; goto end; }
      else if (a == 0x401000) { d=0xd3<<8; goto end; } // this one doesn't seem to be needed, the code does 2 comparisons and only then
                                                       // checks the result, which is of the above one. Left it just in case.
    }
    else if (*(int *)(Pico.rom+0x05254) == 0x0c3962d0 && *(int *)(Pico.rom+0x05258) == 0x00400055) { // Elf Wor (Unl)
      if      (a == 0x400000) { d=0x55<<8; goto end; }
      else if (a == 0x400004) { d=0xc9<<8; goto end; } // this check is done if the above one fails
      else if (a == 0x400002) { d=0x0f<<8; goto end; }
      else if (a == 0x400006) { d=0x18<<8; goto end; } // similar to above
    }
    // our default behaviour is to return whatever was last written a 0x400000-0x7fffff range (used by Squirrel King (R) [!])
    // Lion King II, The (Unl) [!]  writes @ 400000 and wants to get that val @ 400002 and wites another val
    // @ 400004 which is expected @ 400006, so we really remember 2 values here
    d = Pico.m.prot_bytes[(a>>2)&1]<<8;
  }
  else if (a == 0xa13000 && Pico.romsize >= 1024*1024) {
    if      (*(int *)(Pico.rom+0xc8af0) == 0x30133013 && *(int *)(Pico.rom+0xc8af4) == 0x000f0240) { // Rockman X3 (Unl) [!]
      d=0x0c; goto end;
    }
    else if (*(int *)(Pico.rom+0x28888) == 0x07fc0000 && *(int *)(Pico.rom+0x2888c) == 0x4eb94e75) { // Bug's Life, A (Unl) [!]
      d=0x28; goto end; // does the check from RAM
    }
    else if (*(int *)(Pico.rom+0xc8778) == 0x30133013 && *(int *)(Pico.rom+0xc877c) == 0x000f0240) { // Super Mario Bros. (Unl) [!]
      d=0x0c; goto end; // seems to be the same code as in Rockman X3 (Unl) [!]
    }
    else if (*(int *)(Pico.rom+0xf20ec) == 0x30143013 && *(int *)(Pico.rom+0xf20f0) == 0x000f0200) { // Super Mario 2 1998 (Unl) [!]
      d=0x0a; goto end;
    }
  }
  else if (a == 0xa13002) { // Pocket Monsters (Unl)
    d=0x01; goto end;
  }
  else if (a == 0xa1303E) { // Pocket Monsters (Unl)
    d=0x1f; goto end;
  }
  else if (a == 0x30fe02) {
    // Virtua Racing - just for fun
    // this seems to be some flag that SVP is ready or something similar
    d=1; goto end;
  }

end:
  elprintf(EL_UIO, "strange r%i: [%06x] %04x @%06x", realsize, a&0xffffff, d, SekPc);
  return d;
}


//extern UINT32 mz80GetRegisterValue(void *, UINT32);

static void OtherWrite8End(u32 a,u32 d,int realsize)
{
  // sram
  if(a >= SRam.start && a <= SRam.end) {
    elprintf(EL_SRAMIO, "sram w8  [%06x] %02x @ %06x", a, d, SekPc);
    SRAMWrite(a, d);
    return;
  }

#ifdef _ASM_MEMORY_C
  // special ROM hardware (currently only banking and sram reg supported)
  if((a&0xfffff1) == 0xA130F1) {
    PicoWriteRomHW_SSF2(a, d); // SSF2 or SRAM
    return;
  }
#else
  // sram access register
  if(a == 0xA130F1) {
    elprintf(EL_SRAMIO, "sram reg=%02x", d);
    Pico.m.sram_reg &= ~3;
    Pico.m.sram_reg |= (u8)(d&3);
    return;
  }
#endif
  elprintf(EL_UIO, "strange w%i: %06x, %08x @%06x", realsize, a&0xffffff, d, SekPc);

  if(a >= 0xA13004 && a < 0xA13040) {
    // dumb 12-in-1 or 4-in-1 banking support
    int len;
    a &= 0x3f; a <<= 16;
    len = Pico.romsize - a;
    if (len <= 0) return; // invalid/missing bank
    if (len > 0x200000) len = 0x200000; // 2 megs
    memcpy(Pico.rom, Pico.rom+a, len); // code which does this is in RAM so this is safe.
    return;
  }

  // for games with simple protection devices, discovered by Haze
  else if ((a>>22) == 1)
    Pico.m.prot_bytes[(a>>2)&1] = (u8)d;
}


#include "MemoryCmn.c"


// -----------------------------------------------------------------
//                     Read Rom and read Ram

#ifndef _ASM_MEMORY_C
PICO_INTERNAL_ASM u32 PicoRead8(u32 a)
{
  u32 d=0;

  if ((a&0xe00000)==0xe00000) { d = *(u8 *)(Pico.ram+((a^1)&0xffff)); goto end; } // Ram

  a&=0xffffff;

#ifndef EMU_CORE_DEBUG
  // sram
  if (a >= SRam.start && a <= SRam.end && (Pico.m.sram_reg&5)) {
    d = SRAMRead(a);
    elprintf(EL_SRAMIO, "sram r8 [%06x] %02x @ %06x", a, d, SekPc);
    goto end;
  }
#endif

  if (a<Pico.romsize) { d = *(u8 *)(Pico.rom+(a^1)); goto end; } // Rom
  log_io(a, 8, 0);
  if ((a&0xff4000)==0xa00000) { d=z80Read8(a); goto end; } // Z80 Ram

  if ((a&0xe700e0)==0xc00000) // VDP
       d=PicoVideoRead(a);
  else d=OtherRead16(a&~1, 8);
  if ((a&1)==0) d>>=8;

end:
  elprintf(EL_IO, "r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPc);
#ifdef EMU_CORE_DEBUG
  if (a>=Pico.romsize) {
    lastread_a = a;
    lastread_d[lrp_cyc++&15] = (u8)d;
  }
#endif
  return d;
}

PICO_INTERNAL_ASM u32 PicoRead16(u32 a)
{
  u32 d=0;

  if ((a&0xe00000)==0xe00000) { d=*(u16 *)(Pico.ram+(a&0xfffe)); goto end; } // Ram

  a&=0xfffffe;

#ifndef EMU_CORE_DEBUG
  // sram
  if (a >= SRam.start && a <= SRam.end && (Pico.m.sram_reg&5)) {
    d = SRAMRead16(a);
    elprintf(EL_SRAMIO, "sram r16 [%06x] %04x @ %06x", a, d, SekPc);
    goto end;
  }
#endif

  if (a<Pico.romsize) { d = *(u16 *)(Pico.rom+a); goto end; } // Rom
  log_io(a, 16, 0);

  if ((a&0xe700e0)==0xc00000)
       d = PicoVideoRead(a);
  else d = OtherRead16(a, 16);

end:
  elprintf(EL_IO, "r16: %06x, %04x  @%06x", a&0xffffff, d, SekPc);
#ifdef EMU_CORE_DEBUG
  if (a>=Pico.romsize) {
    lastread_a = a;
    lastread_d[lrp_cyc++&15] = d;
  }
#endif
  return d;
}

PICO_INTERNAL_ASM u32 PicoRead32(u32 a)
{
  u32 d=0;

  if ((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xfffe)); d = (pm[0]<<16)|pm[1]; goto end; } // Ram

  a&=0xfffffe;

  // sram
  if(a >= SRam.start && a <= SRam.end && (Pico.m.sram_reg&5)) {
    d = (SRAMRead16(a)<<16)|SRAMRead16(a+2);
    elprintf(EL_SRAMIO, "sram r32 [%06x] %08x @ %06x", a, d, SekPc);
    goto end;
  }

  if (a<Pico.romsize) { u16 *pm=(u16 *)(Pico.rom+a); d = (pm[0]<<16)|pm[1]; goto end; } // Rom
  log_io(a, 32, 0);

  if ((a&0xe700e0)==0xc00000)
       d = (PicoVideoRead(a)<<16)|PicoVideoRead(a+2);
  else d = (OtherRead16(a, 32)<<16)|OtherRead16(a+2, 32);

end:
  elprintf(EL_IO, "r32: %06x, %08x @%06x", a&0xffffff, d, SekPc);
#ifdef EMU_CORE_DEBUG
  if (a>=Pico.romsize) {
    lastread_a = a;
    lastread_d[lrp_cyc++&15] = d;
  }
#endif
  return d;
}
#endif

// -----------------------------------------------------------------
//                            Write Ram

#if !defined(_ASM_MEMORY_C) || defined(_ASM_MEMORY_C_AMIPS)
PICO_INTERNAL_ASM void PicoWrite8(u32 a,u8 d)
{
  elprintf(EL_IO, "w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPc);
#ifdef EMU_CORE_DEBUG
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif

  if ((a&0xe00000)==0xe00000) { *(u8 *)(Pico.ram+((a^1)&0xffff))=d; return; } // Ram
  log_io(a, 8, 1);

  a&=0xffffff;
  OtherWrite8(a,d);
}
#endif

void PicoWrite16(u32 a,u16 d)
{
  elprintf(EL_IO, "w16: %06x, %04x", a&0xffffff, d);
#ifdef EMU_CORE_DEBUG
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif

  if ((a&0xe00000)==0xe00000) { *(u16 *)(Pico.ram+(a&0xfffe))=d; return; } // Ram
  log_io(a, 16, 1);

  a&=0xfffffe;
  if ((a&0xe700e0)==0xc00000) { PicoVideoWrite(a,(u16)d); return; } // VDP
  OtherWrite16(a,d);
}

static void PicoWrite32(u32 a,u32 d)
{
  elprintf(EL_IO, "w32: %06x, %08x", a&0xffffff, d);
#ifdef EMU_CORE_DEBUG
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif

  if ((a&0xe00000)==0xe00000)
  {
    // Ram:
    u16 *pm=(u16 *)(Pico.ram+(a&0xfffe));
    pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    return;
  }
  log_io(a, 32, 1);

  a&=0xfffffe;
  if ((a&0xe700e0)==0xc00000)
  {
    // VDP:
    PicoVideoWrite(a,  (u16)(d>>16));
    PicoVideoWrite(a+2,(u16)d);
    return;
  }

  OtherWrite16(a,  (u16)(d>>16));
  OtherWrite16(a+2,(u16)d);
}


// -----------------------------------------------------------------
PICO_INTERNAL void PicoMemSetup(void)
{
  // Setup memory callbacks:
#ifdef EMU_C68K
  PicoCpuCM68k.checkpc=PicoCheckPc;
  PicoCpuCM68k.fetch8 =PicoCpuCM68k.read8 =PicoRead8;
  PicoCpuCM68k.fetch16=PicoCpuCM68k.read16=PicoRead16;
  PicoCpuCM68k.fetch32=PicoCpuCM68k.read32=PicoRead32;
  PicoCpuCM68k.write8 =PicoWrite8;
  PicoCpuCM68k.write16=PicoWrite16;
  PicoCpuCM68k.write32=PicoWrite32;
#endif
#ifdef EMU_F68K
  PicoCpuFM68k.read_byte =PicoRead8;
  PicoCpuFM68k.read_word =PicoRead16;
  PicoCpuFM68k.read_long =PicoRead32;
  PicoCpuFM68k.write_byte=PicoWrite8;
  PicoCpuFM68k.write_word=PicoWrite16;
  PicoCpuFM68k.write_long=PicoWrite32;

  // setup FAME fetchmap
  {
    int i;
    // by default, point everything to fitst 64k of ROM
    for (i = 0; i < M68K_FETCHBANK1; i++)
      PicoCpuFM68k.Fetch[i] = (unsigned int)Pico.rom - (i<<(24-FAMEC_FETCHBITS));
    // now real ROM
    for (i = 0; i < M68K_FETCHBANK1 && (i<<(24-FAMEC_FETCHBITS)) < Pico.romsize; i++)
      PicoCpuFM68k.Fetch[i] = (unsigned int)Pico.rom;
    // .. and RAM
    for (i = M68K_FETCHBANK1*14/16; i < M68K_FETCHBANK1; i++)
      PicoCpuFM68k.Fetch[i] = (unsigned int)Pico.ram - (i<<(24-FAMEC_FETCHBITS));
  }
#endif
}


#ifdef EMU_M68K
unsigned int  m68k_read_pcrelative_CD8 (unsigned int a);
unsigned int  m68k_read_pcrelative_CD16(unsigned int a);
unsigned int  m68k_read_pcrelative_CD32(unsigned int a);

// these are allowed to access RAM
static unsigned int  m68k_read_8 (unsigned int a, int do_fake)
{
  a&=0xffffff;
  if(a<Pico.romsize && m68ki_cpu_p==&PicoCpuMM68k)    return *(u8 *)(Pico.rom+(a^1)); // Rom
#ifdef EMU_CORE_DEBUG
  if(do_fake&&((ppop&0x3f)==0x3a||(ppop&0x3f)==0x3b)) return lastread_d[lrp_mus++&15];
#endif
  if(PicoMCD&1) return m68k_read_pcrelative_CD8(a);
  if((a&0xe00000)==0xe00000) return *(u8 *)(Pico.ram+((a^1)&0xffff)); // Ram
  return 0;
}
static unsigned int  m68k_read_16(unsigned int a, int do_fake)
{
  a&=0xffffff;
  if(a<Pico.romsize && m68ki_cpu_p==&PicoCpuMM68k)    return *(u16 *)(Pico.rom+(a&~1)); // Rom
#ifdef EMU_CORE_DEBUG
  if(do_fake&&((ppop&0x3f)==0x3a||(ppop&0x3f)==0x3b)) return lastread_d[lrp_mus++&15];
#endif
  if(PicoMCD&1) return m68k_read_pcrelative_CD16(a);
  if((a&0xe00000)==0xe00000) return *(u16 *)(Pico.ram+(a&0xfffe)); // Ram
  return 0;
}
static unsigned int  m68k_read_32(unsigned int a, int do_fake)
{
  a&=0xffffff;
  if(a<Pico.romsize && m68ki_cpu_p==&PicoCpuMM68k) { u16 *pm=(u16 *)(Pico.rom+(a&~1)); return (pm[0]<<16)|pm[1]; }
#ifdef EMU_CORE_DEBUG
  if(do_fake&&((ppop&0x3f)==0x3a||(ppop&0x3f)==0x3b)) return lastread_d[lrp_mus++&15];
#endif
  if(PicoMCD&1) return m68k_read_pcrelative_CD32(a);
  if((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xfffe)); return (pm[0]<<16)|pm[1]; } // Ram
  return 0;
}

unsigned int m68k_read_pcrelative_8 (unsigned int a)   { return m68k_read_8 (a, 1); }
unsigned int m68k_read_pcrelative_16(unsigned int a)   { return m68k_read_16(a, 1); }
unsigned int m68k_read_pcrelative_32(unsigned int a)   { return m68k_read_32(a, 1); }
unsigned int m68k_read_immediate_16(unsigned int a)    { return m68k_read_16(a, 0); }
unsigned int m68k_read_immediate_32(unsigned int a)    { return m68k_read_32(a, 0); }
unsigned int m68k_read_disassembler_8 (unsigned int a) { return m68k_read_8 (a, 0); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return m68k_read_16(a, 0); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return m68k_read_32(a, 0); }

#ifdef EMU_CORE_DEBUG
// ROM only
unsigned int m68k_read_memory_8(unsigned int a)
{
  u8 d;
  if (a<Pico.romsize && m68ki_cpu_p==&PicoCpuMM68k)
       d = *(u8 *) (Pico.rom+(a^1));
  else d = (u8) lastread_d[lrp_mus++&15];
  elprintf(EL_IO, "r8_mu : %06x,   %02x @%06x", a&0xffffff, d, SekPc);
  return d;
}
unsigned int m68k_read_memory_16(unsigned int a)
{
  u16 d;
  if (a<Pico.romsize && m68ki_cpu_p==&PicoCpuMM68k)
       d = *(u16 *)(Pico.rom+(a&~1));
  else d = (u16) lastread_d[lrp_mus++&15];
  elprintf(EL_IO, "r16_mu: %06x, %04x @%06x", a&0xffffff, d, SekPc);
  return d;
}
unsigned int m68k_read_memory_32(unsigned int a)
{
  u32 d;
  if (a<Pico.romsize && m68ki_cpu_p==&PicoCpuMM68k)
       { u16 *pm=(u16 *)(Pico.rom+(a&~1));d=(pm[0]<<16)|pm[1]; }
  else if (a <= 0x78) d = m68k_read_32(a, 0);
  else d = lastread_d[lrp_mus++&15];
  elprintf(EL_IO, "r32_mu: %06x, %08x @%06x", a&0xffffff, d, SekPc);
  return d;
}

// ignore writes, Cyclone already done that
void m68k_write_memory_8(unsigned int address, unsigned int value)  { lastwrite_mus_d[lwp_mus++&15] = value; }
void m68k_write_memory_16(unsigned int address, unsigned int value) { lastwrite_mus_d[lwp_mus++&15] = value; }
void m68k_write_memory_32(unsigned int address, unsigned int value) { lastwrite_mus_d[lwp_mus++&15] = value; }
#else
unsigned char  PicoReadCD8w (unsigned int a);
unsigned short PicoReadCD16w(unsigned int a);
unsigned int   PicoReadCD32w(unsigned int a);
void PicoWriteCD8w (unsigned int a, unsigned char d);
void PicoWriteCD16w(unsigned int a, unsigned short d);
void PicoWriteCD32w(unsigned int a, unsigned int d);

/* it appears that Musashi doesn't always mask the unused bits */
unsigned int  m68k_read_memory_8(unsigned int address)
{
    unsigned int d = (PicoMCD&1) ? PicoReadCD8w(address) : PicoRead8(address);
    return d&0xff;
}

unsigned int  m68k_read_memory_16(unsigned int address)
{
    unsigned int d = (PicoMCD&1) ? PicoReadCD16w(address) : PicoRead16(address);
    return d&0xffff;
}

unsigned int  m68k_read_memory_32(unsigned int address)
{
    return (PicoMCD&1) ? PicoReadCD32w(address) : PicoRead32(address);
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    if (PicoMCD&1) PicoWriteCD8w(address, (u8)value); else PicoWrite8(address, (u8)value);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    if (PicoMCD&1) PicoWriteCD16w(address,(u16)value); else PicoWrite16(address,(u16)value);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    if (PicoMCD&1) PicoWriteCD32w(address, value); else PicoWrite32(address, value);
}
#endif
#endif // EMU_M68K


// -----------------------------------------------------------------
//                        z80 memhandlers

PICO_INTERNAL unsigned char z80_read(unsigned short a)
{
  u8 ret = 0;

  if ((a>>13)==2) // 0x4000-0x5fff (Charles MacDonald)
  {
    if (PicoOpt&1) ret = (u8) YM2612Read();
    return ret;
  }

  if (a>=0x8000)
  {
    extern u32 PicoReadM68k8(u32 a);
    u32 addr68k;
    addr68k=Pico.m.z80_bank68k<<15;
    addr68k+=a&0x7fff;

    if (PicoMCD & 1)
         ret = PicoReadM68k8(addr68k);
    else ret = PicoRead8(addr68k);
    elprintf(EL_Z80BNK, "z80->68k r8 [%06x] %02x", addr68k, ret);
    return ret;
  }

  // should not be needed, cores should be able to access RAM themselves
  if (a<0x4000) return Pico.zram[a&0x1fff];

  elprintf(EL_ANOMALY, "z80 invalid r8 [%06x] %02x", a, ret);
  return ret;
}

#ifndef _USE_CZ80
PICO_INTERNAL_ASM void z80_write(unsigned char data, unsigned short a)
#else
PICO_INTERNAL_ASM void z80_write(unsigned int a, unsigned char data)
#endif
{
  if ((a>>13)==2) // 0x4000-0x5fff (Charles MacDonald)
  {
    if(PicoOpt&1) emustatus|=YM2612Write(a, data) & 1;
    return;
  }

  if ((a&0xfff9)==0x7f11) // 7f11 7f13 7f15 7f17
  {
    if(PicoOpt&2) SN76496Write(data);
    return;
  }

  if ((a>>8)==0x60)
  {
    Pico.m.z80_bank68k>>=1;
    Pico.m.z80_bank68k|=(data&1)<<8;
    Pico.m.z80_bank68k&=0x1ff; // 9 bits and filled in the new top one
    return;
  }

  if (a>=0x8000)
  {
    extern void PicoWriteM68k8(u32 a,u8 d);
    u32 addr68k;
    addr68k=Pico.m.z80_bank68k<<15;
    addr68k+=a&0x7fff;
    elprintf(EL_Z80BNK, "z80->68k w8 [%06x] %02x", addr68k, data);
    if (PicoMCD & 1)
         PicoWriteM68k8(addr68k, data);
    else PicoWrite8(addr68k, data);
    return;
  }

  // should not be needed
  if (a<0x4000) { Pico.zram[a&0x1fff]=data; return; }

  elprintf(EL_ANOMALY, "z80 invalid w8 [%06x] %02x", a, data);
}

#ifndef _USE_CZ80
PICO_INTERNAL unsigned short z80_read16(unsigned short a)
{
  return (u16) ( (u16)z80_read(a) | ((u16)z80_read((u16)(a+1))<<8) );
}

PICO_INTERNAL void z80_write16(unsigned short data, unsigned short a)
{
  z80_write((unsigned char) data,a);
  z80_write((unsigned char)(data>>8),(u16)(a+1));
}
#endif

