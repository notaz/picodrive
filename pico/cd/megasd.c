/*
 * PicoDrive
 * (C) irixxxx, 2024
 *
 * MEGASD enhancement support as "documented" in "MegaSD DEV Manual Rev.2"
 *
 * Emulates parts of the MEGASD API for "CD enhanced Megadrive games". Missing:
 * - all commands directly accessing files on the MEGASD storage
 * - Fader and volume control
 * - enhanced SSF2 mapper (currently uses standard SSF2 mapper instead)
 * - PCM access (only possible through enhanced SSF2 mapper)
 * The missing features are AFAIK not used by any currently available patch.
 * I'm not going to look into these until I see it used somewhere.
 */

#include "../pico_int.h"
#include "../memory.h"

#include "genplus_macros.h"
#include "cdd.h"
#include "megasd.h"

#define CDD_PLAY_OFFSET 3 // CDD play starts this many sectors early

struct megasd Pico_msd; // MEGASD state

static u16 verser[] = // mimick version 1.04 R7, serial 0x12345678
    { 0x4d45, 0x4741, 0x5344, 0x0104, 0x0700, 0xffff, 0x1234, 0x5678 };
//    { 0x4d45, 0x4741, 0x5344, 0x9999, 0x9900, 0xffff, 0x1234, 0x5678 };

// get a 32bit value from the data area
static s32 get32(int offs)
{
  u16 *a = Pico_msd.data + (offs/2);
  return (a[0] << 16) | a[1];
}

// send commands to cdd
static void cdd_play(s32 lba)
{
  // add CDD offset to have it going to the right LBA immediately
  int secs = (lba += CDD_PLAY_OFFSET) / 75;
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
  Pico_msd.index = Pico_msd.readlba = -1;
  s68k_write8(0xff8042, 0x01);
  s68k_write8(0xff804b, 0xff);
}

// play a track, looping from offset if enabled
static void msd_playtrack(int idx, s32 offs, int loop)
{
  track_t *track;

  if (idx < 1 || idx > cdd.toc.last) {
    Pico_msd.result = Pico_msd.command = 0;
    return;
  }

  Pico_msd.index = idx-1;

  track = &cdd.toc.tracks[Pico_msd.index];
  if (track->loop) {
    // overridden by some bizarre proprietary extensions in the cue file
    // NB using these extensions definitely prevents using CHD files with MD+!
    loop = track->loop > 0;
    offs = track->loop_lba;
  }

  Pico_msd.loop = loop;
  Pico_msd.readlba = -1;

  Pico_msd.startlba = track->start + 150;
  Pico_msd.endlba = track->end + 150;
  Pico_msd.looplba = Pico_msd.startlba + offs;

  cdd_play(Pico_msd.startlba);
}

// play a range of sectors, with looping if enabled
static void msd_playsectors(s32 startlba, s32 endlba, s32 looplba, int loop)
{
  if (startlba < 0 || startlba >= cdd.toc.tracks[cdd.toc.last].start) {
    Pico_msd.result = Pico_msd.command = 0;
    return;
  }

  Pico_msd.index = 99;
  Pico_msd.loop = loop;
  Pico_msd.readlba = -1;

  Pico_msd.startlba = startlba + 150;
  Pico_msd.endlba = endlba + 150;
  Pico_msd.looplba = looplba + 150;

  cdd_play(Pico_msd.startlba);
}

// read a block of data
static void msd_readdata(s32 lba)
{
  if (lba < 0 || lba >= cdd.toc.tracks[cdd.toc.last].start) {
    Pico_msd.result = Pico_msd.command = 0;
    return;
  }

  Pico_msd.index = -1;
  Pico_msd.readlba = lba + 150;

  cdd_play(Pico_msd.readlba);
}

// transfer data to data area
static void msd_transfer()
{
  if (cdd.status == CD_PLAY)
    cdd_read_data((u8 *)Pico_msd.data);
}

// update msd state (called every 1/75s, like CDD irq)
void msd_update()
{
  if (Pico_msd.initialized && (Pico_msd.readlba >= 0 || Pico_msd.index >= 0)) {
    // CD LEDs
    s68k_write8(0xff8000,(cdd.status == CD_PLAY) | 0x2);

    cdd.latency = 0; // MEGASD has no latency in this mode

    if (cdd.status == CD_PLAY) {
      if (Pico_msd.readlba >= 0 && cdd.lba >= Pico_msd.readlba) {
        // read done
        Pico_msd.command = 0;
      }
      else if (Pico_msd.index >= 0) {
        Pico_msd.command = 0;
        if (cdd.lba >= Pico_msd.endlba-1 || cdd.index > Pico_msd.index) {
          if (!Pico_msd.loop || Pico_msd.index < 0) {
            cdd_pause();
            // audio done
            Pico_msd.index = -1;
          } else
            cdd_play(Pico_msd.looplba - CDD_PLAY_OFFSET);
        }
      }

      // Hack for D32XR: to prevent CD BIOS freaking out, pretend drive "ready"
      // TODO find out what a real MEGASD is doing with this schizophrenia!
      u8 state = Pico_mcd->s68k_regs[0x38];
      Pico_mcd->s68k_regs[0x41] = ~(~Pico_mcd->s68k_regs[0x41] + CD_READY-state) & 0xf;
      Pico_mcd->s68k_regs[0x38] = CD_READY;
    }
  }
}

// process a MEGASD command
void msd_process(u16 d)
{
  Pico_msd.command = d; // busy

  switch (d >> 8) {
  case 0x10: memcpy(Pico_msd.data, verser, sizeof(verser)); Pico_msd.command = 0; break;

  case 0x11: msd_playtrack(d&0xff, 0, 0); break;
  case 0x12: msd_playtrack(d&0xff, 0, 1); break;
  case 0x1a: msd_playtrack(d&0xff, get32(0), 1); break;
  case 0x1b: msd_playsectors(get32(0), get32(4), get32(8), d&0xff); break;

  case 0x13: cdd_pause();
             Pico_msd.command = 0; break;
  case 0x14: cdd_resume();
             Pico_msd.command = 0; break;

  case 0x16: Pico_msd.result = !(s68k_read8(0xff8036) & 0x1) << 8;
             Pico_msd.command = 0; break;

  case 0x17: msd_readdata(get32(0)); break;
  case 0x18: msd_transfer();
             Pico_msd.command = 0; break;
  case 0x19: msd_readdata(Pico_msd.readlba-150 + 1); break;

  case 0x27: Pico_msd.result = cdd.toc.last << 8;
             Pico_msd.command = 0; break;

  default:   elprintf(EL_ANOMALY, "unknown MEGASD command %02x", Pico_msd.command);
             Pico_msd.command = Pico_msd.result = 0; break; // not supported
  }
}

// initialize MEGASD
static void msd_init(void)
{
  if (!Pico_msd.initialized) {
    Pico_msd.initialized = 1;
    Pico_msd.index = Pico_msd.readlba = -1;

    // enable CD drive
    s68k_write8(0xff8037, 0x4);
    s68k_write8(0xff8000, 0x2);

    PicoResetHook = msd_reset;
  }
}

void msd_reset(void)
{
  if (Pico_msd.initialized) {
    Pico_msd.initialized = Pico_msd.command = 0;
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

  a = (u16)a;
  if (a >= 0x0f800) {
    d = Pico_msd.data[(a&0x7ff)>>1];
  } else if (a >= 0xf7f0) {
    switch (a&0xe) {
      case 0x6: d = 0x5241; break; // RA
      case 0x8: d = 0x5445; break; // TE
      case 0xc: d = Pico_msd.result; break;
      case 0xe: d = Pico_msd.command; break;
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
  a = (u16)a;
  if (a == 0xf7fa) {
    // en/disable overlay
    u32 base = 0x040000-(1<<M68K_MEM_SHIFT);
    if ((u16)d == 0xcd54) {
      msd_init();
      cpu68k_map_set(m68k_read8_map,  base, 0x03ffff, msd_read8, 1);
      cpu68k_map_set(m68k_read16_map, base, 0x03ffff, msd_read16, 1);
      base += 0x800000; // mirror
      cpu68k_map_set(m68k_read8_map,  base, 0x0bffff, msd_read8, 1);
      cpu68k_map_set(m68k_read16_map, base, 0x0bffff, msd_read16, 1);
    } else if (Pico.romsize > base) {
      cpu68k_map_set(m68k_read8_map,  base, 0x03ffff, Pico.rom+base, 0);
      cpu68k_map_set(m68k_read16_map, base, 0x03ffff, Pico.rom+base, 0);
    }
  } else if (a == 0xf7fe) {
    // command port
    msd_process(d);
  } else if (a >= 0xf800) {
    // data area
    Pico_msd.data[(a&0x7ff) >> 1] = d;
  }
}

void msd_write8(u32 a, u32 d)
{
  if ((u16)a >= 0xf800) {
    // data area
    ((u8 *)Pico_msd.data)[MEM_BE2(a&0x7ff)] = d;
  }
}
