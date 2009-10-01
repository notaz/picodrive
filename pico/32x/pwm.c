#include "../pico_int.h"

static int pwm_line_samples;
static int pwm_cycles;
static int pwm_mult;
static int pwm_ptr;
int pwm_frame_smp_cnt;

static int timer_line_ticks[2];

// timers. This includes PWM timer in 32x and internal SH2 timers
void p32x_timers_recalc(void)
{
  int cycles = Pico32x.regs[0x32 / 2];
  int frame_samples;
  int tmp, i;

  cycles = (cycles - 1) & 0x0fff;
  if (cycles < 500) {
    elprintf(EL_32X|EL_PWM|EL_ANOMALY, "pwm: low cycle value: %d", cycles + 1);
    cycles = 500;
  }
  pwm_cycles = cycles;
  pwm_mult = 0x10000 / cycles;
  if (Pico.m.pal)
    frame_samples = OSC_PAL / 7 * 3 / 50 / cycles;
  else
    frame_samples = OSC_NTSC / 7 * 3 / 60 / cycles;

  pwm_line_samples = (frame_samples << 16) / scanlines_total;

  // SH2 timer step
  for (i = 0; i < 2; i++) {
    tmp = PREG8(Pico32xMem->sh2_peri_regs[i], 0x80) & 7;
    // Sclk cycles per timer tick
    if (tmp)
      cycles = 0x20 << tmp;
    else
      cycles = 2;
    if (Pico.m.pal)
      tmp = OSC_PAL / 7 * 3 / 50 / scanlines_total;
    else
      tmp = OSC_NTSC / 7 * 3 / 60 / scanlines_total;
    timer_line_ticks[i] = (tmp << 16) / cycles;
    elprintf(EL_32X, "timer_line_ticks[%d] = %.3f", i, (double)timer_line_ticks[i] / 0x10000);
  }
}

// PWM irq for every tm samples
void p32x_timers_do(int new_line)
{
  int tm, cnt, i;
  tm = (Pico32x.regs[0x30 / 2] & 0x0f00) >> 8;
  if (tm != 0) {
    if (new_line)
      Pico32x.pwm_irq_sample_cnt += pwm_line_samples;
    if (Pico32x.pwm_irq_sample_cnt >= (tm << 16)) {
      Pico32x.pwm_irq_sample_cnt -= tm << 16;
      Pico32x.sh2irqs |= P32XI_PWM;
      p32x_update_irls();
    }
  }

  if (!new_line)
    return;

  for (i = 0; i < 2; i++) {
    void *pregs = Pico32xMem->sh2_peri_regs[i];
    if (PREG8(pregs, 0x80) & 0x20) { // TME
      cnt = PREG8(pregs, 0x81);
      cnt += timer_line_ticks[i];
      if (cnt >= 0x100) {
        int level = PREG8(pregs, 0xe3) >> 4;
        int vector = PREG8(pregs, 0xe4) & 0x7f;
        elprintf(EL_32X, "%csh2 WDT irq (%d, %d)", i ? 's' : 'm', level, vector);
        sh2_internal_irq(&sh2s[i], level, vector);
      }
      cnt &= 0xff;
      PREG8(pregs, 0x81) = cnt;
    }
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
      elprintf(EL_PWM, "pwm: read status: ptr %d/%d, predict %d",
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
    p32x_timers_recalc();
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
        elprintf(EL_PWM, "pwm: smp_cnt %d, ptr %d, smp %x", pwm_frame_smp_cnt, pwm_ptr, d);
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

  elprintf(EL_PWM, "pwm_update: pwm_ptr %d, len %d, step %04x, done %d",
    pwm_ptr, length, step, (pwmb - Pico32xMem->pwm) / 2);

  pwm_ptr = 0;
}

