#include "../PicoInt.h"

// x: 0x03c - 0x19d
// y: 0x1fc - 0x2f7
//    0x2f8 - 0x3f3
picohw_state PicoPicohw;

static int prev_line_cnt_irq3 = 0, prev_line_cnt_irq5 = 0;

static void PicoLineHookPico(int count)
{
  PicoPicohw.line_counter += count;

#if 1
  if ((PicoPicohw.r12 & 0x4003) && PicoPicohw.line_counter - prev_line_cnt_irq3 > 200) {
    prev_line_cnt_irq3 = PicoPicohw.line_counter;
    // just a guess/hack, allows 101 Dalmantians to boot
    elprintf(EL_ANOMALY, "irq3");
    SekInterrupt(3);
    return;
  }

  if (PicoPicohw.fifo_bytes == 16) {
    prev_line_cnt_irq3 = PicoPicohw.line_counter;
    elprintf(EL_ANOMALY, "irq3, fb=%i", PicoPicohw.fifo_bytes);
    SekInterrupt(3);
    PicoPicohw.fifo_bytes--;
    return;
  }
#endif

  if ((PicoPicohw.line_counter & 3) == 0 || count > 10)
  {
    if (PicoPicohw.fifo_bytes > 0)
      PicoPicohw.fifo_bytes--;
  }

#if 0
  if (PicoPicohw.line_counter - prev_line_cnt_irq5 > 512) {
    prev_line_cnt_irq5 = PicoPicohw.line_counter;
    elprintf(EL_ANOMALY, "irq5");
    SekInterrupt(5);
  }
#endif
}

PICO_INTERNAL int PicoInitPico(void)
{
  elprintf(EL_STATUS, "Pico detected");
  PicoLineHook = PicoLineHookPico;

  PicoAHW = PAHW_PICO;
  memset(&PicoPicohw, 0, sizeof(PicoPicohw));
  PicoPicohw.pen_pos[0] = 0x03c + 352/2;
  PicoPicohw.pen_pos[1] = 0x200 + 252/2;
  prev_line_cnt_irq3 = 0, prev_line_cnt_irq5 = 0;

  // map version register
  PicoDetectRegion();
  elprintf(EL_STATUS, "a %x", Pico.m.hardware);
  switch (Pico.m.hardware >> 6) {
    case 0: PicoPicohw.r1 = 0x00; break;
    case 1: PicoPicohw.r1 = 0x00; break;
    case 2: PicoPicohw.r1 = 0x40; break;
    case 3: PicoPicohw.r1 = 0x20; break;
  }

  return 0;
}

