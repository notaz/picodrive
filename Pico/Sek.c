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
struct Cyclone PicoCpu;
#endif
// MUSASHI 68000
#ifdef EMU_M68K
m68ki_cpu_core PicoM68kCPU;
#endif
// FAME 68000
#ifdef EMU_F68K
M68K_CONTEXT PicoCpuM68k;
#endif


/* callbacks */
#ifdef EMU_C68K
// interrupt acknowledgment
static int SekIntAck(int level)
{
  // try to emulate VDP's reaction to 68000 int ack
  if     (level == 4) { Pico.video.pending_ints  =  0;    elprintf(EL_INTS, "hack: @ %06x [%i]", SekPc, SekCycleCnt); }
  else if(level == 6) { Pico.video.pending_ints &= ~0x20; elprintf(EL_INTS, "vack: @ %06x [%i]", SekPc, SekCycleCnt); }
  PicoCpu.irq = 0;
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
  op = PicoCpu.read16(pc);
  elprintf(EL_ANOMALY, "Unrecognized Opcode %04x @ %06x", op, pc);
  // see if we are not executing trash
  if (pc < 0x200 || (pc > Pico.romsize+4 && (pc&0xe00000)!=0xe00000)) {
    PicoCpu.cycles = 0;
    PicoCpu.state_flags |= 1;
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
static void setup_fame_fetchmap(void)
{
  int i;

  // be default, point everything to fitst 64k of ROM
  for (i = 0; i < M68K_FETCHBANK1; i++)
    PicoCpuM68k.Fetch[i] = (unsigned int)Pico.rom - (i<<(24-FAMEC_FETCHBITS));
  // now real ROM
  for (i = 0; i < M68K_FETCHBANK1 && (i<<(24-FAMEC_FETCHBITS)) < Pico.romsize; i++)
    PicoCpuM68k.Fetch[i] = (unsigned int)Pico.rom;
  elprintf(EL_ANOMALY, "ROM end @ #%i %06x", i, (i<<(24-FAMEC_FETCHBITS)));
  // .. and RAM (TODO)
  for (i = M68K_FETCHBANK1*14/16; i < M68K_FETCHBANK1; i++)
    PicoCpuM68k.Fetch[i] = (unsigned int)Pico.ram - (i<<(24-FAMEC_FETCHBITS));

  elprintf(EL_ANOMALY, "rom = %p, ram = %p", Pico.rom, Pico.ram);
  for (i = 0; i < M68K_FETCHBANK1; i++)
    elprintf(EL_ANOMALY, "Fetch[%i] = %p", i, PicoCpuM68k.Fetch[i]);
}

void SekIntAckF68K(unsigned level)
{
  if     (level == 4) { Pico.video.pending_ints  =  0;    elprintf(EL_INTS, "hack: @ %06x [%i]", SekPc, SekCycleCnt); }
  else if(level == 6) { Pico.video.pending_ints &= ~0x20; elprintf(EL_INTS, "vack: @ %06x [%i]", SekPc, SekCycleCnt); }
  PicoCpuM68k.interrupts[0] = 0;
}
#endif


PICO_INTERNAL int SekInit()
{
#ifdef EMU_C68K
  CycloneInit();
  memset(&PicoCpu,0,sizeof(PicoCpu));
  PicoCpu.IrqCallback=SekIntAck;
  PicoCpu.ResetCallback=SekResetAck;
  PicoCpu.UnrecognizedCallback=SekUnrecognizedOpcode;
#endif
#ifdef EMU_M68K
  {
    void *oldcontext = m68ki_cpu_p;
    m68k_set_context(&PicoM68kCPU);
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
    g_m68kcontext = &PicoCpuM68k;
    memset(&PicoCpuM68k, 0, sizeof(PicoCpuM68k));
    m68k_init();
    PicoCpuM68k.iack_handler = SekIntAckF68K;
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
  PicoCpu.state_flags=0;
  PicoCpu.osp=0;
  PicoCpu.srh =0x27; // Supervisor mode
  PicoCpu.flags=4;   // Z set
  PicoCpu.irq=0;
  PicoCpu.a[7]=PicoCpu.read32(0); // Stack Pointer
  PicoCpu.membase=0;
  PicoCpu.pc=PicoCpu.checkpc(PicoCpu.read32(4)); // Program Counter
#endif
#ifdef EMU_M68K
  m68k_set_context(&PicoM68kCPU); // if we ever reset m68k, we always need it's context to be set
  m68ki_cpu.sp[0]=0;
  m68k_set_irq(0);
  m68k_pulse_reset();
#endif
#ifdef EMU_F68K
  {
    unsigned ret;
    g_m68kcontext = &PicoCpuM68k;
    setup_fame_fetchmap();
    ret = m68k_reset();
    /*if (ret)*/ elprintf(EL_ANOMALY, "m68k_reset returned %u", ret);
  }
#endif

  return 0;
}


PICO_INTERNAL int SekInterrupt(int irq)
{
#if defined(EMU_C68K) && defined(EMU_M68K)
  {
    extern unsigned int dbg_irq_level;
    dbg_irq_level=irq;
    return 0;
  }
#endif
#ifdef EMU_C68K
  PicoCpu.irq=irq;
#endif
#ifdef EMU_M68K
  {
    void *oldcontext = m68ki_cpu_p;
    m68k_set_context(&PicoM68kCPU);
    m68k_set_irq(irq); // raise irq (gets lowered after taken or must be done in ack)
    m68k_set_context(oldcontext);
  }
#endif
#ifdef EMU_F68K
  PicoCpuM68k.interrupts[0]=irq;
#endif

  return 0;
}

PICO_INTERNAL void SekState(unsigned char *data)
{
#ifdef EMU_C68K
  memcpy(data,PicoCpu.d,0x44);
#elif defined(EMU_M68K)
  memcpy(data, PicoM68kCPU.dar, 0x40);
  *(int *)(data+0x40) = PicoM68kCPU.pc;
#elif defined(EMU_F68K)
  memcpy(data, PicoCpuM68k.dreg, 0x40);
  *(int *)(data+0x40) = PicoCpuM68k.pc;
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

