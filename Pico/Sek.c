// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"


int SekCycleCnt=0; // cycles done in this frame
int SekCycleAim=0; // cycle aim
unsigned int SekCycleCntT=0;


/* context */
// Cyclone 68000
#ifdef EMU_C68K
struct Cyclone PicoCpuCM68k;
#endif
// MUSASHI 68000
#ifdef EMU_M68K
m68ki_cpu_core PicoCpuMM68k;
#endif
// FAME 68000
#ifdef EMU_F68K
M68K_CONTEXT PicoCpuFM68k;
#endif


/* callbacks */
#ifdef EMU_C68K
// interrupt acknowledgment
static int SekIntAck(int level)
{
  // try to emulate VDP's reaction to 68000 int ack
  if     (level == 4) { Pico.video.pending_ints  =  0;    elprintf(EL_INTS, "hack: @ %06x [%i]", SekPc, SekCycleCnt); }
  else if(level == 6) { Pico.video.pending_ints &= ~0x20; elprintf(EL_INTS, "vack: @ %06x [%i]", SekPc, SekCycleCnt); }
  PicoCpuCM68k.irq = 0;
  return CYCLONE_INT_ACK_AUTOVECTOR;
}

static void SekResetAck(void)
{
  elprintf(EL_ANOMALY, "Reset encountered @ %06x", SekPc);
}

static int SekUnrecognizedOpcode()
{
  unsigned int pc, op;
  pc = SekPc;
  op = PicoCpuCM68k.read16(pc);
  elprintf(EL_ANOMALY, "Unrecognized Opcode %04x @ %06x", op, pc);
  // see if we are not executing trash
  if (pc < 0x200 || (pc > Pico.romsize+4 && (pc&0xe00000)!=0xe00000)) {
    PicoCpuCM68k.cycles = 0;
    PicoCpuCM68k.state_flags |= 1;
    return 1;
  }
#ifdef EMU_M68K // debugging cyclone
  {
    extern int have_illegal;
    have_illegal = 1;
  }
#endif
  return 0;
}
#endif


#ifdef EMU_M68K
static int SekIntAckM68K(int level)
{
  if     (level == 4) { Pico.video.pending_ints  =  0;    elprintf(EL_INTS, "hack: @ %06x [%i]", SekPc, SekCycleCnt); }
  else if(level == 6) { Pico.video.pending_ints &= ~0x20; elprintf(EL_INTS, "vack: @ %06x [%i]", SekPc, SekCycleCnt); }
  CPU_INT_LEVEL = 0;
  return M68K_INT_ACK_AUTOVECTOR;
}

static int SekTasCallback(void)
{
  return 0; // no writeback
}
#endif


#ifdef EMU_F68K
static void SekIntAckF68K(unsigned level)
{
  if     (level == 4) { Pico.video.pending_ints  =  0;    elprintf(EL_INTS, "hack: @ %06x [%i]", SekPc, SekCycleCnt); }
  else if(level == 6) { Pico.video.pending_ints &= ~0x20; elprintf(EL_INTS, "vack: @ %06x [%i]", SekPc, SekCycleCnt); }
  PicoCpuFM68k.interrupts[0] = 0;
}
#endif


PICO_INTERNAL int SekInit()
{
#ifdef EMU_C68K
  CycloneInit();
  memset(&PicoCpuCM68k,0,sizeof(PicoCpuCM68k));
  PicoCpuCM68k.IrqCallback=SekIntAck;
  PicoCpuCM68k.ResetCallback=SekResetAck;
  PicoCpuCM68k.UnrecognizedCallback=SekUnrecognizedOpcode;
  PicoCpuCM68k.flags=4;   // Z set
#endif
#ifdef EMU_M68K
  {
    void *oldcontext = m68ki_cpu_p;
    m68k_set_context(&PicoCpuMM68k);
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_init();
    m68k_set_int_ack_callback(SekIntAckM68K);
    m68k_set_tas_instr_callback(SekTasCallback);
    m68k_pulse_reset(); // Init cpu emulator
    m68k_set_context(oldcontext);
  }
#endif
#ifdef EMU_F68K
  {
    void *oldcontext = g_m68kcontext;
    g_m68kcontext = &PicoCpuFM68k;
    memset(&PicoCpuFM68k, 0, sizeof(PicoCpuFM68k));
    fm68k_init();
    PicoCpuFM68k.iack_handler = SekIntAckF68K;
    PicoCpuFM68k.sr = 0x2704; // Z flag
    g_m68kcontext = oldcontext;
  }
#endif

  return 0;
}


// Reset the 68000:
PICO_INTERNAL int SekReset()
{
  if (Pico.rom==NULL) return 1;

#ifdef EMU_C68K
  PicoCpuCM68k.state_flags=0;
  PicoCpuCM68k.osp=0;
  PicoCpuCM68k.srh =0x27; // Supervisor mode
  PicoCpuCM68k.irq=0;
  PicoCpuCM68k.a[7]=PicoCpuCM68k.read32(0); // Stack Pointer
  PicoCpuCM68k.membase=0;
  PicoCpuCM68k.pc=PicoCpuCM68k.checkpc(PicoCpuCM68k.read32(4)); // Program Counter
#endif
#ifdef EMU_M68K
  m68k_set_context(&PicoCpuMM68k); // if we ever reset m68k, we always need it's context to be set
  m68ki_cpu.sp[0]=0;
  m68k_set_irq(0);
  m68k_pulse_reset();
#endif
#ifdef EMU_F68K
  {
    g_m68kcontext = &PicoCpuFM68k;
    fm68k_reset();
  }
#endif

  return 0;
}


// data must be word aligned
PICO_INTERNAL void SekState(int *data)
{
#ifdef EMU_C68K
  memcpy32(data,(int *)PicoCpuCM68k.d,0x44/4);
#elif defined(EMU_M68K)
  memcpy32(data, (int *)PicoCpuMM68k.dar, 0x40/4);
  data[0x10] = PicoCpuMM68k.pc;
#elif defined(EMU_F68K)
  memcpy32(data, (int *)PicoCpuFM68k.dreg, 0x40/4);
  data[0x10] = PicoCpuFM68k.pc;
#endif
}

PICO_INTERNAL void SekSetRealTAS(int use_real)
{
#ifdef EMU_C68K
  CycloneSetRealTAS(use_real);
#endif
#ifdef EMU_F68K
  // TODO
#endif
}

