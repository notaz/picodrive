// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "pico_int.h"
#include <zlib/zlib.h>

// ym2612
#include "sound/ym2612.h"

// sn76496
extern int *sn76496_regs;


struct PicoArea { void *data; int len; char *name; };

// strange observation on Symbian OS 9.1, m600 organizer fw r3a06:
// taking an address of fread or fwrite causes "application could't be started" error
// on startup randomly depending on binary layout of executable file.

arearw    *areaRead  = (arearw *) 0; // fread;  // read and write function pointers for
arearw    *areaWrite = (arearw *) 0; // fwrite; // gzip save state ability
areaeof   *areaEof   = (areaeof *) 0;
areaseek  *areaSeek  = (areaseek *) 0;
areaclose *areaClose = (areaclose *) 0;

void (*PicoLoadStateHook)(void) = NULL;


// Scan one variable and callback
static int ScanVar(void *data,int len,char *name,void *PmovFile,int is_write)
{
  int ret = 0;
  if (is_write)
    ret = areaWrite(data,1,len,PmovFile);
  else
    ret = areaRead (data,1,len,PmovFile);
  return (ret != len);
}

#define SCAN_VAR(x,y) ScanVar(&x,sizeof(x),y,PmovFile,is_write);
#define SCANP(x)      ScanVar(&Pico.x,sizeof(Pico.x),#x,PmovFile,is_write);

// Pack the cpu into a common format:
PICO_INTERNAL void PicoAreaPackCpu(unsigned char *cpu, int is_sub)
{
  unsigned int pc=0;

#if defined(EMU_C68K)
  struct Cyclone *context = is_sub ? &PicoCpuCS68k : &PicoCpuCM68k;
  memcpy(cpu,context->d,0x40);
  pc=context->pc-context->membase;
  *(unsigned int *)(cpu+0x44)=CycloneGetSr(context);
  *(unsigned int *)(cpu+0x48)=context->osp;
  cpu[0x4c] = context->irq;
  cpu[0x4d] = context->state_flags & 1;
#elif defined(EMU_M68K)
  void *oldcontext = m68ki_cpu_p;
  m68k_set_context(is_sub ? &PicoCpuMS68k : &PicoCpuMM68k);
  memcpy(cpu,m68ki_cpu_p->dar,0x40);
  pc=m68ki_cpu_p->pc;
  *(unsigned int  *)(cpu+0x44)=m68k_get_reg(NULL, M68K_REG_SR);
  *(unsigned int  *)(cpu+0x48)=m68ki_cpu_p->sp[m68ki_cpu_p->s_flag^SFLAG_SET];
  cpu[0x4c] = CPU_INT_LEVEL>>8;
  cpu[0x4d] = CPU_STOPPED;
  m68k_set_context(oldcontext);
#elif defined(EMU_F68K)
  M68K_CONTEXT *context = is_sub ? &PicoCpuFS68k : &PicoCpuFM68k;
  memcpy(cpu,context->dreg,0x40);
  pc=context->pc;
  *(unsigned int  *)(cpu+0x44)=context->sr;
  *(unsigned int  *)(cpu+0x48)=context->asp;
  cpu[0x4c] = context->interrupts[0];
  cpu[0x4d] = (context->execinfo & FM68K_HALTED) ? 1 : 0;
#endif

  *(unsigned int *)(cpu+0x40)=pc;
}

PICO_INTERNAL void PicoAreaUnpackCpu(unsigned char *cpu, int is_sub)
{
#if defined(EMU_C68K)
  struct Cyclone *context = is_sub ? &PicoCpuCS68k : &PicoCpuCM68k;
  CycloneSetSr(context, *(unsigned int *)(cpu+0x44));
  context->osp=*(unsigned int *)(cpu+0x48);
  memcpy(context->d,cpu,0x40);
  context->membase=0;
  context->pc = context->checkpc(*(unsigned int *)(cpu+0x40)); // Base pc
  context->irq = cpu[0x4c];
  context->state_flags = 0;
  if (cpu[0x4d])
    context->state_flags |= 1;
#elif defined(EMU_M68K)
  void *oldcontext = m68ki_cpu_p;
  m68k_set_context(is_sub ? &PicoCpuMS68k : &PicoCpuMM68k);
  m68k_set_reg(M68K_REG_SR, *(unsigned int *)(cpu+0x44));
  memcpy(m68ki_cpu_p->dar,cpu,0x40);
  m68ki_cpu_p->pc=*(unsigned int *)(cpu+0x40);
  m68ki_cpu_p->sp[m68ki_cpu_p->s_flag^SFLAG_SET]=*(unsigned int *)(cpu+0x48);
  CPU_INT_LEVEL = cpu[0x4c] << 8;
  CPU_STOPPED = cpu[0x4d];
  m68k_set_context(oldcontext);
#elif defined(EMU_F68K)
  M68K_CONTEXT *context = is_sub ? &PicoCpuFS68k : &PicoCpuFM68k;
  memcpy(context->dreg,cpu,0x40);
  context->pc =*(unsigned int *)(cpu+0x40);
  context->sr =*(unsigned int *)(cpu+0x44);
  context->asp=*(unsigned int *)(cpu+0x48);
  context->interrupts[0] = cpu[0x4c];
  context->execinfo &= ~FM68K_HALTED;
  if (cpu[0x4d]&1) context->execinfo |= FM68K_HALTED;
#endif
}

// Scan the contents of the virtual machine's memory for saving or loading
static int PicoAreaScan(int is_write, unsigned int ver, void *PmovFile)
{
  void *ym2612_regs;
  unsigned char cpu[0x60];
  unsigned char cpu_z80[0x60];
  int ret;

  memset(&cpu,0,sizeof(cpu));
  memset(&cpu_z80,0,sizeof(cpu_z80));
  Pico.m.scanline=0;

  ym2612_regs = YM2612GetRegs();

  // Scan all the memory areas:
  SCANP(ram) SCANP(vram) SCANP(zram) SCANP(cram) SCANP(vsram)

  // Pack, scan and unpack the cpu data:
  if (is_write)
    PicoAreaPackCpu(cpu, 0);
  SCAN_VAR(cpu,"cpu")
  if (!is_write)
    PicoAreaUnpackCpu(cpu, 0);

  SCAN_VAR(Pico.m    ,"misc")
  SCAN_VAR(Pico.video,"video")

  // no longer keeping eeprom data in sram_reg
  if (!is_write && (Pico.m.sram_reg & 4))
    Pico.m.sram_reg = SRR_MAPPED;

  if (is_write)
    z80_pack(cpu_z80);
  ret = SCAN_VAR(cpu_z80,"cpu_z80")
  // do not unpack if we fail to load z80 state
  if (!is_write) {
    if (ret) z80_reset();
    else     z80_unpack(cpu_z80);
  }

  ScanVar(sn76496_regs, 28*4, "SN76496state", PmovFile, is_write);
  if (is_write)
    ym2612_pack_state();
  ret = ScanVar(ym2612_regs, 0x200+4, "YM2612state", PmovFile, is_write); // regs + addr line
  if (!is_write && !ret)
    ym2612_unpack_state();

  return 0;
}

// ---------------------------------------------------------------------------
// Helper code to save/load to a file handle

// XXX: error checking
// Save or load the state from PmovFile:
static int PmovState(int is_write, void *PmovFile)
{
  int minimum=0;
  unsigned char head[32];

  if ((PicoAHW & PAHW_MCD) || carthw_chunks != NULL)
  {
    if (is_write)
      return PicoCdSaveState(PmovFile);
    else {
      int ret = PicoCdLoadState(PmovFile);
      if (PicoLoadStateHook) PicoLoadStateHook();
      return ret;
    }
  }

  memset(head,0,sizeof(head));

  // Find out minimal compatible version:
  minimum = 0x0021;

  memcpy(head,"Pico",4);
  *(unsigned int *)(head+0x8)=PicoVer;
  *(unsigned int *)(head+0xc)=minimum;

  // Scan header:
  if (is_write)
    areaWrite(head,1,sizeof(head),PmovFile);
  else
    areaRead (head,1,sizeof(head),PmovFile);

  // Scan memory areas:
  PicoAreaScan(is_write, *(unsigned int *)(head+0x8), PmovFile);

  if (!is_write && PicoLoadStateHook)
    PicoLoadStateHook();

  return 0;
}

static size_t gzRead2(void *p, size_t _size, size_t _n, void *file)
{
  return gzread(file, p, _n);
}

static size_t gzWrite2(void *p, size_t _size, size_t _n, void *file)
{
  return gzwrite(file, p, _n);
}

static void set_cbs(int gz)
{
  if (gz) {
    areaRead  = gzRead2;
    areaWrite = gzWrite2;
    areaEof   = (areaeof *) gzeof;
    areaSeek  = (areaseek *) gzseek;
    areaClose = (areaclose *) gzclose;
  } else {
    areaRead  = (arearw *) fread;
    areaWrite = (arearw *) fwrite;
    areaEof   = (areaeof *) feof;
    areaSeek  = (areaseek *) fseek;
    areaClose = (areaclose *) fclose;
  }
}

int PicoState(const char *fname, int is_save)
{
  void *afile = NULL;
  int ret;

  if (strcmp(fname + strlen(fname) - 3, ".gz") == 0)
  {
    if ( (afile = gzopen(fname, is_save ? "wb" : "rb")) ) {
      set_cbs(1);
      if (is_save)
        gzsetparams(afile, 9, Z_DEFAULT_STRATEGY);
    }
  }
  else
  {
    if ( (afile = fopen(fname, is_save ? "wb" : "rb")) ) {
      set_cbs(0);
    }
  }

  if (afile == NULL)
    return -1;

  ret = PmovState(is_save, afile);
  areaClose(afile);
  if (!is_save)
    Pico.m.dirtyPal=1;

  return ret;
}

int PicoStateLoadVDP(const char *fname)
{
  void *afile = NULL;
  if (strcmp(fname + strlen(fname) - 3, ".gz") == 0)
  {
    if ( (afile = gzopen(fname, "rb")) )
      set_cbs(1);
  }
  else
  {
    if ( (afile = fopen(fname, "rb")) )
      set_cbs(0);
  }
  if (afile == NULL)
    return -1;

  if ((PicoAHW & PAHW_MCD) || carthw_chunks != NULL) {
    PicoCdLoadStateGfx(afile);
  } else {
    areaSeek(afile, 0x10020, SEEK_SET);  // skip header and RAM in state file
    areaRead(Pico.vram, 1, sizeof(Pico.vram), afile);
    areaSeek(afile, 0x2000, SEEK_CUR);
    areaRead(Pico.cram, 1, sizeof(Pico.cram), afile);
    areaRead(Pico.vsram, 1, sizeof(Pico.vsram), afile);
    areaSeek(afile, 0x221a0, SEEK_SET);
    areaRead(&Pico.video, 1, sizeof(Pico.video), afile);
  }
  areaClose(afile);
  return 0;
}

