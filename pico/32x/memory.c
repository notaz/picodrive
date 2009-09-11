#include "../pico_int.h"
#include "../memory.h"

static const char str_mars[] = "MARS";

struct Pico32xMem {
  u8 sdram[0x40000];
  u8 dram[0x40000]; // AKA fb
  u8 m68k_rom[M68K_BANK_SIZE]; // 0x100
};

static struct Pico32xMem *Pico32xMem;

static u32 p32x_reg_read16(u32 a)
{
  a &= 0x3e;

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

static void p32x_vdp_write16(u32 a, u32 d)
{
  a &= 0x0e;

  switch (a) {
    case 0x0a:
      Pico32x.pending_fb = d & 1;
      if (Pico.video.status & 8) {
        Pico32x.vdp_regs[0x0a/2] &= ~1;
        Pico32x.vdp_regs[0x0a/2] |= d & 1;
      }
      break;
  }
}

static void p32x_vdp_write8(u32 a, u32 d)
{
  a &= 0x0f;

  switch (a) {
    case 0x0b:
      Pico32x.pending_fb = d & 1;
      if (Pico.video.status & 8) {
        Pico32x.vdp_regs[0x0a/2] &= ~1;
        Pico32x.vdp_regs[0x0a/2] |= d & 1;
      }
      break;
  }
}

// default 32x handlers
u32 PicoRead8_32x(u32 a)
{
  u32 d = 0;
  if ((a & 0xffc0) == 0x5100) { // a15100
    d = p32x_reg_read16(a);
    goto out_16to8;
  }

  if ((a & 0xfff0) == 0x5180 && (Pico32x.regs[0] & 1)) {
    d = p32x_vdp_read16(a);
    goto out_16to8;
  }

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

  if ((a & 0xfff0) == 0x5180 && (Pico32x.regs[0] & 1)) { // a15180
    d = p32x_vdp_read16(a);
    goto out;
  }

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

  if ((a & 0xfff0) == 0x5180 && (Pico32x.regs[0] & 1)) { // a15180
    p32x_vdp_write8(a, d);
    return;
  }

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

  if ((a & 0xfff0) == 0x5180 && (Pico32x.regs[0] & 1)) { // a15180
    p32x_vdp_write16(a, d);
    return;
  }

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
    pl[i] = HWSWAP(0x880200 + i * 6);

  // fill with nops
  for (i = 0xc0/2; i < 0x100/2; i++)
    ps[i] = 0x4e71;

  ps[0xc0/2] = 0x46fc;
  ps[0xc2/2] = 0x2700; // move #0x2700,sr
  ps[0xfe/2] = 0x60fe; // jump to self

  // fill remaining mem with ROM
  memcpy(Pico32xMem->m68k_rom + 0x100, Pico.rom + 0x100, M68K_BANK_SIZE - 0x100);

  // cartridge area becomes unmapped
  // XXX: we take the easy way and don't unmap ROM,
  // so that we can avoid handling the RV bit.
  // m68k_map_unmap(0x000000, 0x3fffff);

  // MD ROM area
  cpu68k_map_set(m68k_read8_map,   0x000000, M68K_BANK_SIZE - 1, Pico32xMem->m68k_rom, 0);
  cpu68k_map_set(m68k_read16_map,  0x000000, M68K_BANK_SIZE - 1, Pico32xMem->m68k_rom, 0);
  cpu68k_map_set(m68k_write8_map,  0x000000, M68K_BANK_SIZE - 1, PicoWrite8_hint, 1); // TODO verify
  cpu68k_map_set(m68k_write16_map, 0x000000, M68K_BANK_SIZE - 1, PicoWrite16_hint, 1);

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

