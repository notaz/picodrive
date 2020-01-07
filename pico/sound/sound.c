/*
 * PicoDrive
 * (c) Copyright Dave, 2004
 * (C) notaz, 2006-2009
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */

#include <string.h>
#include "ym2612.h"
#include "sn76496.h"
#include "../pico_int.h"
#include "../cd/cue.h"
#include "mix.h"

void (*PsndMix_32_to_16l)(short *dest, int *src, int count) = mix_32_to_16l_stereo;

// master int buffer to mix to
static int PsndBuffer[2*(44100+100)/50];

// dac, psg
static unsigned short dac_info[312+4]; // pos in sample buffer

// cdda output buffer
short cdda_out_buffer[2*1152];

// sn76496
extern int *sn76496_regs;


static void dac_recalculate(void)
{
  int lines = Pico.m.pal ? 313 : 262;
  int i, pos;

  pos = 0; // Q16

  for(i = 0; i <= lines; i++)
  {
    dac_info[i] = ((pos+(1<<15)) >> 16); // round to nearest
    pos += Pico.snd.fm_mult;
  }
  for (i = lines+1; i < sizeof(dac_info) / sizeof(dac_info[0]); i++)
    dac_info[i] = dac_info[i-1];
}


PICO_INTERNAL void PsndReset(void)
{
  // PsndRerate calls YM2612Init, which also resets
  PsndRerate(0);
  timers_reset();
  mix_reset();
}


// to be called after changing sound rate or chips
void PsndRerate(int preserve_state)
{
  void *state = NULL;
  int target_fps = Pico.m.pal ? 50 : 60;
  int target_lines = Pico.m.pal ? 313 : 262;

  if (preserve_state) {
    state = malloc(0x204);
    if (state == NULL) return;
    ym2612_pack_state();
    memcpy(state, YM2612GetRegs(), 0x204);
  }
  YM2612Init(Pico.m.pal ? OSC_PAL/7 : OSC_NTSC/7, PicoIn.sndRate);
  if (preserve_state) {
    // feed it back it's own registers, just like after loading state
    memcpy(YM2612GetRegs(), state, 0x204);
    ym2612_unpack_state();
  }

  if (preserve_state) memcpy(state, sn76496_regs, 28*4); // remember old state
  SN76496_init(Pico.m.pal ? OSC_PAL/15 : OSC_NTSC/15, PicoIn.sndRate);
  if (preserve_state) memcpy(sn76496_regs, state, 28*4); // restore old state

  if (state)
    free(state);

  // calculate Pico.snd.len
  Pico.snd.len = PicoIn.sndRate / target_fps;
  Pico.snd.len_e_add = ((PicoIn.sndRate - Pico.snd.len * target_fps) << 16) / target_fps;
  Pico.snd.len_e_cnt = 0;

  // samples per line
  Pico.snd.fm_mult = 65536.0 * PicoIn.sndRate / (target_fps*target_lines);

  // recalculate dac info
  dac_recalculate();

  // clear all buffers
  memset32(PsndBuffer, 0, sizeof(PsndBuffer)/4);
  memset(cdda_out_buffer, 0, sizeof(cdda_out_buffer));
  if (PicoIn.sndOut)
    PsndClear();

  // set mixer
  PsndMix_32_to_16l = (PicoIn.opt & POPT_EN_STEREO) ? mix_32_to_16l_stereo : mix_32_to_16_mono;

  if (PicoIn.AHW & PAHW_PICO)
    PicoReratePico();
}


PICO_INTERNAL void PsndStartFrame(void)
{
  // compensate for float part of Pico.snd.len
  Pico.snd.len_use = Pico.snd.len;
  Pico.snd.len_e_cnt += Pico.snd.len_e_add;
  if (Pico.snd.len_e_cnt >= 0x10000) {
    Pico.snd.len_e_cnt -= 0x10000;
    Pico.snd.len_use++;
  }

  Pico.snd.dac_line = Pico.snd.psg_line = 0;
  Pico.snd.fm_pos = 0;
}

PICO_INTERNAL void PsndDoDAC(int line_to)
{
  int pos, pos1, len;
  int dout = ym2612.dacout;
  int line_from = Pico.snd.dac_line;

  pos  = dac_info[line_from];
  pos1 = dac_info[line_to + 1];
  len = pos1 - pos;
  if (len <= 0)
    return;

  Pico.snd.dac_line = line_to + 1;

  if (!PicoIn.sndOut)
    return;

  if (PicoIn.opt & POPT_EN_STEREO) {
    short *d = PicoIn.sndOut + pos*2;
    for (; len > 0; len--, d+=2) *d += dout;
  } else {
    short *d = PicoIn.sndOut + pos;
    for (; len > 0; len--, d++)  *d += dout;
  }
}

PICO_INTERNAL void PsndDoPSG(int line_to)
{
  int line_from = Pico.snd.psg_line;
  int pos, pos1, len;
  int stereo = 0;

  pos  = dac_info[line_from];
  pos1 = dac_info[line_to + 1];
  len = pos1 - pos;
  if (len <= 0)
    return;

  Pico.snd.psg_line = line_to + 1;

  if (!PicoIn.sndOut || !(PicoIn.opt & POPT_EN_PSG))
    return;

  if (PicoIn.opt & POPT_EN_STEREO) {
    stereo = 1;
    pos <<= 1;
  }
  SN76496Update(PicoIn.sndOut + pos, len, stereo);
}

PICO_INTERNAL void PsndDoFM(int line_to)
{
  int pos, len;
  int stereo = 0;

  // Q16, number of samples to fill in buffer
  len = ((line_to-1) * Pico.snd.fm_mult) - Pico.snd.fm_pos;

  // don't do this too often (no more than 256 per sec)
  if (len >> 16 <= PicoIn.sndRate >> 9)
    return;

  // update position and calculate buffer offset and length
  pos = Pico.snd.fm_pos >> 16;
  Pico.snd.fm_pos += len;
  len = (Pico.snd.fm_pos >> 16) - pos;

  // fill buffer
  if (PicoIn.opt & POPT_EN_STEREO) {
    stereo = 1;
    pos <<= 1;
  }
  if (PicoIn.opt & POPT_EN_FM)
    YM2612UpdateOne(PsndBuffer + pos, len, stereo, 1);
  else
    memset32(PsndBuffer + pos, 0, len<<stereo);
}

// cdda
static void cdda_raw_update(int *buffer, int length)
{
  int ret, cdda_bytes, mult = 1;

  cdda_bytes = length*4;
  if (PicoIn.sndRate <= 22050 + 100) mult = 2;
  if (PicoIn.sndRate <  22050 - 100) mult = 4;
  cdda_bytes *= mult;

  ret = pm_read(cdda_out_buffer, cdda_bytes, Pico_mcd->cdda_stream);
  if (ret < cdda_bytes) {
    memset((char *)cdda_out_buffer + ret, 0, cdda_bytes - ret);
    Pico_mcd->cdda_stream = NULL;
    return;
  }

  // now mix
  switch (mult) {
    case 1: mix_16h_to_32(buffer, cdda_out_buffer, length*2); break;
    case 2: mix_16h_to_32_s1(buffer, cdda_out_buffer, length*2); break;
    case 4: mix_16h_to_32_s2(buffer, cdda_out_buffer, length*2); break;
  }
}

void cdda_start_play(int lba_base, int lba_offset, int lb_len)
{
  if (Pico_mcd->cdda_type == CT_MP3)
  {
    int pos1024 = 0;

    if (lba_offset)
      pos1024 = lba_offset * 1024 / lb_len;

    mp3_start_play(Pico_mcd->cdda_stream, pos1024);
    return;
  }

  pm_seek(Pico_mcd->cdda_stream, (lba_base + lba_offset) * 2352, SEEK_SET);
  if (Pico_mcd->cdda_type == CT_WAV)
  {
    // skip headers, assume it's 44kHz stereo uncompressed
    pm_seek(Pico_mcd->cdda_stream, 44, SEEK_CUR);
  }
}


PICO_INTERNAL void PsndClear(void)
{
  int len = Pico.snd.len;
  if (Pico.snd.len_e_add) len++;
  if (PicoIn.opt & POPT_EN_STEREO)
    memset32((int *) PicoIn.sndOut, 0, len); // assume PicoIn.sndOut to be aligned
  else {
    short *out = PicoIn.sndOut;
    if ((uintptr_t)out & 2) { *out++ = 0; len--; }
    memset32((int *) out, 0, len/2);
    if (len & 1) out[len-1] = 0;
  }
}


static int PsndRender(int offset, int length)
{
  int *buf32;
  int stereo = (PicoIn.opt & 8) >> 3;
  int fmlen = (Pico.snd.fm_pos >> 16) - offset;

  offset <<= stereo;
  buf32 = PsndBuffer+offset;

  pprof_start(sound);

  if (PicoIn.AHW & PAHW_PICO) {
    PicoPicoPCMUpdate(PicoIn.sndOut+offset, length, stereo);
    return length;
  }

  // Add in parts of the FM buffer not yet done
  if (length-fmlen > 0) {
    int *fmbuf = buf32 + (fmlen << stereo);
    if (PicoIn.opt & POPT_EN_FM)
      YM2612UpdateOne(fmbuf, length-fmlen, stereo, 1);
    else
      memset32(fmbuf, 0, (length-fmlen)<<stereo);
    Pico.snd.fm_pos += (length-fmlen)<<16;
  }

  // CD: PCM sound
  if (PicoIn.AHW & PAHW_MCD) {
    pcd_pcm_update(buf32, length, stereo);
    //buf32_updated = 1;
  }

  // CD: CDDA audio
  // CD mode, cdda enabled, not data track, CDC is reading
  if ((PicoIn.AHW & PAHW_MCD) && (PicoIn.opt & POPT_EN_MCD_CDDA)
      && Pico_mcd->cdda_stream != NULL
      && !(Pico_mcd->s68k_regs[0x36] & 1))
  {
    // note: only 44, 22 and 11 kHz supported, with forced stereo
    if (Pico_mcd->cdda_type == CT_MP3)
      mp3_update(buf32, length, stereo);
    else
      cdda_raw_update(buf32, length);
  }

  if ((PicoIn.AHW & PAHW_32X) && (PicoIn.opt & POPT_EN_PWM))
    p32x_pwm_update(buf32, length, stereo);

  // convert + limit to normal 16bit output
  PsndMix_32_to_16l(PicoIn.sndOut+offset, buf32, length);

  pprof_end(sound);

  return length;
}

PICO_INTERNAL void PsndGetSamples(int y)
{
  static int curr_pos = 0;

  if (ym2612.dacen && Pico.snd.dac_line < y)
    PsndDoDAC(y - 1);
  PsndDoPSG(y - 1);

  curr_pos  = PsndRender(0, Pico.snd.len_use);

  if (PicoIn.writeSound)
    PicoIn.writeSound(curr_pos * ((PicoIn.opt & POPT_EN_STEREO) ? 4 : 2));
  // clear sound buffer
  PsndClear();
  Pico.snd.dac_line = y;
}

PICO_INTERNAL void PsndGetSamplesMS(int y)
{
  int length = Pico.snd.len_use;

  PsndDoPSG(y - 1);

  // upmix to "stereo" if needed
  if (PicoIn.opt & POPT_EN_STEREO) {
    int i, *p;
    for (i = length, p = (void *)PicoIn.sndOut; i > 0; i--, p++)
      *p |= *p << 16;
  }

  if (PicoIn.writeSound != NULL)
    PicoIn.writeSound(length * ((PicoIn.opt & POPT_EN_STEREO) ? 4 : 2));
  PsndClear();
}

// vim:shiftwidth=2:ts=2:expandtab
