/*
 * PicoDrive
 * (C) notaz, 2008
 * (C) irixxxx, 2024
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 *
 * The following ADPCM algorithm was derived from MAME upd7759 driver.
 */

#include <math.h>
#include "../pico_int.h"

/* limitter */
#define Limit(val, max, min) \
	(val > max ? max : val < min ? min : val)

#define ADPCM_CLOCK	(1280000/4)

#define FIFO_IRQ_THRESHOLD 16

static const int step_deltas[16][16] =
{
  { 0,  0,  1,  2,  3,   5,   7,  10,  0,   0,  -1,  -2,  -3,   -5,   -7,  -10 },
  { 0,  1,  2,  3,  4,   6,   8,  13,  0,  -1,  -2,  -3,  -4,   -6,   -8,  -13 },
  { 0,  1,  2,  4,  5,   7,  10,  15,  0,  -1,  -2,  -4,  -5,   -7,  -10,  -15 },
  { 0,  1,  3,  4,  6,   9,  13,  19,  0,  -1,  -3,  -4,  -6,   -9,  -13,  -19 },
  { 0,  2,  3,  5,  8,  11,  15,  23,  0,  -2,  -3,  -5,  -8,  -11,  -15,  -23 },
  { 0,  2,  4,  7, 10,  14,  19,  29,  0,  -2,  -4,  -7, -10,  -14,  -19,  -29 },
  { 0,  3,  5,  8, 12,  16,  22,  33,  0,  -3,  -5,  -8, -12,  -16,  -22,  -33 },
  { 1,  4,  7, 10, 15,  20,  29,  43, -1,  -4,  -7, -10, -15,  -20,  -29,  -43 },
  { 1,  4,  8, 13, 18,  25,  35,  53, -1,  -4,  -8, -13, -18,  -25,  -35,  -53 },
  { 1,  6, 10, 16, 22,  31,  43,  64, -1,  -6, -10, -16, -22,  -31,  -43,  -64 },
  { 2,  7, 12, 19, 27,  37,  51,  76, -2,  -7, -12, -19, -27,  -37,  -51,  -76 },
  { 2,  9, 16, 24, 34,  46,  64,  96, -2,  -9, -16, -24, -34,  -46,  -64,  -96 },
  { 3, 11, 19, 29, 41,  57,  79, 117, -3, -11, -19, -29, -41,  -57,  -79, -117 },
  { 4, 13, 24, 36, 50,  69,  96, 143, -4, -13, -24, -36, -50,  -69,  -96, -143 },
  { 4, 16, 29, 44, 62,  85, 118, 175, -4, -16, -29, -44, -62,  -85, -118, -175 },
  { 6, 20, 36, 54, 76, 104, 144, 214, -6, -20, -36, -54, -76, -104, -144, -214 },
};

static const int state_deltas[16] = { -1, -1, 0, 0, 1, 2, 2, 3, -1, -1, 0, 0, 1, 2, 2, 3 };

static int sample = 0, state = 0;
static s32 stepsamples = (44100LL<<16)/ADPCM_CLOCK;
static s32 samplepos;
static int samplegain;

static int startpin, irqenable;
static enum { RESET, START, HDR, COUNT } portstate = RESET;
static int rate, silence, nibbles, highlow, cache;


// SEGA Pico specific filtering

#define QB      16                      // mantissa bits
#define FP(f)	(int)((f)*(1<<QB))      // convert to fixpoint

static struct iir2 { // 2nd order IIR
  s32 a[2], gain; // coefficients
  s32 y[3], x[3]; // filter history
} filters[4];
static struct iir2 *filter;


static void PicoPicoFilterCoeff(struct iir2 *iir, int cutoff, int rate)
{
  if (cutoff >= rate/2) {
    memset(iir, 0, sizeof(*iir));
    return;
  }

  // compute 2nd order butterworth filter coefficients
  double a = 1 / tan(M_PI * cutoff / rate);
  double axa = a*a;
  double gain = 1/(1 + M_SQRT2*a + axa);
  iir->gain = FP(gain);
  iir->a[0] = FP(2 * (axa-1) * gain);
  iir->a[1] = FP(-(1 - M_SQRT2*a + axa) * gain);
}

static int PicoPicoFilterApply(struct iir2 *iir, int sample)
{
  if (!iir)
    return sample;

  iir->x[0] = iir->x[1]; iir->x[1] = iir->x[2];
  iir->x[2] = sample * iir->gain; // Qb
  iir->y[0] = iir->y[1]; iir->y[1] = iir->y[2];
  iir->y[2] = (iir->x[0] + 2*iir->x[1] + iir->x[2]
               + iir->y[0]*iir->a[1] + iir->y[1]*iir->a[0]) >> QB;
  return iir->y[2];
}


// pin functions, N designating a negated pin

PICO_INTERNAL void PicoPicoPCMResetN(int pin)
{
  if (!pin) {
    portstate = RESET;
    sample = samplepos = state = 0;
    portstate = nibbles = silence = 0;
  } else if (portstate == RESET)
    portstate = START;
}

PICO_INTERNAL void PicoPicoPCMStartN(int pin)
{
  startpin = pin;
}

PICO_INTERNAL int PicoPicoPCMBusyN(void)
{
  return (portstate <= START);
}


// configuration functions

PICO_INTERNAL void PicoPicoPCMRerate(void)
{
  // output samples per chip clock
  stepsamples = ((u64)PicoIn.sndRate<<16)/ADPCM_CLOCK;

  // compute filter coefficients, cutoff at half the ADPCM sample rate
  PicoPicoFilterCoeff(&filters[1],  5000/2, PicoIn.sndRate); // 5-6 KHz
  PicoPicoFilterCoeff(&filters[2],  8000/2, PicoIn.sndRate); // 8-12 KHz
  PicoPicoFilterCoeff(&filters[3], 14000/2, PicoIn.sndRate); // 14-16 KHz
}

PICO_INTERNAL void PicoPicoPCMGain(int gain)
{
  samplegain = gain*4;
}

PICO_INTERNAL void PicoPicoPCMFilter(int index)
{
  filter = filters+index;
  if (filter->a[0] == 0)
    filter = NULL;
}

PICO_INTERNAL void PicoPicoPCMIrqEn(int enable)
{
  irqenable = (enable ? 3 : 0);
}

// TODO need an interupt pending mask?
PICO_INTERNAL int PicoPicoIrqAck(int level)
{
  return (PicoPicohw.fifo_bytes < FIFO_IRQ_THRESHOLD && level != irqenable ? irqenable : 0);
}


// adpcm operation

#define apply_filter(v) PicoPicoFilterApply(filter, v)

#define do_sample(nibble) \
{ \
  sample += step_deltas[state][nibble]; \
  state += state_deltas[nibble]; \
  state = (state < 0 ? 0 : state > 15 ? 15 : state); \
}

#define write_sample(buffer, length, stereo) \
{ \
  while (samplepos > 0 && length > 0) { \
    int val = Limit(samplegain*sample, 16383, -16384); \
    samplepos -= 1<<16; \
    length --; \
    if (buffer) { \
      int out = apply_filter(val); \
      *buffer++ += out; \
      if (stereo) *buffer++ += out; \
    } \
  } \
}

PICO_INTERNAL void PicoPicoPCMUpdate(short *buffer, int length, int stereo)
{
  unsigned char *src = PicoPicohw.xpcm_buffer;
  unsigned char *lim = PicoPicohw.xpcm_ptr;
  int srcval, irq = 0;

  // leftover partial sample from last run
  write_sample(buffer, length, stereo);

  // loop over FIFO data, generating ADPCM samples
  while (length > 0 && src < lim)
  {
    if (silence > 0) {
      silence --;
      sample = 0;
      samplepos += stepsamples*256;

    } else if (nibbles > 0) {
      nibbles --;

      if (highlow)
        cache = *src++;
      else
        cache <<= 4;
      highlow = !highlow;

      do_sample((cache & 0xf0) >> 4);
      samplepos += stepsamples*rate;

    } else switch (portstate) {
      case RESET:
        sample = 0;
        samplepos += length<<16;
        break;
      case START:
        if (startpin) {
          if (*src)
            portstate ++;
          else // kill 0x00 bytes at stream start
            src ++;
        } else {
          sample = 0;
          samplepos += length<<16;
        }
        break;
      case HDR:
        srcval = *src++;
        nibbles = silence = rate = 0;
        highlow = 1;
        if (srcval == 0) { // terminator
          // HACK, kill leftover odd byte to avoid restart (Minna de Odorou)
          if (lim-src == 1) src++;
          portstate = START;
        } else switch (srcval >> 6) {
          case 0: silence = (srcval & 0x3f) + 1; break;
          case 1: rate = (srcval & 0x3f) + 1; nibbles = 256; break;
          case 2: rate = (srcval & 0x3f) + 1; portstate = COUNT; break;
          case 3: break;
        }
        break;
      case COUNT:
        nibbles = *src++ + 1; portstate = HDR;
        break;
      }

    write_sample(buffer, length, stereo);
  }

  // buffer cleanup, generate irq if lowwater reached
  if (src < lim && src != PicoPicohw.xpcm_buffer) {
    int di = lim - src;
    memmove(PicoPicohw.xpcm_buffer, src, di);
    PicoPicohw.xpcm_ptr = PicoPicohw.xpcm_buffer + di;
    elprintf(EL_PICOHW, "xpcm update: over %i", di);

    if (!irq && di < FIFO_IRQ_THRESHOLD)
      irq = irqenable;
    PicoPicohw.fifo_bytes = di;
  } else if (src == lim && src != PicoPicohw.xpcm_buffer) {
    PicoPicohw.xpcm_ptr = PicoPicohw.xpcm_buffer;
    elprintf(EL_PICOHW, "xpcm update: under %i", length);

    if (!irq)
      irq = irqenable;
    PicoPicohw.fifo_bytes = 0;
  }

  // TODO need an IRQ mask somewhere to avoid loosing one in cases of HINT/VINT
  if (irq && SekIrqLevel != irq) {
    elprintf(EL_PICOHW, "irq%d", irq);
    if (SekIrqLevel < irq)
      SekInterrupt(irq);
  }

  if (buffer && length) {
    // for underflow, use last sample to avoid clicks
    int val = Limit(samplegain*sample, 16383, -16384);
    while (length--) {
      int out = apply_filter(val);
      *buffer++ += out;
      if (stereo) *buffer++ += out;
    }
  }
}
