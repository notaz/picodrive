// Pico Library - Internal Header File

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006,2007 Grazvydas "notaz" Ignotas, all rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#ifndef PICO_INTERNAL_INCLUDED
#define PICO_INTERNAL_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Pico.h"

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
	(((PicoMCD&1) && (PicoOpt & 0x2000)) ? (SekCycleAim-SekCycleCnt) : SekCyclesLeftNoMCD)
#define SekCyclesLeftS68k \
	((PicoOpt & 0x2000) ? (SekCycleAimS68k-SekCycleCntS68k) : PicoCpuCS68k.cycles)
#define SekSetCyclesLeftNoMCD(c) PicoCpuCM68k.cycles=c
#define SekSetCyclesLeft(c) { \
	if ((PicoMCD&1) && (PicoOpt & 0x2000)) SekCycleCnt=SekCycleAim-(c); else SekSetCyclesLeftNoMCD(c); \
}
#define SekPc (PicoCpuCM68k.pc-PicoCpuCM68k.membase)
#define SekPcS68k (PicoCpuCS68k.pc-PicoCpuCS68k.membase)
#define SekSetStop(x) { PicoCpuCM68k.state_flags&=~1; if (x) { PicoCpuCM68k.state_flags|=1; PicoCpuCM68k.cycles=0; } }
#define SekSetStopS68k(x) { PicoCpuCS68k.state_flags&=~1; if (x) { PicoCpuCS68k.state_flags|=1; PicoCpuCS68k.cycles=0; } }
#define SekIsStoppedS68k() (PicoCpuCS68k.state_flags&1)
#define SekShouldInterrupt (PicoCpuCM68k.irq > (PicoCpuCM68k.srh&7))

#define SekInterrupt(i) PicoCpuCM68k.irq=i

#ifdef EMU_M68K
#define EMU_CORE_DEBUG
#endif
#endif

#ifdef EMU_F68K
#include "../cpu/fame/fame.h"
extern M68K_CONTEXT PicoCpuFM68k, PicoCpuFS68k;
#define SekCyclesLeftNoMCD PicoCpuFM68k.io_cycle_counter
#define SekCyclesLeft \
	(((PicoMCD&1) && (PicoOpt & 0x2000)) ? (SekCycleAim-SekCycleCnt) : SekCyclesLeftNoMCD)
#define SekCyclesLeftS68k \
	((PicoOpt & 0x2000) ? (SekCycleAimS68k-SekCycleCntS68k) : PicoCpuFS68k.io_cycle_counter)
#define SekSetCyclesLeftNoMCD(c) PicoCpuFM68k.io_cycle_counter=c
#define SekSetCyclesLeft(c) { \
	if ((PicoMCD&1) && (PicoOpt & 0x2000)) SekCycleCnt=SekCycleAim-(c); else SekSetCyclesLeftNoMCD(c); \
}
#define SekPc     fm68k_get_pc(&PicoCpuFM68k)
#define SekPcS68k fm68k_get_pc(&PicoCpuFS68k)
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
	(((PicoMCD&1) && (PicoOpt & 0x2000)) ? (SekCycleAim-SekCycleCnt) : SekCyclesLeftNoMCD)
#define SekCyclesLeftS68k \
	((PicoOpt & 0x2000) ? (SekCycleAimS68k-SekCycleCntS68k) : PicoCpuMS68k.cyc_remaining_cycles)
#define SekSetCyclesLeftNoMCD(c) SET_CYCLES(c)
#define SekSetCyclesLeft(c) { \
	if ((PicoMCD&1) && (PicoOpt & 0x2000)) SekCycleCnt=SekCycleAim-(c); else SET_CYCLES(c); \
}
#define SekPc m68k_get_reg(&PicoCpuMM68k, M68K_REG_PC)
#define SekPcS68k m68k_get_reg(&PicoCpuMS68k, M68K_REG_PC)
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

#endif
#endif

extern int SekCycleCnt; // cycles done in this frame
extern int SekCycleAim; // cycle aim
extern unsigned int SekCycleCntT; // total cycle counter, updated once per frame

#define SekCyclesReset() { \
	SekCycleCntT+=SekCycleAim; \
	SekCycleCnt-=SekCycleAim; \
	SekCycleAim=0; \
}
#define SekCyclesBurn(c)  SekCycleCnt+=c
#define SekCyclesDone()  (SekCycleAim-SekCyclesLeft)    // nuber of cycles done in this frame (can be checked anywhere)
#define SekCyclesDoneT() (SekCycleCntT+SekCyclesDone()) // total nuber of cycles done for this rom

#define SekEndRun(after) { \
	SekCycleCnt -= SekCyclesLeft - after; \
	if(SekCycleCnt < 0) SekCycleCnt = 0; \
	SekSetCyclesLeft(after); \
}

extern int SekCycleCntS68k;
extern int SekCycleAimS68k;

#define SekCyclesResetS68k() { \
	SekCycleCntS68k-=SekCycleAimS68k; \
	SekCycleAimS68k=0; \
}
#define SekCyclesDoneS68k()  (SekCycleAimS68k-SekCyclesLeftS68k)

#ifdef EMU_CORE_DEBUG
#undef SekSetCyclesLeftNoMCD
#undef SekSetCyclesLeft
#undef SekCyclesBurn
#undef SekEndRun
#define SekSetCyclesLeftNoMCD(c)
#define SekSetCyclesLeft(c)
#define SekCyclesBurn(c) c
#define SekEndRun(c)
#endif

// ----------------------- Z80 CPU -----------------------

#if defined(_USE_MZ80)
#include "../../cpu/mz80/mz80.h"

#define z80_run(cycles)    mz80_run(cycles)
#define z80_run_nr(cycles) mz80_run(cycles)
#define z80_int()          mz80int(0)
#define z80_resetCycles()  mz80GetElapsedTicks(1)

#elif defined(_USE_DRZ80)
#include "../../cpu/DrZ80/drz80.h"

extern struct DrZ80 drZ80;

#define z80_run(cycles)    ((cycles) - DrZ80Run(&drZ80, cycles))
#define z80_run_nr(cycles) DrZ80Run(&drZ80, cycles)
#define z80_int() { \
  drZ80.z80irqvector = 0xFF; /* default IRQ vector RST opcode */ \
  drZ80.Z80_IRQ = 1; \
}
#define z80_resetCycles()

#elif defined(_USE_CZ80)
#include "../../cpu/cz80/cz80.h"

#define z80_run(cycles)    Cz80_Exec(&CZ80, cycles)
#define z80_run_nr(cycles) Cz80_Exec(&CZ80, cycles)
#define z80_int()          Cz80_Set_IRQ(&CZ80, 0, HOLD_LINE)
#define z80_resetCycles()

#else

#define z80_run(cycles)    (cycles)
#define z80_run_nr(cycles)
#define z80_int()
#define z80_resetCycles()

#endif

// ---------------------------------------------------------

extern int PicoMCD;

// main oscillator clock which controls timing
#define OSC_NTSC 53693100
// seems to be accurate, see scans from http://www.hot.ee/tmeeco/
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
  unsigned char pad[0x12];
};

struct PicoMisc
{
  unsigned char rotate;
  unsigned char z80Run;
  unsigned char padTHPhase[2]; // 02 phase of gamepad TH switches
  short scanline;              // 04 0 to 261||311; -1 in fast mode
  char dirtyPal;               // 06 Is the palette dirty (1 - change @ this frame, 2 - some time before)
  unsigned char hardware;      // 07 Hardware value for country
  unsigned char pal;           // 08 1=PAL 0=NTSC
  unsigned char sram_reg;      // SRAM mode register. bit0: allow read? bit1: deny write? bit2: EEPROM? bit4: detected? (header or by access)
  unsigned short z80_bank68k;  // 0a
  unsigned short z80_lastaddr; // this is for Z80 faking
  unsigned char  z80_fakeval;
  unsigned char  pad0;
  unsigned char  padDelay[2];  // 10 gamepad phase time outs, so we count a delay
  unsigned short eeprom_addr;  // EEPROM address register
  unsigned char  eeprom_cycle; // EEPROM SRAM cycle number
  unsigned char  eeprom_slave; // EEPROM slave word for X24C02 and better SRAMs
  unsigned char prot_bytes[2]; // simple protection faking
  unsigned short dma_xfers;
  unsigned char pad[2];
  unsigned int  frame_count; // mainly for movies
};

// some assembly stuff depend on these, do not touch!
struct Pico
{
  unsigned char ram[0x10000];  // 0x00000 scratch ram
  unsigned short vram[0x8000]; // 0x10000
  unsigned char zram[0x2000];  // 0x20000 Z80 ram
  unsigned char ioports[0x10];
  unsigned int pad[0x3c];      // unused
  unsigned short cram[0x40];   // 0x22100
  unsigned short vsram[0x40];  // 0x22180

  unsigned char *rom;          // 0x22200
  unsigned int romsize;        // 0x22204

  struct PicoMisc m;
  struct PicoVideo video;
};

// sram
struct PicoSRAM
{
  unsigned char *data;		// actual data
  unsigned int start;		// start address in 68k address space
  unsigned int end;
  unsigned char unused1;	// 0c: unused
  unsigned char unused2;
  unsigned char changed;
  unsigned char eeprom_type;    // eeprom type: 0: 7bit (24C01), 2: device with 2 addr words (X24C02+), 3: dev with 3 addr words
  unsigned char eeprom_abits;	// eeprom access must be odd addr for: bit0 ~ cl, bit1 ~ out
  unsigned char eeprom_bit_cl;	// bit number for cl
  unsigned char eeprom_bit_in;  // bit number for in
  unsigned char eeprom_bit_out; // bit number for out
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
	unsigned int   state_flags;	// 04: emu state: reset_pending, dmna_pending
	unsigned int   counter75hz;
	unsigned short audio_offset;	// 0c: for savestates: play pointer offset (0-1023)
	unsigned char  audio_track;	// playing audio track # (zero based)
	char           pad1;
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
			unsigned char unused[0x20000];
		};
		struct {
			unsigned char unused[0x20000];
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

#define Pico_mcd ((mcd_state *)Pico.rom)

// Area.c
PICO_INTERNAL int PicoAreaPackCpu(unsigned char *cpu, int is_sub);
PICO_INTERNAL int PicoAreaUnpackCpu(unsigned char *cpu, int is_sub);

// cd/Area.c
PICO_INTERNAL int PicoCdSaveState(void *file);
PICO_INTERNAL int PicoCdLoadState(void *file);

// Cart.c
PICO_INTERNAL void PicoCartDetect(void);

// Debug.c
int CM_compareRun(int cyc, int is_sub);

// Draw.c
PICO_INTERNAL int PicoLine(int scan);
PICO_INTERNAL void PicoFrameStart(void);

// Draw2.c
PICO_INTERNAL void PicoFrameFull();

// Memory.c
PICO_INTERNAL int PicoInitPc(unsigned int pc);
PICO_INTERNAL_ASM unsigned int PicoRead32(unsigned int a);
PICO_INTERNAL void PicoMemSetup(void);
PICO_INTERNAL_ASM void PicoMemReset(void);
PICO_INTERNAL int PadRead(int i);
PICO_INTERNAL unsigned char z80_read(unsigned short a);
#ifndef _USE_CZ80
PICO_INTERNAL_ASM void z80_write(unsigned char data, unsigned short a);
PICO_INTERNAL void z80_write16(unsigned short data, unsigned short a);
PICO_INTERNAL unsigned short z80_read16(unsigned short a);
#else
PICO_INTERNAL_ASM void z80_write(unsigned int a, unsigned char data);
#endif

// cd/Memory.c
PICO_INTERNAL void PicoMemSetupCD(void);
PICO_INTERNAL_ASM void PicoMemResetCD(int r3);
PICO_INTERNAL_ASM void PicoMemResetCDdecode(int r3);

// Pico.c
extern struct Pico Pico;
extern struct PicoSRAM SRam;
extern int emustatus;
extern int z80startCycle, z80stopCycle; // in 68k cycles
PICO_INTERNAL int CheckDMA(void);

// cd/Pico.c
PICO_INTERNAL int  PicoInitMCD(void);
PICO_INTERNAL void PicoExitMCD(void);
PICO_INTERNAL int PicoResetMCD(int hard);
PICO_INTERNAL int PicoFrameMCD(void);

// Sek.c
PICO_INTERNAL int SekInit(void);
PICO_INTERNAL int SekReset(void);
PICO_INTERNAL void SekState(int *data);
PICO_INTERNAL void SekSetRealTAS(int use_real);

// cd/Sek.c
PICO_INTERNAL int SekInitS68k(void);
PICO_INTERNAL int SekResetS68k(void);
PICO_INTERNAL int SekInterruptS68k(int irq);

// sound/sound.c
extern int PsndLen_exc_cnt;
extern int PsndLen_exc_add;

// VideoPort.c
PICO_INTERNAL_ASM void PicoVideoWrite(unsigned int a,unsigned short d);
PICO_INTERNAL_ASM unsigned int PicoVideoRead(unsigned int a);

// Misc.c
PICO_INTERNAL void SRAMWriteEEPROM(unsigned int d);
PICO_INTERNAL void SRAMUpdPending(unsigned int a, unsigned int d);
PICO_INTERNAL_ASM unsigned int SRAMReadEEPROM(void);
PICO_INTERNAL_ASM void memcpy16(unsigned short *dest, unsigned short *src, int count);
PICO_INTERNAL_ASM void memcpy16bswap(unsigned short *dest, void *src, int count);
PICO_INTERNAL_ASM void memcpy32(int *dest, int *src, int count); // 32bit word count
PICO_INTERNAL_ASM void memset32(int *dest, int c, int count);

// cd/Misc.c
PICO_INTERNAL_ASM void wram_2M_to_1M(unsigned char *m);
PICO_INTERNAL_ASM void wram_1M_to_2M(unsigned char *m);

// cd/buffering.c
PICO_INTERNAL void PicoCDBufferRead(void *dest, int lba);

// sound/sound.c
PICO_INTERNAL void PsndReset(void);
PICO_INTERNAL void Psnd_timers_and_dac(int raster);
PICO_INTERNAL int  PsndRender(int offset, int length);
PICO_INTERNAL void PsndClear(void);
// z80 functionality wrappers
PICO_INTERNAL void z80_init(void);
PICO_INTERNAL void z80_pack(unsigned char *data);
PICO_INTERNAL void z80_unpack(unsigned char *data);
PICO_INTERNAL void z80_reset(void);
PICO_INTERNAL void z80_exit(void);


#ifdef __cplusplus
} // End of extern "C"
#endif

// emulation event logging
#ifndef EL_LOGMASK
#define EL_LOGMASK 0
#endif

#define EL_HVCNT   0x0001 /* hv counter reads */
#define EL_SR      0x0002 /* SR reads */
#define EL_INTS    0x0004 /* ints and acks */
#define EL_YM2612R 0x0008 /* 68k ym2612 reads */
#define EL_INTSW   0x0010 /* log irq switching on/off */
#define EL_ASVDP   0x0020 /* VDP accesses during active scan */
#define EL_VDPDMA  0x0040 /* VDP DMA transfers and their timing */
#define EL_BUSREQ  0x0080 /* z80 busreq r/w or reset w */
#define EL_Z80BNK  0x0100 /* z80 i/o through bank area */
#define EL_SRAMIO  0x0200 /* sram i/o */
#define EL_EEPROM  0x0400 /* eeprom debug */
#define EL_UIO     0x0800 /* unmapped i/o */
#define EL_IO      0x1000 /* all i/o */
#define EL_CDPOLL  0x2000 /* MCD: log poll detection */

#define EL_STATUS  0x4000 /* status messages */
#define EL_ANOMALY 0x8000 /* some unexpected conditions (during emulation) */

#if EL_LOGMASK
extern void lprintf(const char *fmt, ...);
#define elprintf(w,f,...) \
{ \
	if ((w) & EL_LOGMASK) \
		lprintf("%05i:%03i: " f "\n",Pico.m.frame_count,Pico.m.scanline,##__VA_ARGS__); \
}
#else
#define elprintf(w,f,...)
#endif

#endif // PICO_INTERNAL_INCLUDED

