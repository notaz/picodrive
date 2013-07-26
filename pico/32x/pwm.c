/*
 * PicoDrive
 * (C) notaz, 2009,2010
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */
#include "../pico_int.h"

static int pwm_cycle_counter;
static int pwm_cycles;
static int pwm_mult;
static int pwm_ptr;

static int pwm_smp_cnt;
static int pwm_smp_expect;

static int timer_cycles[2];
static int timer_tick_cycles[2];

// timers. This includes PWM timer in 32x and internal SH2 timers
void p32x_timers_recalc(void)
{
  int cycles = Pico32x.regs[0x32 / 2];
  int tmp, i;

  cycles = (cycles - 1) & 0x0fff;
  if (cycles < 500) {
    elprintf(EL_32X|EL_PWM|EL_ANOMALY, "pwm: low cycle value: %d", cycles + 1);
    cycles = 500;
  }
  pwm_cycles = cycles;
  pwm_mult = 0x10000 / cycles;

  // SH2 timer step
  for (i = 0; i < 2; i++) {
    tmp = PREG8(Pico32xMem->sh2_peri_regs[i], 0x80) & 7;
    // Sclk cycles per timer tick
    if (tmp)
      cycles = 0x20 << tmp;
    else
      cycles = 2;
    timer_tick_cycles[i] = cycles;
    elprintf(EL_32X, "WDT cycles[%d] = %d", i, cycles);
  }
}

// PWM irq for every tm samples
void p32x_timers_do(unsigned int cycles)
{
  int cnt, i;

  cycles *= 3;

  // since we run things in async fashion, allow pwm to lag behind
  // but don't allow our "queue" to be infinite
  cnt = pwm_smp_expect - pwm_smp_cnt;
  if (cnt <= 0 || cnt * pwm_cycles < OSC_NTSC/7*3 / 60 / 2) {
    pwm_cycle_counter += cycles;
    while (pwm_cycle_counter > pwm_cycles) {
      pwm_cycle_counter -= pwm_cycles;
      pwm_smp_expect++;
    }
  }

  // WDT timers
  for (i = 0; i < 2; i++) {
    void *pregs = Pico32xMem->sh2_peri_regs[i];
    if (PREG8(pregs, 0x80) & 0x20) { // TME
      timer_cycles[i] += cycles;
      cnt = PREG8(pregs, 0x81);
      while (timer_cycles[i] >= timer_tick_cycles[i]) {
        timer_cycles[i] -= timer_tick_cycles[i];
        cnt++;
      }
      if (cnt >= 0x100) {
        int level = PREG8(pregs, 0xe3) >> 4;
        int vector = PREG8(pregs, 0xe4) & 0x7f;
        elprintf(EL_32X, "%csh2 WDT irq (%d, %d)",
          i ? 's' : 'm', level, vector);
        sh2_internal_irq(&sh2s[i], level, vector);
        cnt &= 0xff;
      }
      PREG8(pregs, 0x81) = cnt;
    }
  }
}

static int p32x_pwm_schedule_(void)
{
  int tm;

  if (Pico32x.emu_flags & P32XF_PWM_PEND)
    return 0; // already scheduled
  if (Pico32x.sh2irqs & P32XI_PWM)
    return 0; // previous not acked
  if (!((Pico32x.sh2irq_mask[0] | Pico32x.sh2irq_mask[1]) & 1))
    return 0; // masked by everyone

  Pico32x.emu_flags |= P32XF_PWM_PEND;
  tm = (Pico32x.regs[0x30 / 2] & 0x0f00) >> 8;
  tm = ((tm - 1) & 0x0f) + 1;
  return pwm_cycles * tm / 3;
}

void p32x_pwm_schedule(unsigned int now)
{
  int after = p32x_pwm_schedule_();
  if (after != 0)
    p32x_event_schedule(now, P32X_EVENT_PWM, after);
}

void p32x_pwm_schedule_sh2(SH2 *sh2)
{
  int after = p32x_pwm_schedule_();
  if (after != 0)
    p32x_event_schedule_sh2(sh2, P32X_EVENT_PWM, after);
}

unsigned int p32x_pwm_read16(unsigned int a)
{
  unsigned int d = 0;
  int diff;

  a &= 0x0e;
  switch (a) {
    case 0: // control
    case 2: // cycle
      return Pico32x.regs[(0x30 + a) / 2];

    case 4: // L ch
    case 6: // R ch
    case 8: // MONO
      diff = pwm_smp_cnt - pwm_smp_expect;
      elprintf(EL_PWM, "pwm: read status: ptr %d/%d %d",
        pwm_smp_cnt, pwm_smp_expect, diff);
      if (diff > 3)
        d |= P32XP_FULL;
      else if (diff < 0)
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
      pwm_smp_cnt++;
      pwm_ptr = (pwm_ptr + 1) & (PWM_BUFF_LEN - 1);
      elprintf(EL_PWM, "pwm: smp_cnt %d, ptr %d, smp %x",
          pwm_smp_cnt, pwm_ptr, d);
    }
  }
}

void p32x_pwm_update(int *buf32, int length, int stereo)
{
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

// vim:shiftwidth=2:ts=2:expandtab
