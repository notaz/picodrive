#include "../PicoInt.h"

// x: 0x03c - 0x19d
// y: 0x1fc - 0x2f7
//    0x2f8 - 0x3f3
int PicoPicoPenPos[2] = { 0x3c, 0x200 };
int PicoPicoPage = 0; // 0-6

PICO_INTERNAL int PicoInitPico(void)
{
  elprintf(EL_STATUS, "Pico detected");
  PicoAHW = PAHW_PICO;
  PicoPicoPage = 0;

  return 0;
}

