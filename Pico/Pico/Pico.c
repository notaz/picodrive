#include "../PicoInt.h"

// x: 0x03c - 0x19d
// y: 0x1fc - 0x2f7
//    0x2f8 - 0x3f3
picohw_state PicoPicohw;

static int prev_line_cnt_irq3 = 0, prev_line_cnt_irq5 = 0;

static void PicoLineHookPico(int count)
{

  PicoPicohw.line_counter += count;
  if ((PicoPicohw.line_counter & 0xf) == 0 || count > 10)
  {
    if (PicoPicohw.fifo_bytes > 0)
      PicoPicohw.fifo_bytes--;
  }

#if 0
  if (PicoPicohw.line_counter - prev_line_cnt_irq3 > 200) {
    prev_line_cnt_irq3 = PicoPicohw.line_counter;
    // just a guess/hack, allows 101 Dalmantians to boot
    elprintf(EL_ANOMALY, "irq3");
    SekInterrupt(3);
  }
#endif

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

  return 0;
}

