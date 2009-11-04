// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "pico_int.h"
#include "memory.h"


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


PICO_INTERNAL void SekInit(void)
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
    //m68k_pulse_reset();
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
}


// Reset the 68000:
PICO_INTERNAL int SekReset(void)
{
  if (Pico.rom==NULL) return 1;

#ifdef EMU_C68K
  CycloneReset(&PicoCpuCM68k);
#endif
#ifdef EMU_M68K
  m68k_set_context(&PicoCpuMM68k); // if we ever reset m68k, we always need it's context to be set
  m68ki_cpu.sp[0]=0;
  m68k_set_irq(0);
  m68k_pulse_reset();
  REG_USP = 0; // ?
#endif
#ifdef EMU_F68K
  {
    g_m68kcontext = &PicoCpuFM68k;
    fm68k_reset();
  }
#endif

  return 0;
}

void SekStepM68k(void)
{
  SekCycleAim=SekCycleCnt+1;
#if defined(EMU_CORE_DEBUG)
  SekCycleCnt+=CM_compareRun(1, 0);
#elif defined(EMU_C68K)
  PicoCpuCM68k.cycles=1;
  CycloneRun(&PicoCpuCM68k);
  SekCycleCnt+=1-PicoCpuCM68k.cycles;
#elif defined(EMU_M68K)
  SekCycleCnt+=m68k_execute(1);
#elif defined(EMU_F68K)
  SekCycleCnt+=fm68k_emulate(1, 0, 0);
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


/* idle loop detection, not to be used in CD mode */
#ifdef EMU_C68K
#include "cpu/Cyclone/tools/idle.h"
#endif

static unsigned short **idledet_ptrs = NULL;
static int idledet_count = 0, idledet_bads = 0;
int idledet_start_frame = 0;

#if 0
#define IDLE_STATS 1
unsigned int idlehit_addrs[128], idlehit_counts[128];

void SekRegisterIdleHit(unsigned int pc)
{
  int i;
  for (i = 0; i < 127 && idlehit_addrs[i]; i++) {
    if (idlehit_addrs[i] == pc) {
      idlehit_counts[i]++;
      return;
    }
  }
  idlehit_addrs[i] = pc;
  idlehit_counts[i] = 1;
  idlehit_addrs[i+1] = 0;
}
#endif

void SekInitIdleDet(void)
{
  unsigned short **tmp = realloc(idledet_ptrs, 0x200*4);
  if (tmp == NULL) {
    free(idledet_ptrs);
    idledet_ptrs = NULL;
  }
  else
    idledet_ptrs = tmp;
  idledet_count = idledet_bads = 0;
  idledet_start_frame = Pico.m.frame_count + 360;
#ifdef IDLE_STATS
  idlehit_addrs[0] = 0;
#endif

#ifdef EMU_C68K
  CycloneInitIdle();
#endif
#ifdef EMU_F68K
  fm68k_emulate(0, 0, 1);
#endif
}

int SekIsIdleCode(unsigned short *dst, int bytes)
{
  // printf("SekIsIdleCode %04x %i\n", *dst, bytes);
  switch (bytes)
  {
    case 2:
      if ((*dst & 0xf000) != 0x6000)     // not another branch
        return 1;
      break;
    case 4:
      if (  (*dst & 0xfff8) == 0x4a10 || // tst.b ($aX)      // there should be no need to wait
            (*dst & 0xfff8) == 0x4a28 || // tst.b ($xxxx,a0) // for byte change anywhere
            (*dst & 0xff3f) == 0x4a38 || // tst.x ($xxxx.w); tas ($xxxx.w)
            (*dst & 0xc1ff) == 0x0038 || // move.x ($xxxx.w), dX
            (*dst & 0xf13f) == 0xb038)   // cmp.x ($xxxx.w), dX
        return 1;
      break;
    case 6:
      if ( ((dst[1] & 0xe0) == 0xe0 && ( // RAM and
            *dst == 0x4a39 ||            //   tst.b ($xxxxxxxx)
            *dst == 0x4a79 ||            //   tst.w ($xxxxxxxx)
            *dst == 0x4ab9 ||            //   tst.l ($xxxxxxxx)
            (*dst & 0xc1ff) == 0x0039 || //   move.x ($xxxxxxxx), dX
            (*dst & 0xf13f) == 0xb039))||//   cmp.x ($xxxxxxxx), dX
            *dst == 0x0838 ||            // btst $X, ($xxxx.w) [6 byte op]
            (*dst & 0xffbf) == 0x0c38)   // cmpi.{b,w} $X, ($xxxx.w)
        return 1;
      break;
    case 8:
      if ( ((dst[2] & 0xe0) == 0xe0 && ( // RAM and
            *dst == 0x0839 ||            //   btst $X, ($xxxxxxxx.w) [8 byte op]
            (*dst & 0xffbf) == 0x0c39))||//   cmpi.{b,w} $X, ($xxxxxxxx)
            *dst == 0x0cb8)              // cmpi.l $X, ($xxxx.w)
        return 1;
      break;
    case 12:
       if ((*dst & 0xf1f8) == 0x3010 && // move.w (aX), dX
            (dst[1]&0xf100) == 0x0000 && // arithmetic
            (dst[3]&0xf100) == 0x0000)   // arithmetic
        return 1;
      break;
  }

  return 0;
}

int SekRegisterIdlePatch(unsigned int pc, int oldop, int newop, void *ctx)
{
  int is_main68k = 1;
  u16 *target;
  uptr v;

#if   defined(EMU_C68K)
  struct Cyclone *cyc = ctx;
  is_main68k = cyc == &PicoCpuCM68k;
  pc -= cyc->membase;
#elif defined(EMU_F68K)
  is_main68k = ctx == &PicoCpuFM68k;
#endif
  pc &= ~0xff000000;
  elprintf(EL_IDLE, "idle: patch %06x %04x %04x %c %c #%i", pc, oldop, newop,
    (newop&0x200)?'n':'y', is_main68k?'m':'s', idledet_count);

  // XXX: probably shouldn't patch RAM too
  v = m68k_read16_map[pc >> M68K_MEM_SHIFT];
  if (!(v & 0x80000000))
    target = (u16 *)((v << 1) + pc);
  else {
    if (++idledet_bads > 128)
      return 2; // remove detector
    return 1; // don't patch
  }

  if (idledet_count >= 0x200 && (idledet_count & 0x1ff) == 0) {
    unsigned short **tmp = realloc(idledet_ptrs, (idledet_count+0x200)*4);
    if (tmp == NULL)
      return 1;
    idledet_ptrs = tmp;
  }

  idledet_ptrs[idledet_count++] = target;

  return 0;
}

void SekFinishIdleDet(void)
{
#ifdef EMU_C68K
  CycloneFinishIdle();
#endif
#ifdef EMU_F68K
  fm68k_emulate(0, 0, 2);
#endif
  while (idledet_count > 0)
  {
    unsigned short *op = idledet_ptrs[--idledet_count];
    if      ((*op & 0xfd00) == 0x7100)
      *op &= 0xff, *op |= 0x6600;
    else if ((*op & 0xfd00) == 0x7500)
      *op &= 0xff, *op |= 0x6700;
    else if ((*op & 0xfd00) == 0x7d00)
      *op &= 0xff, *op |= 0x6000;
    else
      elprintf(EL_STATUS|EL_IDLE, "idle: don't know how to restore %04x", *op);
  }
}


#if defined(EMU_M68K) && M68K_INSTRUCTION_HOOK == OPT_SPECIFY_HANDLER
static unsigned char op_flags[0x400000/2] = { 0, };
static int atexit_set = 0;

static void make_idc(void)
{
  FILE *f = fopen("idc.idc", "w");
  int i;
  if (!f) return;
  fprintf(f, "#include <idc.idc>\nstatic main() {\n");
  for (i = 0; i < 0x400000/2; i++)
    if (op_flags[i] != 0)
      fprintf(f, "  MakeCode(0x%06x);\n", i*2);
  fprintf(f, "}\n");
  fclose(f);
}

void instruction_hook(void)
{
  if (!atexit_set) {
    atexit(make_idc);
    atexit_set = 1;
  }
  if (REG_PC < 0x400000)
    op_flags[REG_PC/2] = 1;
}
#endif
