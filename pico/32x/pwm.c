/*
 * PicoDrive
 * (C) notaz, 2009,2010,2013
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */
#include "../pico_int.h"

static int pwm_cycles;
static int pwm_mult;
static int pwm_ptr;

static int timer_cycles[2];
static int timer_tick_cycles[2];

// timers. This includes PWM timer in 32x and internal SH2 timers
void p32x_timers_recalc(void)
{
  int cycles = Pico32x.regs[0x32 / 2];
  int tmp, i;

  cycles = (cycles - 1) & 0x0fff;
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

#define consume_fifo(cycles) { \
  int cycles_diff = (cycles) - Pico32x.pwm_cycle_p; \
  if (cycles_diff >= pwm_cycles) \
    consume_fifo_do((cycles), cycles_diff); \
}

static void consume_fifo_do(unsigned int cycles, int cycles_diff)
{
  if (pwm_cycles == 0)
    return;

  elprintf(EL_PWM, "pwm: %u: consume %d/%d, %d,%d ptr %d",
    cycles, cycles_diff, cycles_diff / pwm_cycles,
    Pico32x.pwm_p[0], Pico32x.pwm_p[1], pwm_ptr);

  if (cycles_diff > pwm_cycles * 9) {
    // silence/skip
    Pico32x.pwm_cycle_p = cycles;
    Pico32x.pwm_p[0] = Pico32x.pwm_p[1] = 0;
    return;
  }

  for (; cycles_diff >= pwm_cycles; cycles_diff -= pwm_cycles) {
    struct Pico32xMem *mem = Pico32xMem;
    short *fifo_l = mem->pwm_fifo[0];
    short *fifo_r = mem->pwm_fifo[1];

    if (Pico32x.pwm_p[0] > 0) {
      fifo_l[0] = fifo_l[1];
      fifo_l[1] = fifo_l[2];
      fifo_l[2] = fifo_l[3];
      Pico32x.pwm_p[0]--;
    }
    if (Pico32x.pwm_p[1] > 0) {
      fifo_r[0] = fifo_r[1];
      fifo_r[1] = fifo_r[2];
      fifo_r[2] = fifo_r[3];
      Pico32x.pwm_p[1]--;
    }

    mem->pwm[pwm_ptr * 2    ] = fifo_l[0];
    mem->pwm[pwm_ptr * 2 + 1] = fifo_r[0];
    pwm_ptr = (pwm_ptr + 1) & (PWM_BUFF_LEN - 1);
  }
  Pico32x.pwm_cycle_p = cycles - cycles_diff;
}

void p32x_timers_do(unsigned int m68k_now, unsigned int m68k_slice)
{
  unsigned int cycles = m68k_slice * 3;
  int cnt, i;

  consume_fifo(m68k_now * 3);

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

unsigned int p32x_pwm_read16(unsigned int a, unsigned int cycles)
{
  unsigned int d = 0;

  consume_fifo(cycles);

  a &= 0x0e;
  switch (a) {
    case 0: // control
    case 2: // cycle
      d = Pico32x.regs[(0x30 + a) / 2];
      break;

    case 4: // L ch
      if (Pico32x.pwm_p[0] == 3)
        d |= P32XP_FULL;
      else if (Pico32x.pwm_p[0] == 0)
        d |= P32XP_EMPTY;
      break;

    case 6: // R ch
    case 8: // MONO
      if (Pico32x.pwm_p[1] == 3)
        d |= P32XP_FULL;
      else if (Pico32x.pwm_p[1] == 0)
        d |= P32XP_EMPTY;
      break;
  }

  elprintf(EL_PWM, "pwm: read %02x %04x (p %d %d), c %u",
    a, d, Pico32x.pwm_p[0], Pico32x.pwm_p[1], cycles);
  return d;
}

void p32x_pwm_write16(unsigned int a, unsigned int d, unsigned int cycles)
{
  consume_fifo(cycles);

  a &= 0x0e;
  if (a == 0) { // control
    // supposedly we should stop FIFO when xMd is 0,
    // but mars test disagrees
    Pico32x.regs[0x30 / 2] = d;
  }
  else if (a == 2) { // cycle
    Pico32x.regs[0x32 / 2] = d & 0x0fff;
    p32x_timers_recalc();
    Pico32x.pwm_irq_sample_cnt = 0; // resets?
  }
  else if (a <= 8) {
    d = (d - 1) & 0x0fff;
    if (d > pwm_cycles)
      d = pwm_cycles;
    d = (d - pwm_cycles / 2) * pwm_mult;

    if (a == 4 || a == 8) { // L ch or MONO
      short *fifo = Pico32xMem->pwm_fifo[0];
      if (Pico32x.pwm_p[0] < 3)
        Pico32x.pwm_p[0]++;
      else {
        fifo[1] = fifo[2];
        fifo[2] = fifo[3];
      }
      fifo[Pico32x.pwm_p[0]] = d;
    }
    if (a == 6 || a == 8) { // R ch or MONO
      short *fifo = Pico32xMem->pwm_fifo[1];
      if (Pico32x.pwm_p[1] < 3)
        Pico32x.pwm_p[1]++;
      else {
        fifo[1] = fifo[2];
        fifo[2] = fifo[3];
      }
      fifo[Pico32x.pwm_p[1]] = d;
    }
  }
}

void p32x_pwm_update(int *buf32, int length, int stereo)
{
  short *pwmb;
  int step;
  int p = 0;
  int xmd;

  xmd = Pico32x.regs[0x30 / 2] & 0x0f;
  if ((xmd != 0x05 && xmd != 0x0a) || pwm_ptr <= 16)
    goto out;

  step = (pwm_ptr << 16) / length; // FIXME: division..
  pwmb = Pico32xMem->pwm;

  if (stereo)
  {
    if (xmd == 0x0a) {
      // channel swap
      while (length-- > 0) {
        *buf32++ += pwmb[1];
        *buf32++ += pwmb[0];

        p += step;
        pwmb += (p >> 16) * 2;
        p &= 0xffff;
      }
    }
    else {
      while (length-- > 0) {
        *buf32++ += pwmb[0];
        *buf32++ += pwmb[1];

        p += step;
        pwmb += (p >> 16) * 2;
        p &= 0xffff;
      }
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

out:
  pwm_ptr = 0;
}

// vim:shiftwidth=2:ts=2:expandtab
