#include "../PicoInt.h"

// x: 0x03c - 0x19d
// y: 0x1fc - 0x2f7
//    0x2f8 - 0x3f3
picohw_state PicoPicohw;

static int prev_line_cnt_irq3 = 0, prev_line_cnt_irq5 = 0;
static int fifo_bytes_line = (16000<<16)/60/262/2; // fifo bytes/line. FIXME: other rates, modes

static void PicoLinePico(int count)
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

  if (PicoPicohw.fifo_bytes > 0)
  {
    PicoPicohw.fifo_line_bytes += fifo_bytes_line * count;
    if (PicoPicohw.fifo_line_bytes >= (1<<16)) {
      PicoPicohw.fifo_bytes -= PicoPicohw.fifo_line_bytes >> 16;
      PicoPicohw.fifo_line_bytes &= 0xffff;
      if (PicoPicohw.fifo_bytes < 0)
        PicoPicohw.fifo_bytes = 0;
    }
  }
  else
    PicoPicohw.fifo_line_bytes = 0;

#if 0
  if (PicoPicohw.line_counter - prev_line_cnt_irq5 > 512) {
    prev_line_cnt_irq5 = PicoPicohw.line_counter;
    elprintf(EL_ANOMALY, "irq5");
    SekInterrupt(5);
  }
#endif
}

static void PicoResetPico(void)
{
  PicoPicoPCMReset();
  PicoPicohw.xpcm_ptr = PicoPicohw.xpcm_buffer;
}

PICO_INTERNAL int PicoInitPico(void)
{
  elprintf(EL_STATUS, "Pico detected");
  PicoLineHook = PicoLinePico;
  PicoResetHook = PicoResetPico;

  PicoAHW = PAHW_PICO;
  memset(&PicoPicohw, 0, sizeof(PicoPicohw));
  PicoPicohw.pen_pos[0] = 0x03c + 352/2;
  PicoPicohw.pen_pos[1] = 0x200 + 252/2;
  prev_line_cnt_irq3 = prev_line_cnt_irq5 = 0;

  // map version register
  PicoDetectRegion();
  switch (Pico.m.hardware >> 6) {
    case 0: PicoPicohw.r1 = 0x00; break;
    case 1: PicoPicohw.r1 = 0x00; break;
    case 2: PicoPicohw.r1 = 0x40; break;
    case 3: PicoPicohw.r1 = 0x20; break;
  }

  return 0;
}

