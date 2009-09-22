#include "../pico_int.h"

static int pwm_line_samples;
static int pwm_cycles;
static int pwm_mult;
static int pwm_ptr;
int pwm_frame_smp_cnt;


void p32x_pwm_refresh(void)
{
  int cycles = Pico32x.regs[0x32 / 2];
  int frame_samples;

  cycles = (cycles - 1) & 0x0fff;
  if (cycles < 500) {
    elprintf(EL_32X|EL_ANOMALY, "pwm: low cycle value: %d", cycles + 1);
    cycles = 500;
  }
  pwm_cycles = cycles;
  pwm_mult = 0x10000 / cycles;
  if (Pico.m.pal)
    frame_samples = OSC_PAL / 7 * 3 / 50 / cycles;
  else
    frame_samples = OSC_NTSC / 7 * 3 / 60 / cycles;

  pwm_line_samples = (frame_samples << 16) / scanlines_total;
}

// irq for every sample??
// FIXME: we need to hit more than once per line :(
void p32x_pwm_irq_check(void)
{
  int tm = (Pico32x.regs[0x30 / 2] & 0x0f00) >> 8;
  if (tm == 0)
    return; // TODO: verify

  Pico32x.pwm_irq_sample_cnt += pwm_line_samples;
  if (Pico32x.pwm_irq_sample_cnt >= (tm << 16)) {
    Pico32x.pwm_irq_sample_cnt -= tm << 16;
    Pico32x.sh2irqs |= P32XI_PWM;
    p32x_update_irls();
  }
}

unsigned int p32x_pwm_read16(unsigned int a)
{
  unsigned int d = 0;
  int predict;

  a &= 0x0e;
  switch (a) {
    case 0: // control
    case 2: // cycle
      return Pico32x.regs[(0x30 + a) / 2];

    case 4: // L ch
    case 6: // R ch
    case 8: // MONO
      predict = (pwm_line_samples * Pico.m.scanline) >> 16;
      elprintf(EL_32X, "pwm: read status: ptr %d/%d, predict %d",
        pwm_frame_smp_cnt, (pwm_line_samples * scanlines_total) >> 16, predict);
      if (pwm_frame_smp_cnt > predict + 3)
        d |= P32XP_FULL;
      else if (pwm_frame_smp_cnt == 0 || pwm_frame_smp_cnt < predict - 1)
        d |= P32XP_EMPTY;
      break;
  }

  return d;
}

void p32x_pwm_write16(unsigned int a, unsigned int d)
{
  a &= 0x0e;
  if (a == 0) // control
    Pico32x.regs[0x30 / 2] = d;
  else if (a == 2) { // cycle
    Pico32x.regs[0x32 / 2] = d & 0x0fff;
    p32x_pwm_refresh();
    Pico32x.pwm_irq_sample_cnt = 0; // resets?
  }
  else if (a <= 8) {
    d &= 0x0fff;
    if (d > pwm_cycles)
      d = pwm_cycles;
    d = (d - pwm_cycles / 2) * pwm_mult;

    if       (a < 6) // L ch
      Pico32xMem->pwm[pwm_ptr * 2] = d;
    else if (a == 6) // R ch
      Pico32xMem->pwm[pwm_ptr * 2 + 1] = d;
    else             // MONO
      Pico32xMem->pwm[pwm_ptr * 2] = Pico32xMem->pwm[pwm_ptr * 2 + 1] = d;

    if (a >= 6) { // R or MONO
      pwm_frame_smp_cnt++;
      pwm_ptr = (pwm_ptr + 1) & (PWM_BUFF_LEN - 1);
        elprintf(EL_32X, "pwm: smp_cnt %d, ptr %d, smp %x", pwm_frame_smp_cnt, pwm_ptr, d);
    }
  }
}

void p32x_pwm_update(int *buf32, int length, int stereo)
{
  extern int pwm_ptr;
  short *pwmb;
  int step;
  int p = 0;

  if (pwm_ptr <= 16) // at least some samples..
    return;

  step = (pwm_ptr << 16) / length; // FIXME: division..
  pwmb = Pico32xMem->pwm;

  if (stereo)
  {
    while (length-- > 0) {
      *buf32++ += pwmb[0];
      *buf32++ += pwmb[1];

      p += step;
      pwmb += (p >> 16) * 2;
      p &= 0xffff;
    }
  }
  else
  {
    while (length-- > 0) {
      *buf32++ += pwmb[0];

      p += step;
      pwmb += (p >> 16) * 2;
      p &= 0xffff;
    }
  }

  elprintf(EL_STATUS, "pwm_update: pwm_ptr %d, len %d, step %04x, done %d",
    pwm_ptr, length, step, (pwmb - Pico32xMem->pwm) / 2);

  pwm_ptr = 0;
}

