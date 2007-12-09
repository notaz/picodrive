// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006,2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include <string.h>
#include "ym2612.h"
#include "sn76496.h"

#include "../PicoInt.h"
#include "../cd/pcm.h"
#include "mix.h"

void (*PsndMix_32_to_16l)(short *dest, int *src, int count) = mix_32_to_16l_stereo;

// master int buffer to mix to
static int PsndBuffer[2*44100/50];

// dac
static unsigned short dac_info[312]; // pppppppp ppppllll, p - pos in buff, l - length to write for this sample

// for Pico
int PsndRate=0;
int PsndLen=0; // number of mono samples, multiply by 2 for stereo
int PsndLen_exc_add=0; // this is for non-integer sample counts per line, eg. 22050/60
int PsndLen_exc_cnt=0;
short *PsndOut=NULL; // PCM data buffer

// sn76496
extern int *sn76496_regs;


static void dac_recalculate(void)
{
  int i, dac_cnt, pos, len, lines = Pico.m.pal ? 312 : 262, mid = Pico.m.pal ? 68 : 93;

  if(PsndLen <= lines) {
    // shrinking algo
    dac_cnt = -PsndLen;
    len=1; pos=0;
    dac_info[225] = 1;

    for(i=226; i != 225; i++) {
      if (i >= lines) i = 0;
      len = 0;
      if(dac_cnt < 0) {
        len=1;
        pos++;
        dac_cnt += lines;
      }
      dac_cnt -= PsndLen;
      dac_info[i] = (pos<<4)|len;
    }
  } else {
    // stretching
    dac_cnt = PsndLen;
    pos=0;
    for(i = 225; i != 224; i++) {
      if (i >= lines) i = 0;
      len=0;
      while(dac_cnt >= 0) {
        dac_cnt -= lines;
        len++;
      }
      if (i == mid) // midpoint
        while(pos+len < PsndLen/2) {
          dac_cnt -= lines;
          len++;
        }
      dac_cnt += PsndLen;
      dac_info[i] = (pos<<4)|len;
      pos+=len;
    }
    // last sample
    for(len = 0, i = pos; i < PsndLen; i++) len++;
    if (PsndLen_exc_add) len++;
    dac_info[224] = (pos<<4)|len;
  }
  //for(i=len=0; i < lines; i++) {
  //  printf("%03i : %03i : %i\n", i, dac_info[i]>>4, dac_info[i]&0xf);
  //  len+=dac_info[i]&0xf;
  //}
  //printf("rate is %i, len %f\n", PsndRate, (double)PsndRate/(Pico.m.pal ? 50.0 : 60.0));
  //printf("len total: %i, last pos: %i\n", len, pos);
  //exit(8);
}


PICO_INTERNAL void PsndReset(void)
{
  void *ym2612_regs;

  // also clear the internal registers+addr line
  ym2612_regs = YM2612GetRegs();
  memset(ym2612_regs, 0, 0x200+4);
  z80startCycle = z80stopCycle = 0;

  PsndRerate(0);
}


// to be called after changing sound rate or chips
void PsndRerate(int preserve_state)
{
  void *state = NULL;
  int target_fps = Pico.m.pal ? 50 : 60;

  // not all rates are supported in MCD mode due to mp3 decoder limitations
  if (PicoMCD & 1) {
    if (PsndRate != 11025 && PsndRate != 22050 && PsndRate != 44100) PsndRate = 22050;
    PicoOpt |= 8; // force stereo
  }

  if (preserve_state) {
    state = malloc(0x200);
    if (state == NULL) return;
    memcpy(state, YM2612GetRegs(), 0x200);
    if ((PicoMCD & 1) && Pico_mcd->m.audio_track)
      Pico_mcd->m.audio_offset = mp3_get_offset();
  }
  YM2612Init(Pico.m.pal ? OSC_PAL/7 : OSC_NTSC/7, PsndRate);
  if (preserve_state) {
    // feed it back it's own registers, just like after loading state
    memcpy(YM2612GetRegs(), state, 0x200);
    YM2612PicoStateLoad();
    if ((PicoMCD & 1) && Pico_mcd->m.audio_track)
      mp3_start_play(Pico_mcd->TOC.Tracks[Pico_mcd->m.audio_track].F, Pico_mcd->m.audio_offset);
  }

  if (preserve_state) memcpy(state, sn76496_regs, 28*4); // remember old state
  SN76496_init(Pico.m.pal ? OSC_PAL/15 : OSC_NTSC/15, PsndRate);
  if (preserve_state) memcpy(sn76496_regs, state, 28*4); // restore old state

  if (state)
    free(state);

  // calculate PsndLen
  PsndLen=PsndRate / target_fps;
  PsndLen_exc_add=((PsndRate - PsndLen*target_fps)<<16) / target_fps;
  PsndLen_exc_cnt=0;

  // recalculate dac info
  dac_recalculate();

  if (PicoMCD & 1)
    pcm_set_rate(PsndRate);

  // clear all buffers
  memset32(PsndBuffer, 0, sizeof(PsndBuffer)/4);
  if (PsndOut)
    PsndClear();

  // set mixer
  PsndMix_32_to_16l = (PicoOpt & 8) ? mix_32_to_16l_stereo : mix_32_to_16_mono;
}


// This is called once per raster (aka line), but not necessarily for every line
PICO_INTERNAL void Psnd_timers_and_dac(int raster)
{
  int pos, len;
  int do_dac = PsndOut && (PicoOpt&1) && *ym2612_dacen;
//  int do_pcm = PsndOut && (PicoMCD&1) && (PicoOpt&0x400);

  // Our raster lasts 63.61323/64.102564 microseconds (NTSC/PAL)
  YM2612PicoTick(1);

  if (!do_dac /*&& !do_pcm*/) return;

  pos=dac_info[raster], len=pos&0xf;
  if (!len) return;

  pos>>=4;

  if (do_dac) {
    short *d = PsndOut + pos*2;
    int dout = *ym2612_dacout;
    if(PicoOpt&8) {
      // some manual loop unrolling here :)
      d[0] = dout;
      if (len > 1) {
        d[2] = dout;
        if (len > 2)
          d[4] = dout;
      }
    } else {
      short *d = PsndOut + pos;
      d[0] = dout;
      if (len > 1) {
        d[1] = dout;
        if (len > 2)
          d[2] = dout;
      }
    }
  }

#if 0
  if (do_pcm) {
    int *d = PsndBuffer;
    d += (PicoOpt&8) ? pos*2 : pos;
    pcm_update(d, len, 1);
  }
#endif
}


PICO_INTERNAL void PsndClear(void)
{
  int len = PsndLen;
  if (PsndLen_exc_add) len++;
  if (PicoOpt & 8)
    memset32((int *) PsndOut, 0, len); // assume PsndOut to be aligned
  else {
    short *out = PsndOut;
    if ((int)out & 2) { *out++ = 0; len--; }
    memset32((int *) out, 0, len/2);
    if (len & 1) out[len-1] = 0;
  }
}


PICO_INTERNAL int PsndRender(int offset, int length)
{
  int  buf32_updated = 0;
  int *buf32 = PsndBuffer+offset;
  int stereo = (PicoOpt & 8) >> 3;
  // emulating CD && PCM option enabled && PCM chip on && have enabled channels
  int do_pcm = (PicoMCD&1) && (PicoOpt&0x400) && (Pico_mcd->pcm.control & 0x80) && Pico_mcd->pcm.enabled;
  offset <<= stereo;

  if (offset == 0) { // should happen once per frame
    // compensate for float part of PsndLen
    PsndLen_exc_cnt += PsndLen_exc_add;
    if (PsndLen_exc_cnt >= 0x10000) {
      PsndLen_exc_cnt -= 0x10000;
      length++;
    }
  }

  // PSG
  if (PicoOpt & 2)
    SN76496Update(PsndOut+offset, length, stereo);

  // Add in the stereo FM buffer
  if (PicoOpt & 1) {
    buf32_updated = YM2612UpdateOne(buf32, length, stereo, 1);
  } else
    memset32(buf32, 0, length<<stereo);

//printf("active_chs: %02x\n", buf32_updated);

  // CD: PCM sound
  if (do_pcm) {
    pcm_update(buf32, length, stereo);
    //buf32_updated = 1;
  }

  // CD: CDDA audio
  // CD mode, cdda enabled, not data track, CDC is reading
  if ((PicoMCD & 1) && (PicoOpt & 0x800) && !(Pico_mcd->s68k_regs[0x36] & 1) && (Pico_mcd->scd.Status_CDC & 1))
    mp3_update(buf32, length, stereo);

  // convert + limit to normal 16bit output
  PsndMix_32_to_16l(PsndOut+offset, buf32, length);

  return length;
}



#if defined(_USE_MZ80)

// memhandlers for mz80 core
unsigned char mz80_read(UINT32 a,  struct MemoryReadByte *w)  { return z80_read(a); }
void mz80_write(UINT32 a, UINT8 d, struct MemoryWriteByte *w) { z80_write(d, a); }

// structures for mz80 core
static struct MemoryReadByte mz80_mem_read[]=
{
  {0x0000,0xffff,mz80_read},
  {(UINT32) -1,(UINT32) -1,NULL}
};
static struct MemoryWriteByte mz80_mem_write[]=
{
  {0x0000,0xffff,mz80_write},
  {(UINT32) -1,(UINT32) -1,NULL}
};
static struct z80PortRead mz80_io_read[] ={
  {(UINT16) -1,(UINT16) -1,NULL}
};
static struct z80PortWrite mz80_io_write[]={
  {(UINT16) -1,(UINT16) -1,NULL}
};

int mz80_run(int cycles)
{
  int ticks_pre = mz80GetElapsedTicks(0);
  mz80exec(cycles);
  return mz80GetElapsedTicks(0) - ticks_pre;
}

#elif defined(_USE_DRZ80)

struct DrZ80 drZ80;

static unsigned int DrZ80_rebasePC(unsigned short a)
{
  drZ80.Z80PC_BASE = (unsigned int) Pico.zram;
  return drZ80.Z80PC_BASE + a;
}

static unsigned int DrZ80_rebaseSP(unsigned short a)
{
  drZ80.Z80SP_BASE = (unsigned int) Pico.zram;
  return drZ80.Z80SP_BASE + a;
}

static void DrZ80_irq_callback()
{
  drZ80.Z80_IRQ = 0; // lower irq when accepted
}
#endif

#if defined(_USE_DRZ80) || defined(_USE_CZ80)
static unsigned char z80_in(unsigned short p)
{
  elprintf(EL_ANOMALY, "Z80 port %04x read", p);
  return 0xff;
}

static void z80_out(unsigned short p,unsigned char d)
{
  elprintf(EL_ANOMALY, "Z80 port %04x write %02x", p, d);
}
#endif


// z80 functionality wrappers
PICO_INTERNAL void z80_init(void)
{
#if defined(_USE_MZ80)
  struct mz80context z80;

  // z80
  mz80init();
  // Modify the default context
  mz80GetContext(&z80);

  // point mz80 stuff
  z80.z80Base=Pico.zram;
  z80.z80MemRead=mz80_mem_read;
  z80.z80MemWrite=mz80_mem_write;
  z80.z80IoRead=mz80_io_read;
  z80.z80IoWrite=mz80_io_write;

  mz80SetContext(&z80);

#elif defined(_USE_DRZ80)
  memset(&drZ80, 0, sizeof(struct DrZ80));
  drZ80.z80_rebasePC=DrZ80_rebasePC;
  drZ80.z80_rebaseSP=DrZ80_rebaseSP;
  drZ80.z80_read8   =z80_read;
  drZ80.z80_read16  =z80_read16;
  drZ80.z80_write8  =z80_write;
  drZ80.z80_write16 =z80_write16;
  drZ80.z80_in      =z80_in;
  drZ80.z80_out     =z80_out;
  drZ80.z80_irq_callback=DrZ80_irq_callback;

#elif defined(_USE_CZ80)
  memset(&CZ80, 0, sizeof(CZ80));
  Cz80_Init(&CZ80);
  Cz80_Set_Fetch(&CZ80, 0x0000, 0x1fff, (UINT32)Pico.zram); // main RAM
  Cz80_Set_Fetch(&CZ80, 0x2000, 0x3fff, (UINT32)Pico.zram); // mirror
  Cz80_Set_ReadB(&CZ80, (UINT8 (*)(UINT32 address))z80_read); // unused (hacked in)
  Cz80_Set_WriteB(&CZ80, z80_write);
  Cz80_Set_INPort(&CZ80, z80_in);
  Cz80_Set_OUTPort(&CZ80, z80_out);
#endif
}

PICO_INTERNAL void z80_reset(void)
{
#if defined(_USE_MZ80)
  mz80reset();
#elif defined(_USE_DRZ80)
  memset(&drZ80, 0, 0x54);
  drZ80.Z80F  = (1<<2);  // set ZFlag
  drZ80.Z80F2 = (1<<2);  // set ZFlag
  drZ80.Z80IX = 0xFFFF << 16;
  drZ80.Z80IY = 0xFFFF << 16;
  drZ80.Z80IM = 0; // 1?
  drZ80.Z80PC = drZ80.z80_rebasePC(0);
  drZ80.Z80SP = drZ80.z80_rebaseSP(0x2000); // 0xf000 ?
#elif defined(_USE_CZ80)
  Cz80_Reset(&CZ80);
  Cz80_Set_Reg(&CZ80, CZ80_IX, 0xffff);
  Cz80_Set_Reg(&CZ80, CZ80_IY, 0xffff);
  Cz80_Set_Reg(&CZ80, CZ80_SP, 0x2000);
#endif
  Pico.m.z80_fakeval = 0; // for faking when Z80 is disabled
}


PICO_INTERNAL void z80_pack(unsigned char *data)
{
#if defined(_USE_MZ80)
  struct mz80context mz80;
  *(int *)data = 0x00005A6D; // "mZ"
  mz80GetContext(&mz80);
  memcpy(data+4, &mz80.z80clockticks, sizeof(mz80)-5*4); // don't save base&memhandlers
#elif defined(_USE_DRZ80)
  *(int *)data = 0x015A7244; // "DrZ" v1
  drZ80.Z80PC = drZ80.z80_rebasePC(drZ80.Z80PC-drZ80.Z80PC_BASE);
  drZ80.Z80SP = drZ80.z80_rebaseSP(drZ80.Z80SP-drZ80.Z80SP_BASE);
  memcpy(data+4, &drZ80, 0x54);
#elif defined(_USE_CZ80)
  *(int *)data = 0x00007a43; // "Cz"
  *(int *)(data+4) = Cz80_Get_Reg(&CZ80, CZ80_PC);
  memcpy(data+8, &CZ80, (INT32)&CZ80.BasePC - (INT32)&CZ80);
#endif
}

PICO_INTERNAL void z80_unpack(unsigned char *data)
{
#if defined(_USE_MZ80)
  if (*(int *)data == 0x00005A6D) { // "mZ" save?
    struct mz80context mz80;
    mz80GetContext(&mz80);
    memcpy(&mz80.z80clockticks, data+4, sizeof(mz80)-5*4);
    mz80SetContext(&mz80);
  } else {
    z80_reset();
    z80_int();
  }
#elif defined(_USE_DRZ80)
  if (*(int *)data == 0x015A7244) { // "DrZ" v1 save?
    memcpy(&drZ80, data+4, 0x54);
    // update bases
    drZ80.Z80PC = drZ80.z80_rebasePC(drZ80.Z80PC-drZ80.Z80PC_BASE);
    drZ80.Z80SP = drZ80.z80_rebaseSP(drZ80.Z80SP-drZ80.Z80SP_BASE);
  } else {
    z80_reset();
    drZ80.Z80IM = 1;
    z80_int(); // try to goto int handler, maybe we won't execute trash there?
  }
#elif defined(_USE_CZ80)
  if (*(int *)data == 0x00007a43) { // "Cz" save?
    memcpy(&CZ80, data+8, (INT32)&CZ80.BasePC - (INT32)&CZ80);
    Cz80_Set_Reg(&CZ80, CZ80_PC, *(int *)(data+4));
  } else {
    z80_reset();
    z80_int();
  }
#endif
}

PICO_INTERNAL void z80_exit(void)
{
#if defined(_USE_MZ80)
  mz80shutdown();
#endif
}

#if 1 // defined(__DEBUG_PRINT) || defined(__GP2X__) || defined(__GIZ__)
PICO_INTERNAL void z80_debug(char *dstr)
{
#if defined(_USE_DRZ80)
  sprintf(dstr, "Z80 state: PC: %04x SP: %04x\n", drZ80.Z80PC-drZ80.Z80PC_BASE, drZ80.Z80SP-drZ80.Z80SP_BASE);
#elif defined(_USE_CZ80)
  sprintf(dstr, "Z80 state: PC: %04x SP: %04x\n", CZ80.PC - CZ80.BasePC, CZ80.SP.W);
#endif
}
#endif
