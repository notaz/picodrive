/* common code for Memory.c and cd/Memory.c */

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
u32 OtherRead16(u32 a, int realsize)
{
  u32 d=0;

  if ((a&0xff0000)==0xa00000) {
    if ((a&0x4000)==0x0000) { d=z80Read8(a); d|=d<<8; goto end; } // Z80 ram (not byteswaped)
    if ((a&0x6000)==0x4000) { // 0x4000-0x5fff, Fudge if disabled
      if(PicoOpt&1) d=YM2612Read();
      else d=Pico.m.rotate++&3;
      dprintf("read ym2612: %04x", d);
      goto end;
    }
    d=0xffff;
    goto end;
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
  if (a==0xa11100) { // z80 busreq
    d=Pico.m.z80Run&1;
#if 0
    if (!d) {
      // do we need this?
      extern int z80stopCycle;
      int stop_before = SekCyclesDone() - z80stopCycle;
      if (stop_before > 0 && stop_before <= 16) // Gens uses 16 here
        d = 1; // bus not yet available
    }
#endif
    d=(d<<8)|0x8000|Pico.m.rotate++;
    dprintf("get_zrun: %04x [%i|%i] @%06x", d, Pico.m.scanline, SekCyclesDone(), SekPc);
    goto end;
  }

#ifndef _ASM_MEMORY_C
  if ((a&0xe700e0)==0xc00000) { d=PicoVideoRead(a); goto end; }
#endif

  d = OtherRead16End(a, realsize);

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
      // this is for a nasty situation where Z80 was enabled and disabled in the same 68k timeslice (Golden Axe III)
      if (Pico.m.z80Run) {
        int lineCycles=(488-SekCyclesLeft)&0x1ff;
        z80stopCycle = SekCyclesDone();
        lineCycles=(lineCycles>>1)-(lineCycles>>5);
        z80_run(lineCycles);
      }
    } else {
      z80startCycle = SekCyclesDone();
      //if(Pico.m.scanline != -1)
    }
    dprintf("set_zrun: %02x [%i|%i] @%06x", d, Pico.m.scanline, SekCyclesDone(), /*mz80GetRegisterValue(NULL, 0),*/ SekPc);
    Pico.m.z80Run=(u8)d; return;
  }
  if (a==0xa11200) {
    dprintf("write z80Reset: %02x", d);
    if(!(d&1)) z80_reset();
    return;
  }

  if ((a&0xff7f00)==0xa06000) // Z80 BANK register
  {
    Pico.m.z80_bank68k>>=1;
    Pico.m.z80_bank68k|=(d&1)<<8;
    Pico.m.z80_bank68k&=0x1ff; // 9 bits and filled in the new top one
    return;
  }

  if ((a&0xe700e0)==0xc00000) {
    PicoVideoWrite(a,(u16)(d|(d<<8))); // Byte access gets mirrored
    return;
  }

  OtherWrite8End(a, d, realsize);
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


