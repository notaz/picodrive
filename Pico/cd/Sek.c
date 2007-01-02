// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "../PicoInt.h"


int SekCycleCntS68k=0; // cycles done in this frame
int SekCycleAimS68k=0; // cycle aim

#ifdef EMU_M68K
// ---------------------- MUSASHI 68000 ----------------------
m68ki_cpu_core PicoS68kCPU; // Mega CD's CPU
#endif


#ifdef EMU_M68K
int SekIntAckS68k(int level)
{
  dprintf("s68kACK %i", level);
  CPU_INT_LEVEL = 0;
  return M68K_INT_ACK_AUTOVECTOR;
}
#endif


int SekInitS68k()
{
#ifdef EMU_M68K
  {
    // Musashi is not very context friendly..
    void *oldcontext = m68ki_cpu_p;
    m68k_set_context(&PicoS68kCPU);
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_init();
    m68k_set_int_ack_callback(SekIntAckS68k);
//  m68k_pulse_reset(); // not yet, memmap is not set up
    m68k_set_context(oldcontext);
  }
#endif

  return 0;
}

// Reset the 68000:
int SekResetS68k()
{
  if (Pico.rom==NULL) return 1;

#ifdef EMU_M68K
  {
    void *oldcontext = m68ki_cpu_p;

    m68k_set_context(&PicoS68kCPU);
    m68k_pulse_reset();
    m68k_set_context(oldcontext);
  }
#endif

  return 0;
}

int SekInterruptS68k(int irq)
{
#ifdef EMU_M68K
  void *oldcontext = m68ki_cpu_p;
  m68k_set_context(&PicoS68kCPU);
  m68k_set_irq(irq); // raise irq (gets lowered after taken or must be done in ack)
  m68k_set_context(oldcontext);
#endif
  return 0;
}

