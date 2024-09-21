/*
 * PicoDrive
 * (C) irixxxx, 2024
 *
 * MEGASD enhancement support
 */

#include "../pico_int.h"
#include "../memory.h"

#include "genplus_macros.h"
#include "cdd.h"
#include "megasd.h"

// modifiable fields visible through the interface
u16 msd_command, msd_result;
u16 msd_data[0x800/2];

// internal state
static int msd_initialized;
static s32 msd_startlba, msd_endlba, msd_looplba;
static s32 msd_readlba = -1; // >= 0 if sector read is running
static int msd_loop, msd_index = -1; // >= 0 if audio track is playing

static u16 verser[] = // mimick version 1.04 R7, serial 0x01234567
    { 0x4d45, 0x4741, 0x5344, 0x0104, 0x0700, 0xffff, 0x0123, 0x4567 };
//    { 0x4d45, 0x4741, 0x5344, 0x9999, 0x9900, 0xffff, 0x1234, 0x5678 };

// get a 32bit value from the data area
static s32 get32(int offs)
{
  u16 *a = msd_data + (offs/2);
  return (a[0] << 16) | a[1];
}

// send commands to cdd
static void cdd_play(s32 lba)
{
  int secs = lba / 75;
  int mins = secs / 60;
  lba -= secs * 75;
  secs -= mins * 60;
  s68k_write8(0xff8044, mins/10);
  s68k_write8(0xff8045, mins%10);
  s68k_write8(0xff8046, secs/10);
  s68k_write8(0xff8047, secs%10);
  s68k_write8(0xff8048, lba /10);
  s68k_write8(0xff8049, lba %10);

  s68k_write8(0xff8042, 0x03);
  s68k_write8(0xff804b, 0xff);
}

static void cdd_pause(void)
{
  s68k_write8(0xff8042, 0x06);
  s68k_write8(0xff804b, 0xff);
}

static void cdd_resume(void)
{
  s68k_write8(0xff8042, 0x07);
  s68k_write8(0xff804b, 0xff);
}

static void cdd_stop(void)
{
  msd_index = msd_readlba = -1;
  s68k_write8(0xff8042, 0x01);
  s68k_write8(0xff804b, 0xff);
}

// play a track, looping from offset if enabled
static void msd_playtrack(int idx, s32 offs, int loop)
{
  msd_index = idx-1;
  msd_loop = loop;
  msd_readlba = -1;

  msd_startlba = cdd.toc.tracks[msd_index].start + 150;
  msd_endlba = cdd.toc.tracks[msd_index].end;
  msd_looplba = msd_startlba + offs;

  cdd_play(msd_startlba);
}

// play a range of sectors, with looping if enabled
static void msd_playsectors(s32 startlba, s32 endlba, s32 looplba, int loop)
{
  msd_index = 99;
  msd_loop = loop;
  msd_readlba = -1;

  msd_startlba = startlba + 150;
  msd_endlba = endlba + 150;
  msd_looplba = looplba + 150;

  cdd_play(msd_startlba);
}

// read a block of data
static void msd_readdata(s32 lba)
{
  msd_index = -1;
  msd_readlba = lba;

  cdd_play(msd_readlba);
}

// transfer data to data area
static void msd_transfer()
{
  if (cdd.status == CD_PLAY)
    cdd_read_data((u8 *)msd_data);
}

// update msd state (called every 1/75s)
void msd_update()
{
  if (msd_initialized) {
    // CD LEDs
    s68k_write8(0xff8000,(cdd.status == CD_PLAY) | 0x2);

    if (cdd.status == CD_PLAY) {
      if (msd_readlba >= 0 && cdd.lba >= msd_readlba) {
        // read done
        msd_command = 0;
      }
      else if (msd_index >= 0) {
        msd_command = 0;
        if (cdd.lba > msd_endlba || cdd.index > msd_index) {
          if (!msd_loop || msd_index < 0) {
            cdd_stop();
            // audio done
          } else
            cdd_play(msd_looplba);
        }
      }
    }
  }
}

// process a MEGASD command
void msd_process(u16 d)
{
  msd_command = d; // busy

  switch (d >> 8) {
  case 0x10: memcpy(msd_data, verser, sizeof(verser)); msd_command = 0; break;

  case 0x11: msd_playtrack(d&0xff, 0, 0); break;
  case 0x12: msd_playtrack(d&0xff, 0, 1); break;
  case 0x1a: msd_playtrack(d&0xff, get32(0), 1); break;
  case 0x1b: msd_playsectors(get32(0), get32(4), get32(8), d&0xff); break;

  case 0x13: cdd_pause();
             msd_command = 0; break;
  case 0x14: cdd_resume();
             msd_command = 0; break;

  case 0x16: msd_result = !(s68k_read8(0xff8036) & 0x1) << 8;
             msd_command = 0; break;

  case 0x17: msd_readdata(get32(0)); break;
  case 0x18: msd_transfer();
             msd_command = 0; break;
  case 0x19: msd_readdata(++msd_readlba); break;

  case 0x27: msd_result = cdd.toc.last << 8;
             msd_command = 0; break;

  default:   msd_command = msd_result = 0; break; // not supported
  }
}

// initialize MEGASD
static void msd_init(void)
{
  if (!msd_initialized) {
    msd_initialized = 1;

    // enable CD drive
    s68k_write8(0xff8037, 0x4);

    PicoResetHook = msd_reset;
  }
}

void msd_reset(void)
{
  if (msd_initialized) {
    msd_initialized = msd_command = 0;
    cdd_stop();

    s68k_write8(0xff8000, 0x0);
    s68k_write8(0xff8001, 0x0);

    PicoResetHook = NULL;
  }
}

// memory r/w functions
static u32 msd_read16(u32 a)
{
  u16 d = 0;

  if (a >= 0x03f800) {
    d = msd_data[(a&0x7ff)>>1];
  } else if (a >= 0x03f7f0) {
    switch (a&0xe) {
      case 0x6: d = 0x5241; break; // RA
      case 0x8: d = 0x5445; break; // TE
      case 0xc: d = msd_result; break;
      case 0xe: d = msd_command; break;
    }
  } else if (a < Pico.romsize)
    d = *(u16 *)(Pico.rom + a);

  return d;
}

static u32 msd_read8(u32 a)
{
  u16 d = msd_read16(a);

  if (!(a&1)) d >>= 8;
  return d;
}

void msd_write16(u32 a, u32 d)
{
  if (a == 0x03f7fa) {
    // en/disable overlay
    u32 base = 0x040000-(1<<M68K_MEM_SHIFT);
    if ((u16)d == 0xcd54) {
      msd_init();
      cpu68k_map_set(m68k_read8_map,  base, 0x03ffff, msd_read8, 1);
      cpu68k_map_set(m68k_read16_map, base, 0x03ffff, msd_read16, 1);
    } else if (Pico.romsize > base) {
      cpu68k_map_set(m68k_read8_map,  base, 0x03ffff, Pico.rom+base, 0);
      cpu68k_map_set(m68k_read16_map, base, 0x03ffff, Pico.rom+base, 0);
    }
  } else if (a == 0x03f7fe) {
    // command port
    msd_process(d);
  } else if (a >= 0x03f800) {
    // data area
    msd_data[(a&0x7ff) >> 1] = d;
  }
}

void msd_write8(u32 a, u32 d)
{
  if (a >= 0x03f800) {
    // data area
    ((u8 *)msd_data)[MEM_BE2(a&0x7ff)] = d;
  }
}
