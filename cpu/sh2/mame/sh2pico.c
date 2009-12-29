#include "../sh2.h"

// MAME types
typedef signed char  INT8;
typedef signed short INT16;
typedef signed int   INT32;
typedef unsigned int   UINT32;
typedef unsigned short UINT16;
typedef unsigned char  UINT8;

#define RB(a) p32x_sh2_read8(a,sh2)
#define RW(a) p32x_sh2_read16(a,sh2)
#define RL(a) p32x_sh2_read32(a,sh2)
#define WB(a,d) p32x_sh2_write8(a,d,sh2)
#define WW(a,d) p32x_sh2_write16(a,d,sh2)
#define WL(a,d) p32x_sh2_write32(a,d,sh2)

// some stuff from sh2comn.h
#define T	0x00000001
#define S	0x00000002
#define I	0x000000f0
#define Q	0x00000100
#define M	0x00000200

#define AM	0xc7ffffff

#define FLAGS	(M|Q|I|S|T)

#define Rn	((opcode>>8)&15)
#define Rm	((opcode>>4)&15)

#define sh2_icount sh2->icount

#ifdef SH2_STATS
static SH2 sh2_stats;
static unsigned int op_refs[0x10000];
# define LRN  1
# define LRM  2
# define LRNM (LRN|LRM)
# define rlog(rnm) {   \
  int op = opcode;     \
  if ((rnm) & LRN) {   \
    op &= ~0x0f00;     \
    sh2_stats.r[Rn]++; \
  }                    \
  if ((rnm) & LRM) {   \
    op &= ~0x00f0;     \
    sh2_stats.r[Rm]++; \
  }                    \
  op_refs[op]++;       \
}
# define rlog1(x) sh2_stats.r[x]++
# define rlog2(x1,x2) sh2_stats.r[x1]++; sh2_stats.r[x2]++
#else
# define rlog(x)
# define rlog1(...)
# define rlog2(...)
#endif

#include "sh2.c"

#ifndef DRC_SH2

void sh2_execute(SH2 *sh2_, int cycles)
{
	sh2 = sh2_;
	sh2->cycles_aim += cycles;
	sh2->icount = cycles = sh2->cycles_aim - sh2->cycles_done;

	if (sh2->icount <= 0)
		return;

	do
	{
		UINT32 opcode;

		/* FIXME: Darxide doesn't like this */
		if (sh2->test_irq && !sh2->delay && sh2->pending_level > ((sh2->sr >> 4) & 0x0f))
		{
			int level = sh2->pending_level;
			int vector = sh2->irq_callback(sh2, level);
			sh2_do_irq(sh2, level, vector);
			sh2->test_irq = 0;
		}

		if (sh2->delay)
		{
			sh2->ppc = sh2->delay;
			opcode = RW(sh2->delay);
			sh2->pc -= 2;
		}
		else
		{
			sh2->ppc = sh2->pc;
			opcode = RW(sh2->pc);
		}

		sh2->delay = 0;
		sh2->pc += 2;

		switch (opcode & ( 15 << 12))
		{
		case  0<<12: op0000(opcode); break;
		case  1<<12: op0001(opcode); break;
		case  2<<12: op0010(opcode); break;
		case  3<<12: op0011(opcode); break;
		case  4<<12: op0100(opcode); break;
		case  5<<12: op0101(opcode); break;
		case  6<<12: op0110(opcode); break;
		case  7<<12: op0111(opcode); break;
		case  8<<12: op1000(opcode); break;
		case  9<<12: op1001(opcode); break;
		case 10<<12: op1010(opcode); break;
		case 11<<12: op1011(opcode); break;
		case 12<<12: op1100(opcode); break;
		case 13<<12: op1101(opcode); break;
		case 14<<12: op1110(opcode); break;
		default: op1111(opcode); break;
		}

		sh2->icount--;
	}
	while (sh2->icount > 0 || sh2->delay);	/* can't interrupt before delay */

	sh2->cycles_done += cycles - sh2->icount;
}

#else // DRC_SH2

#ifdef __i386__
#define REGPARM(x) __attribute__((regparm(x)))
#else
#define REGPARM(x)
#endif

// drc debug
void REGPARM(2) sh2_do_op(SH2 *sh2_, int opcode)
{
	sh2 = sh2_;
	sh2->pc += 2;

	switch (opcode & ( 15 << 12))
	{
		case  0<<12: op0000(opcode); break;
		case  1<<12: op0001(opcode); break;
		case  2<<12: op0010(opcode); break;
		case  3<<12: op0011(opcode); break;
		case  4<<12: op0100(opcode); break;
		case  5<<12: op0101(opcode); break;
		case  6<<12: op0110(opcode); break;
		case  7<<12: op0111(opcode); break;
		case  8<<12: op1000(opcode); break;
		case  9<<12: op1001(opcode); break;
		case 10<<12: op1010(opcode); break;
		case 11<<12: op1011(opcode); break;
		case 12<<12: op1100(opcode); break;
		case 13<<12: op1101(opcode); break;
		case 14<<12: op1110(opcode); break;
		default: op1111(opcode); break;
	}
}

#endif

#ifdef SH2_STATS
#include <stdio.h>
#include <string.h>
#include "sh2dasm.h"

void sh2_dump_stats(void)
{
	static const char *rnames[] = {
		"R0", "R1", "R2",  "R3",  "R4",  "R5",  "R6",  "R7",
		"R8", "R9", "R10", "R11", "R12", "R13", "R14", "SP",
		"PC", "", "PR", "SR", "GBR", "VBR", "MACH", "MACL"
	};
	long long total;
	char buff[64];
	int u, i;

	// dump reg usage
	total = 0;
	for (i = 0; i < 24; i++)
		total += sh2_stats.r[i];

	for (i = 0; i < 24; i++) {
		if (i == 16 || i == 17 || i == 19)
			continue;
		printf("r %6.3f%% %-4s %9d\n", (double)sh2_stats.r[i] * 100.0 / total,
			rnames[i], sh2_stats.r[i]);
	}

	memset(&sh2_stats, 0, sizeof(sh2_stats));

	// dump ops
	printf("\n");
	total = 0;
	for (i = 0; i < 0x10000; i++)
		total += op_refs[i];

	for (u = 0; u < 16; u++) {
		int max = 0, op = 0;
		for (i = 0; i < 0x10000; i++) {
			if (op_refs[i] > max) {
				max = op_refs[i];
				op = i;
			}
		}
		DasmSH2(buff, 0, op);
		printf("i %6.3f%% %9d %s\n", (double)op_refs[op] * 100.0 / total,
			op_refs[op], buff);
		op_refs[op] = 0;
	}
	memset(op_refs, 0, sizeof(op_refs));
}
#endif

