#include "../pico_int.h"
#include "../memory.h"

static const char str_mars[] = "MARS";

struct Pico32xMem *Pico32xMem;

static void bank_switch(int b);

#define MSB8(x) ((x) >> 8)

// poll detection
struct poll_det {
	int addr, pc, cnt;
};
static struct poll_det m68k_poll;
static struct poll_det msh2_poll;

#define POLL_THRESHOLD 6

static int poll_detect(struct poll_det *pd, u32 a, u32 pc, int flag)
{
  int ret = 0;

  if (a - 2 <= pd->addr && pd->addr <= a + 2 && pd->pc == pc) {
    pd->cnt++;
    if (pd->cnt > POLL_THRESHOLD) {
      if (!(Pico32x.emu_flags & flag)) {
        elprintf(EL_32X, "%s poll addr %08x @ %06x",
          flag == P32XF_68KPOLL ? "m68k" : (flag == P32XF_MSH2POLL ? "msh2" : "ssh2"), a, pc);
        ret = 1;
      }
      Pico32x.emu_flags |= flag;
    }
  }
  else
    pd->cnt = 0;
  pd->addr = a;
  pd->pc = pc;

  return ret;
}

static int poll_undetect(struct poll_det *pd, int flag)
{
  int ret = 0;
  if (pd->cnt > POLL_THRESHOLD)
    ret = 1;
  pd->addr = pd->cnt = 0;
  Pico32x.emu_flags &= ~flag;
  return ret;
}

// SH2 faking
#define FAKE_SH2
int p32x_csum_faked;
#ifdef FAKE_SH2
static const u16 comm_fakevals[] = {
  0x4d5f, 0x4f4b, // M_OK
  0x535f, 0x4f4b, // S_OK
  0x4D41, 0x5346, // MASF - Brutal Unleashed
  0x5331, 0x4d31, // Darxide
  0x5332, 0x4d32,
  0x5333, 0x4d33,
  0x0000, 0x0000, // eq for doom
  0x0002, // Mortal Kombat
//  0, // pad
};

static u32 sh2_comm_faker(u32 a)
{
  static int f = 0;
  if (a == 0x28 && !p32x_csum_faked) {
    p32x_csum_faked = 1;
    return *(unsigned short *)(Pico.rom + 0x18e);
  }
  if (f >= sizeof(comm_fakevals) / sizeof(comm_fakevals[0]))
    f = 0;
  return comm_fakevals[f++];
}
#endif

static u32 p32x_reg_read16(u32 a)
{
  a &= 0x3e;

#if 0
  if ((a & 0x30) == 0x20)
    return sh2_comm_faker(a);
#else
  if (poll_detect(&m68k_poll, a, SekPc, P32XF_68KPOLL)) {
    SekSetStop(1);
    SekEndRun(16);
  }
#endif
#ifdef FAKE_SH2
  // fake only slave for now
  if (a == 0x24 || a == 0x26)
    return sh2_comm_faker(a);
#endif

  return Pico32x.regs[a / 2];
}

static void p32x_reg_write8(u32 a, u32 d)
{
  u16 *r = Pico32x.regs;
  a &= 0x3f;

  if (a == 1 && !(r[0] & 1)) {
    r[0] |= 1;
    Pico32xStartup();
    return;
  }

  if (!(r[0] & 1))
    return;

  switch (a) {
    case 0:
      r[0] = (r[0] & 0x83) | ((d << 8) & P32XS_FM);
      break;
    case 5:
      d &= 7;
      if (r[4/2] != d) {
        r[4/2] = d;
        bank_switch(d);
      }
      break;
  }
}

static void p32x_reg_write16(u32 a, u32 d)
{
  u16 *r = Pico32x.regs;
  a &= 0x3e;

  switch (a) {
    case 0:
      r[0] = (r[0] & 0x83) | (d & P32XS_FM);
      return;
  }

  if ((a & 0x30) == 0x20 && r[a / 2] != d) {
    r[a / 2] = d;
    if (poll_undetect(&msh2_poll, P32XF_MSH2POLL))
      // if SH2 is busy waiting, it needs to see the result ASAP
      SekEndRun(16);
    return;
  }

  p32x_reg_write8(a + 1, d);
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
      // priority inversion is handled in palette
      if ((r[0] ^ d) & P32XV_PRI)
        Pico32x.dirty_pal = 1;
      r[0] = (r[0] & P32XV_nPAL) | (d & 0xff);
      break;
    case 0x0b:
      d &= 1;
      Pico32x.pending_fb = d;
      // if we are blanking and FS bit is changing
      if ((r[0x0a/2] & P32XV_VBLK) && ((r[0x0a/2] ^ d) & P32XV_FS)) {
        r[0x0a/2] ^= 1;
	Pico32xSwapDRAM(d ^ 1);
        elprintf(EL_32X, "VDP FS: %d", r[0x0a/2] & P32XV_FS);
      }
      break;
  }
}

static void p32x_vdp_write16(u32 a, u32 d)
{
  p32x_vdp_write8(a | 1, d);
}

// SH2 regs
static u32 p32x_sh2reg_read16(u32 a)
{
  a &= 0xff; // ?

  if (poll_detect(&msh2_poll, a, ash2_pc(), P32XF_MSH2POLL))
    ash2_end_run(8);

  if (a == 0) {
    return (Pico32x.regs[0] & P32XS_FM) | P32XS2_ADEN;
  }
  if ((a & 0x30) == 0x20)
    return Pico32x.regs[a / 2];

  return 0;
}

static void p32x_sh2reg_write8(u32 a, u32 d)
{
}

static void p32x_sh2reg_write16(u32 a, u32 d)
{
  a &= 0xff;

  if ((a & 0x30) == 0x20) {
    Pico32x.regs[a/2] = d;
    if (poll_undetect(&m68k_poll, P32XF_68KPOLL))
      // dangerous, but let's just assume 68k program
      // didn't issue STOP itself.
      SekSetStop(0);
    return;
  }

  p32x_sh2reg_write8(a | 1, d);
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

static void bank_switch(int b)
{
  unsigned int rs, bank;

  bank = b << 20;
  if (bank >= Pico.romsize) {
    elprintf(EL_32X|EL_ANOMALY, "missing bank @ %06x", bank);
    return;
  }

  // 32X ROM (unbanked, XXX: consider mirroring?)
  rs = (Pico.romsize + M68K_BANK_MASK) & ~M68K_BANK_MASK;
  rs -= bank;
  if (rs > 0x100000)
    rs = 0x100000;
  cpu68k_map_set(m68k_read8_map,   0x900000, 0x900000 + rs - 1, Pico.rom + bank, 0);
  cpu68k_map_set(m68k_read16_map,  0x900000, 0x900000 + rs - 1, Pico.rom + bank, 0);

  elprintf(EL_32X, "bank %06x-%06x -> %06x", 0x900000, 0x900000 + rs - 1, bank);
}

// -----------------------------------------------------------------
//                              SH2  
// -----------------------------------------------------------------

u32 pico32x_read8(u32 a)
{
  u32 d = 0;
  if (a < sizeof(Pico32xMem->sh2_rom_m))
    return Pico32xMem->sh2_rom_m[a ^ 1];

  if ((a & 0x0ffc0000) == 0x06000000)
    return Pico32xMem->sdram[(a & 0x3ffff) ^ 1];

  if ((a & 0x0fc00000) == 0x02000000)
    if ((a & 0x003fffff) < Pico.romsize)
      return Pico.rom[(a & 0x3fffff) ^ 1];

  if ((a & 0x0fffff00) == 0x4000) {
    d = p32x_sh2reg_read16(a);
    goto out_16to8;
  }

  if ((a & 0x0fffff00) == 0x4100) {
    d = p32x_vdp_read16(a);
    goto out_16to8;
  }

  if ((a & 0x0fffff00) == 0x4200) {
    d = Pico32xMem->pal[(a & 0x1ff) / 2];
    goto out_16to8;
  }

  elprintf(EL_UIO, "sh2 unmapped r8  [%08x]       %02x @%06x", a, d, ash2_pc());
  return d;

out_16to8:
  if (a & 1)
    d &= 0xff;
  else
    d >>= 8;

  elprintf(EL_32X, "sh2 r8  [%08x]       %02x @%06x", a, d, ash2_pc());
  return d;
}

u32 pico32x_read16(u32 a)
{
  u32 d = 0;

  if (a < sizeof(Pico32xMem->sh2_rom_m))
    return *(u16 *)(Pico32xMem->sh2_rom_m + a);

  if ((a & 0x0ffc0000) == 0x06000000)
    return ((u16 *)Pico32xMem->sdram)[(a & 0x3ffff) / 2];

  if ((a & 0x0fc00000) == 0x02000000)
    if ((a & 0x003fffff) < Pico.romsize)
      return ((u16 *)Pico.rom)[(a & 0x3fffff) / 2];

  if ((a & 0x0fffff00) == 0x4000) {
    d = p32x_sh2reg_read16(a);
    goto out;
  }

  if ((a & 0x0fffff00) == 0x4100) {
    d = p32x_vdp_read16(a);
    goto out;
  }

  if ((a & 0x0fffff00) == 0x4200) {
    d = Pico32xMem->pal[(a & 0x1ff) / 2];
    goto out;
  }

  elprintf(EL_UIO, "sh2 unmapped r16 [%08x]     %04x @%06x", a, d, ash2_pc());
  return d;

out:
  elprintf(EL_32X, "sh2 r16 [%08x]     %04x @%06x", a, d, ash2_pc());
  return d;
}

u32 pico32x_read32(u32 a)
{
//  elprintf(EL_UIO, "sh2 r32 [%08x] %08x @%06x", a, d, ash2_pc());
  return (pico32x_read16(a) << 16) | pico32x_read16(a + 2);
}

void pico32x_write8(u32 a, u32 d)
{
  if ((a & 0x0ffffc00) == 0x4000)
    elprintf(EL_32X, "sh2 w8  [%08x]       %02x @%06x", a, d & 0xff, ash2_pc());

  if ((a & 0x0ffc0000) == 0x06000000) {
    Pico32xMem->sdram[(a & 0x3ffff) ^ 1] = d;
    return;
  }

  if ((a & 0x0ffe0000) == 0x04000000) {
    u8 *dram = (u8 *)Pico32xMem->dram[(Pico32x.vdp_regs[0x0a/2] & P32XV_FS) ^ 1];
    dram[(a & 0x1ffff) ^ 1] = d;
    return;
  }

  if ((a & 0x0fffff00) == 0x4100) {
    p32x_vdp_write8(a, d);
    return;
  }

  if ((a & 0x0fffff00) == 0x4000) {
    p32x_sh2reg_write8(a, d);
    return;
  }

  elprintf(EL_UIO, "sh2 unmapped w8  [%08x]       %02x @%06x", a, d & 0xff, ash2_pc());
}

void pico32x_write16(u32 a, u32 d)
{
  if ((a & 0x0ffffc00) == 0x4000)
    elprintf(EL_32X, "sh2 w16 [%08x]     %04x @%06x", a, d & 0xffff, ash2_pc());

  if ((a & 0x0ffc0000) == 0x06000000) {
    ((u16 *)Pico32xMem->sdram)[(a & 0x3ffff) / 2] = d;
    return;
  }

  if ((a & 0x0ffe0000) == 0x04000000) {
    Pico32xMem->dram[(Pico32x.vdp_regs[0x0a/2] & P32XV_FS) ^ 1][(a & 0x1ffff) / 2] = d;
    return;
  }

  if ((a & 0x0fffff00) == 0x4100) {
    p32x_vdp_write16(a, d);
    return;
  }

  if ((a & 0x0ffffe00) == 0x4200) {
    Pico32xMem->pal[(a & 0x1ff) / 2] = d;
    Pico32x.dirty_pal = 1;
    return;
  }

  if ((a & 0x0fffff00) == 0x4000) {
    p32x_sh2reg_write16(a, d);
    return;
  }

  elprintf(EL_UIO, "sh2 unmapped w16 [%08x]     %04x @%06x", a, d & 0xffff, ash2_pc());
}

void pico32x_write32(u32 a, u32 d)
{
//  elprintf(EL_UIO, "sh2 w32 [%08x] %08x @%06x", a, d, ash2_pc());
  pico32x_write16(a, d >> 16);
  pico32x_write16(a + 2, d);
}

#define HWSWAP(x) (((x) << 16) | ((x) >> 16))
void PicoMemSetup32x(void)
{
  unsigned short *ps;
  unsigned int *pl;
  unsigned int rs;
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

#if 0
  ps[0xc0/2] = 0x46fc;
  ps[0xc2/2] = 0x2700; // move #0x2700,sr
  ps[0xfe/2] = 0x60fe; // jump to self
#else
  ps[0xfe/2] = 0x4e75; // rts
#endif

  // fill remaining mem with ROM
  memcpy(Pico32xMem->m68k_rom + 0x100, Pico.rom + 0x100, sizeof(Pico32xMem->m68k_rom) - 0x100);

  // 32X ROM
  // TODO: move
  {
    FILE *f = fopen("32X_M_BIOS.BIN", "rb");
    int i;
    if (f == NULL) {
      printf("missing BIOS\n");
      exit(1);
    }
    fread(Pico32xMem->sh2_rom_m, 1, sizeof(Pico32xMem->sh2_rom_m), f);
    fclose(f);
    for (i = 0; i < sizeof(Pico32xMem->sh2_rom_m); i += 2) {
      int t = Pico32xMem->sh2_rom_m[i];
      Pico32xMem->sh2_rom_m[i] = Pico32xMem->sh2_rom_m[i + 1];
      Pico32xMem->sh2_rom_m[i + 1] = t;
    }
  }

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
  rs = (Pico.romsize + M68K_BANK_MASK) & ~M68K_BANK_MASK;
  if (rs > 0x80000)
    rs = 0x80000;
  cpu68k_map_set(m68k_read8_map,   0x880000, 0x880000 + rs - 1, Pico.rom, 0);
  cpu68k_map_set(m68k_read16_map,  0x880000, 0x880000 + rs - 1, Pico.rom, 0);

  // 32X ROM (banked)
  bank_switch(0);
}

