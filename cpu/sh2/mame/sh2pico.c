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

#include "sh2.c"

#ifndef DRC_TMP

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

		if (sh2->test_irq && !sh2->delay && sh2->pending_level > ((sh2->sr >> 4) & 0x0f))
		{
			if (sh2->pending_irl > sh2->pending_int_irq)
				sh2_do_irq(sh2, sh2->pending_irl, 64 + sh2->pending_irl/2);
			else {
				sh2_do_irq(sh2, sh2->pending_int_irq, sh2->pending_int_vector);
				sh2->pending_int_irq = 0; // auto-clear
				sh2->pending_level = sh2->pending_irl;
			}
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

#else // DRC_TMP

// tmp
void __attribute__((regparm(2))) sh2_do_op(SH2 *sh2_, int opcode)
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

