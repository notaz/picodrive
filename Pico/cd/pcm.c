// Emulation routines for the RF5C164 PCM chip.
// Based on Gens code by Stéphane Dallongeville
// (c) Copyright 2007, Grazvydas "notaz" Ignotas

#include "../PicoInt.h"
#include "pcm.h"

static unsigned int g_rate = 0; // 18.14 fixed point

PICO_INTERNAL_ASM void pcm_write(unsigned int a, unsigned int d)
{
//printf("pcm_write(%i, %02x)\n", a, d);

	if (a < 7)
	{
		Pico_mcd->pcm.ch[Pico_mcd->pcm.cur_ch].regs[a] = d;
	}
	else if (a == 7) // control register
	{
		if (d & 0x40)	Pico_mcd->pcm.cur_ch = d & 7;
		else		Pico_mcd->pcm.bank = d & 0xf;
		Pico_mcd->pcm.control = d;
		// dprintf("pcm control=%02x", Pico_mcd->pcm.control);
	}
	else if (a == 8) // sound on/off
	{
		if (!(Pico_mcd->pcm.enabled & 0x01)) Pico_mcd->pcm.ch[0].addr =
			Pico_mcd->pcm.ch[0].regs[6] << (PCM_STEP_SHIFT + 8);
		if (!(Pico_mcd->pcm.enabled & 0x02)) Pico_mcd->pcm.ch[1].addr =
			Pico_mcd->pcm.ch[1].regs[6] << (PCM_STEP_SHIFT + 8);
		if (!(Pico_mcd->pcm.enabled & 0x04)) Pico_mcd->pcm.ch[2].addr =
			Pico_mcd->pcm.ch[2].regs[6] << (PCM_STEP_SHIFT + 8);
		if (!(Pico_mcd->pcm.enabled & 0x08)) Pico_mcd->pcm.ch[3].addr =
			Pico_mcd->pcm.ch[3].regs[6] << (PCM_STEP_SHIFT + 8);
		if (!(Pico_mcd->pcm.enabled & 0x10)) Pico_mcd->pcm.ch[4].addr =
			Pico_mcd->pcm.ch[4].regs[6] << (PCM_STEP_SHIFT + 8);
		if (!(Pico_mcd->pcm.enabled & 0x20)) Pico_mcd->pcm.ch[5].addr =
			Pico_mcd->pcm.ch[5].regs[6] << (PCM_STEP_SHIFT + 8);
		if (!(Pico_mcd->pcm.enabled & 0x40)) Pico_mcd->pcm.ch[6].addr =
			Pico_mcd->pcm.ch[6].regs[6] << (PCM_STEP_SHIFT + 8);
		if (!(Pico_mcd->pcm.enabled & 0x80)) Pico_mcd->pcm.ch[7].addr =
			Pico_mcd->pcm.ch[7].regs[6] << (PCM_STEP_SHIFT + 8);
//		printf("addr %x %x %x %x %x %x %x %x\n", Pico_mcd->pcm.ch[0].addr, Pico_mcd->pcm.ch[1].addr
//		, Pico_mcd->pcm.ch[2].addr, Pico_mcd->pcm.ch[3].addr, Pico_mcd->pcm.ch[4].addr, Pico_mcd->pcm.ch[5].addr
//		, Pico_mcd->pcm.ch[6].addr, Pico_mcd->pcm.ch[7].addr);

		Pico_mcd->pcm.enabled = ~d;
//printf("enabled=%02x\n", Pico_mcd->pcm.enabled);
	}
}


PICO_INTERNAL void pcm_set_rate(int rate)
{
	float step = 31.8 * 1024.0 / (float) rate; // max <4 @ 8000Hz
	step *= 256*256/4;
	g_rate = (unsigned int) step;
	if (step - (float) g_rate >= 0.5) g_rate++;
	elprintf(EL_STATUS, "g_rate: %f %08x\n", (double)step, g_rate);
}


PICO_INTERNAL void pcm_update(int *buffer, int length, int stereo)
{
	struct pcm_chan *ch;
	unsigned int step, addr;
	int mul_l, mul_r, smp;
	int i, j, k;
	int *out;


	// PCM disabled or all channels off (to be checked by caller)
	//if (!(Pico_mcd->pcm.control & 0x80) || !Pico_mcd->pcm.enabled) return;

//printf("-- upd %i\n", length);

	for (i = 0; i < 8; i++)
	{
		if (!(Pico_mcd->pcm.enabled & (1 << i))) continue; // channel disabled

		out = buffer;
		ch = &Pico_mcd->pcm.ch[i];

		addr = ch->addr; // >> PCM_STEP_SHIFT;
		mul_l = ((int)ch->regs[0] * (ch->regs[1] & 0xf)) >> (5+1); // (env * pan) >> 5
		mul_r = ((int)ch->regs[0] * (ch->regs[1] >>  4)) >> (5+1);
		step  = ((unsigned int)(*(unsigned short *)&ch->regs[2]) * g_rate) >> 14; // freq step
//		fprintf(stderr, "step=%i, cstep=%i, mul_l=%i, mul_r=%i, ch=%i, addr=%x, en=%02x\n",
//			*(unsigned short *)&ch->regs[2], step, mul_l, mul_r, i, addr, Pico_mcd->pcm.enabled);

		if (!stereo && mul_l < mul_r) mul_l = mul_r;

		for (j = 0; j < length; j++)
		{
//			printf("addr=%08x\n", addr);
			smp = Pico_mcd->pcm_ram[addr >> PCM_STEP_SHIFT];

			// test for loop signal
			if (smp == 0xff)
			{
				addr = *(unsigned short *)&ch->regs[4]; // loop_addr
				smp = Pico_mcd->pcm_ram[addr];
				addr <<= PCM_STEP_SHIFT;
				if (smp == 0xff) break;
			}

			if (smp & 0x80) smp = -(smp & 0x7f);

			*out++ += smp * mul_l; // max 128 * 119 = 15232
			if(stereo)
				*out++ += smp * mul_r;

			// update address register
			k = (addr >> PCM_STEP_SHIFT) + 1;
			addr = (addr + step) & 0x7FFFFFF;

			for(; k < (addr >> PCM_STEP_SHIFT); k++)
			{
				if (Pico_mcd->pcm_ram[k] == 0xff)
				{
					addr = (unsigned int)(*(unsigned short *)&ch->regs[4]) << PCM_STEP_SHIFT; // loop_addr
					break;
				}
			}
		}

		if (Pico_mcd->pcm_ram[addr >> PCM_STEP_SHIFT] == 0xff)
			addr = (unsigned int)(*(unsigned short *)&ch->regs[4]) << PCM_STEP_SHIFT; // loop_addr

		ch->addr = addr;
	}
}

