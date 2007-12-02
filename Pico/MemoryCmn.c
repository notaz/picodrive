// common code for Memory.c and cd/Memory.c
// (c) Copyright 2006-2007, Grazvydas "notaz" Ignotas

#ifndef UTYPES_DEFINED
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
#define UTYPES_DEFINED
#endif

#ifdef _ASM_MEMORY_C
u32 OtherRead16End(u32 a, int realsize);
#endif
#ifdef _ASM_CD_MEMORY_C
static void OtherWrite8End(u32 a,u32 d,int realsize);
#endif


#ifndef _ASM_MEMORY_C
static
#endif
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

#ifndef _ASM_MEMORY_C
static
#endif
u32 z80ReadBusReq(void)
{
  u32 d=Pico.m.z80Run&1;
  if (!d && Pico.m.scanline != -1) {
    // needed by buggy Terminator (Sega CD)
    int stop_before = SekCyclesDone() - z80stopCycle;
    //elprintf(EL_BUSREQ, "get_zrun: stop before: %i", stop_before);
    // note: if we use 20 or more here, Barkley Shut Up and Jam! will purposedly crash itself.
    // but CD Terminator needs at least 32, so it only works because next frame cycle wrap.
    if (stop_before > 0 && stop_before < 20) // Gens uses 16 here
      d = 1; // bus not yet available
  }
  // |=0x80 for Shadow of the Beast & Super Offroad
  elprintf(EL_BUSREQ, "get_zrun: %02x [%i] @%06x", d|0x80, SekCyclesDone(), SekPc);
  return d|0x80;
}

#ifndef _ASM_MEMORY_C
static
#endif
void z80WriteBusReq(u32 d)
{
  d&=1; d^=1;
  {
    if (!d)
    {
      // this is for a nasty situation where Z80 was enabled and disabled in the same 68k timeslice (Golden Axe III)
      if (Pico.m.z80Run) {
        int lineCycles;
        z80stopCycle = SekCyclesDone();
        if ((Pico.m.z80Run&2) && Pico.m.scanline != -1)
             lineCycles=(488-SekCyclesLeft)&0x1ff;
        else lineCycles=z80stopCycle-z80startCycle; // z80 was started at current line
        if (lineCycles > 0) { // && lineCycles <= 488) {
          //dprintf("zrun: %i/%i cycles", lineCycles, (lineCycles>>1)-(lineCycles>>5));
          lineCycles=(lineCycles>>1)-(lineCycles>>5);
          z80_run_nr(lineCycles);
        }
      }
    } else {
      if (!Pico.m.z80Run)
        z80startCycle = SekCyclesDone();
      else
        d|=Pico.m.z80Run;
    }
  }
  elprintf(EL_BUSREQ, "set_zrun: %i->%i [%i] @%06x", Pico.m.z80Run, d, SekCyclesDone(), SekPc);
  Pico.m.z80Run=(u8)d;
}

#ifndef _ASM_MEMORY_C
static
#endif
u32 OtherRead16(u32 a, int realsize)
{
  u32 d=0;

  if ((a&0xffffe0)==0xa10000) { // I/O ports
    a=(a>>1)&0xf;
    switch(a) {
      case 0:  d=Pico.m.hardware; break; // Hardware value (Version register)
      case 1:  d=PadRead(0); break;
      case 2:  d=PadRead(1); break;
      default: d=Pico.ioports[a]; break; // IO ports can be used as RAM
    }
    d|=d<<8;
    goto end;
  }

  // rotate fakes next fetched instruction for Time Killers
  if (a==0xa11100) { // z80 busreq
    d=(z80ReadBusReq()<<8)|Pico.m.rotate++;
    goto end;
  }

  if ((a&0xff0000)==0xa00000) {
    if ((a&0x4000)==0x0000) { d=z80Read8(a); d|=d<<8; goto end; } // Z80 ram (not byteswaped)
    if ((a&0x6000)==0x4000) { // 0x4000-0x5fff, Fudge if disabled
      if(PicoOpt&1) d=YM2612Read();
      else d=Pico.m.rotate++&3;
      elprintf(EL_YM2612R, "read ym2612: %02x", d);
      goto end;
    }
    d=0xffff;
    goto end;
  }

  d = OtherRead16End(a, realsize);

end:
  return d;
}

static void IoWrite8(u32 a, u32 d)
{
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
}

#ifndef _ASM_CD_MEMORY_C
static
#endif
void OtherWrite8(u32 a,u32 d)
{
#if !defined(_ASM_MEMORY_C) || defined(_ASM_MEMORY_C_AMIPS)
  if ((a&0xe700f9)==0xc00011||(a&0xff7ff9)==0xa07f11) { if(PicoOpt&2) SN76496Write(d); return; } // PSG Sound
  if ((a&0xff4000)==0xa00000)  { if(!(Pico.m.z80Run&1)) Pico.zram[a&0x1fff]=(u8)d; return; } // Z80 ram
  if ((a&0xff6000)==0xa04000)  { if(PicoOpt&1) emustatus|=YM2612Write(a&3, d)&1; return; } // FM Sound
  if ((a&0xffffe0)==0xa10000)  { IoWrite8(a, d); return; } // I/O ports
#endif
  if (a==0xa11100)             { z80WriteBusReq(d); return; }
  if (a==0xa11200) {
    elprintf(EL_BUSREQ, "write z80Reset: %02x", d);
    if(!(d&1)) z80_reset();
    return;
  }
#if !defined(_ASM_MEMORY_C) || defined(_ASM_MEMORY_C_AMIPS)
  if ((a&0xff7f00)==0xa06000) // Z80 BANK register
  {
    Pico.m.z80_bank68k>>=1;
    Pico.m.z80_bank68k|=(d&1)<<8;
    Pico.m.z80_bank68k&=0x1ff; // 9 bits and filled in the new top one
    return;
  }
#endif
  if ((a&0xe700e0)==0xc00000) {
    d&=0xff;
    PicoVideoWrite(a,(u16)(d|(d<<8))); // Byte access gets mirrored
    return;
  }

  OtherWrite8End(a, d, 8);
}


#ifndef _ASM_CD_MEMORY_C
static
#endif
void OtherWrite16(u32 a,u32 d)
{
  if (a==0xa11100)            { z80WriteBusReq(d>>8); return; }
  if (a==0xa11200)            { elprintf(EL_BUSREQ, "write z80reset: %04x", d); if(!(d&0x100)) z80_reset(); return; }
  if ((a&0xffffe0)==0xa10000) { IoWrite8(a, d); return; } // I/O ports
  if ((a&0xff4000)==0xa00000) { if(!(Pico.m.z80Run&1)) Pico.zram[a&0x1fff]=(u8)(d>>8); return; } // Z80 ram (MSB only)
  if ((a&0xe700f8)==0xc00010||(a&0xff7ff8)==0xa07f10) { if(PicoOpt&2) SN76496Write(d); return; } // PSG Sound
  if ((a&0xff6000)==0xa04000)  { if(PicoOpt&1) emustatus|=YM2612Write(a&3, d)&1; return; } // FM Sound (??)
  if ((a&0xff7f00)==0xa06000) // Z80 BANK register
  {
    Pico.m.z80_bank68k>>=1;
    Pico.m.z80_bank68k|=(d&1)<<8;
    Pico.m.z80_bank68k&=0x1ff; // 9 bits and filled in the new top one
    return;
  }

#ifndef _CD_MEMORY_C
  if (a >= SRam.start && a <= SRam.end) {
    elprintf(EL_SRAMIO, "sram w16 [%06x] %04x @ %06x", a, d, SekPc);
    if ((Pico.m.sram_reg&0x16)==0x10) { // detected, not EEPROM, write not disabled
      u8 *pm=(u8 *)(SRam.data-SRam.start+a);
      *pm++=d>>8;
      *pm++=d;
      SRam.changed = 1;
    }
    else
      SRAMWrite(a, d);
    return;
  }
#else
  OtherWrite8End(a,  d>>8, 16);
  OtherWrite8End(a+1,d&0xff, 16);
#endif
}


