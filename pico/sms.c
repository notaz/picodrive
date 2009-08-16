#include "pico_int.h"

static unsigned char z80_sms_in(unsigned short p)
{
  elprintf(EL_ANOMALY, "Z80 port %04x read", p);
  return 0xff;
}

static void z80_sms_out(unsigned short p, unsigned char d)
{
  elprintf(EL_ANOMALY, "Z80 port %04x write %02x", p, d);
}

static unsigned char MEMH_FUNC xread(unsigned short a)
{
  elprintf(EL_ANOMALY, "Z80 read  [%04x]", a);
  return 0;
}

static void MEMH_FUNC xwrite(unsigned int a, unsigned char data)
{
  elprintf(EL_ANOMALY, "Z80 write [%04x] %02x", a, data);
}

void PicoPowerMS(void)
{
}

void PicoMemSetupMS(void)
{
  z80_map_set(z80_read_map, 0x0000, 0xbfff, Pico.rom, 0);
  z80_map_set(z80_read_map, 0xc000, 0xdfff, Pico.zram, 0);
  z80_map_set(z80_read_map, 0xe000, 0xffff, xread, 1);

  z80_map_set(z80_write_map, 0x0000, 0xbfff, Pico.rom, 0);
  z80_map_set(z80_write_map, 0xc000, 0xdfff, Pico.zram, 0);
  z80_map_set(z80_write_map, 0xe000, 0xffff, xwrite, 1);
 
#ifdef _USE_DRZ80
  drZ80.z80_in = z80_sms_in;
  drZ80.z80_out = z80_sms_out;
#endif
#ifdef _USE_CZ80
  Cz80_Set_Fetch(&CZ80, 0x0000, 0xbfff, (UINT32)Pico.rom);
  Cz80_Set_Fetch(&CZ80, 0xc000, 0xdfff, (UINT32)Pico.zram);
  Cz80_Set_INPort(&CZ80, z80_sms_in);
  Cz80_Set_OUTPort(&CZ80, z80_sms_out);
#endif
}

void PicoFrameMS(void)
{
  z80_run(OSC_NTSC / 13 / 60);
}

