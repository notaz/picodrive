// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006-2007, Grazvydas "notaz" Ignotas
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"
#include "cd/gfx_cd.h"

extern const unsigned char  hcounts_32[];
extern const unsigned char  hcounts_40[];
extern const unsigned short vcounts[];
extern int rendstatus;

#ifndef UTYPES_DEFINED
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
#define UTYPES_DEFINED
#endif


static __inline void AutoIncrement(void)
{
  Pico.video.addr=(unsigned short)(Pico.video.addr+Pico.video.reg[0xf]);
}

static void VideoWrite(u16 d)
{
  unsigned int a=Pico.video.addr;

  switch (Pico.video.type)
  {
    case 1: if(a&1) d=(u16)((d<<8)|(d>>8)); // If address is odd, bytes are swapped (which game needs this?)
            Pico.vram [(a>>1)&0x7fff]=d;
            rendstatus|=0x10; break;
    case 3: Pico.m.dirtyPal = 1;
            Pico.cram [(a>>1)&0x003f]=d; break; // wraps (Desert Strike)
    case 5: Pico.vsram[(a>>1)&0x003f]=d; break;
    default:elprintf(EL_ANOMALY, "VDP write %04x with bad type %i", d, Pico.video.type); break;
  }

  //dprintf("w[%i] @ %04x, inc=%i [%i|%i]", Pico.video.type, a, Pico.video.reg[0xf], Pico.m.scanline, SekCyclesDone());
  AutoIncrement();
}

static unsigned int VideoRead(void)
{
  unsigned int a=0,d=0;

  a=Pico.video.addr; a>>=1;

  switch (Pico.video.type)
  {
    case 0: d=Pico.vram [a&0x7fff]; break;
    case 8: d=Pico.cram [a&0x003f]; break;
    case 4: d=Pico.vsram[a&0x003f]; break;
    default:elprintf(EL_ANOMALY, "VDP read with bad type %i", Pico.video.type); break;
  }

  AutoIncrement();
  return d;
}

static int GetDmaLength(void)
{
  struct PicoVideo *pvid=&Pico.video;
  int len=0;
  // 16-bit words to transfer:
  len =pvid->reg[0x13];
  len|=pvid->reg[0x14]<<8;
  // Charles MacDonald:
  if(!len) len = 0xffff;
  return len;
}

static void DmaSlow(int len)
{
  u16 *pd=0, *pdend, *r;
  unsigned int a=Pico.video.addr, a2, d;
  unsigned char inc=Pico.video.reg[0xf];
  unsigned int source;

  source =Pico.video.reg[0x15]<<1;
  source|=Pico.video.reg[0x16]<<9;
  source|=Pico.video.reg[0x17]<<17;

  elprintf(EL_VDPDMA, "DmaSlow[%i] %06x->%04x len %i inc=%i blank %i [%i] @ %06x",
    Pico.video.type, source, a, len, inc, (Pico.video.status&8)||!(Pico.video.reg[1]&0x40),
    SekCyclesDone(), SekPc);

  if(Pico.m.scanline != -1) {
    Pico.m.dma_xfers += len;
    if ((PicoMCD&1) && (PicoOpt & 0x2000)) SekCyclesBurn(CheckDMA());
    else SekSetCyclesLeftNoMCD(SekCyclesLeftNoMCD - CheckDMA());
  } else {
    // be approximate in non-accurate mode
    SekSetCyclesLeft(SekCyclesLeft - (len*(((488<<8)/167))>>8));
  }

  if ((source&0xe00000)==0xe00000) { // Ram
    pd=(u16 *)(Pico.ram+(source&0xfffe));
    pdend=(u16 *)(Pico.ram+0x10000);
  } else if(PicoMCD & 1) {
    elprintf(EL_VDPDMA, "DmaSlow CD, r3=%02x", Pico_mcd->s68k_regs[3]);
    if(source<0x20000) { // Bios area
      pd=(u16 *)(Pico_mcd->bios+(source&~1));
      pdend=(u16 *)(Pico_mcd->bios+0x20000);
    } else if ((source&0xfc0000)==0x200000) { // Word Ram
      source -= 2;
      if (!(Pico_mcd->s68k_regs[3]&4)) { // 2M mode
        pd=(u16 *)(Pico_mcd->word_ram2M+(source&0x3fffe));
        pdend=(u16 *)(Pico_mcd->word_ram2M+0x40000);
      } else {
        if (source < 0x220000) { // 1M mode
          int bank = Pico_mcd->s68k_regs[3]&1;
          pd=(u16 *)(Pico_mcd->word_ram1M[bank]+(source&0x1fffe));
          pdend=(u16 *)(Pico_mcd->word_ram1M[bank]+0x20000);
        } else {
          DmaSlowCell(source, a, len, inc);
          return;
        }
      }
    } else if ((source&0xfe0000)==0x020000) { // Prg Ram
      u8 *prg_ram = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
      pd=(u16 *)(prg_ram+(source&0x1fffe));
      pdend=(u16 *)(prg_ram+0x20000);
    } else {
      elprintf(EL_VDPDMA|EL_ANOMALY, "DmaSlow[%i] %06x->%04x: FIXME: unsupported src", Pico.video.type, source, a);
      return;
    }
  } else {
    if(source<Pico.romsize) { // Rom
      pd=(u16 *)(Pico.rom+(source&~1));
      pdend=(u16 *)(Pico.rom+Pico.romsize);
    } else {
      elprintf(EL_VDPDMA|EL_ANOMALY, "DmaSlow[%i] %06x->%04x: invalid src", Pico.video.type, source, a);
      return;
    }
  }

  // overflow protection, might break something..
  if (len > pdend - pd) {
    len = pdend - pd;
    elprintf(EL_VDPDMA|EL_ANOMALY, "DmaSlow overflow");
  }

  switch (Pico.video.type)
  {
    case 1: // vram
      r = Pico.vram;
      if (inc == 2 && !(a&1) && a+len*2 < 0x10000)
      {
        // most used DMA mode
        memcpy16(r + (a>>1), pd, len);
        a += len*2;
      }
      else
      {
        for(; len; len--)
        {
          d=*pd++;
          if(a&1) d=(d<<8)|(d>>8);
          r[a>>1] = (u16)d; // will drop the upper bits
          // AutoIncrement
          a=(u16)(a+inc);
          // didn't src overlap?
          //if(pd >= pdend) pd-=0x8000; // should be good for RAM, bad for ROM
        }
      }
      rendstatus|=0x10;
      break;

    case 3: // cram
      Pico.m.dirtyPal = 1;
      r = Pico.cram;
      for(a2=a&0x7f; len; len--)
      {
        r[a2>>1] = (u16)*pd++; // bit 0 is ignored
        // AutoIncrement
        a2+=inc;
        // didn't src overlap?
        //if(pd >= pdend) pd-=0x8000;
        // good dest?
        if(a2 >= 0x80) break; // Todds Adventures in Slime World / Andre Agassi tennis
      }
      a=(a&0xff00)|a2;
      break;

    case 5: // vsram[a&0x003f]=d;
      r = Pico.vsram;
      for(a2=a&0x7f; len; len--)
      {
        r[a2>>1] = (u16)*pd++;
        // AutoIncrement
        a2+=inc;
        // didn't src overlap?
        //if(pd >= pdend) pd-=0x8000;
        // good dest?
        if(a2 >= 0x80) break;
      }
      a=(a&0xff00)|a2;
      break;

    default:
      elprintf(EL_VDPDMA|EL_ANOMALY, "DMA with bad type %i", Pico.video.type);
      break;
  }
  // remember addr
  Pico.video.addr=(u16)a;
}

static void DmaCopy(int len)
{
  u16 a=Pico.video.addr;
  unsigned char *vr = (unsigned char *) Pico.vram;
  unsigned char *vrs;
  unsigned char inc=Pico.video.reg[0xf];
  int source;
  elprintf(EL_VDPDMA, "DmaCopy len %i [%i]", len, SekCyclesDone());

  Pico.m.dma_xfers += len;
  if(Pico.m.scanline != -1)
    Pico.video.status|=2; // dma busy

  source =Pico.video.reg[0x15];
  source|=Pico.video.reg[0x16]<<8;
  vrs=vr+source;

  if(source+len > 0x10000) len=0x10000-source; // clip??

  for(;len;len--)
  {
    vr[a] = *vrs++;
    // AutoIncrement
    a=(u16)(a+inc);
  }
  // remember addr
  Pico.video.addr=a;
  rendstatus|=0x10;
}

// check: Contra, Megaman
// note: this is still inaccurate
static void DmaFill(int data)
{
  int len;
  unsigned short a=Pico.video.addr;
  unsigned char *vr=(unsigned char *) Pico.vram;
  unsigned char high = (unsigned char) (data >> 8);
  unsigned char inc=Pico.video.reg[0xf];

  len=GetDmaLength();
  elprintf(EL_VDPDMA, "DmaFill len %i inc %i [%i]", len, inc, SekCyclesDone());

  Pico.m.dma_xfers += len;
  if(Pico.m.scanline != -1)
    Pico.video.status|=2; // dma busy (in accurate mode)

  // from Charles MacDonald's genvdp.txt:
  // Write lower byte to address specified
  vr[a] = (unsigned char) data;
  a=(u16)(a+inc);

  if(!inc) len=1;

  for(;len;len--) {
    // Write upper byte to adjacent address
    // (here we are byteswapped, so address is already 'adjacent')
    vr[a] = high;

    // Increment address register
    a=(u16)(a+inc);
  }
  // remember addr
  Pico.video.addr=a;
  // update length
  Pico.video.reg[0x13] = Pico.video.reg[0x14] = 0; // Dino Dini's Soccer (E) (by Haze)

  rendstatus|=0x10;
}

static void CommandDma(void)
{
  struct PicoVideo *pvid=&Pico.video;
  int len=0,method=0;

  if ((pvid->reg[1]&0x10)==0) return; // DMA not enabled

  len=GetDmaLength();

  method=pvid->reg[0x17]>>6;
  if (method< 2) DmaSlow(len); // 68000 to VDP
  if (method==3) DmaCopy(len); // VRAM Copy
}

static void CommandChange(void)
{
  struct PicoVideo *pvid=&Pico.video;
  unsigned int cmd=0,addr=0;

  cmd=pvid->command;

  // Get type of transfer 0xc0000030 (v/c/vsram read/write)
  pvid->type=(unsigned char)(((cmd>>2)&0xc)|(cmd>>30));

  // Get address 0x3fff0003
  addr =(cmd>>16)&0x3fff;
  addr|=(cmd<<14)&0xc000;
  pvid->addr=(unsigned short)addr;

  // Check for dma:
  if (cmd&0x80) CommandDma();
}

PICO_INTERNAL_ASM void PicoVideoWrite(unsigned int a,unsigned short d)
{
  struct PicoVideo *pvid=&Pico.video;

  a&=0x1c;

  if (a==0x00) // Data port 0 or 2
  {
    if (pvid->pending) {
      CommandChange();
      pvid->pending=0;
    }

    // If a DMA fill has been set up, do it
    if ((pvid->command&0x80) && (pvid->reg[1]&0x10) && (pvid->reg[0x17]>>6)==2)
    {
      DmaFill(d);
    }
    else
    {
      // preliminary FIFO emulation for Chaos Engine, The (E)
      if(!(pvid->status&8) && (pvid->reg[1]&0x40) && Pico.m.scanline!=-1 && !(PicoOpt&0x10000)) // active display, accurate mode?
      {
        pvid->status&=~0x200; // FIFO no longer empty
        pvid->lwrite_cnt++;
        if (pvid->lwrite_cnt >= 4) pvid->status|=0x100; // FIFO full
        if (pvid->lwrite_cnt >  4) {
          SekCyclesBurn(32); // penalty // 488/12-8
          if (SekCycleCnt>=SekCycleAim) SekEndRun(0);
        }
        elprintf(EL_ASVDP, "VDP data write: %04x {%i} #%i @ %06x", d, Pico.video.type, pvid->lwrite_cnt, SekPc);
      }
      VideoWrite(d);
    }
    return;
  }

  if (a==0x04) // Control (command) port 4 or 6
  {
    if(pvid->pending)
    {
      // Low word of command:
      pvid->command&=0xffff0000;
      pvid->command|=d;
      pvid->pending=0;
      CommandChange();
    } else {
      if((d&0xc000)==0x8000)
      {
        // Register write:
        int num=(d>>8)&0x1f;
        if(num==00) elprintf(EL_INTSW, "hint_onoff: %i->%i [%i] pend=%i @ %06x", (pvid->reg[0]&0x10)>>4,
                        (d&0x10)>>4, SekCyclesDone(), (pvid->pending_ints&0x10)>>4, SekPc);
        if(num==01) elprintf(EL_INTSW, "vint_onoff: %i->%i [%i] pend=%i @ %06x", (pvid->reg[1]&0x20)>>5,
                        (d&0x20)>>5, SekCyclesDone(), (pvid->pending_ints&0x20)>>5, SekPc);
        //if(num==01) dprintf("set_blank: %i @ %06x [%i|%i]", !((d&0x40)>>6), SekPc, Pico.m.scanline, SekCyclesDone());
        //if(num==10) dprintf("hint_set: %i @ %06x [%i|%i]", (unsigned char)d, SekPc, Pico.m.scanline, SekCyclesDone());
        pvid->reg[num]=(unsigned char)d;
#ifndef EMU_CORE_DEBUG
        // update IRQ level (Lemmings, Wiz 'n' Liz intro, ... )
        // may break if done improperly:
        // International Superstar Soccer Deluxe (crash), Street Racer (logos), Burning Force (gfx),
        // Fatal Rewind (crash), Sesame Street Counting Cafe
        if (num < 2)
        {
          if (!SekShouldInterrupt) // hack
          {
            int lines, pints, irq=0;
            lines = (pvid->reg[1] & 0x20) | (pvid->reg[0] & 0x10);
            pints = (pvid->pending_ints&lines);
                 if(pints & 0x20) irq = 6;
            else if(pints & 0x10) irq = 4;
            SekInterrupt(irq); // update line

            if (irq) SekEndRun(24); // make it delayed
          }
        }
        else
#endif
        if(num == 5) rendstatus|=1;
        else if(num == 0xc) Pico.m.dirtyPal = 2; // renderers should update their palettes if sh/hi mode is changed
        pvid->type=0; // register writes clear command (else no Sega logo in Golden Axe II)
      } else {
        // High word of command:
        pvid->command&=0x0000ffff;
        pvid->command|=d<<16;
        pvid->pending=1;
      }
    }
  }
}

PICO_INTERNAL_ASM unsigned int PicoVideoRead(unsigned int a)
{
  unsigned int d=0;

  a&=0x1c;


  if (a==0x00) // data port
  {
    d=VideoRead();
    goto end;
  }

  if (a==0x04) // control port
  {
    struct PicoVideo *pv=&Pico.video;
    d=pv->status;
    if (PicoOpt&0x10)         d|=0x0020; // sprite collision (Shadow of the Beast)
    if (!(pv->reg[1]&0x40))   d|=0x0008; // set V-Blank if display is disabled
    if (SekCyclesLeft < 84+4) d|=0x0004; // H-Blank (Sonic3 vs)

    d|=(pv->pending_ints&0x20)<<2; // V-int pending?
    if (d&0x100) pv->status&=~0x100; // FIFO no longer full

    pv->pending=0; // ctrl port reads clear write-pending flag (Charles MacDonald)

    elprintf(EL_SR, "SR read: %04x @ %06x", d, SekPc);
    goto end;
  }

  // H-counter info (based on Generator):
  // frame:
  //                       |       <- hblank? ->      |
  // start    <416>       hint  <36> hdisplay <38>  end // CPU cycles
  // |---------...---------|------------|-------------|
  // 0                   B6 E4                       FF // 40 cells
  // 0                   93 E8                       FF // 32 cells

  // Gens (?)              v-render
  // start  <hblank=84>   hint    hdisplay <404>      |
  // |---------------------|--------------------------|
  // E4  (hc[0x43]==0)    07                         B1 // 40
  // E8  (hc[0x45]==0)    05                         91 // 32

  // check: Sonic 3D Blast bonus, Cannon Fodder, Chase HQ II, 3 Ninjas kick back, Road Rash 3, Skitchin', Wheel of Fortune
  if ((a&0x1c)==0x08)
  {
    unsigned int hc;

    if(Pico.m.scanline != -1) {
      int lineCycles=(488-SekCyclesLeft)&0x1ff;
      d=Pico.m.scanline; // V-Counter

      if(Pico.video.reg[12]&1)
           hc=hcounts_40[lineCycles];
      else hc=hcounts_32[lineCycles];

      //if(lineCycles > 488-12) d++; // Wheel of Fortune
    } else {
      // get approximate V-Counter
      d=vcounts[SekCyclesDone()>>8];
      hc = Pico.m.rotate&0xff;
    }

    if(Pico.m.pal) {
      if(d >= 0x103) d-=56; // based on Gens
    } else {
      if(d >= 0xEB)  d-=6;
    }

    if((Pico.video.reg[12]&6) == 6) {
      // interlace mode 2 (Combat Cars (UE) [!])
      d <<= 1;
      if (d&0xf00) d|= 1;
    }

    elprintf(EL_HVCNT, "hv: %02x %02x (%i) @ %06x", hc, d, SekCyclesDone(), SekPc);
    d&=0xff; d<<=8;
    d|=hc;
    goto end;
  }

end:

  return d;
}
