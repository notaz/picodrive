// Memory I/O handlers for Sega/Mega CD.
// Loosely based on Gens code.
// (c) Copyright 2007, Grazvydas "notaz" Ignotas


#include "../pico_int.h"

#include "../sound/ym2612.h"
#include "../sound/sn76496.h"

#include "gfx_cd.h"
#include "pcm.h"

#ifndef UTYPES_DEFINED
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
#define UTYPES_DEFINED
#endif

#ifdef _MSC_VER
#define rdprintf
#define wrdprintf
#define r3printf
#else
//#define rdprintf dprintf
#define rdprintf(...)
//#define wrdprintf dprintf
#define wrdprintf(...)
//#define r3printf elprintf
#define r3printf(...)
#endif

#ifdef EMU_CORE_DEBUG
extern u32 lastread_a, lastread_d[16], lastwrite_cyc_d[16];
extern int lrp_cyc, lwp_cyc;
#undef USE_POLL_DETECT
#endif

// -----------------------------------------------------------------

// poller detection
#define POLL_LIMIT 16
#define POLL_CYCLES 124
// int m68k_poll_addr, m68k_poll_cnt;
unsigned int s68k_poll_adclk, s68k_poll_cnt;

#ifndef _ASM_CD_MEMORY_C
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
      r3printf(EL_STATUS, "m68k_regs r3: %02x @%06x", (u8)d, SekPc);
      goto end;
    case 4:
      d = Pico_mcd->s68k_regs[4]<<8;
      goto end;
    case 6:
      d = *(u16 *)(Pico_mcd->bios + 0x72);
      goto end;
    case 8:
      d = Read_CDC_Host(0);
      goto end;
    case 0xA:
      elprintf(EL_UIO, "m68k FIXME: reserved read");
      goto end;
    case 0xC:
      d = Pico_mcd->m.timer_stopwatch >> 16;
      dprintf("m68k stopwatch timer read (%04x)", d);
      goto end;
  }

  if (a < 0x30) {
    // comm flag/cmd/status (0xE-0x2F)
    d = (Pico_mcd->s68k_regs[a]<<8) | Pico_mcd->s68k_regs[a+1];
    goto end;
  }

  elprintf(EL_UIO, "m68k_regs FIXME invalid read @ %02x", a);

end:

  return d;
}
#endif

#ifndef _ASM_CD_MEMORY_C
static
#endif
void m68k_reg_write8(u32 a, u32 d)
{
  a &= 0x3f;
  // dprintf("m68k_regs w%2i: [%02x] %02x @%06x", realsize, a, d, SekPc);

  switch (a) {
    case 0:
      d &= 1;
      if ((d&1) && (Pico_mcd->s68k_regs[0x33]&(1<<2))) { elprintf(EL_INTS, "m68k: s68k irq 2"); SekInterruptS68k(2); }
      return;
    case 1:
      d &= 3;
      if (!(d&1)) Pico_mcd->m.state_flags |= 1; // reset pending, needed to be sure we fetch the right vectors on reset
      if ( (Pico_mcd->m.busreq&1) != (d&1)) elprintf(EL_INTSW, "m68k: s68k reset %i", !(d&1));
      if ( (Pico_mcd->m.busreq&2) != (d&2)) elprintf(EL_INTSW, "m68k: s68k brq %i", (d&2)>>1);
      if ((Pico_mcd->m.state_flags&1) && (d&3)==1) {
        SekResetS68k(); // S68k comes out of RESET or BRQ state
        Pico_mcd->m.state_flags&=~1;
        dprintf("m68k: resetting s68k, cycles=%i", SekCyclesLeft);
      }
      Pico_mcd->m.busreq = d;
      return;
    case 2:
      dprintf("m68k: prg wp=%02x", d);
      Pico_mcd->s68k_regs[2] = d; // really use s68k side register
      return;
    case 3: {
      u32 dold = Pico_mcd->s68k_regs[3]&0x1f;
      r3printf(EL_STATUS, "m68k_regs w3: %02x @%06x", (u8)d, SekPc);
      d &= 0xc2;
      if ((dold>>6) != ((d>>6)&3))
        dprintf("m68k: prg bank: %i -> %i", (Pico_mcd->s68k_regs[a]>>6), ((d>>6)&3));
      //if ((Pico_mcd->s68k_regs[3]&4) != (d&4)) dprintf("m68k: ram mode %i mbit", (d&4) ? 1 : 2);
      //if ((Pico_mcd->s68k_regs[3]&2) != (d&2)) dprintf("m68k: %s", (d&4) ? ((d&2) ? "word swap req" : "noop?") :
      //                                             ((d&2) ? "word ram to s68k" : "word ram to m68k"));
      if (dold & 4) {   // 1M mode
        d ^= 2;         // writing 0 to DMNA actually sets it, 1 does nothing
      } else {
	if ((d ^ dold) & d & 2) { // DMNA is being set
          dold &= ~1;   // return word RAM to s68k
          /* Silpheed hack: bset(w3), r3, btst, bne, r3 */
          SekEndRun(20+16+10+12+16);
        }
      }
      Pico_mcd->s68k_regs[3] = d | dold; // really use s68k side register
#ifdef USE_POLL_DETECT
      if ((s68k_poll_adclk&0xfe) == 2 && s68k_poll_cnt > POLL_LIMIT) {
        SekSetStopS68k(0); s68k_poll_adclk = 0;
        elprintf(EL_CDPOLL, "s68k poll release, a=%02x", a);
      }
#endif
      return;
    }
    case 6:
      Pico_mcd->bios[0x72 + 1] = d; // simple hint vector changer
      return;
    case 7:
      Pico_mcd->bios[0x72] = d;
      dprintf("hint vector set to %08x", PicoRead32(0x70));
      return;
    case 0xf:
      d = (d << 1) | ((d >> 7) & 1); // rol8 1 (special case)
    case 0xe:
      //dprintf("m68k: comm flag: %02x", d);
      Pico_mcd->s68k_regs[0xe] = d;
#ifdef USE_POLL_DETECT
      if ((s68k_poll_adclk&0xfe) == 0xe && s68k_poll_cnt > POLL_LIMIT) {
        SekSetStopS68k(0); s68k_poll_adclk = 0;
        elprintf(EL_CDPOLL, "s68k poll release, a=%02x", a);
      }
#endif
      return;
  }

  if ((a&0xf0) == 0x10) {
      Pico_mcd->s68k_regs[a] = d;
#ifdef USE_POLL_DETECT
      if ((a&0xfe) == (s68k_poll_adclk&0xfe) && s68k_poll_cnt > POLL_LIMIT) {
        SekSetStopS68k(0); s68k_poll_adclk = 0;
        elprintf(EL_CDPOLL, "s68k poll release, a=%02x", a);
      }
#endif
      return;
  }

  elprintf(EL_UIO, "m68k FIXME: invalid write? [%02x] %02x", a, d);
}

#ifndef _ASM_CD_MEMORY_C
static
#endif
u32 s68k_poll_detect(u32 a, u32 d)
{
#ifdef USE_POLL_DETECT
  // needed mostly for Cyclone, which doesn't always check it's cycle counter
  if (SekIsStoppedS68k()) return d;
  // polling detection
  if (a == (s68k_poll_adclk&0xff)) {
    unsigned int clkdiff = SekCyclesDoneS68k() - (s68k_poll_adclk>>8);
    if (clkdiff <= POLL_CYCLES) {
      s68k_poll_cnt++;
      //printf("-- diff: %u, cnt = %i\n", clkdiff, s68k_poll_cnt);
      if (s68k_poll_cnt > POLL_LIMIT) {
        SekSetStopS68k(1);
        elprintf(EL_CDPOLL, "s68k poll detected @ %06x, a=%02x", SekPcS68k, a);
      }
      s68k_poll_adclk = (SekCyclesDoneS68k() << 8) | a;
      return d;
    }
  }
  s68k_poll_adclk = (SekCyclesDoneS68k() << 8) | a;
  s68k_poll_cnt = 0;
#endif
  return d;
}

#define READ_FONT_DATA(basemask) \
{ \
      unsigned int fnt = *(unsigned int *)(Pico_mcd->s68k_regs + 0x4c); \
      unsigned int col0 = (fnt >> 8) & 0x0f, col1 = (fnt >> 12) & 0x0f;   \
      if (fnt & (basemask << 0)) d  = col1      ; else d  = col0;       \
      if (fnt & (basemask << 1)) d |= col1 <<  4; else d |= col0 <<  4; \
      if (fnt & (basemask << 2)) d |= col1 <<  8; else d |= col0 <<  8; \
      if (fnt & (basemask << 3)) d |= col1 << 12; else d |= col0 << 12; \
}


#ifndef _ASM_CD_MEMORY_C
static
#endif
u32 s68k_reg_read16(u32 a)
{
  u32 d=0;

  // dprintf("s68k_regs r%2i: [%02x] @ %06x", realsize&~1, a+(realsize&1), SekPcS68k);

  switch (a) {
    case 0:
      return ((Pico_mcd->s68k_regs[0]&3)<<8) | 1; // ver = 0, not in reset state
    case 2:
      d = (Pico_mcd->s68k_regs[2]<<8) | (Pico_mcd->s68k_regs[3]&0x1f);
      r3printf(EL_STATUS, "s68k_regs r3: %02x @%06x", (u8)d, SekPcS68k);
      return s68k_poll_detect(a, d);
    case 6:
      return CDC_Read_Reg();
    case 8:
      return Read_CDC_Host(1); // Gens returns 0 here on byte reads
    case 0xC:
      d = Pico_mcd->m.timer_stopwatch >> 16;
      dprintf("s68k stopwatch timer read (%04x)", d);
      return d;
    case 0x30:
      dprintf("s68k int3 timer read (%02x)", Pico_mcd->s68k_regs[31]);
      return Pico_mcd->s68k_regs[31];
    case 0x34: // fader
      return 0; // no busy bit
    case 0x50: // font data (check: Lunar 2, Silpheed)
      READ_FONT_DATA(0x00100000);
      return d;
    case 0x52:
      READ_FONT_DATA(0x00010000);
      return d;
    case 0x54:
      READ_FONT_DATA(0x10000000);
      return d;
    case 0x56:
      READ_FONT_DATA(0x01000000);
      return d;
  }

  d = (Pico_mcd->s68k_regs[a]<<8) | Pico_mcd->s68k_regs[a+1];

  if (a >= 0x0e && a < 0x30)
    return s68k_poll_detect(a, d);

  return d;
}

#ifndef _ASM_CD_MEMORY_C
static
#endif
void s68k_reg_write8(u32 a, u32 d)
{
  //dprintf("s68k_regs w%2i: [%02x] %02x @ %06x", realsize, a, d, SekPcS68k);

  // Warning: d might have upper bits set
  switch (a) {
    case 2:
      return; // only m68k can change WP
    case 3: {
      int dold = Pico_mcd->s68k_regs[3];
      r3printf(EL_STATUS, "s68k_regs w3: %02x @%06x", (u8)d, SekPcS68k);
      d &= 0x1d;
      d |= dold&0xc2;
      if (d&4)
      {
        if ((d ^ dold) & 5) {
          d &= ~2; // in case of mode or bank change we clear DMNA (m68k req) bit
          PicoMemResetCD(d);
        }
#ifdef _ASM_CD_MEMORY_C
        if ((d ^ dold) & 0x1d)
          PicoMemResetCDdecode(d);
#endif
        if (!(dold & 4)) {
          r3printf(EL_STATUS, "wram mode 2M->1M");
          wram_2M_to_1M(Pico_mcd->word_ram2M);
        }
      }
      else
      {
        if (dold & 4) {
          r3printf(EL_STATUS, "wram mode 1M->2M");
          if (!(d&1)) { // it didn't set the ret bit, which means it doesn't want to give WRAM to m68k
            d &= ~3;
            d |= (dold&1) ? 2 : 1; // then give it to the one which had bank0 in 1M mode
          }
          wram_1M_to_2M(Pico_mcd->word_ram2M);
          PicoMemResetCD(d);
        }
        else
          d |= dold&1;
        // s68k can only set RET, writing 0 has no effect
        if (d&1) d &= ~2; // return word RAM to m68k in 2M mode
      }
      break;
    }
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
    case 0xc:
    case 0xd:
      dprintf("s68k set stopwatch timer");
      Pico_mcd->m.timer_stopwatch = 0;
      return;
    case 0xe:
      Pico_mcd->s68k_regs[0xf] = (d>>1) | (d<<7); // ror8 1, Gens note: Dragons lair
      return;
    case 0x31:
      dprintf("s68k set int3 timer: %02x", d);
      Pico_mcd->m.timer_int3 = (d & 0xff) << 16;
      break;
    case 0x33: // IRQ mask
      dprintf("s68k irq mask: %02x", d);
      if ((d&(1<<4)) && (Pico_mcd->s68k_regs[0x37]&4) && !(Pico_mcd->s68k_regs[0x33]&(1<<4))) {
        CDD_Export_Status();
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
      }
      return;
    }
    case 0x4b:
      Pico_mcd->s68k_regs[a] = (u8) d;
      CDD_Import_Command();
      return;
  }

  if ((a&0x1f0) == 0x10 || (a >= 0x38 && a < 0x42))
  {
    elprintf(EL_UIO, "s68k FIXME: invalid write @ %02x?", a);
    return;
  }

  Pico_mcd->s68k_regs[a] = (u8) d;
}


static u32 OtherRead16End(u32 a, int realsize)
{
  u32 d=0;

#ifndef _ASM_CD_MEMORY_C
  if ((a&0xffffc0)==0xa12000) {
    d=m68k_reg_read16(a);
    goto end;
  }

  if (a==0x400000) {
    if (SRam.data != NULL) d=3; // 64k cart
    goto end;
  }

  if ((a&0xfe0000)==0x600000) {
    if (SRam.data != NULL) {
      d=SRam.data[((a>>1)&0xffff)+0x2000];
      if (realsize == 8) d|=d<<8;
    }
    goto end;
  }

  if (a==0x7ffffe) {
    d=Pico_mcd->m.bcram_reg;
    goto end;
  }
#endif

  elprintf(EL_UIO, "m68k FIXME: unusual r%i: %06x @%06x", realsize&~1, (a&0xfffffe)+(realsize&1), SekPc);

#ifndef _ASM_CD_MEMORY_C
end:
#endif
  return d;
}


static void OtherWrite8End(u32 a, u32 d, int realsize)
{
#ifndef _ASM_CD_MEMORY_C
  if ((a&0xffffc0)==0xa12000) { m68k_reg_write8(a, d); return; }

  if ((a&0xfe0000)==0x600000) {
    if (SRam.data != NULL && (Pico_mcd->m.bcram_reg&1)) {
      SRam.data[((a>>1)&0xffff)+0x2000]=d;
      SRam.changed = 1;
    }
    return;
  }

  if (a==0x7fffff) {
    Pico_mcd->m.bcram_reg=d;
    return;
  }
#endif

  elprintf(EL_UIO, "m68k FIXME: strange w%i: [%06x], %08x @%06x", realsize, a&0xffffff, d, SekPc);
}

#ifndef _ASM_CD_MEMORY_C
#define _CD_MEMORY_C
#undef _ASM_MEMORY_C
#include "../memory_cmn.c"
#include "cell_map.c"
#endif


// -----------------------------------------------------------------
//                     Read Rom and read Ram

#ifdef _ASM_CD_MEMORY_C
u32 PicoReadM68k8(u32 a);
#else
u32 PicoReadM68k8(u32 a)
{
  u32 d=0;

  a&=0xffffff;

  switch (a >> 17)
  {
    case 0x00>>1: // BIOS: 000000 - 020000
      d = *(u8 *)(Pico_mcd->bios+(a^1));
      break;
    case 0x02>>1: // prg RAM
      if ((Pico_mcd->m.busreq&3)!=1) {
        u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
        d = *(prg_bank+((a^1)&0x1ffff));
      }
      break;
    case 0x20>>1: // word RAM: 200000 - 220000
      wrdprintf("m68k_wram r8: [%06x] @%06x", a, SekPc);
      a &= 0x1ffff;
      if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
        int bank = Pico_mcd->s68k_regs[3]&1;
        d = Pico_mcd->word_ram1M[bank][a^1];
      } else {
        // allow access in any mode, like Gens does
        d = Pico_mcd->word_ram2M[a^1];
      }
      wrdprintf("ret = %02x", (u8)d);
      break;
    case 0x22>>1: // word RAM: 220000 - 240000
      wrdprintf("m68k_wram r8: [%06x] @%06x", a, SekPc);
      if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
        int bank = Pico_mcd->s68k_regs[3]&1;
        a = (a&3) | (cell_map(a >> 2) << 2); // cell arranged
        d = Pico_mcd->word_ram1M[bank][a^1];
      } else {
        // allow access in any mode, like Gens does
        d = Pico_mcd->word_ram2M[(a^1)&0x3ffff];
      }
      wrdprintf("ret = %02x", (u8)d);
      break;
    case 0xc0>>1: case 0xc2>>1: case 0xc4>>1: case 0xc6>>1:
    case 0xc8>>1: case 0xca>>1: case 0xcc>>1: case 0xce>>1:
    case 0xd0>>1: case 0xd2>>1: case 0xd4>>1: case 0xd6>>1:
    case 0xd8>>1: case 0xda>>1: case 0xdc>>1: case 0xde>>1:
      // VDP
      if ((a&0xe700e0)==0xc00000)
        d=PicoVideoRead8(a);
      break;
    case 0xe0>>1: case 0xe2>>1: case 0xe4>>1: case 0xe6>>1:
    case 0xe8>>1: case 0xea>>1: case 0xec>>1: case 0xee>>1:
    case 0xf0>>1: case 0xf2>>1: case 0xf4>>1: case 0xf6>>1:
    case 0xf8>>1: case 0xfa>>1: case 0xfc>>1: case 0xfe>>1:
      // RAM:
      d = *(u8 *)(Pico.ram+((a^1)&0xffff));
      break;
    default:
      if ((a&0xff4000)==0xa00000) { d=z80Read8(a); break; } // Z80 Ram
      if ((a&0xffffc0)==0xa12000)
        rdprintf("m68k_regs r8: [%02x] @%06x", a&0x3f, SekPc);

      d=OtherRead16(a&~1, 8|(a&1)); if ((a&1)==0) d>>=8;

      if ((a&0xffffc0)==0xa12000)
        rdprintf("ret = %02x", (u8)d);
      break;
  }


  elprintf(EL_IO, "r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPc);
#ifdef EMU_CORE_DEBUG
  if (a>=Pico.romsize) {
    lastread_a = a;
    lastread_d[lrp_cyc++&15] = d;
  }
#endif
  return d;
}
#endif


#ifdef _ASM_CD_MEMORY_C
u32 PicoReadM68k16(u32 a);
#else
static u32 PicoReadM68k16(u32 a)
{
  u32 d=0;

  a&=0xfffffe;

  switch (a >> 17)
  {
    case 0x00>>1: // BIOS: 000000 - 020000
      d = *(u16 *)(Pico_mcd->bios+a);
      break;
    case 0x02>>1: // prg RAM
      if ((Pico_mcd->m.busreq&3)!=1) {
        u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
        wrdprintf("m68k_prgram r16: [%i,%06x] @%06x", Pico_mcd->s68k_regs[3]>>6, a, SekPc);
        d = *(u16 *)(prg_bank+(a&0x1fffe));
        wrdprintf("ret = %04x", d);
      }
      break;
    case 0x20>>1: // word RAM: 200000 - 220000
      wrdprintf("m68k_wram r16: [%06x] @%06x", a, SekPc);
      a &= 0x1fffe;
      if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
        int bank = Pico_mcd->s68k_regs[3]&1;
        d = *(u16 *)(Pico_mcd->word_ram1M[bank]+a);
      } else {
        // allow access in any mode, like Gens does
        d = *(u16 *)(Pico_mcd->word_ram2M+a);
      }
      wrdprintf("ret = %04x", d);
      break;
    case 0x22>>1: // word RAM: 220000 - 240000
      wrdprintf("m68k_wram r16: [%06x] @%06x", a, SekPc);
      if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
        int bank = Pico_mcd->s68k_regs[3]&1;
        a = (a&2) | (cell_map(a >> 2) << 2); // cell arranged
        d = *(u16 *)(Pico_mcd->word_ram1M[bank]+a);
      } else {
        // allow access in any mode, like Gens does
        d = *(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe));
      }
      wrdprintf("ret = %04x", d);
      break;
    case 0xc0>>1: case 0xc2>>1: case 0xc4>>1: case 0xc6>>1:
    case 0xc8>>1: case 0xca>>1: case 0xcc>>1: case 0xce>>1:
    case 0xd0>>1: case 0xd2>>1: case 0xd4>>1: case 0xd6>>1:
    case 0xd8>>1: case 0xda>>1: case 0xdc>>1: case 0xde>>1:
      // VDP
      if ((a&0xe700e0)==0xc00000)
        d=PicoVideoRead(a);
      break;
    case 0xe0>>1: case 0xe2>>1: case 0xe4>>1: case 0xe6>>1:
    case 0xe8>>1: case 0xea>>1: case 0xec>>1: case 0xee>>1:
    case 0xf0>>1: case 0xf2>>1: case 0xf4>>1: case 0xf6>>1:
    case 0xf8>>1: case 0xfa>>1: case 0xfc>>1: case 0xfe>>1:
      // RAM:
      d=*(u16 *)(Pico.ram+(a&0xfffe));
      break;
    default:
      if ((a&0xffffc0)==0xa12000)
        rdprintf("m68k_regs r16: [%02x] @%06x", a&0x3f, SekPc);

      d = OtherRead16(a, 16);

      if ((a&0xffffc0)==0xa12000)
        rdprintf("ret = %04x", d);
      break;
  }


  elprintf(EL_IO, "r16: %06x, %04x  @%06x", a&0xffffff, d, SekPc);
#ifdef EMU_CORE_DEBUG
  if (a>=Pico.romsize) {
    lastread_a = a;
    lastread_d[lrp_cyc++&15] = d;
  }
#endif
  return d;
}
#endif


#ifdef _ASM_CD_MEMORY_C
u32 PicoReadM68k32(u32 a);
#else
static u32 PicoReadM68k32(u32 a)
{
  u32 d=0;

  a&=0xfffffe;

  switch (a >> 17)
  {
    case 0x00>>1: { // BIOS: 000000 - 020000
      u16 *pm=(u16 *)(Pico_mcd->bios+a);
      d = (pm[0]<<16)|pm[1];
      break;
    }
    case 0x02>>1: // prg RAM
      if ((Pico_mcd->m.busreq&3)!=1) {
        u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
        u16 *pm=(u16 *)(prg_bank+(a&0x1fffe));
        d = (pm[0]<<16)|pm[1];
      }
      break;
    case 0x20>>1: // word RAM: 200000 - 220000
      wrdprintf("m68k_wram r32: [%06x] @%06x", a, SekPc);
      a&=0x1fffe;
      if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
        int bank = Pico_mcd->s68k_regs[3]&1;
        u16 *pm=(u16 *)(Pico_mcd->word_ram1M[bank]+a);
	d = (pm[0]<<16)|pm[1];
      } else {
        // allow access in any mode, like Gens does
        u16 *pm=(u16 *)(Pico_mcd->word_ram2M+a);
	d = (pm[0]<<16)|pm[1];
      }
      wrdprintf("ret = %08x", d);
      break;
    case 0x22>>1: // word RAM: 220000 - 240000
      wrdprintf("m68k_wram r32: [%06x] @%06x", a, SekPc);
      if (Pico_mcd->s68k_regs[3]&4) { // 1M mode, cell arranged?
        u32 a1, a2;
        int bank = Pico_mcd->s68k_regs[3]&1;
        a1 = (a&2) | (cell_map(a >> 2) << 2);
        if (a&2) a2 = cell_map((a+2) >> 2) << 2;
        else     a2 = a1 + 2;
        d  = *(u16 *)(Pico_mcd->word_ram1M[bank]+a1) << 16;
        d |= *(u16 *)(Pico_mcd->word_ram1M[bank]+a2);
      } else {
        // allow access in any mode, like Gens does
        u16 *pm=(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe));
	d = (pm[0]<<16)|pm[1];
      }
      wrdprintf("ret = %08x", d);
      break;
    case 0xc0>>1: case 0xc2>>1: case 0xc4>>1: case 0xc6>>1:
    case 0xc8>>1: case 0xca>>1: case 0xcc>>1: case 0xce>>1:
    case 0xd0>>1: case 0xd2>>1: case 0xd4>>1: case 0xd6>>1:
    case 0xd8>>1: case 0xda>>1: case 0xdc>>1: case 0xde>>1:
      // VDP
      d = (PicoVideoRead(a)<<16)|PicoVideoRead(a+2);
      break;
    case 0xe0>>1: case 0xe2>>1: case 0xe4>>1: case 0xe6>>1:
    case 0xe8>>1: case 0xea>>1: case 0xec>>1: case 0xee>>1:
    case 0xf0>>1: case 0xf2>>1: case 0xf4>>1: case 0xf6>>1:
    case 0xf8>>1: case 0xfa>>1: case 0xfc>>1: case 0xfe>>1: {
      // RAM:
      u16 *pm=(u16 *)(Pico.ram+(a&0xfffe));
      d = (pm[0]<<16)|pm[1];
      break;
    }
    default:
      if ((a&0xffffc0)==0xa12000)
        rdprintf("m68k_regs r32: [%02x] @%06x", a&0x3f, SekPc);

      d = (OtherRead16(a, 32)<<16)|OtherRead16(a+2, 32);

      if ((a&0xffffc0)==0xa12000)
        rdprintf("ret = %08x", d);
      break;
  }


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

#ifdef _ASM_CD_MEMORY_C
void PicoWriteM68k8(u32 a,u8 d);
#else
void PicoWriteM68k8(u32 a,u8 d)
{
  elprintf(EL_IO, "w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPc);
#ifdef EMU_CORE_DEBUG
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif

  if ((a&0xe00000)==0xe00000) { // Ram
    *(u8 *)(Pico.ram+((a^1)&0xffff)) = d;
    return;
  }

  // prg RAM
  if ((a&0xfe0000)==0x020000 && (Pico_mcd->m.busreq&3)!=1) {
    u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
    *(u8 *)(prg_bank+((a^1)&0x1ffff))=d;
    return;
  }

  a&=0xffffff;

  // word RAM
  if ((a&0xfc0000)==0x200000) {
    wrdprintf("m68k_wram w8: [%06x] %02x @%06x", a, d, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      int bank = Pico_mcd->s68k_regs[3]&1;
      if (a >= 0x220000)
           a = (a&3) | (cell_map(a >> 2) << 2); // cell arranged
      else a &= 0x1ffff;
      *(u8 *)(Pico_mcd->word_ram1M[bank]+(a^1))=d;
    } else {
      // allow access in any mode, like Gens does
      *(u8 *)(Pico_mcd->word_ram2M+((a^1)&0x3ffff))=d;
    }
    return;
  }

  if ((a&0xffffc0)==0xa12000) {
    rdprintf("m68k_regs w8: [%02x] %02x @%06x", a&0x3f, d, SekPc);
    m68k_reg_write8(a, d);
    return;
  }

  OtherWrite8(a,d);
}
#endif


#ifdef _ASM_CD_MEMORY_C
void PicoWriteM68k16(u32 a,u16 d);
#else
static void PicoWriteM68k16(u32 a,u16 d)
{
  elprintf(EL_IO, "w16: %06x, %04x", a&0xffffff, d);
#ifdef EMU_CORE_DEBUG
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif

  if ((a&0xe00000)==0xe00000) { // Ram
    *(u16 *)(Pico.ram+(a&0xfffe))=d;
    return;
  }

  // prg RAM
  if ((a&0xfe0000)==0x020000 && (Pico_mcd->m.busreq&3)!=1) {
    u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
    wrdprintf("m68k_prgram w16: [%i,%06x] %04x @%06x", Pico_mcd->s68k_regs[3]>>6, a, d, SekPc);
    *(u16 *)(prg_bank+(a&0x1fffe))=d;
    return;
  }

  a&=0xfffffe;

  // word RAM
  if ((a&0xfc0000)==0x200000) {
    wrdprintf("m68k_wram w16: [%06x] %04x @%06x", a, d, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      int bank = Pico_mcd->s68k_regs[3]&1;
      if (a >= 0x220000)
           a = (a&2) | (cell_map(a >> 2) << 2); // cell arranged
      else a &= 0x1fffe;
      *(u16 *)(Pico_mcd->word_ram1M[bank]+a)=d;
    } else {
      // allow access in any mode, like Gens does
      *(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe))=d;
    }
    return;
  }

  // regs
  if ((a&0xffffc0)==0xa12000) {
    rdprintf("m68k_regs w16: [%02x] %04x @%06x", a&0x3f, d, SekPc);
    if (a == 0xe) { // special case, 2 byte writes would be handled differently
      Pico_mcd->s68k_regs[0xe] = d >> 8;
#ifdef USE_POLL_DETECT
      if ((s68k_poll_adclk&0xfe) == 0xe && s68k_poll_cnt > POLL_LIMIT) {
        SekSetStopS68k(0); s68k_poll_adclk = 0;
        elprintf(EL_CDPOLL, "s68k poll release, a=%02x", a);
      }
#endif
      return;
    }
    m68k_reg_write8(a,  d>>8);
    m68k_reg_write8(a+1,d&0xff);
    return;
  }

  // VDP
  if ((a&0xe700e0)==0xc00000) {
    PicoVideoWrite(a,(u16)d);
    return;
  }

  OtherWrite16(a,d);
}
#endif


#ifdef _ASM_CD_MEMORY_C
void PicoWriteM68k32(u32 a,u32 d);
#else
static void PicoWriteM68k32(u32 a,u32 d)
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

  // prg RAM
  if ((a&0xfe0000)==0x020000 && (Pico_mcd->m.busreq&3)!=1) {
    u8 *prg_bank = Pico_mcd->prg_ram_b[Pico_mcd->s68k_regs[3]>>6];
    u16 *pm=(u16 *)(prg_bank+(a&0x1fffe));
    pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    return;
  }

  a&=0xfffffe;

  // word RAM
  if ((a&0xfc0000)==0x200000) {
    if (d != 0) // don't log clears
      wrdprintf("m68k_wram w32: [%06x] %08x @%06x", a, d, SekPc);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M mode?
      int bank = Pico_mcd->s68k_regs[3]&1;
      if (a >= 0x220000) { // cell arranged
        u32 a1, a2;
        a1 = (a&2) | (cell_map(a >> 2) << 2);
        if (a&2) a2 = cell_map((a+2) >> 2) << 2;
        else     a2 = a1 + 2;
        *(u16 *)(Pico_mcd->word_ram1M[bank]+a1) = d >> 16;
        *(u16 *)(Pico_mcd->word_ram1M[bank]+a2) = d;
      } else {
        u16 *pm=(u16 *)(Pico_mcd->word_ram1M[bank]+(a&0x1fffe));
        pm[0]=(u16)(d>>16); pm[1]=(u16)d;
      }
    } else {
      // allow access in any mode, like Gens does
      u16 *pm=(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe));
      pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    }
    return;
  }

  if ((a&0xffffc0)==0xa12000) {
    rdprintf("m68k_regs w32: [%02x] %08x @%06x", a&0x3f, d, SekPc);
    if ((a&0x3e) == 0xe) dprintf("m68k FIXME: w32 [%02x]", a&0x3f);
  }

  // VDP
  if ((a&0xe700e0)==0xc00000)
  {
    PicoVideoWrite(a,  (u16)(d>>16));
    PicoVideoWrite(a+2,(u16)d);
    return;
  }

  OtherWrite16(a,  (u16)(d>>16));
  OtherWrite16(a+2,(u16)d);
}
#endif


// -----------------------------------------------------------------
//                            S68k
// -----------------------------------------------------------------

#ifdef _ASM_CD_MEMORY_C
u32 PicoReadS68k8(u32 a);
#else
static u32 PicoReadS68k8(u32 a)
{
  u32 d=0;

#ifdef EMU_CORE_DEBUG
  u32 ab=a&0xfffffe;
#endif
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
    if (a >= 0x0e && a < 0x30) {
      d = Pico_mcd->s68k_regs[a];
      s68k_poll_detect(a, d);
      rdprintf("ret = %02x", (u8)d);
      goto end;
    }
    else if (a >= 0x58 && a < 0x68)
         d = gfx_cd_read(a&~1);
    else d = s68k_reg_read16(a&~1);
    if ((a&1)==0) d>>=8;
    rdprintf("ret = %02x", (u8)d);
    goto end;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    // test: batman returns
    wrdprintf("s68k_wram2M r8: [%06x] @%06x", a, SekPcS68k);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M decode mode?
      int bank = (Pico_mcd->s68k_regs[3]&1)^1;
      d = Pico_mcd->word_ram1M[bank][((a>>1)^1)&0x1ffff];
      if (a&1) d &= 0x0f;
      else d >>= 4;
    } else {
      // allow access in any mode, like Gens does
      d = Pico_mcd->word_ram2M[(a^1)&0x3ffff];
    }
    wrdprintf("ret = %02x", (u8)d);
    goto end;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    int bank;
    wrdprintf("s68k_wram1M r8: [%06x] @%06x", a, SekPcS68k);
//    if (!(Pico_mcd->s68k_regs[3]&4))
//      dprintf("s68k_wram1M FIXME: wrong mode");
    bank = (Pico_mcd->s68k_regs[3]&1)^1;
    d = Pico_mcd->word_ram1M[bank][(a^1)&0x1ffff];
    wrdprintf("ret = %02x", (u8)d);
    goto end;
  }

  // PCM
  if ((a&0xff8000)==0xff0000) {
    elprintf(EL_IO, "s68k_pcm r8: [%06x] @%06x", a, SekPcS68k);
    a &= 0x7fff;
    if (a >= 0x2000)
      d = Pico_mcd->pcm_ram_b[Pico_mcd->pcm.bank][(a>>1)&0xfff];
    else if (a >= 0x20) {
      a &= 0x1e;
      d = Pico_mcd->pcm.ch[a>>2].addr >> PCM_STEP_SHIFT;
      if (a & 2) d >>= 8;
    }
    elprintf(EL_IO, "ret = %02x", (u8)d);
    goto end;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    d = Pico_mcd->bram[(a>>1)&0x1fff];
    goto end;
  }

  elprintf(EL_UIO, "s68k r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPcS68k);

  end:

  elprintf(EL_IO, "s68k r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPcS68k);
#ifdef EMU_CORE_DEBUG
  lastread_a = ab;
  lastread_d[lrp_cyc++&15] = d;
#endif
  return d;
}
#endif


#ifdef _ASM_CD_MEMORY_C
u32 PicoReadS68k16(u32 a);
#else
static u32 PicoReadS68k16(u32 a)
{
  u32 d=0;

#ifdef EMU_CORE_DEBUG
  u32 ab=a&0xfffffe;
#endif
  a&=0xfffffe;

  // prg RAM
  if (a < 0x80000) {
    wrdprintf("s68k_prgram r16: [%06x] @%06x", a, SekPcS68k);
    d = *(u16 *)(Pico_mcd->prg_ram+a);
    wrdprintf("ret = %04x", d);
    goto end;
  }

  // regs
  if ((a&0xfffe00) == 0xff8000) {
    a &= 0x1fe;
    rdprintf("s68k_regs r16: [%02x] @ %06x", a, SekPcS68k);
    if (a >= 0x58 && a < 0x68)
         d = gfx_cd_read(a);
    else d = s68k_reg_read16(a);
    rdprintf("ret = %04x", d);
    goto end;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    wrdprintf("s68k_wram2M r16: [%06x] @%06x", a, SekPcS68k);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M decode mode?
      int bank = (Pico_mcd->s68k_regs[3]&1)^1;
      d = Pico_mcd->word_ram1M[bank][((a>>1)^1)&0x1ffff];
      d |= d << 4; d &= ~0xf0;
    } else {
      // allow access in any mode, like Gens does
      d = *(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe));
    }
    wrdprintf("ret = %04x", d);
    goto end;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    int bank;
    wrdprintf("s68k_wram1M r16: [%06x] @%06x", a, SekPcS68k);
//    if (!(Pico_mcd->s68k_regs[3]&4))
//      dprintf("s68k_wram1M FIXME: wrong mode");
    bank = (Pico_mcd->s68k_regs[3]&1)^1;
    d = *(u16 *)(Pico_mcd->word_ram1M[bank]+(a&0x1fffe));
    wrdprintf("ret = %04x", d);
    goto end;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    dprintf("FIXME: s68k_bram r16: [%06x] @%06x", a, SekPcS68k);
    a = (a>>1)&0x1fff;
    d = Pico_mcd->bram[a++];		// Gens does little endian here, and so do we..
    d|= Pico_mcd->bram[a++] << 8;	// This is most likely wrong
    dprintf("ret = %04x", d);
    goto end;
  }

  // PCM
  if ((a&0xff8000)==0xff0000) {
    dprintf("FIXME: s68k_pcm r16: [%06x] @%06x", a, SekPcS68k);
    a &= 0x7fff;
    if (a >= 0x2000)
      d = Pico_mcd->pcm_ram_b[Pico_mcd->pcm.bank][(a>>1)&0xfff];
    else if (a >= 0x20) {
      a &= 0x1e;
      d = Pico_mcd->pcm.ch[a>>2].addr >> PCM_STEP_SHIFT;
      if (a & 2) d >>= 8;
    }
    dprintf("ret = %04x", d);
    goto end;
  }

  elprintf(EL_UIO, "s68k r16: %06x, %04x  @%06x", a&0xffffff, d, SekPcS68k);

  end:

  elprintf(EL_IO, "s68k r16: %06x, %04x  @%06x", a&0xffffff, d, SekPcS68k);
#ifdef EMU_CORE_DEBUG
  lastread_a = ab;
  lastread_d[lrp_cyc++&15] = d;
#endif
  return d;
}
#endif


#ifdef _ASM_CD_MEMORY_C
u32 PicoReadS68k32(u32 a);
#else
static u32 PicoReadS68k32(u32 a)
{
  u32 d=0;

#ifdef EMU_CORE_DEBUG
  u32 ab=a&0xfffffe;
#endif
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
    if (a >= 0x58 && a < 0x68)
         d = (gfx_cd_read(a)<<16)|gfx_cd_read(a+2);
    else d = (s68k_reg_read16(a)<<16)|s68k_reg_read16(a+2);
    rdprintf("ret = %08x", d);
    goto end;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    wrdprintf("s68k_wram2M r32: [%06x] @%06x", a, SekPcS68k);
    if (Pico_mcd->s68k_regs[3]&4) { // 1M decode mode?
      int bank = (Pico_mcd->s68k_regs[3]&1)^1;
      a >>= 1;
      d  = Pico_mcd->word_ram1M[bank][((a+0)^1)&0x1ffff] << 16;
      d |= Pico_mcd->word_ram1M[bank][((a+1)^1)&0x1ffff];
      d |= d << 4; d &= 0x0f0f0f0f;
    } else {
      // allow access in any mode, like Gens does
      u16 *pm=(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe)); d = (pm[0]<<16)|pm[1];
    }
    wrdprintf("ret = %08x", d);
    goto end;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    int bank;
    u16 *pm;
    wrdprintf("s68k_wram1M r32: [%06x] @%06x", a, SekPcS68k);
//    if (!(Pico_mcd->s68k_regs[3]&4))
//      dprintf("s68k_wram1M FIXME: wrong mode");
    bank = (Pico_mcd->s68k_regs[3]&1)^1;
    pm=(u16 *)(Pico_mcd->word_ram1M[bank]+(a&0x1fffe)); d = (pm[0]<<16)|pm[1];
    wrdprintf("ret = %08x", d);
    goto end;
  }

  // PCM
  if ((a&0xff8000)==0xff0000) {
    dprintf("s68k_pcm r32: [%06x] @%06x", a, SekPcS68k);
    a &= 0x7fff;
    if (a >= 0x2000) {
      a >>= 1;
      d  = Pico_mcd->pcm_ram_b[Pico_mcd->pcm.bank][a&0xfff] << 16;
      d |= Pico_mcd->pcm_ram_b[Pico_mcd->pcm.bank][(a+1)&0xfff];
    } else if (a >= 0x20) {
      a &= 0x1e;
      if (a & 2) {
        a >>= 2;
        d  = (Pico_mcd->pcm.ch[a].addr >> (PCM_STEP_SHIFT-8)) & 0xff0000;
        d |= (Pico_mcd->pcm.ch[(a+1)&7].addr >> PCM_STEP_SHIFT)   & 0xff;
      } else {
        d = Pico_mcd->pcm.ch[a>>2].addr >> PCM_STEP_SHIFT;
        d = ((d<<16)&0xff0000) | ((d>>8)&0xff); // PCM chip is LE
      }
    }
    dprintf("ret = %08x", d);
    goto end;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    dprintf("FIXME: s68k_bram r32: [%06x] @%06x", a, SekPcS68k);
    a = (a>>1)&0x1fff;
    d = Pico_mcd->bram[a++] << 16;		// middle endian? TODO: verify against Fusion..
    d|= Pico_mcd->bram[a++] << 24;
    d|= Pico_mcd->bram[a++];
    d|= Pico_mcd->bram[a++] << 8;
    dprintf("ret = %08x", d);
    goto end;
  }

  elprintf(EL_UIO, "s68k r32: %06x, %08x @%06x", a&0xffffff, d, SekPcS68k);

  end:

  elprintf(EL_IO, "s68k r32: %06x, %08x @%06x", a&0xffffff, d, SekPcS68k);
#ifdef EMU_CORE_DEBUG
  if (ab > 0x78) { // not vectors and stuff
    lastread_a = ab;
    lastread_d[lrp_cyc++&15] = d;
  }
#endif
  return d;
}
#endif


#ifndef _ASM_CD_MEMORY_C
/* check: jaguar xj 220 (draws entire world using decode) */
static void decode_write8(u32 a, u8 d, int r3)
{
  u8 *pd = Pico_mcd->word_ram1M[(r3 & 1)^1] + (((a>>1)^1)&0x1ffff);
  u8 oldmask = (a&1) ? 0xf0 : 0x0f;

  r3 &= 0x18;
  d  &= 0x0f;
  if (!(a&1)) d <<= 4;

  if (r3 == 8) {
    if ((!(*pd & (~oldmask))) && d) goto do_it;
  } else if (r3 > 8) {
    if (d) goto do_it;
  } else {
    goto do_it;
  }

  return;
do_it:
  *pd = d | (*pd & oldmask);
}


static void decode_write16(u32 a, u16 d, int r3)
{
  u8 *pd = Pico_mcd->word_ram1M[(r3 & 1)^1] + (((a>>1)^1)&0x1ffff);

  //if ((a & 0x3ffff) < 0x28000) return;

  r3 &= 0x18;
  d  &= 0x0f0f;
  d  |= d >> 4;

  if (r3 == 8) {
    u8 dold = *pd;
    if (!(dold & 0xf0)) dold |= d & 0xf0;
    if (!(dold & 0x0f)) dold |= d & 0x0f;
    *pd = dold;
  } else if (r3 > 8) {
    u8 dold = *pd;
    if (!(d & 0xf0)) d |= dold & 0xf0;
    if (!(d & 0x0f)) d |= dold & 0x0f;
    *pd = d;
  } else {
    *pd = d;
  }
}
#endif

// -----------------------------------------------------------------

#ifdef _ASM_CD_MEMORY_C
void PicoWriteS68k8(u32 a,u8 d);
#else
static void PicoWriteS68k8(u32 a,u8 d)
{
  elprintf(EL_IO, "s68k w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPcS68k);

  a&=0xffffff;

#ifdef EMU_CORE_DEBUG
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif

  // prg RAM
  if (a < 0x80000) {
    u8 *pm=(u8 *)(Pico_mcd->prg_ram+(a^1));
    if (a >= (Pico_mcd->s68k_regs[2]<<8)) *pm=d;
    return;
  }

  // regs
  if ((a&0xfffe00) == 0xff8000) {
    a &= 0x1ff;
    rdprintf("s68k_regs w8: [%02x] %02x @ %06x", a, d, SekPcS68k);
    if (a >= 0x58 && a < 0x68)
         gfx_cd_write16(a&~1, (d<<8)|d);
    else s68k_reg_write8(a,d);
    return;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    int r3 = Pico_mcd->s68k_regs[3];
    wrdprintf("s68k_wram2M w8: [%06x] %02x @%06x", a, d, SekPcS68k);
    if (r3 & 4) { // 1M decode mode?
      decode_write8(a, d, r3);
    } else {
      // allow access in any mode, like Gens does
      *(u8 *)(Pico_mcd->word_ram2M+((a^1)&0x3ffff))=d;
    }
    return;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    // Wing Commander tries to write here in wrong mode
    int bank;
    if (d)
      wrdprintf("s68k_wram1M w8: [%06x] %02x @%06x", a, d, SekPcS68k);
//    if (!(Pico_mcd->s68k_regs[3]&4))
//      dprintf("s68k_wram1M FIXME: wrong mode");
    bank = (Pico_mcd->s68k_regs[3]&1)^1;
    *(u8 *)(Pico_mcd->word_ram1M[bank]+((a^1)&0x1ffff))=d;
    return;
  }

  // PCM
  if ((a&0xff8000)==0xff0000) {
    a &= 0x7fff;
    if (a >= 0x2000)
      Pico_mcd->pcm_ram_b[Pico_mcd->pcm.bank][(a>>1)&0xfff] = d;
    else if (a < 0x12)
      pcm_write(a>>1, d);
    return;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    Pico_mcd->bram[(a>>1)&0x1fff] = d;
    SRam.changed = 1;
    return;
  }

  elprintf(EL_UIO, "s68k w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPcS68k);
}
#endif


#ifdef _ASM_CD_MEMORY_C
void PicoWriteS68k16(u32 a,u16 d);
#else
static void PicoWriteS68k16(u32 a,u16 d)
{
  elprintf(EL_IO, "s68k w16: %06x, %04x @%06x", a&0xffffff, d, SekPcS68k);

  a&=0xfffffe;

#ifdef EMU_CORE_DEBUG
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif

  // prg RAM
  if (a < 0x80000) {
    wrdprintf("s68k_prgram w16: [%06x] %04x @%06x", a, d, SekPcS68k);
    if (a >= (Pico_mcd->s68k_regs[2]<<8)) // needed for Dungeon Explorer
      *(u16 *)(Pico_mcd->prg_ram+a)=d;
    return;
  }

  // regs
  if ((a&0xfffe00) == 0xff8000) {
    a &= 0x1fe;
    rdprintf("s68k_regs w16: [%02x] %04x @ %06x", a, d, SekPcS68k);
    if (a >= 0x58 && a < 0x68)
      gfx_cd_write16(a, d);
    else {
      if (a == 0xe) { // special case, 2 byte writes would be handled differently
        Pico_mcd->s68k_regs[0xf] = d;
        return;
      }
      s68k_reg_write8(a,  d>>8);
      s68k_reg_write8(a+1,d&0xff);
    }
    return;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    int r3 = Pico_mcd->s68k_regs[3];
    wrdprintf("s68k_wram2M w16: [%06x] %04x @%06x", a, d, SekPcS68k);
    if (r3 & 4) { // 1M decode mode?
      decode_write16(a, d, r3);
    } else {
      // allow access in any mode, like Gens does
      *(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe))=d;
    }
    return;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    int bank;
    if (d)
      wrdprintf("s68k_wram1M w16: [%06x] %04x @%06x", a, d, SekPcS68k);
//    if (!(Pico_mcd->s68k_regs[3]&4))
//      dprintf("s68k_wram1M FIXME: wrong mode");
    bank = (Pico_mcd->s68k_regs[3]&1)^1;
    *(u16 *)(Pico_mcd->word_ram1M[bank]+(a&0x1fffe))=d;
    return;
  }

  // PCM
  if ((a&0xff8000)==0xff0000) {
    a &= 0x7fff;
    if (a >= 0x2000)
      Pico_mcd->pcm_ram_b[Pico_mcd->pcm.bank][(a>>1)&0xfff] = d;
    else if (a < 0x12)
      pcm_write(a>>1, d & 0xff);
    return;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    dprintf("s68k_bram w16: [%06x] %04x @%06x", a, d, SekPcS68k);
    a = (a>>1)&0x1fff;
    Pico_mcd->bram[a++] = d;		// Gens does little endian here, an so do we..
    Pico_mcd->bram[a++] = d >> 8;
    SRam.changed = 1;
    return;
  }

  elprintf(EL_UIO, "s68k w16: %06x, %04x @%06x", a&0xffffff, d, SekPcS68k);
}
#endif


#ifdef _ASM_CD_MEMORY_C
void PicoWriteS68k32(u32 a,u32 d);
#else
static void PicoWriteS68k32(u32 a,u32 d)
{
  elprintf(EL_IO, "s68k w32: %06x, %08x @%06x", a&0xffffff, d, SekPcS68k);

  a&=0xfffffe;

#ifdef EMU_CORE_DEBUG
  lastwrite_cyc_d[lwp_cyc++&15] = d;
#endif

  // prg RAM
  if (a < 0x80000) {
    if (a >= (Pico_mcd->s68k_regs[2]<<8)) {
      u16 *pm=(u16 *)(Pico_mcd->prg_ram+a);
      pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    }
    return;
  }

  // regs
  if ((a&0xfffe00) == 0xff8000) {
    a &= 0x1fe;
    rdprintf("s68k_regs w32: [%02x] %08x @ %06x", a, d, SekPcS68k);
    if (a >= 0x58 && a < 0x68) {
      gfx_cd_write16(a,   d>>16);
      gfx_cd_write16(a+2, d&0xffff);
    } else {
      if ((a&0x1fe) == 0xe) dprintf("s68k FIXME: w32 [%02x]", a&0x3f);
      s68k_reg_write8(a,   d>>24);
      s68k_reg_write8(a+1,(d>>16)&0xff);
      s68k_reg_write8(a+2,(d>>8) &0xff);
      s68k_reg_write8(a+3, d     &0xff);
    }
    return;
  }

  // word RAM (2M area)
  if ((a&0xfc0000)==0x080000) { // 080000-0bffff
    int r3 = Pico_mcd->s68k_regs[3];
    wrdprintf("s68k_wram2M w32: [%06x] %08x @%06x", a, d, SekPcS68k);
    if (r3 & 4) { // 1M decode mode?
      decode_write16(a  , d >> 16, r3);
      decode_write16(a+2, d      , r3);
    } else {
      // allow access in any mode, like Gens does
      u16 *pm=(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe));
      pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    }
    return;
  }

  // word RAM (1M area)
  if ((a&0xfe0000)==0x0c0000 && (Pico_mcd->s68k_regs[3]&4)) { // 0c0000-0dffff
    int bank;
    u16 *pm;
    if (d)
      wrdprintf("s68k_wram1M w32: [%06x] %08x @%06x", a, d, SekPcS68k);
//    if (!(Pico_mcd->s68k_regs[3]&4))
//      dprintf("s68k_wram1M FIXME: wrong mode");
    bank = (Pico_mcd->s68k_regs[3]&1)^1;
    pm=(u16 *)(Pico_mcd->word_ram1M[bank]+(a&0x1fffe));
    pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    return;
  }

  // PCM
  if ((a&0xff8000)==0xff0000) {
    a &= 0x7fff;
    if (a >= 0x2000) {
      a >>= 1;
      Pico_mcd->pcm_ram_b[Pico_mcd->pcm.bank][a&0xfff] = (d >> 16);
      Pico_mcd->pcm_ram_b[Pico_mcd->pcm.bank][(a+1)&0xfff] = d;
    } else if (a < 0x12) {
      a >>= 1;
      pcm_write(a,  (d>>16) & 0xff);
      pcm_write(a+1, d & 0xff);
    }
    return;
  }

  // bram
  if ((a&0xff0000)==0xfe0000) {
    dprintf("s68k_bram w32: [%06x] %08x @%06x", a, d, SekPcS68k);
    a = (a>>1)&0x1fff;
    Pico_mcd->bram[a++] = d >> 16;		// middle endian? verify?
    Pico_mcd->bram[a++] = d >> 24;
    Pico_mcd->bram[a++] = d;
    Pico_mcd->bram[a++] = d >> 8;
    SRam.changed = 1;
    return;
  }

  elprintf(EL_UIO, "s68k w32: %06x, %08x @%06x", a&0xffffff, d, SekPcS68k);
}
#endif


// -----------------------------------------------------------------


#ifdef EMU_C68K
static __inline int PicoMemBaseM68k(u32 pc)
{
  if ((pc&0xe00000)==0xe00000)
    return (int)Pico.ram-(pc&0xff0000); // Program Counter in Ram

  if (pc < 0x20000)
    return (int)Pico_mcd->bios; // Program Counter in BIOS

  if ((pc&0xfc0000)==0x200000)
  {
    if (!(Pico_mcd->s68k_regs[3]&4))
      return (int)Pico_mcd->word_ram2M - 0x200000; // Program Counter in Word Ram
    if (pc < 0x220000) {
      int bank = Pico_mcd->s68k_regs[3]&1;
      return (int)Pico_mcd->word_ram1M[bank] - 0x200000;
    }
  }

  // Error - Program Counter is invalid
  elprintf(EL_ANOMALY, "m68k FIXME: unhandled jump to %06x", pc);

  return (int)Pico_mcd->bios;
}


static u32 PicoCheckPcM68k(u32 pc)
{
  pc-=PicoCpuCM68k.membase; // Get real pc
  pc&=0xfffffe;

  PicoCpuCM68k.membase=PicoMemBaseM68k(pc);

  return PicoCpuCM68k.membase+pc;
}


static __inline int PicoMemBaseS68k(u32 pc)
{
  if (pc < 0x80000)                     // PRG RAM
    return (int)Pico_mcd->prg_ram;

  if ((pc&0xfc0000)==0x080000)          // WORD RAM 2M area (assume we are in the right mode..)
    return (int)Pico_mcd->word_ram2M - 0x080000;

  if ((pc&0xfe0000)==0x0c0000) {        // word RAM 1M area
    int bank = (Pico_mcd->s68k_regs[3]&1)^1;
    return (int)Pico_mcd->word_ram1M[bank] - 0x0c0000;
  }

  // Error - Program Counter is invalid
  elprintf(EL_ANOMALY, "s68k FIXME: unhandled jump to %06x", pc);

  return (int)Pico_mcd->prg_ram;
}


static u32 PicoCheckPcS68k(u32 pc)
{
  pc-=PicoCpuCS68k.membase; // Get real pc
  pc&=0xfffffe;

  PicoCpuCS68k.membase=PicoMemBaseS68k(pc);

  return PicoCpuCS68k.membase+pc;
}
#endif

#ifndef _ASM_CD_MEMORY_C
void PicoMemResetCD(int r3)
{
#ifdef EMU_F68K
  // update fetchmap..
  int i;
  if (!(r3 & 4))
  {
    for (i = M68K_FETCHBANK1*2/16; (i<<(24-FAMEC_FETCHBITS)) < 0x240000; i++)
      PicoCpuFM68k.Fetch[i] = (unsigned int)Pico_mcd->word_ram2M - 0x200000;
  }
  else
  {
    for (i = M68K_FETCHBANK1*2/16; (i<<(24-FAMEC_FETCHBITS)) < 0x220000; i++)
      PicoCpuFM68k.Fetch[i] = (unsigned int)Pico_mcd->word_ram1M[r3 & 1] - 0x200000;
    for (i = M68K_FETCHBANK1*0x0c/0x100; (i<<(24-FAMEC_FETCHBITS)) < 0x0e0000; i++)
      PicoCpuFS68k.Fetch[i] = (unsigned int)Pico_mcd->word_ram1M[(r3&1)^1] - 0x0c0000;
  }
#endif
}
#endif

#ifdef EMU_M68K
static void m68k_mem_setup_cd(void);
#endif

PICO_INTERNAL void PicoMemSetupCD(void)
{
  // additional handlers for common code
  PicoRead16Hook = OtherRead16End;
  PicoWrite8Hook = OtherWrite8End;

#ifdef EMU_C68K
  // Setup m68k memory callbacks:
  PicoCpuCM68k.checkpc=PicoCheckPcM68k;
  PicoCpuCM68k.fetch8 =PicoCpuCM68k.read8 =PicoReadM68k8;
  PicoCpuCM68k.fetch16=PicoCpuCM68k.read16=PicoReadM68k16;
  PicoCpuCM68k.fetch32=PicoCpuCM68k.read32=PicoReadM68k32;
  PicoCpuCM68k.write8 =PicoWriteM68k8;
  PicoCpuCM68k.write16=PicoWriteM68k16;
  PicoCpuCM68k.write32=PicoWriteM68k32;
  // s68k
  PicoCpuCS68k.checkpc=PicoCheckPcS68k;
  PicoCpuCS68k.fetch8 =PicoCpuCS68k.read8 =PicoReadS68k8;
  PicoCpuCS68k.fetch16=PicoCpuCS68k.read16=PicoReadS68k16;
  PicoCpuCS68k.fetch32=PicoCpuCS68k.read32=PicoReadS68k32;
  PicoCpuCS68k.write8 =PicoWriteS68k8;
  PicoCpuCS68k.write16=PicoWriteS68k16;
  PicoCpuCS68k.write32=PicoWriteS68k32;
#endif
#ifdef EMU_F68K
  // m68k
  PicoCpuFM68k.read_byte =PicoReadM68k8;
  PicoCpuFM68k.read_word =PicoReadM68k16;
  PicoCpuFM68k.read_long =PicoReadM68k32;
  PicoCpuFM68k.write_byte=PicoWriteM68k8;
  PicoCpuFM68k.write_word=PicoWriteM68k16;
  PicoCpuFM68k.write_long=PicoWriteM68k32;
  // s68k
  PicoCpuFS68k.read_byte =PicoReadS68k8;
  PicoCpuFS68k.read_word =PicoReadS68k16;
  PicoCpuFS68k.read_long =PicoReadS68k32;
  PicoCpuFS68k.write_byte=PicoWriteS68k8;
  PicoCpuFS68k.write_word=PicoWriteS68k16;
  PicoCpuFS68k.write_long=PicoWriteS68k32;

  // setup FAME fetchmap
  {
    int i;
    // M68k
    // by default, point everything to fitst 64k of ROM (BIOS)
    for (i = 0; i < M68K_FETCHBANK1; i++)
      PicoCpuFM68k.Fetch[i] = (unsigned int)Pico.rom - (i<<(24-FAMEC_FETCHBITS));
    // now real ROM (BIOS)
    for (i = 0; i < M68K_FETCHBANK1 && (i<<(24-FAMEC_FETCHBITS)) < Pico.romsize; i++)
      PicoCpuFM68k.Fetch[i] = (unsigned int)Pico.rom;
    // .. and RAM
    for (i = M68K_FETCHBANK1*14/16; i < M68K_FETCHBANK1; i++)
      PicoCpuFM68k.Fetch[i] = (unsigned int)Pico.ram - (i<<(24-FAMEC_FETCHBITS));
    // S68k
    // PRG RAM is default
    for (i = 0; i < M68K_FETCHBANK1; i++)
      PicoCpuFS68k.Fetch[i] = (unsigned int)Pico_mcd->prg_ram - (i<<(24-FAMEC_FETCHBITS));
    // real PRG RAM
    for (i = 0; i < M68K_FETCHBANK1 && (i<<(24-FAMEC_FETCHBITS)) < 0x80000; i++)
      PicoCpuFS68k.Fetch[i] = (unsigned int)Pico_mcd->prg_ram;
    // WORD RAM 2M area
    for (i = M68K_FETCHBANK1*0x08/0x100; i < M68K_FETCHBANK1 && (i<<(24-FAMEC_FETCHBITS)) < 0xc0000; i++)
      PicoCpuFS68k.Fetch[i] = (unsigned int)Pico_mcd->word_ram2M - 0x80000;
    // PicoMemResetCD() will setup word ram for both
  }
#endif
#ifdef EMU_M68K
  m68k_mem_setup_cd();
#endif

  // m68k_poll_addr = m68k_poll_cnt = 0;
  s68k_poll_adclk = s68k_poll_cnt = 0;
}


#ifdef EMU_M68K
static unsigned int PicoReadCD8w (unsigned int a) {
	return m68ki_cpu_p == &PicoCpuMS68k ? PicoReadS68k8(a) : PicoReadM68k8(a);
}
static unsigned int PicoReadCD16w(unsigned int a) {
	return m68ki_cpu_p == &PicoCpuMS68k ? PicoReadS68k16(a) : PicoReadM68k16(a);
}
static unsigned int PicoReadCD32w(unsigned int a) {
	return m68ki_cpu_p == &PicoCpuMS68k ? PicoReadS68k32(a) : PicoReadM68k32(a);
}
static void PicoWriteCD8w (unsigned int a, unsigned char d) {
	if (m68ki_cpu_p == &PicoCpuMS68k) PicoWriteS68k8(a, d); else PicoWriteM68k8(a, d);
}
static void PicoWriteCD16w(unsigned int a, unsigned short d) {
	if (m68ki_cpu_p == &PicoCpuMS68k) PicoWriteS68k16(a, d); else PicoWriteM68k16(a, d);
}
static void PicoWriteCD32w(unsigned int a, unsigned int d) {
	if (m68ki_cpu_p == &PicoCpuMS68k) PicoWriteS68k32(a, d); else PicoWriteM68k32(a, d);
}

// these are allowed to access RAM
static unsigned int  m68k_read_pcrelative_CD8 (unsigned int a)
{
  a&=0xffffff;
  if(m68ki_cpu_p == &PicoCpuMS68k) {
    if (a < 0x80000) return *(u8 *)(Pico_mcd->prg_ram+(a^1)); // PRG Ram
    if ((a&0xfc0000)==0x080000 && !(Pico_mcd->s68k_regs[3]&4)) // word RAM (2M area: 080000-0bffff)
      return *(u8 *)(Pico_mcd->word_ram2M+((a^1)&0x3ffff));
    if ((a&0xfe0000)==0x0c0000 &&  (Pico_mcd->s68k_regs[3]&4)) { // word RAM (1M area: 0c0000-0dffff)
      int bank = (Pico_mcd->s68k_regs[3]&1)^1;
      return *(u8 *)(Pico_mcd->word_ram1M[bank]+((a^1)&0x1ffff));
    }
    elprintf(EL_ANOMALY, "s68k_read_pcrelative_CD8 FIXME: can't handle %06x", a);
  } else {
    if((a&0xe00000)==0xe00000) return *(u8 *)(Pico.ram+((a^1)&0xffff)); // Ram
    if(a<0x20000)              return *(u8 *)(Pico.rom+(a^1)); // Bios
    if((a&0xfc0000)==0x200000) { // word RAM
      if(!(Pico_mcd->s68k_regs[3]&4)) // 2M?
        return *(u8 *)(Pico_mcd->word_ram2M+((a^1)&0x3ffff));
      else if (a < 0x220000) {
        int bank = Pico_mcd->s68k_regs[3]&1;
        return *(u8 *)(Pico_mcd->word_ram1M[bank]+((a^1)&0x1ffff));
      }
    }
    elprintf(EL_ANOMALY, "m68k_read_pcrelative_CD8 FIXME: can't handle %06x", a);
  }
  return 0;//(u8)  lastread_d;
}
static unsigned int  m68k_read_pcrelative_CD16(unsigned int a)
{
  a&=0xffffff;
  if(m68ki_cpu_p == &PicoCpuMS68k) {
    if (a < 0x80000) return *(u16 *)(Pico_mcd->prg_ram+(a&~1)); // PRG Ram
    if ((a&0xfc0000)==0x080000 && !(Pico_mcd->s68k_regs[3]&4)) // word RAM (2M area: 080000-0bffff)
      return *(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe));
    if ((a&0xfe0000)==0x0c0000 &&  (Pico_mcd->s68k_regs[3]&4)) { // word RAM (1M area: 0c0000-0dffff)
      int bank = (Pico_mcd->s68k_regs[3]&1)^1;
      return *(u16 *)(Pico_mcd->word_ram1M[bank]+(a&0x1fffe));
    }
    elprintf(EL_ANOMALY, "s68k_read_pcrelative_CD16 FIXME: can't handle %06x", a);
  } else {
    if((a&0xe00000)==0xe00000) return *(u16 *)(Pico.ram+(a&0xfffe)); // Ram
    if(a<0x20000)              return *(u16 *)(Pico.rom+(a&~1)); // Bios
    if((a&0xfc0000)==0x200000) { // word RAM
      if(!(Pico_mcd->s68k_regs[3]&4)) // 2M?
        return *(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe));
      else if (a < 0x220000) {
        int bank = Pico_mcd->s68k_regs[3]&1;
        return *(u16 *)(Pico_mcd->word_ram1M[bank]+(a&0x1fffe));
      }
    }
    elprintf(EL_ANOMALY, "m68k_read_pcrelative_CD16 FIXME: can't handle %06x", a);
  }
  return 0;
}
static unsigned int  m68k_read_pcrelative_CD32(unsigned int a)
{
  u16 *pm;
  a&=0xffffff;
  if(m68ki_cpu_p == &PicoCpuMS68k) {
    if (a < 0x80000) { u16 *pm=(u16 *)(Pico_mcd->prg_ram+(a&~1)); return (pm[0]<<16)|pm[1]; } // PRG Ram
    if ((a&0xfc0000)==0x080000 && !(Pico_mcd->s68k_regs[3]&4)) // word RAM (2M area: 080000-0bffff)
      { pm=(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe)); return (pm[0]<<16)|pm[1]; }
    if ((a&0xfe0000)==0x0c0000 &&  (Pico_mcd->s68k_regs[3]&4)) { // word RAM (1M area: 0c0000-0dffff)
      int bank = (Pico_mcd->s68k_regs[3]&1)^1;
      pm=(u16 *)(Pico_mcd->word_ram1M[bank]+(a&0x1fffe));
      return (pm[0]<<16)|pm[1];
    }
    elprintf(EL_ANOMALY, "s68k_read_pcrelative_CD32 FIXME: can't handle %06x", a);
  } else {
    if((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xfffe)); return (pm[0]<<16)|pm[1]; } // Ram
    if(a<0x20000)              { u16 *pm=(u16 *)(Pico.rom+(a&~1));     return (pm[0]<<16)|pm[1]; }
    if((a&0xfc0000)==0x200000) { // word RAM
      if(!(Pico_mcd->s68k_regs[3]&4)) // 2M?
        { pm=(u16 *)(Pico_mcd->word_ram2M+(a&0x3fffe)); return (pm[0]<<16)|pm[1]; }
      else if (a < 0x220000) {
        int bank = Pico_mcd->s68k_regs[3]&1;
        pm=(u16 *)(Pico_mcd->word_ram1M[bank]+(a&0x1fffe));
        return (pm[0]<<16)|pm[1];
      }
    }
    elprintf(EL_ANOMALY, "m68k_read_pcrelative_CD32 FIXME: can't handle %06x", a);
  }
  return 0;
}

extern unsigned int (*pm68k_read_memory_8) (unsigned int address);
extern unsigned int (*pm68k_read_memory_16)(unsigned int address);
extern unsigned int (*pm68k_read_memory_32)(unsigned int address);
extern void (*pm68k_write_memory_8) (unsigned int address, unsigned char  value);
extern void (*pm68k_write_memory_16)(unsigned int address, unsigned short value);
extern void (*pm68k_write_memory_32)(unsigned int address, unsigned int   value);
extern unsigned int (*pm68k_read_memory_pcr_8) (unsigned int address);
extern unsigned int (*pm68k_read_memory_pcr_16)(unsigned int address);
extern unsigned int (*pm68k_read_memory_pcr_32)(unsigned int address);

static void m68k_mem_setup_cd(void)
{
  pm68k_read_memory_8  = PicoReadCD8w;
  pm68k_read_memory_16 = PicoReadCD16w;
  pm68k_read_memory_32 = PicoReadCD32w;
  pm68k_write_memory_8  = PicoWriteCD8w;
  pm68k_write_memory_16 = PicoWriteCD16w;
  pm68k_write_memory_32 = PicoWriteCD32w;
  pm68k_read_memory_pcr_8  = m68k_read_pcrelative_CD8;
  pm68k_read_memory_pcr_16 = m68k_read_pcrelative_CD16;
  pm68k_read_memory_pcr_32 = m68k_read_pcrelative_CD32;
}
#endif // EMU_M68K

