// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

// A68K no longer supported here

//#define __debug_io

#include "../PicoInt.h"

#include "../sound/sound.h"
#include "../sound/ym2612.h"
#include "../sound/sn76496.h"

#include "gfx_cd.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

//#define __debug_io
//#define __debug_io2
#define rdprintf dprintf
//#define rdprintf(...)

// -----------------------------------------------------------------

// extern m68ki_cpu_core m68ki_cpu;

extern int counter75hz;


static u32 m68k_reg_read16(u32 a)
{
  u32 d=0;
  a &= 0x3e;
  // dprintf("m68k_regs r%2i: [%02x] @%06x", realsize&~1, a+(realsize&1), SekPc);

  switch (a) {
    case 0:
      d = ((Pico_mcd->s68k_regs[0x33]<<13)&0x8000) | Pico_mcd->m.busreq; // here IFL2 is always 0, just like in Gens
      goto end;
    case 2:
      d = (Pico_mcd->s68k_regs[a]<<8) | (Pico_mcd->s68k_regs[a+1]&0xc7);
      dprintf("m68k_regs r3: %02x @%06x", (u8)d, SekPc);
      goto end;
    case 4:
      d = Pico_mcd->s68k_regs[4]<<8;
      goto end;
    case 6:
      d = Pico_mcd->m.hint_vector;
      goto end;
    case 8:
      d = Read_CDC_Host(0);
      goto end;
    case 0xA:
      dprintf("m68k reserved read");
      goto end;
    case 0xC:
      dprintf("m68k stopwatch read");
      break;
  }

  if (a < 0x30) {
    // comm flag/cmd/status (0xE-0x2F)
    d = (Pico_mcd->s68k_regs[a]<<8) | Pico_mcd->s68k_regs[a+1];
    goto end;
  }

  dprintf("m68k_regs invalid read @ %02x", a);

end:

  // dprintf("ret = %04x", d);
  return d;
}

static void m68k_reg_write8(u32 a, u32 d)
{
  a &= 0x3f;
  // dprintf("m68k_regs w%2i: [%02x] %02x @%06x", realsize, a, d, SekPc);

  switch (a) {
    case 0:
      d &= 1;
      if ((d&1) && (Pico_mcd->s68k_regs[0x33]&(1<<2))) { dprintf("m68k: s68k irq 2"); SekInterruptS68k(2); }
      return;
    case 1:
      d &= 3;
      if (!(d&1)) PicoMCD |= 2; // reset pending, needed to be sure we fetch the right vectors on reset
      if ( (Pico_mcd->m.busreq&1) != (d&1)) dprintf("m68k: s68k reset %i", !(d&1));
      if ( (Pico_mcd->m.busreq&2) != (d&2)) dprintf("m68k: s68k brq %i", (d&2)>>1);
      if ((PicoMCD&2) && (d&3)==1) {
        SekResetS68k(); // S68k comes out of RESET or BRQ state
	PicoMCD&=~2;
	dprintf("m68k: resetting s68k, cycles=%i", SekCyclesLeft);
      }
      Pico_mcd->m.busreq = d;
      return;
    case 2:
      Pico_mcd->s68k_regs[2] = d; // really use s68k side register
      return;
    case 3:
      dprintf("m68k_regs w3: %02x @%06x", (u8)d, SekPc);
      d &= 0xc2;
      if ((Pico_mcd->s68k_regs[3]>>6) != ((d>>6)&3))
        dprintf("m68k: prg bank: %i -> %i", (Pico_mcd->s68k_regs[a]>>6), ((d>>6)&3));
      //if ((Pico_mcd->s68k_regs[3]&4) != (d&4)) dprintf("m68k: ram mode %i mbit", (d&4) ? 1 : 2);
      //if ((Pico_mcd->s68k_regs[3]&2) != (d&2)) dprintf("m68k: %s", (d&4) ? ((d&2) ? "word swap req" : "noop?") :
      //                                             ((d&2) ? "word ram to s68k" : "word ram to m68k"));
      d |= Pico_mcd->s68k_regs[3]&0x1d;
      if (!(d & 4) && (d & 2)) d &= ~1; // return word RAM to s68k in 2M mode
      Pico_mcd->s68k_regs[3] = d; // really use s68k side register
      return;
    case 6:
      *((char *)&Pico_mcd->m.hint_vector+1) = d;
      return;
    case 7:
      *(char *)&Pico_mcd->m.hint_vector = d;
      return;
    case 0xe:
      //dprintf("m68k: comm flag: %02x", d);

      //dprintf("s68k @ %06x", SekPcS68k);

      Pico_mcd->s68k_regs[0xe] = d;
      return;
  }

  if ((a&0xf0) == 0x10) {
      Pico_mcd->s68k_regs[a] = d;
      return;
  }

  dprintf("m68k: invalid write? [%02x] %02x", a, d);
}



static u32 s68k_reg_read16(u32 a)
{
  u32 d=0;

  // dprintf("s68k_regs r%2i: [%02x] @ %06x", realsize&~1, a+(realsize&1), SekPcS68k);

  switch (a) {
    case 0:
      d = ((Pico_mcd->s68k_regs[0]&3)<<8) | 1; // ver = 0, not in reset state
      goto end;
    case 2:
      d = (Pico_mcd->s68k_regs[a]<<8) | (Pico_mcd->s68k_regs[a+1]&0x1f);
      dprintf("s68k_regs r3: %02x @%06x", (u8)d, SekPc);
      goto end;
    case 6:
      d = CDC_Read_Reg();
      goto end;
    case 8:
      d = Read_CDC_Host(1); // Gens returns 0 here on byte reads
      goto end;
    case 0xC:
      dprintf("s68k stopwatch read");
      break;
    case 0x34: // fader
      d = 0; // no busy bit
      goto end;
  }

  d = (Pico_mcd->s68k_regs[a]<<8) | Pico_mcd->s68k_regs[a+1];

end:

  // dprintf("ret = %04x", d);

  return d;
}

static void s68k_reg_write8(u32 a, u32 d)
{
  //dprintf("s68k_regs w%2i: [%02x] %02x @ %06x", realsize, a, d, SekPcS68k);

  // TODO: review against Gens
  switch (a) {
    case 2:
      return; // only m68k can change WP
    case 3:
      dprintf("s68k_regs w3: %02x @%06x", (u8)d, SekPc);
      d &= 0x1d;
      if (d&4) {
        d |= Pico_mcd->s68k_regs[3]&0xc2;
        if ((d ^ Pico_mcd->s68k_regs[3]) & 5) d &= ~2; // in case of mode or bank change we clear DMNA (m68k req) bit
      } else {
        d |= Pico_mcd->s68k_regs[3]&0xc3;
        if (d&1) d &= ~2; // return word RAM to m68k in 2M mode
      }
      break;
    case 4:
      dprintf("s68k CDC dest: %x", d&7);
      Pico_mcd->s68k_regs[4] = (Pico_mcd->s68k_regs[4]&0xC0) | (d&7); // CDC mode
      return;
    case 5:
      //dprintf("s68k CDC reg addr: %x", d&0xf);
      break;
    case 7:
      CDC_Write_Reg(d);
      return;
    case 0xa:
      dprintf("s68k set CDC dma addr");
      break;
    case 0x33: // IRQ mask
      dprintf("s68k irq mask: %02x", d);
      if ((d&(1<<4)) && (Pico_mcd->s68k_regs[0x37]&4) && !(Pico_mcd->s68k_regs[0x33]&(1<<4))) {
        CDD_Export_Status();
	// counter75hz = 0; // ???
      }
      break;
    case 0x34: // fader
      Pico_mcd->s68k_regs[a] = (u8) d & 0x7f;
      return;
    case 0x36:
      return; // d/m bit is unsetable
    case 0x37: {
      u32 d_old = Pico_mcd->s68k_regs[0x37];
      Pico_mcd->s68k_regs[0x37] = d&7;
      if ((d&4) && !(d_old&4)) {
        CDD_Export_Status();
	// counter75hz = 0; // ???
      }
      return;
    }
    case 0x4b:
      Pico_mcd->s68k_regs[a] = (u8) d;
      CDD_Import_Command();
      return;
  }

  if ((a&0x1f0) == 0x10 || a == 0x0e || (a >= 0x38 && a < 0x42))
  {
    dprintf("m68k: invalid write @ %02x?", a);
    return;
  }

  Pico_mcd->s68k_regs[a] = (u8) d;
}





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

static u8 z80Read8(u32 a)
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
static u32 UnusualRead16(u32 a, int realsize)
{
  u32 d=0;

  dprintf("unusual r%i: %06x @%06x", realsize&~1, (a&0xfffffe)+(realsize&1), SekPc);


  dprintf("ret = %04x", d);
  return d;
}

static u32 OtherRead16(u32 a, int realsize)
{
  u32 d=0;

  if ((a&0xff0000)==0xa00000) {
    if ((a&0x4000)==0x0000) { d=z80Read8(a); d|=d<<8; goto end; } // Z80 ram (not byteswaped)
    if ((a&0x6000)==0x4000) { if(PicoOpt&1) d=YM2612Read(); else d=Pico.m.rotate++&3; goto end; } // 0x4000-0x5fff, Fudge if disabled
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
  if (a==0xa11100) { d=((Pico.m.z80Run&1)<<8)|0x8000|Pico.m.rotate++; goto end; }

  if ((a&0xe700e0)==0xc00000) { d=PicoVideoRead(a); goto end; }

  if ((a&0xffffc0)==0xa12000) {
    d=m68k_reg_read16(a);
    goto end;
  }

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
      if((PicoOpt&4) && Pico.m.z80Run==1) z80_run(20);
	  z80stopCycle = SekCyclesDone();
	  //z80ExtraCycles += (lineCycles>>1)-(lineCycles>>5); // only meaningful in PicoFrameHints()
	} else {
	  z80startCycle = SekCyclesDone();
	  //if(Pico.m.scanline != -1)
	  //z80ExtraCycles -= (lineCycles>>1)-(lineCycles>>5)+16;
	}
    //dprintf("set_zrun: %i [%i|%i] zPC=%04x @%06x", d, Pico.m.scanline, SekCyclesDone(), mz80GetRegisterValue(NULL, 0), SekPc);
	Pico.m.z80Run=(u8)d; return;
  }
  if (a==0xa11200) { if(!(d&1)) z80_reset(); return; }

  if ((a&0xff7f00)==0xa06000) // Z80 BANK register
  {
    Pico.m.z80_bank68k>>=1;
    Pico.m.z80_bank68k|=(d&1)<<8;
    Pico.m.z80_bank68k&=0x1ff; // 9 bits and filled in the new top one
    return;
  }

  if ((a&0xe700e0)==0xc00000) { PicoVideoWrite(a,(u16)(d|(d<<8))); return; } // Byte access gets mirrored

  if ((a&0xffffc0)==0xa12000) { m68k_reg_write8(a, d); return; }

  dprintf("strange w%i: %06x, %08x @%06x", realsize, a&0xffffff, d, SekPc);
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
  if (a==0xa11200) { if(!(d&0x100)) z80_reset(); return; }

  OtherWrite8(a,  d>>8, 16);
  OtherWrite8(a+1,d&0xff, 16);
}

// -----------------------------------------------------------------
//                     Read Rom and read Ram

u8 PicoReadM68k8(u32 a)
{
  u32 d=0;

  if ((a&0xe00000)==0xe00000) { d = *(u8 *)(Pico.ram+((a^1)&0xffff)); goto end; } // Ram

  a&=0xffffff;

  if (a < 0x20000) { d = *(u8 *)(Pico_mcd->bios+(a^1)); goto end; } // bios

  // prg RAM
  if ((a&0xfe0000)==0x020000) {
    u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
    d = *(prg_bank+((a^1)&0x1ffff));
    goto end;
  }

  // word RAM
  if ((a&0xfc0000)==0x200000) {
    dprintf("m68k_wram r8: [%06x] @%06x", a, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      if (a >= 0x220000) {
        dprintf("cell");
      } else {
        a=((a&0x1fffe)<<1)|(a&1);
	if (Pico_mcd->s68k_regs[3]&1) a+=2;
        d = Pico_mcd->word_ram[a^1];
      }
    } else {
      // allow access in any mode, like Gens does
      d = Pico_mcd->word_ram[(a^1)&0x3ffff];
    }
    dprintf("ret = %02x", (u8)d);
    goto end;
  }

  if ((a&0xff4000)==0xa00000) { d=z80Read8(a); goto end; } // Z80 Ram

  if ((a&0xffffc0)==0xa12000)
    rdprintf("m68k_regs r8: [%02x] @%06x", a&0x3f, SekPc);

  d=OtherRead16(a&~1, 8|(a&1)); if ((a&1)==0) d>>=8;

  if ((a&0xffffc0)==0xa12000)
    rdprintf("ret = %02x", (u8)d);

  end:

#ifdef __debug_io
  dprintf("r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPc);
#endif
  return (u8)d;
}


u16 PicoReadM68k16(u32 a)
{
  u16 d=0;

  if ((a&0xe00000)==0xe00000) { d=*(u16 *)(Pico.ram+(a&0xfffe)); goto end; } // Ram

  a&=0xfffffe;

  if (a < 0x20000) { d = *(u16 *)(Pico_mcd->bios+a); goto end; } // bios

  // prg RAM
  if ((a&0xfe0000)==0x020000) {
    u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
    d = *(u16 *)(prg_bank+(a&0x1fffe));
    goto end;
  }

  // word RAM
  if ((a&0xfc0000)==0x200000) {
    dprintf("m68k_wram r16: [%06x] @%06x", a, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      if (a >= 0x220000) {
        dprintf("cell");
      } else {
        a=((a&0x1fffe)<<1);
	if (Pico_mcd->s68k_regs[3]&1) a+=2;
        d = *(u16 *)(Pico_mcd->word_ram+a);
      }
    } else {
      // allow access in any mode, like Gens does
      d = *(u16 *)(Pico_mcd->word_ram+(a&0x3fffe));
    }
    dprintf("ret = %04x", d);
    goto end;
  }

  if ((a&0xffffc0)==0xa12000)
    rdprintf("m68k_regs r16: [%02x] @%06x", a&0x3f, SekPc);

  d = (u16)OtherRead16(a, 16);

  if ((a&0xffffc0)==0xa12000)
    rdprintf("ret = %04x", d);

  end:

#ifdef __debug_io
  dprintf("r16: %06x, %04x  @%06x", a&0xffffff, d, SekPc);
#endif
  return d;
}


u32 PicoReadM68k32(u32 a)
{
  u32 d=0;

  if ((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xfffe)); d = (pm[0]<<16)|pm[1]; goto end; } // Ram

  a&=0xfffffe;

  if (a < 0x20000) { u16 *pm=(u16 *)(Pico_mcd->bios+a); d = (pm[0]<<16)|pm[1]; goto end; } // bios

  // prg RAM
  if ((a&0xfe0000)==0x020000) {
    u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
    u16 *pm=(u16 *)(prg_bank+(a&0x1fffe));
    d = (pm[0]<<16)|pm[1];
    goto end;
  }

  // word RAM
  if ((a&0xfc0000)==0x200000) {
    dprintf("m68k_wram r32: [%06x] @%06x", a, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      if (a >= 0x220000) {
        dprintf("cell");
      } else {
        a=((a&0x1fffe)<<1);
	if (Pico_mcd->s68k_regs[3]&1) a+=2;
        d  = *(u16 *)(Pico_mcd->word_ram+a) << 16;
        d |= *(u16 *)(Pico_mcd->word_ram+a+4);
      }
    } else {
      // allow access in any mode, like Gens does
      u16 *pm=(u16 *)(Pico_mcd->word_ram+(a&0x3fffe)); d = (pm[0]<<16)|pm[1];
    }
    dprintf("ret = %08x", d);
    goto end;
  }

  if ((a&0xffffc0)==0xa12000)
    rdprintf("m68k_regs r32: [%02x] @%06x", a&0x3f, SekPc);

  d = (OtherRead16(a, 32)<<16)|OtherRead16(a+2, 32);

  if ((a&0xffffc0)==0xa12000)
    rdprintf("ret = %08x", d);

  end:
#ifdef __debug_io
  dprintf("r32: %06x, %08x @%06x", a&0xffffff, d, SekPc);
#endif
  return d;
}


// -----------------------------------------------------------------
//                            Write Ram

void PicoWriteM68k8(u32 a,u8 d)
{
#ifdef __debug_io
  dprintf("w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPc);
#endif
  //if ((a&0xe0ffff)==0xe0a9ba+0x69c)
  //  dprintf("w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPc);


  if ((a&0xe00000)==0xe00000) { // Ram
    *(u8 *)(Pico.ram+((a^1)&0xffff)) = d;
    return;
  }

  a&=0xffffff;

  // prg RAM
  if ((a&0xfe0000)==0x020000) {
    u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
    *(u8 *)(prg_bank+((a^1)&0x1ffff))=d;
    return;
  }

  // word RAM
  if ((a&0xfc0000)==0x200000) {
    dprintf("m68k_wram w8: [%06x] %02x @%06x", a, d, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      if (a >= 0x220000) {
        dprintf("cell");
      } else {
        a=((a&0x1fffe)<<1)|(a&1);
	if (Pico_mcd->s68k_regs[3]&1) a+=2;
        *(u8 *)(Pico_mcd->word_ram+(a^1))=d;
      }
    } else {
      // allow access in any mode, like Gens does
      *(u8 *)(Pico_mcd->word_ram+((a^1)&0x3ffff))=d;
    }
    return;
  }

  if ((a&0xffffc0)==0xa12000)
    rdprintf("m68k_regs w8: [%02x] %02x @%06x", a&0x3f, d, SekPc);

  OtherWrite8(a,d,8);
}


void PicoWriteM68k16(u32 a,u16 d)
{
#ifdef __debug_io
  dprintf("w16: %06x, %04x", a&0xffffff, d);
#endif
  //  dprintf("w16: %06x, %04x  @%06x", a&0xffffff, d, SekPc);

  if ((a&0xe00000)==0xe00000) { // Ram
    *(u16 *)(Pico.ram+(a&0xfffe))=d;
    return;
  }

  a&=0xfffffe;

  // prg RAM
  if ((a&0xfe0000)==0x020000) {
    u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
    *(u16 *)(prg_bank+(a&0x1fffe))=d;
    return;
  }

  // word RAM
  if ((a&0xfc0000)==0x200000) {
    dprintf("m68k_wram w16: [%06x] %04x @%06x", a, d, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      if (a >= 0x220000) {
        dprintf("cell");
      } else {
        a=((a&0x1fffe)<<1);
	if (Pico_mcd->s68k_regs[3]&1) a+=2;
        *(u16 *)(Pico_mcd->word_ram+a)=d;
      }
    } else {
      // allow access in any mode, like Gens does
      *(u16 *)(Pico_mcd->word_ram+(a&0x3fffe))=d;
    }
    return;
  }

  if ((a&0xffffc0)==0xa12000)
    rdprintf("m68k_regs w16: [%02x] %04x @%06x", a&0x3f, d, SekPc);

  OtherWrite16(a,d);
}


void PicoWriteM68k32(u32 a,u32 d)
{
#ifdef __debug_io
  dprintf("w32: %06x, %08x", a&0xffffff, d);
#endif

  if ((a&0xe00000)==0xe00000)
  {
    // Ram:
    u16 *pm=(u16 *)(Pico.ram+(a&0xfffe));
    pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    return;
  }

  a&=0xfffffe;

  // prg RAM
  if ((a&0xfe0000)==0x020000) {
    u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
    u16 *pm=(u16 *)(prg_bank+(a&0x1fffe));
    pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    return;
  }

  // word RAM
  if ((a&0xfc0000)==0x200000) {
    if (d != 0) // don't log clears
      dprintf("m68k_wram w32: [%06x] %08x @%06x", a, d, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      if (a >= 0x220000) {
        dprintf("cell");
      } else {
        a=((a&0x1fffe)<<1);
	if (Pico_mcd->s68k_regs[3]&1) a+=2;
        *(u16 *)(Pico_mcd->word_ram+a) = d>>16;
        *(u16 *)(Pico_mcd->word_ram+a+4) = d;
      }
    } else {
      // allow access in any mode, like Gens does
      u16 *pm=(u16 *)(Pico_mcd->word_ram+(a&0x3fffe));
      pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    }
    return;
  }

  if ((a&0xffffc0)==0xa12000)
    rdprintf("m68k_regs w32: [%02x] %08x @%06x", a&0x3f, d, SekPc);

#if 0
  if ((a&0x3f) == 0x1c && SekPc == 0xffff05ba)
  {
	  int i;
	  FILE *ff;
	  unsigned short *ram = (unsigned short *) Pico.ram;
	  // unswap and dump RAM
	  for (i = 0; i < 0x10000/2; i++)
		  ram[i] = (ram[i]>>8) | (ram[i]<<8);
	  ff = fopen("ram.bin", "wb");
	  fwrite(ram, 1, 0x10000, ff);
	  fclose(ff);
	  exit(0);
  }
#endif

  OtherWrite16(a,  (u16)(d>>16));
  OtherWrite16(a+2,(u16)d);
}


// -----------------------------------------------------------------


u8 PicoReadS68k8(u32 a)
{
  u32 d=0;

  a&=0xffffff;

  // prg RAM
  if (a < 0x80000) {
    d = *(Pico_mcd->prg_ram+(a^1));
    goto end;
  }

  // regs
  if ((a&0xfffe00) == 0xff8000) {
    a &= 0x1ff;
    rdprintf("s68k_regs r8: [%02x] @ %06x", a, SekPcS68k);
    if (a >= 0x50 && a < 0x68)
         d = gfx_cd_read(a&~1);
    else d = s68k_reg_read16(a&~1);
    if ((a&1)==0) d>>=8;
    rdprintf("ret = %02x", (u8)d);
    goto end;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    dprintf("s68k_wram2M r8: [%06x] @%06x", a, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      // TODO (decode)
      dprintf("(decode)");
    } else {
      // allow access in any mode, like Gens does
      d = Pico_mcd->word_ram[(a^1)&0x3ffff];
    }
    dprintf("ret = %02x", (u8)d);
    goto end;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    dprintf("s68k_wram1M r8: [%06x] @%06x", a, SekPc);
    a=((a&0x1fffe)<<1)|(a&1);
    if (!(Pico_mcd->s68k_regs[3]&1)) a+=2;
    d = Pico_mcd->word_ram[a^1];
    dprintf("ret = %02x", (u8)d);
    goto end;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    d = Pico_mcd->bram[(a>>1)&0x1fff];
    goto end;
  }

  dprintf("s68k r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPcS68k);

  end:

#ifdef __debug_io2
  dprintf("s68k r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPcS68k);
#endif
  return (u8)d;
}


u16 PicoReadS68k16(u32 a)
{
  u16 d=0;

  a&=0xfffffe;

  // prg RAM
  if (a < 0x80000) {
    d = *(u16 *)(Pico_mcd->prg_ram+a);
    goto end;
  }

  // regs
  if ((a&0xfffe00) == 0xff8000) {
    a &= 0x1fe;
    rdprintf("s68k_regs r16: [%02x] @ %06x", a, SekPcS68k);
    if (a >= 0x50 && a < 0x68)
         d = gfx_cd_read(a);
    else d = s68k_reg_read16(a);
    rdprintf("ret = %04x", d);
    goto end;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    dprintf("s68k_wram2M r16: [%06x] @%06x", a, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      // TODO (decode)
      dprintf("(decode)");
    } else {
      // allow access in any mode, like Gens does
      d = *(u16 *)(Pico_mcd->word_ram+(a&0x3fffe));
    }
    dprintf("ret = %04x", d);
    goto end;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    dprintf("s68k_wram1M r16: [%06x] @%06x", a, SekPc);
    a=((a&0x1fffe)<<1);
    if (!(Pico_mcd->s68k_regs[3]&1)) a+=2;
    d = *(u16 *)(Pico_mcd->word_ram+a);
    dprintf("ret = %04x", d);
    goto end;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    dprintf("s68k_bram r16: [%06x] @%06x", a, SekPc);
    a = (a>>1)&0x1fff;
    d = Pico_mcd->bram[a++];		// Gens does little endian here, an so do we..
    d|= Pico_mcd->bram[a++] << 8;
    dprintf("ret = %04x", d);
    goto end;
  }

  dprintf("s68k r16: %06x, %04x  @%06x", a&0xffffff, d, SekPcS68k);

  end:

#ifdef __debug_io2
  dprintf("s68k r16: %06x, %04x  @%06x", a&0xffffff, d, SekPcS68k);
#endif
  return d;
}


u32 PicoReadS68k32(u32 a)
{
  u32 d=0;

  a&=0xfffffe;

  // prg RAM
  if (a < 0x80000) {
    u16 *pm=(u16 *)(Pico_mcd->prg_ram+a);
    d = (pm[0]<<16)|pm[1];
    goto end;
  }

  // regs
  if ((a&0xfffe00) == 0xff8000) {
    a &= 0x1fe;
    rdprintf("s68k_regs r32: [%02x] @ %06x", a, SekPcS68k);
    if (a >= 0x50 && a < 0x68)
         d = (gfx_cd_read(a)<<16)|gfx_cd_read(a+2);
    else d = (s68k_reg_read16(a)<<16)|s68k_reg_read16(a+2);
    rdprintf("ret = %08x", d);
    goto end;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    dprintf("s68k_wram2M r32: [%06x] @%06x", a, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      // TODO (decode)
      dprintf("(decode)");
    } else {
      // allow access in any mode, like Gens does
      u16 *pm=(u16 *)(Pico_mcd->word_ram+(a&0x3fffe)); d = (pm[0]<<16)|pm[1];
    }
    dprintf("ret = %08x", d);
    goto end;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    dprintf("s68k_wram1M r32: [%06x] @%06x", a, SekPc);
    a=((a&0x1fffe)<<1);
    if (!(Pico_mcd->s68k_regs[3]&1)) a+=2;
    d  = *(u16 *)(Pico_mcd->word_ram+a) << 16;
    d |= *(u16 *)(Pico_mcd->word_ram+a+4);
    dprintf("ret = %08x", d);
    goto end;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    dprintf("s68k_bram r32: [%06x] @%06x", a, SekPc);
    a = (a>>1)&0x1fff;
    d = Pico_mcd->bram[a++] << 16;		// middle endian? TODO: verify against Fusion..
    d|= Pico_mcd->bram[a++] << 24;
    d|= Pico_mcd->bram[a++];
    d|= Pico_mcd->bram[a++] << 8;
    dprintf("ret = %08x", d);
    goto end;
  }

  dprintf("s68k r32: %06x, %08x @%06x", a&0xffffff, d, SekPcS68k);

  end:

#ifdef __debug_io2
  dprintf("s68k r32: %06x, %08x @%06x", a&0xffffff, d, SekPcS68k);
#endif
  return d;
}


// -----------------------------------------------------------------

void PicoWriteS68k8(u32 a,u8 d)
{
#ifdef __debug_io2
  dprintf("s68k w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPcS68k);
#endif

  a&=0xffffff;

  // prg RAM
  if (a < 0x80000) {
    u8 *pm=(u8 *)(Pico_mcd->prg_ram+(a^1));
    *pm=d;
    return;
  }

  if (a != 0xff0011 && (a&0xff8000) == 0xff0000) // PCM hack
    return;

  // regs
  if ((a&0xfffe00) == 0xff8000) {
    a &= 0x1ff;
    rdprintf("s68k_regs w8: [%02x] %02x @ %06x", a, d, SekPcS68k);
    if (a >= 0x50 && a < 0x68)
         gfx_cd_write(a&~1, (d<<8)|d);
    else s68k_reg_write8(a,d);
    return;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    dprintf("s68k_wram2M w8: [%06x] %02x @%06x", a, d, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      // TODO (decode)
      dprintf("(decode)");
    } else {
      // allow access in any mode, like Gens does
      *(u8 *)(Pico_mcd->word_ram+((a^1)&0x3ffff))=d;
    }
    return;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    if (d)
      dprintf("s68k_wram1M w8: [%06x] %02x @%06x", a, d, SekPc);
    a=((a&0x1fffe)<<1)|(a&1);
    if (!(Pico_mcd->s68k_regs[3]&1)) a+=2;
    *(u8 *)(Pico_mcd->word_ram+(a^1))=d;
    return;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    Pico_mcd->bram[(a>>1)&0x1fff] = d;
    SRam.changed = 1;
    return;
  }

  dprintf("s68k w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPcS68k);
}


void PicoWriteS68k16(u32 a,u16 d)
{
#ifdef __debug_io2
  dprintf("s68k w16: %06x, %04x @%06x", a&0xffffff, d, SekPcS68k);
#endif

  a&=0xfffffe;

  // prg RAM
  if (a < 0x80000) {
    *(u16 *)(Pico_mcd->prg_ram+a)=d;
    return;
  }

  // regs
  if ((a&0xfffe00) == 0xff8000) {
    a &= 0x1fe;
    rdprintf("s68k_regs w16: [%02x] %04x @ %06x", a, d, SekPcS68k);
    if (a >= 0x50 && a < 0x68)
      gfx_cd_write(a, d);
    else {
      s68k_reg_write8(a,  d>>8);
      s68k_reg_write8(a+1,d&0xff);
    }
    return;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    dprintf("s68k_wram2M w16: [%06x] %04x @%06x", a, d, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      // TODO (decode)
      dprintf("(decode)");
    } else {
      // allow access in any mode, like Gens does
      *(u16 *)(Pico_mcd->word_ram+(a&0x3fffe))=d;
    }
    return;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    if (d)
      dprintf("s68k_wram1M w16: [%06x] %04x @%06x", a, d, SekPc);
    a=((a&0x1fffe)<<1);
    if (!(Pico_mcd->s68k_regs[3]&1)) a+=2;
    *(u16 *)(Pico_mcd->word_ram+a)=d;
    return;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    dprintf("s68k_bram w16: [%06x] %04x @%06x", a, d, SekPc);
    a = (a>>1)&0x1fff;
    Pico_mcd->bram[a++] = d;		// Gens does little endian here, an so do we..
    Pico_mcd->bram[a++] = d >> 8;
    SRam.changed = 1;
    return;
  }

  dprintf("s68k w16: %06x, %04x @%06x", a&0xffffff, d, SekPcS68k);
}


void PicoWriteS68k32(u32 a,u32 d)
{
#ifdef __debug_io2
  dprintf("s68k w32: %06x, %08x @%06x", a&0xffffff, d, SekPcS68k);
#endif

  a&=0xfffffe;

  // prg RAM
  if (a < 0x80000) {
    u16 *pm=(u16 *)(Pico_mcd->prg_ram+a);
    pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    return;
  }

  // regs
  if ((a&0xfffe00) == 0xff8000) {
    a &= 0x1fe;
    rdprintf("s68k_regs w32: [%02x] %08x @ %06x", a, d, SekPcS68k);
    if (a >= 0x50 && a < 0x68) {
      gfx_cd_write(a,   d>>16);
      gfx_cd_write(a+2, d&0xffff);
    } else {
      s68k_reg_write8(a,   d>>24);
      s68k_reg_write8(a+1,(d>>16)&0xff);
      s68k_reg_write8(a+2,(d>>8) &0xff);
      s68k_reg_write8(a+3, d     &0xff);
    }
    return;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    dprintf("s68k_wram2M w32: [%06x] %08x @%06x", a, d, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      // TODO (decode)
      dprintf("(decode)");
    } else {
      // allow access in any mode, like Gens does
      u16 *pm=(u16 *)(Pico_mcd->word_ram+(a&0x3fffe));
      pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    }
    return;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    if (d)
      dprintf("s68k_wram1M w32: [%06x] %08x @%06x", a, d, SekPc);
    a=((a&0x1fffe)<<1);
    if (!(Pico_mcd->s68k_regs[3]&1)) a+=2;
    *(u16 *)(Pico_mcd->word_ram+a) = d>>16;
    *(u16 *)(Pico_mcd->word_ram+a+4) = d;
    return;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    dprintf("s68k_bram w32: [%06x] %08x @%06x", a, d, SekPc);
    a = (a>>1)&0x1fff;
    Pico_mcd->bram[a++] = d >> 16;		// middle endian? verify?
    Pico_mcd->bram[a++] = d >> 24;
    Pico_mcd->bram[a++] = d;
    Pico_mcd->bram[a++] = d >> 8;
    SRam.changed = 1;
    return;
  }

  dprintf("s68k w32: %06x, %08x @%06x", a&0xffffff, d, SekPcS68k);
}



// -----------------------------------------------------------------

#ifdef EMU_M68K
unsigned char  PicoReadCD8w (unsigned int a) {
	return m68ki_cpu_p == &PicoS68kCPU ? PicoReadS68k8(a) : PicoReadM68k8(a);
}
unsigned short PicoReadCD16w(unsigned int a) {
	return m68ki_cpu_p == &PicoS68kCPU ? PicoReadS68k16(a) : PicoReadM68k16(a);
}
unsigned int   PicoReadCD32w(unsigned int a) {
	return m68ki_cpu_p == &PicoS68kCPU ? PicoReadS68k32(a) : PicoReadM68k32(a);
}
void PicoWriteCD8w (unsigned int a, unsigned char d) {
	if (m68ki_cpu_p == &PicoS68kCPU) PicoWriteS68k8(a, d); else PicoWriteM68k8(a, d);
}
void PicoWriteCD16w(unsigned int a, unsigned short d) {
	if (m68ki_cpu_p == &PicoS68kCPU) PicoWriteS68k16(a, d); else PicoWriteM68k16(a, d);
}
void PicoWriteCD32w(unsigned int a, unsigned int d) {
	if (m68ki_cpu_p == &PicoS68kCPU) PicoWriteS68k32(a, d); else PicoWriteM68k32(a, d);
}

// these are allowed to access RAM
unsigned int  m68k_read_pcrelative_CD8 (unsigned int a) {
  a&=0xffffff;
  if(m68ki_cpu_p == &PicoS68kCPU) {
    if (a < 0x80000) return *(u8 *)(Pico_mcd->prg_ram+(a^1)); // PRG Ram
    else dprintf("s68k read_pcrel8 @ %06x", a);
  } else {
    if(a<Pico.romsize)         return *(u8 *)(Pico.rom+(a^1)); // Rom
    if((a&0xe00000)==0xe00000) return *(u8 *)(Pico.ram+((a^1)&0xffff)); // Ram
  }
  return 0;//(u8)  lastread_d;
}
unsigned int  m68k_read_pcrelative_CD16(unsigned int a) {
  a&=0xffffff;
  if(m68ki_cpu_p == &PicoS68kCPU) {
    if (a < 0x80000) return *(u16 *)(Pico_mcd->prg_ram+(a&~1)); // PRG Ram
    else dprintf("s68k read_pcrel16 @ %06x", a);
  } else {
    if(a<Pico.romsize)         return *(u16 *)(Pico.rom+(a&~1)); // Rom
    if((a&0xe00000)==0xe00000) return *(u16 *)(Pico.ram+(a&0xfffe)); // Ram
  }
  return 0;//(u16) lastread_d;
}
unsigned int  m68k_read_pcrelative_CD32(unsigned int a) {
  a&=0xffffff;
  if(m68ki_cpu_p == &PicoS68kCPU) {
    if (a < 0x80000) { u16 *pm=(u16 *)(Pico_mcd->prg_ram+(a&~1)); return (pm[0]<<16)|pm[1]; } // PRG Ram
    else dprintf("s68k read_pcrel32 @ %06x", a);
  } else {
    if(a<Pico.romsize)         { u16 *pm=(u16 *)(Pico.rom+(a&~1));     return (pm[0]<<16)|pm[1]; }
    if((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xfffe)); return (pm[0]<<16)|pm[1]; } // Ram
  }
  return 0; //lastread_d;
}
#endif // EMU_M68K

