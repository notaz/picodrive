#include "../pico_int.h"
#include "../memory.h"

static const char str_mars[] = "MARS";

struct Pico32xMem *Pico32xMem;

// SH2 faking
static const u16 comm_fakevals[] = {
  0x4d5f, 0x4f4b, // M_OK
  0x535f, 0x4f4b, // S_OK
  0x0002, // Mortal Kombat
  0, // pad
};

static u32 p32x_reg_read16(u32 a)
{
  a &= 0x3e;

  // SH2 faker
  if ((a & 0x30) == 0x20)
  {
    static int f = 0, csum_faked = 0;
    if (a == 0x28 && !csum_faked) {
      csum_faked = 1;
      return *(unsigned short *)(Pico.rom + 0x18e);
    }
    if (f >= sizeof(comm_fakevals) / sizeof(comm_fakevals[0]))
      f = 0;
    return comm_fakevals[f++];
  }

  return Pico32x.regs[a / 2];
}

static void p32x_reg_write16(u32 a, u32 d)
{
  a &= 0x3e;

  if (a == 0 && !(Pico32x.regs[0] & 1)) {
    Pico32x.regs[0] |= 1;
    Pico32xStartup();
    return;
  }
}

static void p32x_reg_write8(u32 a, u32 d)
{
  a &= 0x3f;

  if (a == 1 && !(Pico32x.regs[0] & 1)) {
    Pico32x.regs[0] |= 1;
    Pico32xStartup();
    return;
  }
}

// VDP regs
static u32 p32x_vdp_read16(u32 a)
{
  a &= 0x0e;

  return Pico32x.vdp_regs[a / 2];
}

static void p32x_vdp_write8(u32 a, u32 d)
{
  u16 *r = Pico32x.vdp_regs;
  a &= 0x0f;

  // TODO: verify what's writeable
  switch (a) {
    case 0x01:
      if (((r[0] & 3) == 0) != ((d & 3) == 0)) { // forced blanking changed
        if (Pico.video.status & 8)
          r[0x0a/2] |=  P32XV_VBLK;
        else
          r[0x0a/2] &= ~P32XV_VBLK;
      }
      r[0] = (r[0] & P32XV_nPAL) | (d & 0xff);
      break;
    case 0x0b:
      d &= 1;
      Pico32x.pending_fb = d;
      // if we are blanking and FS bit is changing
      if ((r[0x0a/2] & P32XV_VBLK) && ((r[0x0a/2] ^ d) & P32XV_FS)) {
        r[0x0a/2] ^= 1;
	Pico32xSwapDRAM(d ^ 1);
      }
      break;
  }
}

static void p32x_vdp_write16(u32 a, u32 d)
{
  p32x_vdp_write8(a | 1, d);
}

// default 32x handlers
u32 PicoRead8_32x(u32 a)
{
  u32 d = 0;
  if ((a & 0xffc0) == 0x5100) { // a15100
    d = p32x_reg_read16(a);
    goto out_16to8;
  }

  if (!(Pico32x.regs[0] & 1))
    goto no_vdp;

  if ((a & 0xfff0) == 0x5180) { // a15180
    d = p32x_vdp_read16(a);
    goto out_16to8;
  }

  if ((a & 0xfe00) == 0x5200) { // a15200
    d = Pico32xMem->pal[(a & 0x1ff) / 2];
    goto out_16to8;
  }

no_vdp:
  if ((a & 0xfffc) == 0x30ec) { // a130ec
    d = str_mars[a & 3];
    goto out;
  }

  elprintf(EL_UIO, "m68k unmapped r8  [%06x] @%06x", a, SekPc);
  return d;

out_16to8:
  if (a & 1)
    d &= 0xff;
  else
    d >>= 8;

out:
  elprintf(EL_32X, "m68k 32x r8  [%06x]   %02x @%06x", a, d, SekPc);
  return d;
}

u32 PicoRead16_32x(u32 a)
{
  u32 d = 0;
  if ((a & 0xffc0) == 0x5100) { // a15100
    d = p32x_reg_read16(a);
    goto out;
  }

  if (!(Pico32x.regs[0] & 1))
    goto no_vdp;

  if ((a & 0xfff0) == 0x5180) { // a15180
    d = p32x_vdp_read16(a);
    goto out;
  }

  if ((a & 0xfe00) == 0x5200) { // a15200
    d = Pico32xMem->pal[(a & 0x1ff) / 2];
    goto out;
  }

no_vdp:
  if ((a & 0xfffc) == 0x30ec) { // a130ec
    d = !(a & 2) ? ('M'<<8)|'A' : ('R'<<8)|'S';
    goto out;
  }

  elprintf(EL_UIO, "m68k unmapped r16 [%06x] @%06x", a, SekPc);
  return d;

out:
  elprintf(EL_32X, "m68k 32x r16 [%06x] %04x @%06x", a, d, SekPc);
  return d;
}

void PicoWrite8_32x(u32 a, u32 d)
{
  if ((a & 0xfc00) == 0x5000)
    elprintf(EL_32X, "m68k 32x w8  [%06x]   %02x @%06x", a, d & 0xff, SekPc);

  if ((a & 0xffc0) == 0x5100) { // a15100
    p32x_reg_write8(a, d);
    return;
  }

  if (!(Pico32x.regs[0] & 1))
    goto no_vdp;

  if ((a & 0xfff0) == 0x5180) { // a15180
    p32x_vdp_write8(a, d);
    return;
  }

  // TODO: verify
  if ((a & 0xfe00) == 0x5200) { // a15200
    elprintf(EL_32X|EL_ANOMALY, "m68k 32x PAL w8  [%06x]   %02x @%06x", a, d & 0xff, SekPc);
    ((u8 *)Pico32xMem->pal)[(a & 0x1ff) ^ 1] = d;
    Pico32x.dirty_pal = 1;
    return;
  }

no_vdp:
  elprintf(EL_UIO, "m68k unmapped w8  [%06x]   %02x @%06x", a, d & 0xff, SekPc);
}

void PicoWrite16_32x(u32 a, u32 d)
{
  if ((a & 0xfc00) == 0x5000)
    elprintf(EL_UIO, "m68k 32x w16 [%06x] %04x @%06x", a, d & 0xffff, SekPc);

  if ((a & 0xffc0) == 0x5100) { // a15100
    p32x_reg_write16(a, d);
    return;
  }

  if (!(Pico32x.regs[0] & 1))
    goto no_vdp;

  if ((a & 0xfff0) == 0x5180) { // a15180
    p32x_vdp_write16(a, d);
    return;
  }

  if ((a & 0xfe00) == 0x5200) { // a15200
    Pico32xMem->pal[(a & 0x1ff) / 2] = d;
    Pico32x.dirty_pal = 1;
    return;
  }

no_vdp:
  elprintf(EL_UIO, "m68k unmapped w16 [%06x] %04x @%06x", a, d & 0xffff, SekPc);
}

// hint vector is writeable
static void PicoWrite8_hint(u32 a, u32 d)
{
  if ((a & 0xfffc) == 0x0070) {
    Pico32xMem->m68k_rom[a ^ 1] = d;
    return;
  }

  elprintf(EL_UIO, "m68k unmapped w8  [%06x]   %02x @%06x", a, d & 0xff, SekPc);
}

static void PicoWrite16_hint(u32 a, u32 d)
{
  if ((a & 0xfffc) == 0x0070) {
    ((u16 *)Pico32xMem->m68k_rom)[a/2] = d;
    return;
  }

  elprintf(EL_UIO, "m68k unmapped w16 [%06x] %04x @%06x", a, d & 0xffff, SekPc);
}

void Pico32xSwapDRAM(int b)
{
  cpu68k_map_set(m68k_read8_map,   0x840000, 0x85ffff, Pico32xMem->dram[b], 0);
  cpu68k_map_set(m68k_read16_map,  0x840000, 0x85ffff, Pico32xMem->dram[b], 0);
  cpu68k_map_set(m68k_write8_map,  0x840000, 0x85ffff, Pico32xMem->dram[b], 0);
  cpu68k_map_set(m68k_write16_map, 0x840000, 0x85ffff, Pico32xMem->dram[b], 0);
}

#define HWSWAP(x) (((x) << 16) | ((x) >> 16))
void PicoMemSetup32x(void)
{
  unsigned short *ps;
  unsigned int *pl;
  unsigned int rs, rs1;
  int i;

  Pico32xMem = calloc(1, sizeof(*Pico32xMem));
  if (Pico32xMem == NULL) {
    elprintf(EL_STATUS, "OOM");
    return;
  }

  // generate 68k ROM
  ps = (unsigned short *)Pico32xMem->m68k_rom;
  pl = (unsigned int *)Pico32xMem->m68k_rom;
  for (i = 1; i < 0xc0/4; i++)
    pl[i] = HWSWAP(0x880200 + (i - 1) * 6);

  // fill with nops
  for (i = 0xc0/2; i < 0x100/2; i++)
    ps[i] = 0x4e71;

  ps[0xc0/2] = 0x46fc;
  ps[0xc2/2] = 0x2700; // move #0x2700,sr
  ps[0xfe/2] = 0x60fe; // jump to self

  // fill remaining mem with ROM
  memcpy(Pico32xMem->m68k_rom + 0x100, Pico.rom + 0x100, sizeof(Pico32xMem->m68k_rom) - 0x100);

  // cartridge area becomes unmapped
  // XXX: we take the easy way and don't unmap ROM,
  // so that we can avoid handling the RV bit.
  // m68k_map_unmap(0x000000, 0x3fffff);

  // MD ROM area
  rs = sizeof(Pico32xMem->m68k_rom);
  cpu68k_map_set(m68k_read8_map,   0x000000, rs - 1, Pico32xMem->m68k_rom, 0);
  cpu68k_map_set(m68k_read16_map,  0x000000, rs - 1, Pico32xMem->m68k_rom, 0);
  cpu68k_map_set(m68k_write8_map,  0x000000, rs - 1, PicoWrite8_hint, 1); // TODO verify
  cpu68k_map_set(m68k_write16_map, 0x000000, rs - 1, PicoWrite16_hint, 1);

  // DRAM area
  Pico32xSwapDRAM(1);

  // 32X ROM (unbanked, XXX: consider mirroring?)
  rs1 = rs = (Pico.romsize + M68K_BANK_MASK) & ~M68K_BANK_MASK;
  if (rs1 > 0x80000)
    rs1 = 0x80000;
  cpu68k_map_set(m68k_read8_map,   0x880000, 0x880000 + rs1 - 1, Pico.rom, 0);
  cpu68k_map_set(m68k_read16_map,  0x880000, 0x880000 + rs1 - 1, Pico.rom, 0);

  // 32X ROM (banked)
  if (rs > 0x100000)
    rs = 0x100000;
  cpu68k_map_set(m68k_read8_map,   0x900000, 0x900000 + rs - 1, Pico.rom, 0);
  cpu68k_map_set(m68k_read16_map,  0x900000, 0x900000 + rs - 1, Pico.rom, 0);
}

