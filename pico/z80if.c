#include "pico_int.h"
#include "sound/sn76496.h"

#define Z80_MEM_SHIFT 13

unsigned long z80_read_map [0x10000 >> Z80_MEM_SHIFT];
unsigned long z80_write_map[0x10000 >> Z80_MEM_SHIFT];

void MEMH_FUNC z80_map_set(unsigned long *map, int start_addr, int end_addr,
    void *func_or_mh, int is_func)
{
  unsigned long addr = (unsigned long)func_or_mh;
  int mask = (1 << Z80_MEM_SHIFT) - 1;
  int i;

  if ((start_addr & mask) != 0 || (end_addr & mask) != mask)
    elprintf(EL_STATUS|EL_ANOMALY, "z80_map_set: tried to map bad range: %04x-%04x",
      start_addr, end_addr);

  if (addr & 1) {
    elprintf(EL_STATUS|EL_ANOMALY, "z80_map_set: ptr is not aligned: %08lx", addr);
    return;
  }

  for (i = start_addr >> Z80_MEM_SHIFT; i <= end_addr >> Z80_MEM_SHIFT; i++)
    if (is_func)
      map[i] = (addr >> 1) | (1 << (sizeof(addr) * 8 - 1));
    else
      map[i] = (addr - (i << Z80_MEM_SHIFT)) >> 1;
}

#ifdef _USE_MZ80

// memhandlers for mz80 core
unsigned char mz80_read(UINT32 a,  struct MemoryReadByte *w)  { return z80_read(a); }
void mz80_write(UINT32 a, UINT8 d, struct MemoryWriteByte *w) { z80_write(d, a); }

// structures for mz80 core
static struct MemoryReadByte mz80_mem_read[]=
{
  {0x0000,0xffff,mz80_read},
  {(UINT32) -1,(UINT32) -1,NULL}
};
static struct MemoryWriteByte mz80_mem_write[]=
{
  {0x0000,0xffff,mz80_write},
  {(UINT32) -1,(UINT32) -1,NULL}
};
static struct z80PortRead mz80_io_read[] ={
  {(UINT16) -1,(UINT16) -1,NULL}
};
static struct z80PortWrite mz80_io_write[]={
  {(UINT16) -1,(UINT16) -1,NULL}
};

int mz80_run(int cycles)
{
  int ticks_pre = mz80GetElapsedTicks(0);
  mz80exec(cycles);
  return mz80GetElapsedTicks(0) - ticks_pre;
}

#endif

#ifdef _USE_DRZ80
struct DrZ80 drZ80;
#endif


PICO_INTERNAL void z80_init(void)
{
#ifdef _USE_MZ80
  struct mz80context z80;

  // z80
  mz80init();
  // Modify the default context
  mz80GetContext(&z80);

  // point mz80 stuff
  z80.z80Base=Pico.zram;
  z80.z80MemRead=mz80_mem_read;
  z80.z80MemWrite=mz80_mem_write;
  z80.z80IoRead=mz80_io_read;
  z80.z80IoWrite=mz80_io_write;

  mz80SetContext(&z80);
#endif
#ifdef _USE_DRZ80
  memset(&drZ80, 0, sizeof(drZ80));
  drZ80.z80_rebasePC=NULL; // unused, handled by xmap
  drZ80.z80_rebaseSP=NULL;
  drZ80.z80_read8   =(void *)z80_read_map;
  drZ80.z80_read16  =NULL;
  drZ80.z80_write8  =(void *)z80_write_map;
  drZ80.z80_write16 =NULL;
  drZ80.z80_irq_callback=NULL;
#endif
#ifdef _USE_CZ80
  memset(&CZ80, 0, sizeof(CZ80));
  Cz80_Init(&CZ80);
  Cz80_Set_ReadB(&CZ80, NULL); // unused (hacked in)
  Cz80_Set_WriteB(&CZ80, NULL);
#endif
}

PICO_INTERNAL void z80_reset(void)
{
#ifdef _USE_MZ80
  mz80reset();
#endif
#ifdef _USE_DRZ80
  memset(&drZ80, 0, 0x54);
  drZ80.Z80F  = (1<<2);  // set ZFlag
  drZ80.Z80F2 = (1<<2);  // set ZFlag
  drZ80.Z80IX = 0xFFFF << 16;
  drZ80.Z80IY = 0xFFFF << 16;
  drZ80.Z80IM = 0; // 1?
  drZ80.z80irqvector = 0xff0000; // RST 38h
  drZ80.Z80PC_BASE = drZ80.Z80PC = z80_read_map[0] << 1;
  // drZ80 is locked in single bank
  drZ80.Z80SP_BASE = z80_read_map[0] << 1;
//  drZ80.Z80SP = drZ80.z80_rebaseSP(0x2000); // 0xf000 ?
#endif
#ifdef _USE_CZ80
  Cz80_Reset(&CZ80);
  Cz80_Set_Reg(&CZ80, CZ80_IX, 0xffff);
  Cz80_Set_Reg(&CZ80, CZ80_IY, 0xffff);
  Cz80_Set_Reg(&CZ80, CZ80_SP, 0x2000);
#endif
  Pico.m.z80_fakeval = 0; // for faking when Z80 is disabled
}

// XXX TODO: should better use universal z80 save format
PICO_INTERNAL void z80_pack(unsigned char *data)
{
#if defined(_USE_MZ80)
  struct mz80context mz80;
  *(int *)data = 0x00005A6D; // "mZ"
  mz80GetContext(&mz80);
  memcpy(data+4, &mz80.z80clockticks, sizeof(mz80)-5*4); // don't save base&memhandlers
#elif defined(_USE_DRZ80)
  *(int *)data = 0x015A7244; // "DrZ" v1
//  drZ80.Z80PC = drZ80.z80_rebasePC(drZ80.Z80PC-drZ80.Z80PC_BASE);
//  drZ80.Z80SP = drZ80.z80_rebaseSP(drZ80.Z80SP-drZ80.Z80SP_BASE);
  memcpy(data+4, &drZ80, 0x54);
#elif defined(_USE_CZ80)
  *(int *)data = 0x00007a43; // "Cz"
  *(int *)(data+4) = Cz80_Get_Reg(&CZ80, CZ80_PC);
  memcpy(data+8, &CZ80, (INT32)&CZ80.BasePC - (INT32)&CZ80);
#endif
}

PICO_INTERNAL void z80_unpack(unsigned char *data)
{
#if defined(_USE_MZ80)
  if (*(int *)data == 0x00005A6D) { // "mZ" save?
    struct mz80context mz80;
    mz80GetContext(&mz80);
    memcpy(&mz80.z80clockticks, data+4, sizeof(mz80)-5*4);
    mz80SetContext(&mz80);
  } else {
    z80_reset();
    z80_int();
  }
#elif defined(_USE_DRZ80)
  if (*(int *)data == 0x015A7244) { // "DrZ" v1 save?
    int pc, sp;
    memcpy(&drZ80, data+4, 0x54);
    pc = (drZ80.Z80PC - drZ80.Z80PC_BASE) & 0xffff;
    sp = (drZ80.Z80SP - drZ80.Z80SP_BASE) & 0xffff;
    // update bases
    drZ80.Z80PC_BASE = z80_read_map[pc >> Z80_MEM_SHIFT];
    if (drZ80.Z80PC & (1<<31)) {
      elprintf(EL_STATUS|EL_ANOMALY, "bad PC in z80 save: %04x", pc);
      drZ80.Z80PC_BASE = drZ80.Z80PC = z80_read_map[0];
    } else {
      drZ80.Z80PC_BASE <<= 1;
      drZ80.Z80PC = drZ80.Z80PC_BASE + pc;
    }
    drZ80.Z80SP_BASE = z80_read_map[sp >> Z80_MEM_SHIFT];
    if (drZ80.Z80SP & (1<<31)) {
      elprintf(EL_STATUS|EL_ANOMALY, "bad SP in z80 save: %04x", sp);
      drZ80.Z80SP_BASE = z80_read_map[0];
      drZ80.Z80SP = drZ80.Z80SP_BASE + (1 << Z80_MEM_SHIFT);
    } else {
      drZ80.Z80SP_BASE <<= 1;
      drZ80.Z80SP = drZ80.Z80SP_BASE + sp;
    }
  } else {
    z80_reset();
    drZ80.Z80IM = 1;
    z80_int(); // try to goto int handler, maybe we won't execute trash there?
  }
#elif defined(_USE_CZ80)
  if (*(int *)data == 0x00007a43) { // "Cz" save?
    memcpy(&CZ80, data+8, (INT32)&CZ80.BasePC - (INT32)&CZ80);
    Cz80_Set_Reg(&CZ80, CZ80_PC, *(int *)(data+4));
  } else {
    z80_reset();
    z80_int();
  }
#endif
}

PICO_INTERNAL void z80_exit(void)
{
#if defined(_USE_MZ80)
  mz80shutdown();
#endif
}

PICO_INTERNAL void z80_debug(char *dstr)
{
#if defined(_USE_DRZ80)
  sprintf(dstr, "Z80 state: PC: %04x SP: %04x\n", drZ80.Z80PC-drZ80.Z80PC_BASE, drZ80.Z80SP-drZ80.Z80SP_BASE);
#elif defined(_USE_CZ80)
  sprintf(dstr, "Z80 state: PC: %04x SP: %04x\n", CZ80.PC - CZ80.BasePC, CZ80.SP.W);
#endif
}
