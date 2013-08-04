/*
 * SH2 peripherals/"system on chip"
 * (C) notaz, 2013
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 *
 * rough fffffe00-ffffffff map:
 * e00-e05 SCI    serial communication interface
 * e10-e1a FRT    free-running timer
 * e60-e68 VCRx   irq vectors
 * e71-e72 DRCR   dma selection
 * e80-e83 WDT    watchdog timer
 * e91     SBYCR  standby control
 * e92     CCR    cache control
 * ee0     ICR    irq control
 * ee2     IPRA   irq priorities
 * ee4     VCRWDT WDT irq vectors
 * f00-f17 DIVU
 * f40-f7b UBC   user break controller
 * f80-fb3 DMAC
 * fe0-ffb BSC   bus state controller
 */

#include "../pico_int.h"
#include "../memory.h"

// DMAC handling
struct dma_chan {
  unsigned int sar, dar;  // src, dst addr
  unsigned int tcr;       // transfer count
  unsigned int chcr;      // chan ctl
  // -- dm dm sm sm  ts ts ar am  al ds dl tb  ta ie te de
  // ts - transfer size: 1, 2, 4, 16 bytes
  // ar - auto request if 1, else dreq signal
  // ie - irq enable
  // te - transfer end
  // de - dma enable
  #define DMA_AR (1 << 9)
  #define DMA_IE (1 << 2)
  #define DMA_TE (1 << 1)
  #define DMA_DE (1 << 0)
};

struct dmac {
  struct dma_chan chan[2];
  unsigned int vcrdma0;
  unsigned int unknown0;
  unsigned int vcrdma1;
  unsigned int unknown1;
  unsigned int dmaor;
  // -- pr ae nmif dme
  // pr - priority: chan0 > chan1 or round-robin
  // ae - address error
  // nmif - nmi occurred
  // dme - DMA master enable
  #define DMA_DME  (1 << 0)
};

static void dmac_te_irq(SH2 *sh2, struct dma_chan *chan)
{
  char *regs = (void *)Pico32xMem->sh2_peri_regs[sh2->is_slave];
  struct dmac *dmac = (void *)(regs + 0x180);
  int level = PREG8(regs, 0xe2) & 0x0f; // IPRA
  int vector = (chan == &dmac->chan[0]) ?
               dmac->vcrdma0 : dmac->vcrdma1;

  elprintf(EL_32XP, "dmac irq %d %d", level, vector);
  sh2_internal_irq(sh2, level, vector & 0x7f);
}

static void dmac_transfer_complete(SH2 *sh2, struct dma_chan *chan)
{
  chan->chcr |= DMA_TE; // DMA has ended normally

  p32x_sh2_poll_event(sh2, SH2_STATE_SLEEP, SekCyclesDoneT());
  if (chan->chcr & DMA_IE)
    dmac_te_irq(sh2, chan);
}

static void dmac_transfer_one(SH2 *sh2, struct dma_chan *chan)
{
  u32 size, d;

  size = (chan->chcr >> 10) & 3;
  switch (size) {
  case 0:
    d = p32x_sh2_read8(chan->sar, sh2);
    p32x_sh2_write8(chan->dar, d, sh2);
  case 1:
    d = p32x_sh2_read16(chan->sar, sh2);
    p32x_sh2_write16(chan->dar, d, sh2);
    break;
  case 2:
    d = p32x_sh2_read32(chan->sar, sh2);
    p32x_sh2_write32(chan->dar, d, sh2);
    break;
  case 3:
    d = p32x_sh2_read32(chan->sar + 0x00, sh2);
    p32x_sh2_write32(chan->dar + 0x00, d, sh2);
    d = p32x_sh2_read32(chan->sar + 0x04, sh2);
    p32x_sh2_write32(chan->dar + 0x04, d, sh2);
    d = p32x_sh2_read32(chan->sar + 0x08, sh2);
    p32x_sh2_write32(chan->dar + 0x08, d, sh2);
    d = p32x_sh2_read32(chan->sar + 0x0c, sh2);
    p32x_sh2_write32(chan->dar + 0x0c, d, sh2);
    chan->sar += 16; // always?
    if (chan->chcr & (1 << 15))
      chan->dar -= 16;
    if (chan->chcr & (1 << 14))
      chan->dar += 16;
    chan->tcr -= 4;
    return;
  }
  chan->tcr--;

  size = 1 << size;
  if (chan->chcr & (1 << 15))
    chan->dar -= size;
  if (chan->chcr & (1 << 14))
    chan->dar += size;
  if (chan->chcr & (1 << 13))
    chan->sar -= size;
  if (chan->chcr & (1 << 12))
    chan->sar += size;
}

// DMA trigger by SH2 register write
static void dmac_trigger(SH2 *sh2, struct dma_chan *chan)
{
  elprintf(EL_32XP, "sh2 DMA %08x->%08x, cnt %d, chcr %04x @%06x",
    chan->sar, chan->dar, chan->tcr, chan->chcr, sh2->pc);
  chan->tcr &= 0xffffff;

  if (chan->chcr & DMA_AR) {
    // auto-request transfer
    while ((int)chan->tcr > 0)
      dmac_transfer_one(sh2, chan);
    dmac_transfer_complete(sh2, chan);
    return;
  }

  // DREQ0 is only sent after first 4 words are written.
  // we do multiple of 4 words to avoid messing up alignment
  if (chan->sar == 0x20004012) {
    if (Pico32x.dmac0_fifo_ptr && (Pico32x.dmac0_fifo_ptr & 3) == 0) {
      elprintf(EL_32XP, "68k -> sh2 DMA");
      p32x_dreq0_trigger();
    }
    return;
  }

  elprintf(EL_32XP|EL_ANOMALY, "unhandled DMA: "
    "%08x->%08x, cnt %d, chcr %04x @%06x",
    chan->sar, chan->dar, chan->tcr, chan->chcr, sh2->pc);
}

// timer state - FIXME
static int timer_cycles[2];
static int timer_tick_cycles[2];

// timers
void p32x_timers_recalc(void)
{
  int cycles;
  int tmp, i;

  // SH2 timer step
  for (i = 0; i < 2; i++) {
    tmp = PREG8(Pico32xMem->sh2_peri_regs[i], 0x80) & 7;
    // Sclk cycles per timer tick
    if (tmp)
      cycles = 0x20 << tmp;
    else
      cycles = 2;
    timer_tick_cycles[i] = cycles;
    timer_cycles[i] = 0;
    elprintf(EL_32XP, "WDT cycles[%d] = %d", i, cycles);
  }
}

void p32x_timers_do(unsigned int m68k_slice)
{
  unsigned int cycles = m68k_slice * 3;
  int cnt, i;

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
        elprintf(EL_32XP, "%csh2 WDT irq (%d, %d)",
          i ? 's' : 'm', level, vector);
        sh2_internal_irq(&sh2s[i], level, vector);
        cnt &= 0xff;
      }
      PREG8(pregs, 0x81) = cnt;
    }
  }
}

// ------------------------------------------------------------------
// SH2 internal peripheral memhandlers
// we keep them in little endian format

u32 sh2_peripheral_read8(u32 a, int id)
{
  u8 *r = (void *)Pico32xMem->sh2_peri_regs[id];
  u32 d;

  a &= 0x1ff;
  d = PREG8(r, a);

  elprintf(EL_32XP, "%csh2 peri r8  [%08x]       %02x @%06x", id ? 's' : 'm', a | ~0x1ff, d, sh2_pc(id));
  return d;
}

u32 sh2_peripheral_read16(u32 a, int id)
{
  u16 *r = (void *)Pico32xMem->sh2_peri_regs[id];
  u32 d;

  a &= 0x1ff;
  d = r[(a / 2) ^ 1];

  elprintf(EL_32XP, "%csh2 peri r16 [%08x]     %04x @%06x", id ? 's' : 'm', a | ~0x1ff, d, sh2_pc(id));
  return d;
}

u32 sh2_peripheral_read32(u32 a, int id)
{
  u32 d;
  a &= 0x1fc;
  d = Pico32xMem->sh2_peri_regs[id][a / 4];

  elprintf(EL_32XP, "%csh2 peri r32 [%08x] %08x @%06x", id ? 's' : 'm', a | ~0x1ff, d, sh2_pc(id));
  return d;
}

int REGPARM(3) sh2_peripheral_write8(u32 a, u32 d, int id)
{
  u8 *r = (void *)Pico32xMem->sh2_peri_regs[id];
  elprintf(EL_32XP, "%csh2 peri w8  [%08x]       %02x @%06x", id ? 's' : 'm', a, d, sh2_pc(id));

  a &= 0x1ff;
  PREG8(r, a) = d;

  // X-men SCI hack
  if ((a == 2 &&  (d & 0x20)) || // transmiter enabled
      (a == 4 && !(d & 0x80))) { // valid data in TDR
    void *oregs = Pico32xMem->sh2_peri_regs[id ^ 1];
    if ((PREG8(oregs, 2) & 0x50) == 0x50) { // receiver + irq enabled
      int level = PREG8(oregs, 0x60) >> 4;
      int vector = PREG8(oregs, 0x63) & 0x7f;
      elprintf(EL_32XP, "%csh2 SCI recv irq (%d, %d)", (id ^ 1) ? 's' : 'm', level, vector);
      sh2_internal_irq(&sh2s[id ^ 1], level, vector);
      return 1;
    }
  }
  return 0;
}

int REGPARM(3) sh2_peripheral_write16(u32 a, u32 d, int id)
{
  u16 *r = (void *)Pico32xMem->sh2_peri_regs[id];
  elprintf(EL_32XP, "%csh2 peri w16 [%08x]     %04x @%06x", id ? 's' : 'm', a, d, sh2_pc(id));

  a &= 0x1ff;

  // evil WDT
  if (a == 0x80) {
    if ((d & 0xff00) == 0xa500) { // WTCSR
      PREG8(r, 0x80) = d;
      p32x_timers_recalc();
    }
    if ((d & 0xff00) == 0x5a00) // WTCNT
      PREG8(r, 0x81) = d;
    return 0;
  }

  r[(a / 2) ^ 1] = d;
  return 0;
}

void sh2_peripheral_write32(u32 a, u32 d, int id)
{
  u32 *r = Pico32xMem->sh2_peri_regs[id];
  elprintf(EL_32XP, "%csh2 peri w32 [%08x] %08x @%06x", id ? 's' : 'm', a, d, sh2_pc(id));

  a &= 0x1fc;
  r[a / 4] = d;

  switch (a) {
    // division unit (TODO: verify):
    case 0x104: // DVDNT: divident L, starts divide
      elprintf(EL_32XP, "%csh2 divide %08x / %08x", id ? 's' : 'm', d, r[0x100 / 4]);
      if (r[0x100 / 4]) {
        signed int divisor = r[0x100 / 4];
                       r[0x118 / 4] = r[0x110 / 4] = (signed int)d % divisor;
        r[0x104 / 4] = r[0x11c / 4] = r[0x114 / 4] = (signed int)d / divisor;
      }
      else
        r[0x110 / 4] = r[0x114 / 4] = r[0x118 / 4] = r[0x11c / 4] = 0; // ?
      break;
    case 0x114:
      elprintf(EL_32XP, "%csh2 divide %08x%08x / %08x @%08x",
        id ? 's' : 'm', r[0x110 / 4], d, r[0x100 / 4], sh2_pc(id));
      if (r[0x100 / 4]) {
        signed long long divident = (signed long long)r[0x110 / 4] << 32 | d;
        signed int divisor = r[0x100 / 4];
        // XXX: undocumented mirroring to 0x118,0x11c?
        r[0x118 / 4] = r[0x110 / 4] = divident % divisor;
        divident /= divisor;
        r[0x11c / 4] = r[0x114 / 4] = divident;
        divident >>= 31;
        if ((unsigned long long)divident + 1 > 1) {
          //elprintf(EL_32XP, "%csh2 divide overflow! @%08x", id ? 's' : 'm', sh2_pc(id));
          r[0x11c / 4] = r[0x114 / 4] = divident > 0 ? 0x7fffffff : 0x80000000; // overflow
        }
      }
      else
        r[0x110 / 4] = r[0x114 / 4] = r[0x118 / 4] = r[0x11c / 4] = 0; // ?
      break;
  }

  // perhaps starting a DMA?
  if (a == 0x1b0 || a == 0x18c || a == 0x19c) {
    struct dmac *dmac = (void *)&Pico32xMem->sh2_peri_regs[id][0x180 / 4];
    if (!(dmac->dmaor & DMA_DME))
      return;

    if ((dmac->chan[0].chcr & (DMA_TE|DMA_DE)) == DMA_DE)
      dmac_trigger(&sh2s[id], &dmac->chan[0]);
    if ((dmac->chan[1].chcr & (DMA_TE|DMA_DE)) == DMA_DE)
      dmac_trigger(&sh2s[id], &dmac->chan[1]);
  }
}

/* 32X specific */
static void dreq0_do(SH2 *sh2, struct dma_chan *chan)
{
  unsigned short *dreqlen = &Pico32x.regs[0x10 / 2];
  int i;

  // debug/sanity checks
  if (chan->tcr != *dreqlen)
    elprintf(EL_32XP|EL_ANOMALY, "dreq0: tcr0 and len differ: %d != %d",
      chan->tcr, *dreqlen);
  // note: DACK is not connected, single addr mode should not be used
  if ((chan->chcr & 0x3f08) != 0x0400)
    elprintf(EL_32XP|EL_ANOMALY, "dreq0: bad control: %04x", chan->chcr);
  if (chan->sar != 0x20004012)
    elprintf(EL_32XP|EL_ANOMALY, "dreq0: bad sar?: %08x\n", chan->sar);

  // HACK: assume bus is busy and SH2 is halted
  sh2->state |= SH2_STATE_SLEEP;

  for (i = 0; i < Pico32x.dmac0_fifo_ptr && chan->tcr > 0; i++) {
    elprintf(EL_32XP, "dmaw [%08x] %04x, left %d",
      chan->dar, Pico32x.dmac_fifo[i], *dreqlen);
    p32x_sh2_write16(chan->dar, Pico32x.dmac_fifo[i], sh2);
    chan->dar += 2;
    chan->tcr--;
    (*dreqlen)--;
  }

  if (Pico32x.dmac0_fifo_ptr != i)
    memmove(Pico32x.dmac_fifo, &Pico32x.dmac_fifo[i],
      (Pico32x.dmac0_fifo_ptr - i) * 2);
  Pico32x.dmac0_fifo_ptr -= i;

  Pico32x.regs[6 / 2] &= ~P32XS_FULL;
  if (*dreqlen == 0)
    Pico32x.regs[6 / 2] &= ~P32XS_68S; // transfer complete
  if (chan->tcr == 0)
    dmac_transfer_complete(sh2, chan);
  else
    sh2_end_run(sh2, 16);
}

static void dreq1_do(SH2 *sh2, struct dma_chan *chan)
{
  // debug/sanity checks
  if ((chan->chcr & 0xc308) != 0x0000)
    elprintf(EL_32XP|EL_ANOMALY, "dreq1: bad control: %04x", chan->chcr);
  if ((chan->dar & ~0xf) != 0x20004030)
    elprintf(EL_32XP|EL_ANOMALY, "dreq1: bad dar?: %08x\n", chan->dar);

  dmac_transfer_one(sh2, chan);
  if (chan->tcr == 0)
    dmac_transfer_complete(sh2, chan);
}

void p32x_dreq0_trigger(void)
{
  struct dmac *mdmac = (void *)&Pico32xMem->sh2_peri_regs[0][0x180 / 4];
  struct dmac *sdmac = (void *)&Pico32xMem->sh2_peri_regs[1][0x180 / 4];

  elprintf(EL_32XP, "dreq0_trigger");
  if ((mdmac->dmaor & DMA_DME) && (mdmac->chan[0].chcr & 3) == DMA_DE) {
    dreq0_do(&msh2, &mdmac->chan[0]);
  }
  if ((sdmac->dmaor & DMA_DME) && (sdmac->chan[0].chcr & 3) == DMA_DE) {
    dreq0_do(&ssh2, &sdmac->chan[0]);
  }
}

void p32x_dreq1_trigger(void)
{
  struct dmac *mdmac = (void *)&Pico32xMem->sh2_peri_regs[0][0x180 / 4];
  struct dmac *sdmac = (void *)&Pico32xMem->sh2_peri_regs[1][0x180 / 4];
  int hit = 0;

  elprintf(EL_32XP, "dreq1_trigger");
  if ((mdmac->dmaor & DMA_DME) && (mdmac->chan[1].chcr & 3) == DMA_DE) {
    dreq1_do(&msh2, &mdmac->chan[1]);
    hit = 1;
  }
  if ((sdmac->dmaor & DMA_DME) && (sdmac->chan[1].chcr & 3) == DMA_DE) {
    dreq1_do(&ssh2, &sdmac->chan[1]);
    hit = 1;
  }

  if (!hit)
    elprintf(EL_32XP|EL_ANOMALY, "dreq1: nobody cared");
}

// vim:shiftwidth=2:ts=2:expandtab
