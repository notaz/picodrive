// Pico Library - Header File

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Pico.h"


// to select core, define EMU_C68K, EMU_M68K or EMU_A68K in your makefile or project

#ifdef __cplusplus
extern "C" {
#endif


// ----------------------- 68000 CPU -----------------------
#ifdef EMU_C68K
#include "../cpu/Cyclone/Cyclone.h"
extern struct Cyclone PicoCpu, PicoCpuS68k;
#define SekCyclesLeftNoMCD PicoCpu.cycles // cycles left for this run
#define SekCyclesLeft \
	(((PicoMCD&1) && (PicoOpt & 0x2000)) ? (SekCycleAim-SekCycleCnt) : SekCyclesLeftNoMCD)
#define SekSetCyclesLeftNoMCD(c) PicoCpu.cycles=c
#define SekSetCyclesLeft(c) { \
	if ((PicoMCD&1) && (PicoOpt & 0x2000)) SekCycleCnt=SekCycleAim-(c); else SekSetCyclesLeftNoMCD(c); \
}
#define SekPc (PicoCpu.pc-PicoCpu.membase)
#define SekPcS68k (PicoCpuS68k.pc-PicoCpuS68k.membase)
#endif

#ifdef EMU_A68K
void __cdecl M68000_RUN();
// The format of the data in a68k.asm (at the _M68000_regs location)
struct A68KContext
{
  unsigned int d[8],a[8];
  unsigned int isp,srh,ccr,xc,pc,irq,sr;
  int (*IrqCallback) (int nIrq);
  unsigned int ppc;
  void *pResetCallback;
  unsigned int sfc,dfc,usp,vbr;
  unsigned int AsmBank,CpuVersion;
};
struct A68KContext M68000_regs;
extern int m68k_ICount;
#define SekCyclesLeft m68k_ICount
#define SekSetCyclesLeft(c) m68k_ICount=c
#define SekPc M68000_regs.pc
#endif

#ifdef EMU_M68K
#include "../cpu/musashi/m68kcpu.h"
extern m68ki_cpu_core PicoM68kCPU; // MD's CPU
extern m68ki_cpu_core PicoS68kCPU; // Mega CD's CPU
#ifndef SekCyclesLeft
#define SekCyclesLeftNoMCD m68k_cycles_remaining()
#define SekCyclesLeft \
	(((PicoMCD&1) && (PicoOpt & 0x2000)) ? (SekCycleAim-SekCycleCnt) : SekCyclesLeftNoMCD)
#define SekSetCyclesLeftNoMCD(c) SET_CYCLES(c)
#define SekSetCyclesLeft(c) { \
	if ((PicoMCD&1) && (PicoOpt & 0x2000)) SekCycleCnt=SekCycleAim-(c); else SET_CYCLES(c); \
}
#define SekPc m68k_get_reg(&PicoM68kCPU, M68K_REG_PC)
#define SekPcS68k m68k_get_reg(&PicoS68kCPU, M68K_REG_PC)
#endif
#endif

extern int SekCycleCnt; // cycles done in this frame
extern int SekCycleAim; // cycle aim
extern unsigned int SekCycleCntT; // total cycle counter, updated once per frame

#define SekCyclesReset() {SekCycleCntT+=SekCycleCnt;SekCycleCnt=SekCycleAim=0;}
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

#define SekCyclesResetS68k() {SekCycleCntS68k=SekCycleAimS68k=0;}

// does not work as expected
//extern int z80ExtraCycles; // extra z80 cycles, used when z80 is [en|dis]abled

extern int PicoMCD;

// ---------------------------------------------------------

// main oscillator clock which controls timing
#define OSC_NTSC 53693100
#define OSC_PAL  53203424 // not accurate

struct PicoVideo
{
  unsigned char reg[0x20];
  unsigned int command; // 32-bit Command
  unsigned char pending; // 1 if waiting for second half of 32-bit command
  unsigned char type; // Command type (v/c/vsram read/write)
  unsigned short addr; // Read/Write address
  int status; // Status bits
  unsigned char pending_ints; // pending interrupts: ??VH????
  unsigned char pad[0x13];
};

struct PicoMisc
{
  unsigned char rotate;
  unsigned char z80Run;
  unsigned char padTHPhase[2]; // phase of gamepad TH switches
  short scanline; // 0 to 261||311; -1 in fast mode
  char dirtyPal; // Is the palette dirty (1 - change @ this frame, 2 - some time before)
  unsigned char hardware; // Hardware value for country
  unsigned char pal; // 1=PAL 0=NTSC
  unsigned char sram_reg; // SRAM mode register. bit0: allow read? bit1: deny write? bit2: EEPROM?
  unsigned short z80_bank68k;
  unsigned short z80_lastaddr; // this is for Z80 faking
  unsigned char  z80_fakeval;
  unsigned char  pad0;
  unsigned char  padDelay[2];  // gamepad phase time outs, so we count a delay
  unsigned short sram_addr;  // EEPROM address register
  unsigned char sram_cycle;  // EEPROM SRAM cycle number
  unsigned char sram_slave;  // EEPROM slave word for X24C02 and better SRAMs
  unsigned char prot_bytes[2]; // simple protection fakeing
  unsigned short dma_bytes;  //
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
  unsigned char resize;		// 0c: 1=SRAM size changed and needs to be reallocated on PicoReset
  unsigned char reg_back;	// copy of Pico.m.sram_reg to set after reset
  unsigned char changed;
  unsigned char pad;
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
	unsigned int   state_flags;	// 04: emu state: reset_pending,
	unsigned int   counter75hz;
	unsigned short audio_offset;	// 0c: for savestates: play pointer offset (0-1023)
	unsigned char  audio_track;	// playing audio track # (zero based)
	char pad1;
	int            timer_int3;	// 10
	unsigned int   timer_stopwatch;
	int pad[10];
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
int PicoAreaPackCpu(unsigned char *cpu, int is_sub);
int PicoAreaUnpackCpu(unsigned char *cpu, int is_sub);

// cd/Area.c
int PicoCdSaveState(void *file);
int PicoCdLoadState(void *file);
int PicoCdLoadStateGfx(void *file);

// Draw.c
int PicoLine(int scan);
void PicoFrameStart();

// Draw2.c
void PicoFrameFull();

// Memory.c
int PicoInitPc(unsigned int pc);
unsigned int CPU_CALL PicoRead32(unsigned int a);
void PicoMemSetup();
void PicoMemReset();
//void PicoDasm(int start,int len);
unsigned char z80_read(unsigned short a);
unsigned short z80_read16(unsigned short a);
void z80_write(unsigned char data, unsigned short a);
void z80_write16(unsigned short data, unsigned short a);

// cd/Memory.c
void PicoMemSetupCD(void);
void PicoMemResetCD(int r3);
void PicoMemResetCDdecode(int r3);
unsigned char  PicoReadCD8 (unsigned int a);
unsigned short PicoReadCD16(unsigned int a);
unsigned int   PicoReadCD32(unsigned int a);
void PicoWriteCD8 (unsigned int a, unsigned char d);
void PicoWriteCD16(unsigned int a, unsigned short d);
void PicoWriteCD32(unsigned int a, unsigned int d);

// Pico.c
extern struct Pico Pico;
extern struct PicoSRAM SRam;
extern int emustatus;
int CheckDMA(void);

// cd/Pico.c
int  PicoInitMCD(void);
void PicoExitMCD(void);
int  PicoResetMCD(int hard);

// Sek.c
int SekInit(void);
int SekReset(void);
int SekInterrupt(int irq);
void SekState(unsigned char *data);

// cd/Sek.c
int SekInitS68k(void);
int SekResetS68k(void);
int SekInterruptS68k(int irq);

// sound/sound.c
extern int PsndLen_exc_cnt;
extern int PsndLen_exc_add;

// VideoPort.c
void PicoVideoWrite(unsigned int a,unsigned short d);
unsigned int PicoVideoRead(unsigned int a);

// Misc.c
void SRAMWriteEEPROM(unsigned int d);
unsigned int SRAMReadEEPROM();
void SRAMUpdPending(unsigned int a, unsigned int d);
void memcpy16(unsigned short *dest, unsigned short *src, int count);
void memcpy16bswap(unsigned short *dest, void *src, int count);
void memcpy32(int *dest, int *src, int count);
void memset32(int *dest, int c, int count);

// cd/Misc.c
void wram_2M_to_1M(unsigned char *m);
void wram_1M_to_2M(unsigned char *m);


#ifdef __cplusplus
} // End of extern "C"
#endif
