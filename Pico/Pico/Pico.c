#include "../PicoInt.h"

PICO_INTERNAL int PicoInitPico(void)
{
  elprintf(EL_STATUS, "Pico detected");
  PicoAHW = PAHW_PICO;

  return 0;
}

