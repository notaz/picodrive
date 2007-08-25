// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"


int SekCycleCnt=0; // cycles done in this frame
int SekCycleAim=0; // cycle aim
unsigned int SekCycleCntT=0;

#ifdef EMU_C68K
// ---------------------- Cyclone 68000 ----------------------
struct Cyclone PicoCpu;
#endif

#ifdef EMU_M68K
// ---------------------- MUSASHI 68000 ----------------------
m68ki_cpu_core PicoM68kCPU; // MD's CPU
#endif

#ifdef EMU_A68K
// ---------------------- A68K ----------------------

void __cdecl M68000_RESET();
int m68k_ICount=0;
unsigned int mem_amask=0xffffff; // 24-bit bus
unsigned int mame_debug=0,cur_mrhard=0,m68k_illegal_opcode=0,illegal_op=0,illegal_pc=0,opcode_entry=0; // filler

static int IrqCallback(int i) { i; return -1; }
static int DoReset() { return 0; }
static int (*ResetCallback)()=DoReset;

#pragma warning (disable:4152)
#endif



#ifdef EMU_C68K
// interrupt acknowledgment
static int SekIntAck(int level)
{
  // try to emulate VDP's reaction to 68000 int ack
  if     (level == 4) Pico.video.pending_ints  =  0;
  else if(level == 6) Pico.video.pending_ints &= ~0x20;
  PicoCpu.irq = 0;
  return CYCLONE_INT_ACK_AUTOVECTOR;
}

static void SekResetAck()
{
#if defined(__DEBUG_PRINT) || defined(WIN32)
  dprintf("Reset encountered @ %06x", SekPc);
#endif
}

static int SekUnrecognizedOpcode()
{
  unsigned int pc, op;
  pc = SekPc;
  op = PicoCpu.read16(pc);
#if defined(__DEBUG_PRINT) || defined(WIN32)
  dprintf("Unrecognized Opcode %04x @ %06x", op, pc);
#endif
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
  //exit(1);
  return 0;
}
#endif


#ifdef EMU_M68K
static int SekIntAckM68K(int level)
{
  if     (level == 4) { Pico.video.pending_ints  =  0;    dprintf("hack: [%i|%i]", Pico.m.scanline, SekCyclesDone()); }
  else if(level == 6) { Pico.video.pending_ints &= ~0x20; dprintf("vack: [%i|%i]", Pico.m.scanline, SekCyclesDone()); }
  CPU_INT_LEVEL = 0;
  return M68K_INT_ACK_AUTOVECTOR;
}

static int SekTasCallback(void)
{
  return 0; // no writeback
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
#ifdef EMU_A68K
  memset(&M68000_regs,0,sizeof(M68000_regs));
  M68000_regs.IrqCallback=IrqCallback;
  M68000_regs.pResetCallback=ResetCallback;
  M68000_RESET(); // Init cpu emulator
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
#ifdef EMU_A68K
  // Reset CPU: fetch SP and PC
  M68000_regs.srh=0x27; // Supervisor mode
  M68000_regs.a[7]=PicoRead32(0);
  M68000_regs.pc  =PicoRead32(4);
  PicoInitPc(M68000_regs.pc);
#endif
#ifdef EMU_M68K
  m68k_set_context(&PicoM68kCPU); // if we ever reset m68k, we always need it's context to be set
  m68ki_cpu.sp[0]=0;
  m68k_set_irq(0);
  m68k_pulse_reset();
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
#ifdef EMU_A68K
  M68000_regs.irq=irq; // raise irq (gets lowered after taken)
#endif
#ifdef EMU_M68K
  {
    void *oldcontext = m68ki_cpu_p;
    m68k_set_context(&PicoM68kCPU);
    m68k_set_irq(irq); // raise irq (gets lowered after taken or must be done in ack)
    m68k_set_context(oldcontext);
  }
#endif
  return 0;
}

//int SekPc() { return PicoCpu.pc-PicoCpu.membase; }
//int SekPc() { return M68000_regs.pc; }
//int SekPc() { return m68k_get_reg(NULL, M68K_REG_PC); }

PICO_INTERNAL void SekState(unsigned char *data)
{
#ifdef EMU_C68K
  memcpy(data,PicoCpu.d,0x44);
#elif defined(EMU_A68K)
  memcpy(data,      M68000_regs.d, 0x40);
  memcpy(data+0x40,&M68000_regs.pc,0x04);
#elif defined(EMU_M68K)
  memcpy(data,      PicoM68kCPU.dar,0x40);
  memcpy(data+0x40,&PicoM68kCPU.pc, 0x04);
#endif
}

PICO_INTERNAL void SekSetRealTAS(int use_real)
{
#ifdef EMU_C68K
  CycloneSetRealTAS(use_real);
#endif
}

