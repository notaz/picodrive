/*
 * SMS emulation
 * (C) notaz, 2009-2010
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */
/*
 * TODO:
 * - start in a state as if BIOS ran
 * - region support
 * - H counter
 */
#include "pico_int.h"
#include "memory.h"
#include "sound/sn76496.h"
#include "sound/emu2413/emu2413.h"

extern void YM2413_regWrite(unsigned reg);
extern void YM2413_dataWrite(unsigned data);


static unsigned short ymflag = 0xffff;
static u8 vdp_buffer;
static u8 vdp_hlatch;

static unsigned char vdp_data_read(void)
{
  struct PicoVideo *pv = &Pico.video;
  unsigned char d;

  d = vdp_buffer;
  vdp_buffer = PicoMem.vramb[MEM_LE2(pv->addr)];
  pv->addr = (pv->addr + 1) & 0x3fff;
  pv->pending = 0;
  return d;
}

static unsigned char vdp_ctl_read(void)
{
  struct PicoVideo *pv = &Pico.video;
  unsigned char d;

  z80_int_assert(0);
  d = pv->status | (pv->pending_ints << 7);
  pv->pending = pv->pending_ints = 0;
  pv->status = 0;

  elprintf(EL_SR, "VDP sr: %02x", d);
  return d;
}

static void vdp_data_write(unsigned char d)
{
  struct PicoVideo *pv = &Pico.video;

  if (pv->type == 3) {
    if (Pico.m.hardware & 0x1) { // GG, same layout as MD
      unsigned a = pv->addr & 0x3f;
      if (a & 0x1) { // write complete color on high byte write
        u16 c = ((d&0x0f) << 8) | vdp_buffer;
        if (PicoMem.cram[a >> 1] != c) Pico.m.dirtyPal = 1;
        PicoMem.cram[a >> 1] = c;
      }
    } else { // SMS, convert to MD layout (00BbGgRr to 0000BbBbGgGgRrRr)
      unsigned a = pv->addr & 0x1f;
      u16 c = ((d&0x30)<<6) + ((d&0x0c)<<4) + ((d&0x03)<<2);
      if (PicoMem.cram[a] != (c | (c>>2))) Pico.m.dirtyPal = 1;
      PicoMem.cram[a] = c | (c>>2);
    }
  } else {
    PicoMem.vramb[MEM_LE2(pv->addr)] = d;
  }
  pv->addr = (pv->addr + 1) & 0x3fff;

  vdp_buffer = d;
  pv->pending = 0;
}

static NOINLINE void vdp_reg_write(struct PicoVideo *pv, u8 a, u8 d)
{
  int l;

  pv->reg[a] = d;
  switch (a) {
  case 0:
    l = pv->pending_ints & (d >> 3) & 2;
    elprintf(EL_INTS, "hint %d", l);
    z80_int_assert(l);
    break;
  case 1:
    l = pv->pending_ints & (d >> 5) & 1;
    elprintf(EL_INTS, "vint %d", l);
    z80_int_assert(l);
    break;
  }
}

static void vdp_ctl_write(u8 d)
{
  struct PicoVideo *pv = &Pico.video;

  if (pv->pending) {
    pv->type = d >> 6;
    if (pv->type == 2) {
      elprintf(EL_IO, "  VDP r%02x=%02x", d & 0x0f, pv->addr & 0xff);
      if (pv->reg[d & 0x0f] != (u8)pv->addr)
        vdp_reg_write(pv, d & 0x0f, pv->addr);
    }
    pv->addr &= 0x00ff;
    pv->addr |= (d & 0x3f) << 8;
    if (pv->type == 0) {
      vdp_buffer = PicoMem.vramb[MEM_LE2(pv->addr)];
      pv->addr = (pv->addr + 1) & 0x3fff;
    }
  } else {
    pv->addr &= 0x3f00;
    pv->addr |= d;
  }
  pv->pending ^= 1;
}

static unsigned char z80_sms_in(unsigned short a)
{
  unsigned char d = 0;

  elprintf(EL_IO, "z80 port %04x read", a);
  if((a&0xff)>= 0xf0){
    if (PicoIn.opt & POPT_EN_YM2413){
      switch((a&0xff))
      {
      case 0xf0:
        // FM reg port
        break;
      case 0xf1:
        // FM data port
        break;
      case 0xf2:
        // bit 0 = 1 active FM Pac
        d = 0xf8 | ymflag;
        break;
      }
    }
  }
  else{
    a &= 0xc1;
    switch (a)
    {
      case 0x00:
      case 0x01:
        d = 0xff & ~(PicoIn.pad[0] & 0x80);
        break;

      case 0x40: /* V counter */
        d = Pico.video.v_counter;
        elprintf(EL_HVCNT, "V counter read: %02x", d);
        break;

      case 0x41: /* H counter */
	d = vdp_hlatch;
        elprintf(EL_HVCNT, "H counter read: %02x", d);
        break;

      case 0x80:
        d = vdp_data_read();
        break;

      case 0x81:
        d = vdp_ctl_read();
        break;

      case 0xc0: /* I/O port A and B */
        d = ~((PicoIn.pad[0] & 0x3f) | (PicoIn.pad[1] << 6));
        break;

      case 0xc1: /* I/O port B and miscellaneous */
        d = (Pico.ms.io_ctl & 0x80) | ((Pico.ms.io_ctl << 1) & 0x40) | 0x30;
        d |= ~(PicoIn.pad[1] >> 2) & 0x0f;
        break;
    }
  }
  elprintf(EL_IO, "ret = %02x", d);
  return d;
}

static void z80_sms_out(unsigned short a, unsigned char d)
{
  elprintf(EL_IO, "z80 port %04x write %02x", a, d);

  if((a&0xff)>= 0xf0){
    if (PicoIn.opt & POPT_EN_YM2413){
      switch((a&0xff))
      {
        case 0xf0:
          // FM reg port
          YM2413_regWrite(d);
          break;
        case 0xf1:
          // FM data port
          YM2413_dataWrite(d);
          break;
        case 0xf2:
          // bit 0 = 1 active FM Pac
          ymflag = d & 0x1;
          break;
      }
    }
  }
  else{
    a &= 0xc1;
    switch (a)
    {
      case 0x01:
        // latch hcounter if one of the TH lines is switched to 1
        if ((Pico.ms.io_ctl ^ d) & d & 0xa0) {
          unsigned c = 228-z80_cyclesLeft;
          // 171 slots per scanline of 228 clocks, runs from 0xf4-0x93,0xe9-0xf3
          // this matches h counter tables in SMSVDPTest
          c = (((c+2) * ((171<<8)/228))>>8)-1 + 0xf4; // Q8 to avoid dividing
          if (c > 0x193) c += 0xe9-0x93-1;
          vdp_hlatch = (u8)c;
        }
        Pico.ms.io_ctl = d;
        break;

      case 0x40:
      case 0x41:
        PsndDoPSG(z80_cyclesDone());
        SN76496Write(d);
        break;

      case 0x80:
        vdp_data_write(d);
        break;

      case 0x81:
        vdp_ctl_write(d);
        break;
    }
  }
}

static int bank_mask;

static void xwrite(unsigned int a, unsigned char d);

static void write_sram(unsigned short a, unsigned char d)
{
  // SRAM is mapped in 2 16KB banks, selected by bit 2 in control reg
  a &= 0x3fff;
  a += ((Pico.ms.carthw[0x0c] & 0x04) >> 2) * 0x4000;

  Pico.sv.changed |= (Pico.sv.data[a] != d);
  Pico.sv.data[a] = d;
}

// 16KB bank mapping for Sega mapper
static void write_bank_sega(unsigned short a, unsigned char d)
{
  if (Pico.ms.mapper != 1 && d == 0) return;
  elprintf(EL_Z80BNK, "bank 16k %04x %02x @ %04x", a, d, z80_pc());
  Pico.ms.mapper = 1;
  Pico.ms.carthw[a & 0x0f] = d;

  switch (a & 0x0f)
  {
    case 0x0d:
      d &= bank_mask;
      z80_map_set(z80_read_map, 0x0400, 0x3fff, Pico.rom+0x400 + (d << 14), 0);
      break;
    case 0x0e:
      d &= bank_mask;
      z80_map_set(z80_read_map, 0x4000, 0x7fff, Pico.rom + (d << 14), 0);
      break;

    case 0x0c:
      if (d & ~0x8c)
        elprintf(EL_STATUS|EL_ANOMALY, "%02x written to control reg!", d);
      /*FALLTHROUGH*/
    case 0x0f:
      if (Pico.ms.carthw[0xc] & 0x08) {
        d = (Pico.ms.carthw[0xc] & 0x04) >> 2;
        z80_map_set(z80_read_map, 0x8000, 0xbfff, Pico.sv.data + d*0x4000, 0);
        z80_map_set(z80_write_map, 0x8000, 0xbfff, write_sram, 1);
      } else {
        d = Pico.ms.carthw[0xf] & bank_mask;
        z80_map_set(z80_read_map, 0x8000, 0xbfff, Pico.rom + (d << 14), 0);
        z80_map_set(z80_write_map, 0x8000, 0xbfff, xwrite, 1);
      }
      break;
  }
}

// 8KB ROM mapping for MSX mapper
static void write_bank_msx(unsigned short a, unsigned char d)
{
  if (Pico.ms.mapper != 2 && (a|d) == 0) return;
  elprintf(EL_Z80BNK, "bank  8k %04x %02x @ %04x", a, d, z80_pc());
  Pico.ms.mapper = 2; // TODO define (more) mapper types
  Pico.ms.carthw[a] = d;

  a = (a^2)*0x2000 + 0x4000;
  d &= 2*bank_mask + 1;
  z80_map_set(z80_read_map, a, a+0x1fff, Pico.rom + (d << 13), 0);
}

// TODO mapping is currently auto-selecting, but that's not very reliable.
static void xwrite(unsigned int a, unsigned char d)
{
  elprintf(EL_IO, "z80 write [%04x] %02x", a, d);
  if (a >= 0xc000)
    PicoMem.zram[a & 0x1fff] = d;

  // Sega. Maps 4 bank 16KB each
  if (a >= 0xfff8 && Pico.ms.mapper != 2)
    write_bank_sega(a, d);
  // Codemasters. Similar to Sega, but different addresses
//  if (a == 0x0000 && Pico.ms.mapper != 2)
//    write_bank_sega(0xfffd, d);
  if (a == 0x4000 && Pico.ms.mapper != 2)
    write_bank_sega(0xfffe, d);
  if (a == 0x8000 && Pico.ms.mapper != 2)
    write_bank_sega(0xffff, d);
  // Korean. 1 selectable 16KB bank at the top
  if (a == 0xa000 && Pico.ms.mapper != 2)
    write_bank_sega(0xffff, d);
  // MSX. 4 selectable 8KB banks at the top
  if (a <= 0x0003 && Pico.ms.mapper != 1)
    write_bank_msx(a, d);
}

void PicoResetMS(void)
{
  z80_reset();
  PsndReset(); // pal must be known here
  ymflag = 0xffff;
  Pico.m.dirtyPal = 1;

  if (PicoIn.hwSelect) {
    switch (PicoIn.hwSelect) {
    case PHWS_GG:  Pico.m.hardware |=  0x1; break;
    default:       Pico.m.hardware &= ~0x1; break;
    }
  } else {
    unsigned tmr;

    // check if the ROM header contains more system information to detect GG
    for (tmr = 0x2000; tmr < 0xbfff && tmr <= Pico.romsize; tmr *= 2) {
      if (!memcmp(Pico.rom + tmr-16, "TMR SEGA", 8)) {
        if (Pico.rom[tmr-1] >= 0x50 && Pico.rom[tmr-1] < 0x80)
          Pico.m.hardware |= 0x1;
        break;
      }
    }
  }

  // reset memory mapping
  PicoMemSetupMS();
}

void PicoPowerMS(void)
{
  int s, tmp;

  memset(&PicoMem,0,sizeof(PicoMem));
  memset(&Pico.video,0,sizeof(Pico.video));
  memset(&Pico.m,0,sizeof(Pico.m));
  Pico.m.pal = 0;

  // calculate a mask for bank writes.
  // ROM loader has aligned the size for us, so this is safe.
  s = 0; tmp = Pico.romsize;
  while ((tmp >>= 1) != 0)
    s++;
  if (Pico.romsize > (1 << s))
    s++;
  tmp = 1 << s;
  bank_mask = (tmp - 1) >> 14;

  Pico.ms.mapper = 0;
  PicoReset();
}

void PicoMemSetupMS(void)
{
  z80_map_set(z80_read_map, 0x0000, 0xbfff, Pico.rom, 0);
  z80_map_set(z80_read_map, 0xc000, 0xdfff, PicoMem.zram, 0);
  z80_map_set(z80_read_map, 0xe000, 0xffff, PicoMem.zram, 0);

  z80_map_set(z80_write_map, 0x0000, 0xbfff, xwrite, 1);
  z80_map_set(z80_write_map, 0xc000, 0xdfff, PicoMem.zram, 0);
  z80_map_set(z80_write_map, 0xe000, 0xffff, xwrite, 1);
 
#ifdef _USE_DRZ80
  drZ80.z80_in = z80_sms_in;
  drZ80.z80_out = z80_sms_out;
#endif
#ifdef _USE_CZ80
  Cz80_Set_INPort(&CZ80, z80_sms_in);
  Cz80_Set_OUTPort(&CZ80, z80_sms_out);
#endif

  // memory mapper setup
  u8 mapper = Pico.ms.mapper;
  if (Pico.ms.mapper == 2) {
    xwrite(0x0000, 0);
    xwrite(0x0001, 0);
    xwrite(0x0002, 0);
    xwrite(0x0003, 0);
  } else {
    xwrite(0xfffc, 0);
    xwrite(0xfffd, 0);
    xwrite(0xfffe, 1);
    xwrite(0xffff, 2);
  }
  Pico.ms.mapper = mapper;
}

void PicoStateLoadedMS(void)
{
  u8 mapper = Pico.ms.mapper;
  if (Pico.ms.mapper == 2) {
    xwrite(0x0000, Pico.ms.carthw[0]);
    xwrite(0x0001, Pico.ms.carthw[1]);
    xwrite(0x0002, Pico.ms.carthw[2]);
    xwrite(0x0003, Pico.ms.carthw[3]);
  } else {
    xwrite(0xfffc, Pico.ms.carthw[0x0c]);
    xwrite(0xfffd, Pico.ms.carthw[0x0d]);
    xwrite(0xfffe, Pico.ms.carthw[0x0e]);
    xwrite(0xffff, Pico.ms.carthw[0x0f]);
  }
  Pico.ms.mapper = mapper;
}

void PicoFrameMS(void)
{
  struct PicoVideo *pv = &Pico.video;
  int is_pal = Pico.m.pal;
  int lines = is_pal ? 313 : 262;
  int cycles_line = 228;
  int cycles_done = 0, cycles_aim = 0;
  int skip = PicoIn.skipFrame;
  int lines_vis = 192;
  int hint; // Hint counter
  int nmi;
  int y;

  PsndStartFrame();

  nmi = (PicoIn.pad[0] >> 7) & 1;
  if (!Pico.ms.nmi_state && nmi)
    z80_nmi();
  Pico.ms.nmi_state = nmi;

  if ((pv->reg[0] & 6) == 6 && (pv->reg[1] & 0x18))
    lines_vis = (pv->reg[1] & 0x08) ? 240 : 224;
  PicoFrameStartSMS();
  hint = pv->reg[0x0a];

  for (y = 0; y < lines; y++)
  {
    pv->v_counter = Pico.m.scanline = y;
    if (y > 218)
      pv->v_counter = y - 6;

    if (y < lines_vis && !skip)
      PicoLineSMS(y);

    if (y <= lines_vis)
    {
      if (--hint < 0)
      {
        hint = pv->reg[0x0a];
        pv->pending_ints |= 2;
        if (pv->reg[0] & 0x10) {
          elprintf(EL_INTS, "hint");
          z80_int_assert(1);
	}
      } else if (pv->pending_ints & 2) {
        pv->pending_ints &= ~2;
        z80_int_assert(0);
      }
    }
    else if (y == lines_vis + 1) {
      pv->pending_ints &= ~2;
      pv->pending_ints |= 1;
      if (pv->reg[1] & 0x20) {
        elprintf(EL_INTS, "vint");
        z80_int_assert(1);
      } else
        z80_int_assert(0);
    }

    cycles_aim += cycles_line;
    Pico.t.z80c_aim = cycles_aim;
    cycles_done += z80_run(cycles_aim - cycles_done);
  }

  if (PicoIn.sndOut)
    PsndGetSamplesMS(lines);
}

void PicoFrameDrawOnlyMS(void)
{
  int lines_vis = 192;
  int y;

  PicoFrameStartSMS();

  for (y = 0; y < lines_vis; y++)
    PicoLineSMS(y);
}

// vim:ts=2:sw=2:expandtab
