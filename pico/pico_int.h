// Pico Library - Internal Header File

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006-2009 Grazvydas "notaz" Ignotas, all rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#ifndef PICO_INTERNAL_INCLUDED
#define PICO_INTERNAL_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico.h"
#include "carthw/carthw.h"

//
#define USE_POLL_DETECT

#ifndef PICO_INTERNAL
#define PICO_INTERNAL
#endif
#ifndef PICO_INTERNAL_ASM
#define PICO_INTERNAL_ASM
#endif

// to select core, define EMU_C68K, EMU_M68K or EMU_F68K in your makefile or project

#ifdef __cplusplus
extern "C" {
#endif


// ----------------------- 68000 CPU -----------------------
#ifdef EMU_C68K
#include "../cpu/Cyclone/Cyclone.h"
extern struct Cyclone PicoCpuCM68k, PicoCpuCS68k;
#define SekCyclesLeftNoMCD PicoCpuCM68k.cycles // cycles left for this run
#define SekCyclesLeft \
	(((PicoAHW&1) && (PicoOpt & POPT_EN_MCD_PSYNC)) ? (SekCycleAim-SekCycleCnt) : SekCyclesLeftNoMCD)
#define SekCyclesLeftS68k \
	((PicoOpt & POPT_EN_MCD_PSYNC) ? (SekCycleAimS68k-SekCycleCntS68k) : PicoCpuCS68k.cycles)
#define SekEndTimeslice(after) PicoCpuCM68k.cycles=after
#define SekEndTimesliceS68k(after) PicoCpuCS68k.cycles=after
#define SekPc (PicoCpuCM68k.pc-PicoCpuCM68k.membase)
#define SekPcS68k (PicoCpuCS68k.pc-PicoCpuCS68k.membase)
#define SekDar(x) PicoCpuCM68k.d[x]
#define SekSr     CycloneGetSr(&PicoCpuCM68k)
#define SekSetStop(x) { PicoCpuCM68k.state_flags&=~1; if (x) { PicoCpuCM68k.state_flags|=1; PicoCpuCM68k.cycles=0; } }
#define SekSetStopS68k(x) { PicoCpuCS68k.state_flags&=~1; if (x) { PicoCpuCS68k.state_flags|=1; PicoCpuCS68k.cycles=0; } }
#define SekIsStoppedS68k() (PicoCpuCS68k.state_flags&1)
#define SekShouldInterrupt (PicoCpuCM68k.irq > (PicoCpuCM68k.srh&7))

#define SekInterrupt(i) PicoCpuCM68k.irq=i
#define SekIrqLevel     PicoCpuCM68k.irq

#ifdef EMU_M68K
#define EMU_CORE_DEBUG
#endif
#endif

#ifdef EMU_F68K
#include "../cpu/fame/fame.h"
extern M68K_CONTEXT PicoCpuFM68k, PicoCpuFS68k;
#define SekCyclesLeftNoMCD PicoCpuFM68k.io_cycle_counter
#define SekCyclesLeft \
	(((PicoAHW&1) && (PicoOpt & POPT_EN_MCD_PSYNC)) ? (SekCycleAim-SekCycleCnt) : SekCyclesLeftNoMCD)
#define SekCyclesLeftS68k \
	((PicoOpt & POPT_EN_MCD_PSYNC) ? (SekCycleAimS68k-SekCycleCntS68k) : PicoCpuFS68k.io_cycle_counter)
#define SekEndTimeslice(after) PicoCpuFM68k.io_cycle_counter=after
#define SekEndTimesliceS68k(after) PicoCpuFS68k.io_cycle_counter=after
#define SekPc     fm68k_get_pc(&PicoCpuFM68k)
#define SekPcS68k fm68k_get_pc(&PicoCpuFS68k)
#define SekDar(x) PicoCpuFM68k.dreg[x].D
#define SekSr     PicoCpuFM68k.sr
#define SekSetStop(x) { \
	PicoCpuFM68k.execinfo &= ~FM68K_HALTED; \
	if (x) { PicoCpuFM68k.execinfo |= FM68K_HALTED; PicoCpuFM68k.io_cycle_counter = 0; } \
}
#define SekSetStopS68k(x) { \
	PicoCpuFS68k.execinfo &= ~FM68K_HALTED; \
	if (x) { PicoCpuFS68k.execinfo |= FM68K_HALTED; PicoCpuFS68k.io_cycle_counter = 0; } \
}
#define SekIsStoppedS68k() (PicoCpuFS68k.execinfo&FM68K_HALTED)
#define SekShouldInterrupt fm68k_would_interrupt()

#define SekInterrupt(irq) PicoCpuFM68k.interrupts[0]=irq
#define SekIrqLevel       PicoCpuFM68k.interrupts[0]

#ifdef EMU_M68K
#define EMU_CORE_DEBUG
#endif
#endif

#ifdef EMU_M68K
#include "../cpu/musashi/m68kcpu.h"
extern m68ki_cpu_core PicoCpuMM68k, PicoCpuMS68k;
#ifndef SekCyclesLeft
#define SekCyclesLeftNoMCD PicoCpuMM68k.cyc_remaining_cycles
#define SekCyclesLeft \
	(((PicoAHW&1) && (PicoOpt & POPT_EN_MCD_PSYNC)) ? (SekCycleAim-SekCycleCnt) : SekCyclesLeftNoMCD)
#define SekCyclesLeftS68k \
	((PicoOpt & POPT_EN_MCD_PSYNC) ? (SekCycleAimS68k-SekCycleCntS68k) : PicoCpuMS68k.cyc_remaining_cycles)
#define SekEndTimeslice(after) SET_CYCLES(after)
#define SekEndTimesliceS68k(after) PicoCpuMS68k.cyc_remaining_cycles=after
#define SekPc m68k_get_reg(&PicoCpuMM68k, M68K_REG_PC)
#define SekPcS68k m68k_get_reg(&PicoCpuMS68k, M68K_REG_PC)
#define SekDar(x) PicoCpuMM68k.dar[x]
#define SekSr m68k_get_reg(&PicoCpuMM68k, M68K_REG_SR)
#define SekSetStop(x) { \
	if(x) { SET_CYCLES(0); PicoCpuMM68k.stopped=STOP_LEVEL_STOP; } \
	else PicoCpuMM68k.stopped=0; \
}
#define SekSetStopS68k(x) { \
	if(x) { SET_CYCLES(0); PicoCpuMS68k.stopped=STOP_LEVEL_STOP; } \
	else PicoCpuMS68k.stopped=0; \
}
#define SekIsStoppedS68k() (PicoCpuMS68k.stopped==STOP_LEVEL_STOP)
#define SekShouldInterrupt (CPU_INT_LEVEL > FLAG_INT_MASK)

#define SekInterrupt(irq) { \
	void *oldcontext = m68ki_cpu_p; \
	m68k_set_context(&PicoCpuMM68k); \
	m68k_set_irq(irq); \
	m68k_set_context(oldcontext); \
}
#define SekIrqLevel (PicoCpuMM68k.int_level >> 8)

#endif
#endif // EMU_M68K

extern int SekCycleCnt; // cycles done in this frame
extern int SekCycleAim; // cycle aim
extern unsigned int SekCycleCntT; // total cycle counter, updated once per frame

#define SekCyclesReset() { \
	SekCycleCntT+=SekCycleAim; \
	SekCycleCnt-=SekCycleAim; \
	SekCycleAim=0; \
}
#define SekCyclesBurn(c)  SekCycleCnt+=c
#define SekCyclesDone()  (SekCycleAim-SekCyclesLeft)    // number of cycles done in this frame (can be checked anywhere)
#define SekCyclesDoneT() (SekCycleCntT+SekCyclesDone()) // total nuber of cycles done for this rom

#define SekEndRun(after) { \
	SekCycleCnt -= SekCyclesLeft - (after); \
	if (SekCycleCnt < 0) SekCycleCnt = 0; \
	SekEndTimeslice(after); \
}

#define SekEndRunS68k(after) { \
	SekCycleCntS68k -= SekCyclesLeftS68k - (after); \
	if (SekCycleCntS68k < 0) SekCycleCntS68k = 0; \
	SekEndTimesliceS68k(after); \
}

extern int SekCycleCntS68k;
extern int SekCycleAimS68k;

#define SekCyclesResetS68k() { \
	SekCycleCntS68k-=SekCycleAimS68k; \
	SekCycleAimS68k=0; \
}
#define SekCyclesDoneS68k()  (SekCycleAimS68k-SekCyclesLeftS68k)

#ifdef EMU_CORE_DEBUG
extern int dbg_irq_level;
#undef SekEndTimeslice
#undef SekCyclesBurn
#undef SekEndRun
#undef SekInterrupt
#define SekEndTimeslice(c)
#define SekCyclesBurn(c) c
#define SekEndRun(c)
#define SekInterrupt(irq) dbg_irq_level=irq
#endif

// ----------------------- Z80 CPU -----------------------

#if defined(_USE_MZ80)
#include "../cpu/mz80/mz80.h"

#define z80_run(cycles)    { mz80GetElapsedTicks(1); mz80_run(cycles) }
#define z80_run_nr(cycles) mz80_run(cycles)
#define z80_int()          mz80int(0)

#elif defined(_USE_DRZ80)
#include "../cpu/DrZ80/drz80.h"

extern struct DrZ80 drZ80;

#define z80_run(cycles)    ((cycles) - DrZ80Run(&drZ80, cycles))
#define z80_run_nr(cycles) DrZ80Run(&drZ80, cycles)
#define z80_int()          drZ80.Z80_IRQ = 1

#define z80_cyclesLeft     drZ80.cycles
#define z80_pc()           (drZ80.Z80PC - drZ80.Z80PC_BASE)

#elif defined(_USE_CZ80)
#include "../cpu/cz80/cz80.h"

#define z80_run(cycles)    Cz80_Exec(&CZ80, cycles)
#define z80_run_nr(cycles) Cz80_Exec(&CZ80, cycles)
#define z80_int()          Cz80_Set_IRQ(&CZ80, 0, HOLD_LINE)

#define z80_cyclesLeft     (CZ80.ICount - CZ80.ExtraCycles)
#define z80_pc()           Cz80_Get_Reg(&CZ80, CZ80_PC)

#else

#define z80_run(cycles)    (cycles)
#define z80_run_nr(cycles)
#define z80_int()

#endif

extern int z80stopCycle;         /* in 68k cycles */
extern int z80_cycle_cnt;        /* 'done' z80 cycles before z80_run() */
extern int z80_cycle_aim;
extern int z80_scanline;
extern int z80_scanline_cycles;  /* cycles done until z80_scanline */

#define z80_resetCycles() \
  z80_cycle_cnt = z80_cycle_aim = z80_scanline = z80_scanline_cycles = 0;

#define z80_cyclesDone() \
  (z80_cycle_aim - z80_cyclesLeft)

#define cycles_68k_to_z80(x) ((x)*957 >> 11)

// ----------------------- SH2 CPU -----------------------

#include "cpu/sh2/sh2.h"

extern SH2 sh2s[2];
#define msh2 sh2s[0]
#define ssh2 sh2s[1]

#ifndef DRC_SH2
# define ash2_end_run(after) if (sh2->icount > (after)) sh2->icount = after
# define ash2_cycles_done() (sh2->cycles_aim - sh2->icount)
#else
# define ash2_end_run(after) { \
   if ((sh2->sr >> 12) > (after)) \
     { sh2->sr &= 0xfff; sh2->sr |= (after) << 12; } \
}
# define ash2_cycles_done() (sh2->cycles_aim - (sh2->sr >> 12))
#endif

//#define sh2_pc(c)     (c) ? ssh2.ppc : msh2.ppc
#define sh2_pc(c)     (c) ? ssh2.pc : msh2.pc
#define sh2_reg(c, x) (c) ? ssh2.r[x] : msh2.r[x]
#define sh2_gbr(c)    (c) ? ssh2.gbr : msh2.gbr
#define sh2_vbr(c)    (c) ? ssh2.vbr : msh2.vbr
#define sh2_sr(c)   (((c) ? ssh2.sr : msh2.sr) & 0xfff)

#define sh2_set_gbr(c, v) \
  { if (c) ssh2.gbr = v; else msh2.gbr = v; }
#define sh2_set_vbr(c, v) \
  { if (c) ssh2.vbr = v; else msh2.vbr = v; }

// ---------------------------------------------------------

// main oscillator clock which controls timing
#define OSC_NTSC 53693100
#define OSC_PAL  53203424

struct PicoVideo
{
  unsigned char reg[0x20];
  unsigned int command;       // 32-bit Command
  unsigned char pending;      // 1 if waiting for second half of 32-bit command
  unsigned char type;         // Command type (v/c/vsram read/write)
  unsigned short addr;        // Read/Write address
  int status;                 // Status bits
  unsigned char pending_ints; // pending interrupts: ??VH????
  signed char lwrite_cnt;     // VDP write count during active display line
  unsigned short v_counter;   // V-counter
  unsigned char pad[0x10];
};

struct PicoMisc
{
  unsigned char rotate;
  unsigned char z80Run;
  unsigned char padTHPhase[2]; // 02 phase of gamepad TH switches
  unsigned short scanline;     // 04 0 to 261||311
  char dirtyPal;               // 06 Is the palette dirty (1 - change @ this frame, 2 - some time before)
  unsigned char hardware;      // 07 Hardware value for country
  unsigned char pal;           // 08 1=PAL 0=NTSC
  unsigned char sram_reg;      // 09 SRAM reg. See SRR_* below
  unsigned short z80_bank68k;  // 0a
  unsigned short pad0;
  unsigned char  pad1;
  unsigned char  z80_reset;    // 0f z80 reset held
  unsigned char  padDelay[2];  // 10 gamepad phase time outs, so we count a delay
  unsigned short eeprom_addr;  // EEPROM address register
  unsigned char  eeprom_cycle; // EEPROM cycle number
  unsigned char  eeprom_slave; // EEPROM slave word for X24C02 and better SRAMs
  unsigned char  eeprom_status;
  unsigned char  pad2;
  unsigned short dma_xfers;    // 18
  unsigned char  eeprom_wb[2]; // EEPROM latch/write buffer
  unsigned int  frame_count;   // 1c for movies and idle det
};

// some assembly stuff depend on these, do not touch!
struct Pico
{
  unsigned char ram[0x10000];  // 0x00000 scratch ram
  union {                      // vram is byteswapped for easier reads when drawing
    unsigned short vram[0x8000];  // 0x10000
    unsigned char  vramb[0x4000]; // VRAM in SMS mode
  };
  unsigned char zram[0x2000];  // 0x20000 Z80 ram
  unsigned char ioports[0x10];
  unsigned char sms_io_ctl;
  unsigned char pad[0xef];     // unused
  unsigned short cram[0x40];   // 0x22100
  unsigned short vsram[0x40];  // 0x22180

  unsigned char *rom;          // 0x22200
  unsigned int romsize;        // 0x22204

  struct PicoMisc m;
  struct PicoVideo video;
};

// sram
#define SRR_MAPPED   (1 << 0)
#define SRR_READONLY (1 << 1)

#define SRF_ENABLED  (1 << 0)
#define SRF_EEPROM   (1 << 1)

struct PicoSRAM
{
  unsigned char *data;		// actual data
  unsigned int start;		// start address in 68k address space
  unsigned int end;
  unsigned char flags;		// 0c: SRF_*
  unsigned char unused2;
  unsigned char changed;
  unsigned char eeprom_type;    // eeprom type: 0: 7bit (24C01), 2: 2 addr words (X24C02+), 3: 3 addr words
  unsigned char unused3;
  unsigned char eeprom_bit_cl;	// bit number for cl
  unsigned char eeprom_bit_in;  // bit number for in
  unsigned char eeprom_bit_out; // bit number for out
  unsigned int size;
};

// MCD
#include "cd/cd_sys.h"
#include "cd/LC89510.h"
#include "cd/gfx_cd.h"

struct mcd_pcm
{
	unsigned char control; // reg7
	unsigned char enabled; // reg8
	unsigned char cur_ch;
	unsigned char bank;
	int pad1;

	struct pcm_chan			// 08, size 0x10
	{
		unsigned char regs[8];
		unsigned int  addr;	// .08: played sample address
		int pad;
	} ch[8];
};

struct mcd_misc
{
	unsigned short hint_vector;
	unsigned char  busreq;
	unsigned char  s68k_pend_ints;
	unsigned int   state_flags;	// 04: emu state: reset_pending
	unsigned int   counter75hz;
	unsigned int   pad0;
	int            timer_int3;	// 10
	unsigned int   timer_stopwatch;
	unsigned char  bcram_reg;	// 18: battery-backed RAM cart register
	unsigned char  pad2;
	unsigned short pad3;
	int pad[9];
};

typedef struct
{
	unsigned char bios[0x20000];			// 000000: 128K
	union {						// 020000: 512K
		unsigned char prg_ram[0x80000];
		unsigned char prg_ram_b[4][0x20000];
	};
	union {						// 0a0000: 256K
		struct {
			unsigned char word_ram2M[0x40000];
			unsigned char unused0[0x20000];
		};
		struct {
			unsigned char unused1[0x20000];
			unsigned char word_ram1M[2][0x20000];
		};
	};
	union {						// 100000: 64K
		unsigned char pcm_ram[0x10000];
		unsigned char pcm_ram_b[0x10][0x1000];
	};
	unsigned char s68k_regs[0x200];			// 110000: GA, not CPU regs
	unsigned char bram[0x2000];			// 110200: 8K
	struct mcd_misc m;				// 112200: misc
	struct mcd_pcm pcm;				// 112240:
	_scd_toc TOC;					// not to be saved
	CDD  cdd;
	CDC  cdc;
	_scd scd;
	Rot_Comp rot_comp;
} mcd_state;

// XXX: this will need to be reworked for cart+cd support.
#define Pico_mcd ((mcd_state *)Pico.rom)

// 32X
#define P32XS_FM    (1<<15)
#define P32XS_REN   (1<< 7)
#define P32XS_nRES  (1<< 1)
#define P32XS_ADEN  (1<< 0)
#define P32XS2_ADEN (1<< 9)
#define P32XS_FULL  (1<< 7) // DREQ FIFO full
#define P32XS_68S   (1<< 2)
#define P32XS_DMA   (1<< 1)
#define P32XS_RV    (1<< 0)

#define P32XV_nPAL  (1<<15) // VDP
#define P32XV_PRI   (1<< 7)
#define P32XV_Mx    (3<< 0) // display mode mask

#define P32XV_VBLK  (1<<15)
#define P32XV_HBLK  (1<<14)
#define P32XV_PEN   (1<<13)
#define P32XV_nFEN  (1<< 1)
#define P32XV_FS    (1<< 0)

#define P32XP_FULL  (1<<15) // PWM
#define P32XP_EMPTY (1<<14)

#define P32XF_68KPOLL   (1 << 0)
#define P32XF_MSH2POLL  (1 << 1)
#define P32XF_SSH2POLL  (1 << 2)
#define P32XF_68KVPOLL  (1 << 3)
#define P32XF_MSH2VPOLL (1 << 4)
#define P32XF_SSH2VPOLL (1 << 5)

#define P32XI_VRES (1 << 14/2) // IRL/2
#define P32XI_VINT (1 << 12/2)
#define P32XI_HINT (1 << 10/2)
#define P32XI_CMD  (1 <<  8/2)
#define P32XI_PWM  (1 <<  6/2)

// peripheral reg access
#define PREG8(regs,offs) ((unsigned char *)regs)[offs ^ 3]

// real one is 4*2, but we use more because we don't lockstep
#define DMAC_FIFO_LEN (4*4)
#define PWM_BUFF_LEN 1024 // in one channel samples

#define SH2_DRCBLK_RAM_SHIFT 1
#define SH2_DRCBLK_DA_SHIFT  1

struct Pico32x
{
  unsigned short regs[0x20];
  unsigned short vdp_regs[0x10];
  unsigned short sh2_regs[3];
  unsigned char pending_fb;
  unsigned char dirty_pal;
  unsigned int emu_flags;
  unsigned char sh2irq_mask[2];
  unsigned char sh2irqi[2];      // individual
  unsigned int sh2irqs;          // common irqs
  unsigned short dmac_fifo[DMAC_FIFO_LEN];
  unsigned int dmac_ptr;
  unsigned int pwm_irq_sample_cnt;
};

struct Pico32xMem
{
  unsigned char  sdram[0x40000];
#ifdef DRC_SH2
  unsigned short drcblk_ram[1 << (18 - SH2_DRCBLK_RAM_SHIFT)];
#endif
  unsigned short dram[2][0x20000/2];    // AKA fb
  unsigned char  m68k_rom[0x10000];     // 0x100; using M68K_BANK_SIZE
  unsigned char  data_array[2][0x1000]; // cache in SH2s (can be used as RAM)
#ifdef DRC_SH2
  unsigned short drcblk_da[2][1 << (12 - SH2_DRCBLK_DA_SHIFT)];
#endif
  unsigned char  sh2_rom_m[0x800];
  unsigned char  sh2_rom_s[0x400];
  unsigned short pal[0x100];
  unsigned short pal_native[0x100];     // converted to native (for renderer)
  unsigned int   sh2_peri_regs[2][0x200/4]; // periphereal regs of SH2s
  signed short   pwm[2*PWM_BUFF_LEN];   // PWM buffer for current frame
};

// area.c
PICO_INTERNAL void PicoAreaPackCpu(unsigned char *cpu, int is_sub);
PICO_INTERNAL void PicoAreaUnpackCpu(unsigned char *cpu, int is_sub);
extern void (*PicoLoadStateHook)(void);

// cd/area.c
PICO_INTERNAL int PicoCdSaveState(void *file);
PICO_INTERNAL int PicoCdLoadState(void *file);

typedef struct {
	int chunk;
	int size;
	void *ptr;
} carthw_state_chunk;
extern carthw_state_chunk *carthw_chunks;
#define CHUNK_CARTHW 64

// area.c
typedef size_t (arearw)(void *p, size_t _size, size_t _n, void *file);
typedef size_t (areaeof)(void *file);
typedef int    (areaseek)(void *file, long offset, int whence);
typedef int    (areaclose)(void *file);
extern arearw  *areaRead;  // external read and write function pointers for
extern arearw  *areaWrite; // gzip save state ability
extern areaeof *areaEof;
extern areaseek *areaSeek;
extern areaclose *areaClose;

// cart.c
void Byteswap(void *dst, const void *src, int len);
extern void (*PicoCartMemSetup)(void);
extern void (*PicoCartUnloadHook)(void);

// debug.c
int CM_compareRun(int cyc, int is_sub);

// draw.c
PICO_INTERNAL void PicoFrameStart(void);
void PicoDrawSync(int to, int blank_last_line);
void BackFill(int reg7, int sh);
void FinalizeLineRGB555(int sh, int line);
extern int DrawScanline;
#define MAX_LINE_SPRITES 29
extern unsigned char HighLnSpr[240][3 + MAX_LINE_SPRITES];

// draw2.c
PICO_INTERNAL void PicoFrameFull();

// mode4.c
void PicoFrameStartMode4(void);
void PicoLineMode4(int line);
void PicoDoHighPal555M4(void);
void PicoDrawSetColorFormatMode4(int which);

// memory.c
PICO_INTERNAL void PicoMemSetup(void);
unsigned int PicoRead8_io(unsigned int a);
unsigned int PicoRead16_io(unsigned int a);
void PicoWrite8_io(unsigned int a, unsigned int d);
void PicoWrite16_io(unsigned int a, unsigned int d);

// pico/memory.c
PICO_INTERNAL void PicoMemSetupPico(void);

// cd/memory.c
PICO_INTERNAL void PicoMemSetupCD(void);
void PicoMemStateLoaded(void);

// pico.c
extern struct Pico Pico;
extern struct PicoSRAM SRam;
extern int PicoPadInt[2];
extern int emustatus;
extern int scanlines_total;
extern void (*PicoResetHook)(void);
extern void (*PicoLineHook)(void);
PICO_INTERNAL int  CheckDMA(void);
PICO_INTERNAL void PicoDetectRegion(void);
PICO_INTERNAL void PicoSyncZ80(int m68k_cycles_done);

// cd/pico.c
PICO_INTERNAL void PicoInitMCD(void);
PICO_INTERNAL void PicoExitMCD(void);
PICO_INTERNAL void PicoPowerMCD(void);
PICO_INTERNAL int  PicoResetMCD(void);
PICO_INTERNAL void PicoFrameMCD(void);

// pico/pico.c
PICO_INTERNAL void PicoInitPico(void);
PICO_INTERNAL void PicoReratePico(void);

// pico/xpcm.c
PICO_INTERNAL void PicoPicoPCMUpdate(short *buffer, int length, int stereo);
PICO_INTERNAL void PicoPicoPCMReset(void);
PICO_INTERNAL void PicoPicoPCMRerate(int xpcm_rate);

// sek.c
PICO_INTERNAL void SekInit(void);
PICO_INTERNAL int  SekReset(void);
PICO_INTERNAL void SekState(int *data);
PICO_INTERNAL void SekSetRealTAS(int use_real);
void SekStepM68k(void);
void SekInitIdleDet(void);
void SekFinishIdleDet(void);

// cd/sek.c
PICO_INTERNAL void SekInitS68k(void);
PICO_INTERNAL int  SekResetS68k(void);
PICO_INTERNAL int  SekInterruptS68k(int irq);

// sound/sound.c
PICO_INTERNAL void cdda_start_play();
extern short cdda_out_buffer[2*1152];
extern int PsndLen_exc_cnt;
extern int PsndLen_exc_add;
extern int timer_a_next_oflow, timer_a_step; // in z80 cycles
extern int timer_b_next_oflow, timer_b_step;

void ym2612_sync_timers(int z80_cycles, int mode_old, int mode_new);
void ym2612_pack_state(void);
void ym2612_unpack_state(void);

#define TIMER_NO_OFLOW 0x70000000
// tA =   72 * (1024 - NA) / M
#define TIMER_A_TICK_ZCYCLES  17203
// tB = 1152 * (256 - NA) / M
#define TIMER_B_TICK_ZCYCLES 262800 // 275251 broken, see Dai Makaimura

#define timers_cycle() \
  if (timer_a_next_oflow > 0 && timer_a_next_oflow < TIMER_NO_OFLOW) \
    timer_a_next_oflow -= Pico.m.pal ? 70938*256 : 59659*256; \
  if (timer_b_next_oflow > 0 && timer_b_next_oflow < TIMER_NO_OFLOW) \
    timer_b_next_oflow -= Pico.m.pal ? 70938*256 : 59659*256; \
  ym2612_sync_timers(0, ym2612.OPN.ST.mode, ym2612.OPN.ST.mode);

#define timers_reset() \
  timer_a_next_oflow = timer_b_next_oflow = TIMER_NO_OFLOW; \
  timer_a_step = TIMER_A_TICK_ZCYCLES * 1024; \
  timer_b_step = TIMER_B_TICK_ZCYCLES * 256;


// videoport.c
PICO_INTERNAL_ASM void PicoVideoWrite(unsigned int a,unsigned short d);
PICO_INTERNAL_ASM unsigned int PicoVideoRead(unsigned int a);
PICO_INTERNAL_ASM unsigned int PicoVideoRead8(unsigned int a);
extern int (*PicoDmaHook)(unsigned int source, int len, unsigned short **srcp, unsigned short **limitp);

// misc.c
PICO_INTERNAL_ASM void memcpy16(unsigned short *dest, unsigned short *src, int count);
PICO_INTERNAL_ASM void memcpy16bswap(unsigned short *dest, void *src, int count);
PICO_INTERNAL_ASM void memcpy32(int *dest, int *src, int count); // 32bit word count
PICO_INTERNAL_ASM void memset32(int *dest, int c, int count);

// eeprom.c
void EEPROM_write8(unsigned int a, unsigned int d);
void EEPROM_write16(unsigned int d);
unsigned int EEPROM_read(void);

// z80 functionality wrappers
PICO_INTERNAL void z80_init(void);
PICO_INTERNAL void z80_pack(unsigned char *data);
PICO_INTERNAL void z80_unpack(unsigned char *data);
PICO_INTERNAL void z80_reset(void);
PICO_INTERNAL void z80_exit(void);

// cd/misc.c
PICO_INTERNAL_ASM void wram_2M_to_1M(unsigned char *m);
PICO_INTERNAL_ASM void wram_1M_to_2M(unsigned char *m);

// cd/buffering.c
PICO_INTERNAL void PicoCDBufferRead(void *dest, int lba);

// sound/sound.c
PICO_INTERNAL void PsndReset(void);
PICO_INTERNAL void PsndDoDAC(int line_to);
PICO_INTERNAL void PsndClear(void);
PICO_INTERNAL void PsndGetSamples(int y);
PICO_INTERNAL void PsndGetSamplesMS(void);
extern int PsndDacLine;

// sms.c
void PicoPowerMS(void);
void PicoResetMS(void);
void PicoMemSetupMS(void);
void PicoFrameMS(void);
void PicoFrameDrawOnlyMS(void);

// 32x/32x.c
extern struct Pico32x Pico32x;
void Pico32xInit(void);
void PicoPower32x(void);
void PicoReset32x(void);
void Pico32xStartup(void);
void PicoUnload32x(void);
void PicoFrame32x(void);
void p32x_update_irls(void);
void p32x_reset_sh2s(void);

// 32x/memory.c
struct Pico32xMem *Pico32xMem;
unsigned int PicoRead8_32x(unsigned int a);
unsigned int PicoRead16_32x(unsigned int a);
void PicoWrite8_32x(unsigned int a, unsigned int d);
void PicoWrite16_32x(unsigned int a, unsigned int d);
void PicoMemSetup32x(void);
void Pico32xSwapDRAM(int b);
void p32x_poll_event(int cpu_mask, int is_vdp);

// 32x/draw.c
void FinalizeLine32xRGB555(int sh, int line);

// 32x/pwm.c
unsigned int p32x_pwm_read16(unsigned int a);
void p32x_pwm_write16(unsigned int a, unsigned int d);
void p32x_pwm_update(int *buf32, int length, int stereo);
void p32x_timers_do(int new_line);
void p32x_timers_recalc(void);
extern int pwm_frame_smp_cnt;

/* avoid dependency on newer glibc */
static __inline int isspace_(int c)
{
	return (0x09 <= c && c <= 0x0d) || c == ' ';
}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

// emulation event logging
#ifndef EL_LOGMASK
#define EL_LOGMASK 0
#endif

#define EL_HVCNT   0x00000001 /* hv counter reads */
#define EL_SR      0x00000002 /* SR reads */
#define EL_INTS    0x00000004 /* ints and acks */
#define EL_YMTIMER 0x00000008 /* ym2612 timer stuff */
#define EL_INTSW   0x00000010 /* log irq switching on/off */
#define EL_ASVDP   0x00000020 /* VDP accesses during active scan */
#define EL_VDPDMA  0x00000040 /* VDP DMA transfers and their timing */
#define EL_BUSREQ  0x00000080 /* z80 busreq r/w or reset w */
#define EL_Z80BNK  0x00000100 /* z80 i/o through bank area */
#define EL_SRAMIO  0x00000200 /* sram i/o */
#define EL_EEPROM  0x00000400 /* eeprom debug */
#define EL_UIO     0x00000800 /* unmapped i/o */
#define EL_IO      0x00001000 /* all i/o */
#define EL_CDPOLL  0x00002000 /* MCD: log poll detection */
#define EL_SVP     0x00004000 /* SVP stuff */
#define EL_PICOHW  0x00008000 /* Pico stuff */
#define EL_IDLE    0x00010000 /* idle loop det. */
#define EL_CDREGS  0x00020000 /* MCD: register access */
#define EL_CDREG3  0x00040000 /* MCD: register 3 only */
#define EL_32X     0x00080000
#define EL_PWM     0x00100000 /* 32X PWM stuff (LOTS of output) */

#define EL_STATUS  0x40000000 /* status messages */
#define EL_ANOMALY 0x80000000 /* some unexpected conditions (during emulation) */

#if EL_LOGMASK
extern void lprintf(const char *fmt, ...);
#define elprintf(w,f,...) \
{ \
	if ((w) & EL_LOGMASK) \
		lprintf("%05i:%03i: " f "\n",Pico.m.frame_count,Pico.m.scanline,##__VA_ARGS__); \
}
#elif defined(_MSC_VER)
#define elprintf
#else
#define elprintf(w,f,...)
#endif

#ifdef _MSC_VER
#define cdprintf
#else
#define cdprintf(x...)
#endif

#ifdef __i386__
#define REGPARM(x) __attribute__((regparm(x)))
#else
#define REGPARM(x)
#endif

#ifdef __GNUC__
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

#ifdef __cplusplus
} // End of extern "C"
#endif

#endif // PICO_INTERNAL_INCLUDED

