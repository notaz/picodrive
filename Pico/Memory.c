// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


//#define __debug_io

#include "PicoInt.h"

#include "sound/sound.h"
#include "sound/ym2612.h"
#include "sound/sn76496.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

extern unsigned int lastSSRamWrite; // used by serial SRAM code

#ifdef _ASM_MEMORY_C
u8   PicoRead8(u32 a);
u16  PicoRead16(u32 a);
void PicoWriteRomHW_SSF2(u32 a,u32 d);
void PicoWriteRomHW_in1 (u32 a,u32 d);
#endif


#if defined(EMU_C68K) && defined(EMU_M68K)
// cyclone debug mode
u32 lastread_a, lastread_d[16]={0,}, lastwrite_cyc_d[16]={0,}, lastwrite_mus_d[16]={0,};
int lrp_cyc=0, lrp_mus=0, lwp_cyc=0, lwp_mus=0;
extern unsigned int ppop;
#endif

#if defined(EMU_C68K) || defined(EMU_A68K)
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


#ifdef EMU_A68K
extern u8 *OP_ROM=NULL,*OP_RAM=NULL;
#endif

static u32 CPU_CALL PicoCheckPc(u32 pc)
{
  u32 ret=0;
#if defined(EMU_C68K)
  pc-=PicoCpu.membase; // Get real pc
  pc&=0xfffffe;

  PicoCpu.membase=PicoMemBase(pc);

  ret = PicoCpu.membase+pc;
#elif defined(EMU_A68K)
  OP_ROM=(u8 *)PicoMemBase(pc);

  // don't bother calling us back unless it's outside the 64k segment
  M68000_regs.AsmBank=(pc>>16);
#endif
  return ret;
}


int PicoInitPc(u32 pc)
{
  PicoCheckPc(pc);
  return 0;
}

#ifndef _ASM_MEMORY_C
void PicoMemReset()
{
}
#endif

// -----------------------------------------------------------------

#ifndef _ASM_MEMORY_C
// address must already be checked
static int SRAMRead(u32 a)
{
  u8 *d = SRam.data-SRam.start+a;
  return (d[0]<<8)|d[1];
}
#endif

static int PadRead(int i)
{
  int pad=0,value=0,TH;
  pad=~PicoPad[i]; // Get inverse of pad MXYZ SACB RLDU
  TH=Pico.ioports[i+1]&0x40;

  if(PicoOpt & 0x20) { // 6 button gamepad enabled
    int phase = Pico.m.padTHPhase[i];

    if(phase == 2 && !TH) {
      value=(pad&0xc0)>>2;              // ?0SA 0000
      goto end;
    } else if(phase == 3 && TH) {
      value=(pad&0x30)|((pad>>8)&0xf);  // ?1CB MXYZ
      goto end;
    } else if(phase == 3 && !TH) {
      value=((pad&0xc0)>>2)|0x0f;       // ?0SA 1111
      goto end;
    }
  }

  if(TH) value=(pad&0x3f);              // ?1CB RLDU
  else   value=((pad&0xc0)>>2)|(pad&3); // ?0SA 00DU

  end:

  // orr the bits, which are set as output
  value |= Pico.ioports[i+1]&Pico.ioports[i+4];

  return value; // will mirror later
}

u8 z80Read8(u32 a)
{
  if(Pico.m.z80Run&1) return 0;

  a&=0x1fff;

  if(!(PicoOpt&4)) {
    // Z80 disabled, do some faking
    static u8 zerosent = 0;
    if(a == Pico.m.z80_lastaddr) { // probably polling something
      u8 d = Pico.m.z80_fakeval;
      if((d & 0xf) == 0xf && !zerosent) {
        d = 0; zerosent = 1;
      } else {
        Pico.m.z80_fakeval++;
        zerosent = 0;
      }
      return d;
    } else {
      Pico.m.z80_fakeval = 0;
    }
  }

  Pico.m.z80_lastaddr = (u16) a;
  return Pico.zram[a];
}


// for nonstandard reads
#ifndef _ASM_MEMORY_C
static
#endif
u32 UnusualRead16(u32 a, int realsize)
{
  u32 d=0;

  dprintf("strange r%i: %06x @%06x", realsize, a&0xffffff, SekPc);

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
  dprintf("ret = %04x", d);
  return d;
}

#ifndef _ASM_MEMORY_C
static
#endif
u32 OtherRead16(u32 a, int realsize)
{
  u32 d=0;

  if ((a&0xff0000)==0xa00000) {
    if ((a&0x4000)==0x0000) { d=z80Read8(a); d|=d<<8; goto end; } // Z80 ram (not byteswaped)
    if ((a&0x6000)==0x4000) { if(PicoOpt&1) d=YM2612Read(); else d=Pico.m.rotate++&3; dprintf("read ym2612: %04x", d); goto end; } // 0x4000-0x5fff, Fudge if disabled
    d=0xffff; goto end;
  }
  if ((a&0xffffe0)==0xa10000) { // I/O ports
    a=(a>>1)&0xf;
    switch(a) {
      case 0:  d=Pico.m.hardware; break; // Hardware value (Version register)
      case 1:  d=PadRead(0); d|=Pico.ioports[1]&0x80; break;
      case 2:  d=PadRead(1); d|=Pico.ioports[2]&0x80; break;
      default: d=Pico.ioports[a]; break; // IO ports can be used as RAM
    }
    d|=d<<8;
    goto end;
  }
  // |=0x80 for Shadow of the Beast & Super Offroad; rotate fakes next fetched instruction for Time Killers
  if (a==0xa11100) {
    d=Pico.m.z80Run&1;
#if 0
    if (!d) {
      // do we need this?
      extern int z80stopCycle; // TODO: tidy
      int stop_before = SekCyclesDone() - z80stopCycle;
      if (stop_before > 0 && stop_before <= 16) // Gens uses 16 here
        d = 1; // bus not yet available
    }
#endif
    d=(d<<8)|0x8000|Pico.m.rotate++;
    dprintf("get_zrun: %04x [%i|%i] @%06x", d, Pico.m.scanline, SekCyclesDone(), SekPc);
    goto end; }

#ifndef _ASM_MEMORY_C
  if ((a&0xe700e0)==0xc00000) { d=PicoVideoRead(a); goto end; }
#endif

  d = UnusualRead16(a, realsize);

end:
  return d;
}

//extern UINT32 mz80GetRegisterValue(void *, UINT32);

static void OtherWrite8(u32 a,u32 d,int realsize)
{
  if ((a&0xe700f9)==0xc00011||(a&0xff7ff9)==0xa07f11) { if(PicoOpt&2) SN76496Write(d); return; } // PSG Sound
  if ((a&0xff4000)==0xa00000)  { if(!(Pico.m.z80Run&1)) Pico.zram[a&0x1fff]=(u8)d; return; } // Z80 ram
  if ((a&0xff6000)==0xa04000)  { if(PicoOpt&1) emustatus|=YM2612Write(a&3, d); return; } // FM Sound
  if ((a&0xffffe0)==0xa10000)  { // I/O ports
    a=(a>>1)&0xf;
    // 6 button gamepad: if TH went from 0 to 1, gamepad changes state
    if(PicoOpt&0x20) {
      if(a==1) {
        Pico.m.padDelay[0] = 0;
        if(!(Pico.ioports[1]&0x40) && (d&0x40)) Pico.m.padTHPhase[0]++;
      }
      else if(a==2) {
        Pico.m.padDelay[1] = 0;
        if(!(Pico.ioports[2]&0x40) && (d&0x40)) Pico.m.padTHPhase[1]++;
      }
    }
    Pico.ioports[a]=(u8)d; // IO ports can be used as RAM
    return;
  }
  if (a==0xa11100) {
    extern int z80startCycle, z80stopCycle;
    //int lineCycles=(488-SekCyclesLeft)&0x1ff;
    d&=1; d^=1;
    if(!d) {
       // hack: detect a nasty situation where Z80 was enabled and disabled in the same 68k timeslice (Golden Axe III)
      // if((PicoOpt&4) && Pico.m.z80Run==1) z80_run(20); // FIXME: movies
      z80stopCycle = SekCyclesDone();
      //z80ExtraCycles += (lineCycles>>1)-(lineCycles>>5); // only meaningful in PicoFrameHints()
    } else {
      z80startCycle = SekCyclesDone();
      //if(Pico.m.scanline != -1)
      //z80ExtraCycles -= (lineCycles>>1)-(lineCycles>>5)+16;
    }
    dprintf("set_zrun: %02x [%i|%i] @%06x", d, Pico.m.scanline, SekCyclesDone(), /*mz80GetRegisterValue(NULL, 0),*/ SekPc);
    Pico.m.z80Run=(u8)d; return;
  }
  if (a==0xa11200) { dprintf("write z80Reset: %02x", d); if(!(d&1)) z80_reset(); return; }

  if ((a&0xff7f00)==0xa06000) // Z80 BANK register
  {
    Pico.m.z80_bank68k>>=1;
    Pico.m.z80_bank68k|=(d&1)<<8;
    Pico.m.z80_bank68k&=0x1ff; // 9 bits and filled in the new top one
    return;
  }

  if ((a&0xe700e0)==0xc00000)  { PicoVideoWrite(a,(u16)(d|(d<<8))); return; } // Byte access gets mirrored

  // sram
  //if(a==0x200000) dprintf("cc : %02x @ %06x [%i|%i]", d, SekPc, SekCyclesDoneT(), SekCyclesDone());
  //if(a==0x200001) dprintf("w8 : %02x @ %06x [%i]", d, SekPc, SekCyclesDoneT());
  if(a >= SRam.start && a <= SRam.end) {
    unsigned int sreg = Pico.m.sram_reg;
    if(!(sreg & 0x10)) {
      // not detected SRAM
      if((a&~1)==0x200000) {
        Pico.m.sram_reg|=4; // this should be a game with EEPROM (like NBA Jam)
        SRam.start=0x200000; SRam.end=SRam.start+1;
      }
      Pico.m.sram_reg|=0x10;
    }
    if(sreg & 4) { // EEPROM write
      if(SekCyclesDoneT()-lastSSRamWrite < 46) {
        // just update pending state
        SRAMUpdPending(a, d);
      } else {
        SRAMWriteEEPROM(sreg>>6); // execute pending
        SRAMUpdPending(a, d);
        lastSSRamWrite = SekCyclesDoneT();
      }
    } else if(!(sreg & 2)) {
      u8 *pm=(u8 *)(SRam.data-SRam.start+a);
      if(*pm != (u8)d) {
        SRam.changed = 1;
        *pm=(u8)d;
      }
    }
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
    Pico.m.sram_reg = (u8)(d&3);
    return;
  }
#endif
  dprintf("strange w%i: %06x, %08x @%06x", realsize, a&0xffffff, d, SekPc);

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

static void OtherWrite16(u32 a,u32 d)
{
  if ((a&0xe700e0)==0xc00000) { PicoVideoWrite(a,(u16)d); return; }
  if ((a&0xff4000)==0xa00000) { if(!(Pico.m.z80Run&1)) Pico.zram[a&0x1fff]=(u8)(d>>8); return; } // Z80 ram (MSB only)

  if ((a&0xffffe0)==0xa10000) { // I/O ports
    a=(a>>1)&0xf;
    // 6 button gamepad: if TH went from 0 to 1, gamepad changes state
    if(PicoOpt&0x20) {
      if(a==1) {
        Pico.m.padDelay[0] = 0;
        if(!(Pico.ioports[1]&0x40) && (d&0x40)) Pico.m.padTHPhase[0]++;
      }
      else if(a==2) {
        Pico.m.padDelay[1] = 0;
        if(!(Pico.ioports[2]&0x40) && (d&0x40)) Pico.m.padTHPhase[1]++;
      }
    }
    Pico.ioports[a]=(u8)d; // IO ports can be used as RAM
    return;
  }
  if (a==0xa11100) { OtherWrite8(a, d>>8, 16); return; }
  if (a==0xa11200) { dprintf("write z80reset: %04x", d); if(!(d&0x100)) z80_reset(); return; }

  OtherWrite8(a,  d>>8, 16);
  OtherWrite8(a+1,d&0xff, 16);
}

// -----------------------------------------------------------------
//                     Read Rom and read Ram

#ifndef _ASM_MEMORY_C
u8 CPU_CALL PicoRead8(u32 a)
{
  u32 d=0;

  if ((a&0xe00000)==0xe00000) { d = *(u8 *)(Pico.ram+((a^1)&0xffff)); goto end; } // Ram

  a&=0xffffff;

#if !(defined(EMU_C68K) && defined(EMU_M68K))
  // sram
  if(a >= SRam.start && a <= SRam.end) {
    unsigned int sreg = Pico.m.sram_reg;
    if(!(sreg & 0x10) && (sreg & 1) && a > 0x200001) { // not yet detected SRAM
      Pico.m.sram_reg|=0x10; // should be normal SRAM
    }
    if(sreg & 4) { // EEPROM read
      d = SRAMReadEEPROM();
      goto end;
    } else if(sreg & 1) {
      d = *(u8 *)(SRam.data-SRam.start+a);
      goto end;
    }
  }
#endif

  if (a<Pico.romsize) { d = *(u8 *)(Pico.rom+(a^1)); goto end; } // Rom
  if ((a&0xff4000)==0xa00000) { d=z80Read8(a); goto end; } // Z80 Ram

  d=OtherRead16(a&~1, 8); if ((a&1)==0) d>>=8;

  end:

  //if ((a&0xe0ffff)==0xe0AE57+0x69c)
  //  dprintf("r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPc);
  //if ((a&0xe0ffff)==0xe0a9ba+0x69c)
  //  dprintf("r8 : %06x,   %02x @%06x", a&0xffffff, d, SekPc);

  //if(a==0x200001) dprintf("r8 : %02x @ %06x [%i]", d, SekPc, SekCyclesDoneT());
  //dprintf("r8 : %06x,   %02x @%06x [%03i]", a&0xffffff, (u8)d, SekPc, Pico.m.scanline);
#ifdef __debug_io
  dprintf("r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPc);
#endif
#if defined(EMU_C68K) && defined(EMU_M68K)
  if(a>=Pico.romsize&&(ppop&0x3f)!=0x3a&&(ppop&0x3f)!=0x3b) {
    lastread_a = a;
    lastread_d[lrp_cyc++&15] = (u8)d;
  }
#endif
  return (u8)d;
}

u16 CPU_CALL PicoRead16(u32 a)
{
  u16 d=0;

  if ((a&0xe00000)==0xe00000) { d=*(u16 *)(Pico.ram+(a&0xfffe)); goto end; } // Ram

  a&=0xfffffe;

#if !(defined(EMU_C68K) && defined(EMU_M68K))
  // sram
  if(a >= SRam.start && a <= SRam.end && (Pico.m.sram_reg & 1)) {
    d = (u16) SRAMRead(a);
    goto end;
  }
#endif

  if (a<Pico.romsize) { d = *(u16 *)(Pico.rom+a); goto end; } // Rom

  d = (u16)OtherRead16(a, 16);

  end:
  //if ((a&0xe0ffff)==0xe0AF0E+0x69c||(a&0xe0ffff)==0xe0A9A8+0x69c||(a&0xe0ffff)==0xe0A9AA+0x69c||(a&0xe0ffff)==0xe0A9AC+0x69c)
  //  dprintf("r16: %06x, %04x  @%06x", a&0xffffff, d, SekPc);

#ifdef __debug_io
  dprintf("r16: %06x, %04x  @%06x", a&0xffffff, d, SekPc);
#endif
#if defined(EMU_C68K) && defined(EMU_M68K)
  if(a>=Pico.romsize&&(ppop&0x3f)!=0x3a&&(ppop&0x3f)!=0x3b) {
    lastread_a = a;
    lastread_d[lrp_cyc++&15] = d;
  }
#endif
  return d;
}

u32 CPU_CALL PicoRead32(u32 a)
{
  u32 d=0;

  if ((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xfffe)); d = (pm[0]<<16)|pm[1]; goto end; } // Ram

  a&=0xfffffe;

  // sram
  if(a >= SRam.start && a <= SRam.end && (Pico.m.sram_reg & 1)) {
    d = (SRAMRead(a)<<16)|SRAMRead(a+2);
    goto end;
  }

  if (a<Pico.romsize) { u16 *pm=(u16 *)(Pico.rom+a); d = (pm[0]<<16)|pm[1]; goto end; } // Rom

  d = (OtherRead16(a, 32)<<16)|OtherRead16(a+2, 32);

  end:
#ifdef __debug_io
  dprintf("r32: %06x, %08x @%06x", a&0xffffff, d, SekPc);
#endif
#if defined(EMU_C68K) && defined(EMU_M68K)
  if(a>=Pico.romsize&&(ppop&0x3f)!=0x3a&&(ppop&0x3f)!=0x3b) {
    lastread_a = a;
    lastread_d[lrp_cyc++&15] = d;
  }
#endif
  return d;
}
#endif

// -----------------------------------------------------------------
//                            Write Ram

static void CPU_CALL PicoWrite8(u32 a,u8 d)
{
#ifdef __debug_io
  dprintf("w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPc);
#endif
#if defined(EMU_C68K) && defined(EMU_M68K)
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif
  //if ((a&0xe0ffff)==0xe0a9ba+0x69c)
  //  dprintf("w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPc);


  if ((a&0xe00000)==0xe00000) {
	  if((a&0xffff)==0xf62a) dprintf("(f62a) = %02x [%i|%i] @ %x", d, Pico.m.scanline, SekCyclesDone(), SekPc);
	   u8 *pm=(u8 *)(Pico.ram+((a^1)&0xffff)); pm[0]=d; return; } // Ram

  a&=0xffffff;
  OtherWrite8(a,d,8);
}

static void CPU_CALL PicoWrite16(u32 a,u16 d)
{
#ifdef __debug_io
  dprintf("w16: %06x, %04x", a&0xffffff, d);
#endif
#if defined(EMU_C68K) && defined(EMU_M68K)
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif
  //if ((a&0xe0ffff)==0xe0AF0E+0x69c||(a&0xe0ffff)==0xe0A9A8+0x69c||(a&0xe0ffff)==0xe0A9AA+0x69c||(a&0xe0ffff)==0xe0A9AC+0x69c)
  //  dprintf("w16: %06x, %04x  @%06x", a&0xffffff, d, SekPc);

  if ((a&0xe00000)==0xe00000) { *(u16 *)(Pico.ram+(a&0xfffe))=d; return; } // Ram

  a&=0xfffffe;
  OtherWrite16(a,d);
}

static void CPU_CALL PicoWrite32(u32 a,u32 d)
{
#ifdef __debug_io
  dprintf("w32: %06x, %08x", a&0xffffff, d);
#endif
#if defined(EMU_C68K) && defined(EMU_M68K)
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif

  if ((a&0xe00000)==0xe00000)
  {
    // Ram:
    u16 *pm=(u16 *)(Pico.ram+(a&0xfffe));
    pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    return;
  }

  a&=0xfffffe;
  OtherWrite16(a,  (u16)(d>>16));
  OtherWrite16(a+2,(u16)d);
}


// -----------------------------------------------------------------
int PicoMemInit()
{
#ifdef EMU_C68K
  // Setup memory callbacks:
  PicoCpu.checkpc=PicoCheckPc;
  PicoCpu.fetch8 =PicoCpu.read8 =PicoRead8;
  PicoCpu.fetch16=PicoCpu.read16=PicoRead16;
  PicoCpu.fetch32=PicoCpu.read32=PicoRead32;
  PicoCpu.write8 =PicoWrite8;
  PicoCpu.write16=PicoWrite16;
  PicoCpu.write32=PicoWrite32;
#endif
  return 0;
}

#ifdef EMU_A68K
struct A68KInter
{
  u32 unknown;
  u8  (__fastcall *Read8) (u32 a);
  u16 (__fastcall *Read16)(u32 a);
  u32 (__fastcall *Read32)(u32 a);
  void (__fastcall *Write8)  (u32 a,u8 d);
  void (__fastcall *Write16) (u32 a,u16 d);
  void (__fastcall *Write32) (u32 a,u32 d);
  void (__fastcall *ChangePc)(u32 a);
  u8  (__fastcall *PcRel8) (u32 a);
  u16 (__fastcall *PcRel16)(u32 a);
  u32 (__fastcall *PcRel32)(u32 a);
  u16 (__fastcall *Dir16)(u32 a);
  u32 (__fastcall *Dir32)(u32 a);
};

struct A68KInter a68k_memory_intf=
{
  0,
  PicoRead8,
  PicoRead16,
  PicoRead32,
  PicoWrite8,
  PicoWrite16,
  PicoWrite32,
  PicoCheckPc,
  PicoRead8,
  PicoRead16,
  PicoRead32,
  PicoRead16, // unused
  PicoRead32, // unused
};
#endif

#ifdef EMU_M68K
unsigned int  m68k_read_pcrelative_CD8 (unsigned int a);
unsigned int  m68k_read_pcrelative_CD16(unsigned int a);
unsigned int  m68k_read_pcrelative_CD32(unsigned int a);

// these are allowed to access RAM
unsigned int  m68k_read_pcrelative_8 (unsigned int a) {
  a&=0xffffff;
  if(PicoMCD&1) return m68k_read_pcrelative_CD8(a);
  if(a<Pico.romsize)         return *(u8 *)(Pico.rom+(a^1)); // Rom
  if((a&0xe00000)==0xe00000) return *(u8 *)(Pico.ram+((a^1)&0xffff)); // Ram
  return 0;//(u8)  lastread_d;
}
unsigned int  m68k_read_pcrelative_16(unsigned int a) {
  a&=0xffffff;
  if(PicoMCD&1) return m68k_read_pcrelative_CD16(a);
  if(a<Pico.romsize)         return *(u16 *)(Pico.rom+(a&~1)); // Rom
  if((a&0xe00000)==0xe00000) return *(u16 *)(Pico.ram+(a&0xfffe)); // Ram
  return 0;//(u16) lastread_d;
}
unsigned int  m68k_read_pcrelative_32(unsigned int a) {
  a&=0xffffff;
  if(PicoMCD&1) return m68k_read_pcrelative_CD32(a);
  if(a<Pico.romsize)         { u16 *pm=(u16 *)(Pico.rom+(a&~1));     return (pm[0]<<16)|pm[1]; }
  if((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xfffe)); return (pm[0]<<16)|pm[1]; } // Ram
  return 0;//lastread_d;
}

unsigned int m68k_read_immediate_16(unsigned int a)    { return m68k_read_pcrelative_16(a); }
unsigned int m68k_read_immediate_32(unsigned int a)    { return m68k_read_pcrelative_32(a); }
unsigned int m68k_read_disassembler_8 (unsigned int a) { return m68k_read_pcrelative_8 (a); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return m68k_read_pcrelative_16(a); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return m68k_read_pcrelative_32(a); }

#ifdef EMU_C68K
// ROM only
unsigned int  m68k_read_memory_8(unsigned int a)  { if(a<Pico.romsize) return  *(u8 *) (Pico.rom+(a^1)); return (u8)  lastread_d[lrp_mus++&15]; }
unsigned int  m68k_read_memory_16(unsigned int a) { if(a<Pico.romsize) return  *(u16 *)(Pico.rom+(a&~1));return (u16) lastread_d[lrp_mus++&15]; }
unsigned int  m68k_read_memory_32(unsigned int a) { if(a<Pico.romsize) {u16 *pm=(u16 *)(Pico.rom+(a&~1));return (pm[0]<<16)|pm[1];} return lastread_d[lrp_mus++&15]; }

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

unsigned int  m68k_read_memory_8(unsigned int address)
{
    return (PicoMCD&1) ? PicoReadCD8w(address)  : PicoRead8(address);
}

unsigned int  m68k_read_memory_16(unsigned int address)
{
    return (PicoMCD&1) ? PicoReadCD16w(address) : PicoRead16(address);
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

unsigned char z80_read(unsigned short a)
{
  u8 ret = 0;

  if ((a>>13)==2) // 0x4000-0x5fff (Charles MacDonald)
  {
    if(PicoOpt&1) ret = (u8) YM2612Read();
    goto end;
  }

  if (a>=0x8000)
  {
    u32 addr68k;
    addr68k=Pico.m.z80_bank68k<<15;
    addr68k+=a&0x7fff;

    ret = (u8) PicoRead8(addr68k);
    //dprintf("z80->68k w8 : %06x,   %02x", addr68k, ret);
    goto end;
  }

  // should not be needed || dprintf("z80_read RAM");
  if (a<0x4000) { ret = (u8) Pico.zram[a&0x1fff]; goto end; }

end:
  return ret;
}

unsigned short z80_read16(unsigned short a)
{
  //dprintf("z80_read16");

  return (u16) ( (u16)z80_read(a) | ((u16)z80_read((u16)(a+1))<<8) );
}

void z80_write(unsigned char data, unsigned short a)
{
  //if (a<0x4000)
  //  dprintf("z80 w8 : %06x,   %02x @%04x", a, data, mz80GetRegisterValue(NULL, 0));

  if ((a>>13)==2) // 0x4000-0x5fff (Charles MacDonald)
  {
    if(PicoOpt&1) emustatus|=YM2612Write(a, data);
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
    u32 addr68k;
    addr68k=Pico.m.z80_bank68k<<15;
    addr68k+=a&0x7fff;
    PicoWrite8(addr68k, data);
    //dprintf("z80->68k w8 : %06x,   %02x", addr68k, data);
    return;
  }

  // should not be needed, drZ80 knows how to access RAM itself || dprintf("z80_write RAM @ %08x", lr);
  if (a<0x4000) { Pico.zram[a&0x1fff]=data; return; }
}

void z80_write16(unsigned short data, unsigned short a)
{
  //dprintf("z80_write16");

  z80_write((unsigned char) data,a);
  z80_write((unsigned char)(data>>8),(u16)(a+1));
}

