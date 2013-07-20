/*
 * PicoDrive
 * (C) notaz, 2009,2010
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */
#include <string.h>
#include <stddef.h>

#include "sh2.h"
#include "../debug.h"
#include "compiler.h"

#define I 0xf0

int sh2_init(SH2 *sh2, int is_slave)
{
	int ret = 0;

	memset(sh2, 0, offsetof(SH2, mult_m68k_to_sh2));
	sh2->is_slave = is_slave;
	pdb_register_cpu(sh2, PDBCT_SH2, is_slave ? "ssh2" : "msh2");
#ifdef DRC_SH2
	ret = sh2_drc_init(sh2);
#endif
	return ret;
}

void sh2_finish(SH2 *sh2)
{
#ifdef DRC_SH2
	sh2_drc_finish(sh2);
#endif
}

void sh2_reset(SH2 *sh2)
{
	sh2->pc = p32x_sh2_read32(0, sh2);
	sh2->r[15] = p32x_sh2_read32(4, sh2);
	sh2->sr = I;
	sh2->vbr = 0;
	sh2->pending_int_irq = 0;
}

void sh2_do_irq(SH2 *sh2, int level, int vector)
{
	sh2->sr &= 0x3f3;

	sh2->r[15] -= 4;
	p32x_sh2_write32(sh2->r[15], sh2->sr, sh2);	/* push SR onto stack */
	sh2->r[15] -= 4;
	p32x_sh2_write32(sh2->r[15], sh2->pc, sh2);	/* push PC onto stack */

	/* set I flags in SR */
	sh2->sr = (sh2->sr & ~I) | (level << 4);

	/* fetch PC */
	sh2->pc = p32x_sh2_read32(sh2->vbr + vector * 4, sh2);

	/* 13 cycles at best */
	sh2->icount -= 13;
}

int sh2_irl_irq(SH2 *sh2, int level, int nested_call)
{
	int taken;

	sh2->pending_irl = level;
	if (level < sh2->pending_int_irq)
		level = sh2->pending_int_irq;
	sh2->pending_level = level;

	taken = (level > ((sh2->sr >> 4) & 0x0f));
	if (taken) {
		if (!nested_call) {
			// not in memhandler, so handle this now (recompiler friendly)
			// do this to avoid missing irqs that other SH2 might clear
			int vector = sh2->irq_callback(sh2, level);
			sh2_do_irq(sh2, level, vector);
			sh2->m68krcycles_done += C_SH2_TO_M68K(*sh2, 13);
		}
		else
			sh2->test_irq = 1;
	}
	return taken;
}

void sh2_internal_irq(SH2 *sh2, int level, int vector)
{
	// FIXME: multiple internal irqs not handled..
	// assuming internal irqs never clear until accepted
	sh2->pending_int_irq = level;
	sh2->pending_int_vector = vector;
	if (level > sh2->pending_level)
		sh2->pending_level = level;

	sh2->test_irq = 1;
}

#define SH2_REG_SIZE (offsetof(SH2, macl) + sizeof(sh2->macl))

void sh2_pack(const SH2 *sh2, unsigned char *buff)
{
	unsigned int *p;

	memcpy(buff, sh2, SH2_REG_SIZE);
	p = (void *)(buff + SH2_REG_SIZE);

	p[0] = sh2->pending_int_irq;
	p[1] = sh2->pending_int_vector;
}

void sh2_unpack(SH2 *sh2, const unsigned char *buff)
{
	unsigned int *p;

	memcpy(sh2, buff, SH2_REG_SIZE);
	p = (void *)(buff + SH2_REG_SIZE);

	sh2->pending_int_irq = p[0];
	sh2->pending_int_vector = p[1];
	sh2->test_irq = 1;
}

#ifdef DRC_CMP

/* trace/compare */
#include <stdio.h>
#include <stdlib.h>
#include <pico/memory.h>
#undef _USE_CZ80 // HACK
#include <pico/pico_int.h>
#include <pico/debug.h>

static SH2 sh2ref[2];
static unsigned int mem_val;
static FILE *f;

enum ctl_byte {
	CTL_MASTERSLAVE = 0x80,
	CTL_EA = 0x82,
	CTL_EAVAL = 0x83,
	CTL_M68KPC = 0x84,
	CTL_CYCLES = 0x85,
};

#define SH2MAP_ADDR2OFFS_R(a) \
  ((((a) >> 25) & 3) | (((a) >> 27) & 0x1c))

static unsigned int local_read32(SH2 *sh2, u32 a)
{
	const sh2_memmap *sh2_map = sh2->read16_map;
	u16 *pd;
	uptr p;

	sh2_map += SH2MAP_ADDR2OFFS_R(a);
	p = sh2_map->addr;
	if (!map_flag_set(p)) {
		pd = (u16 *)((p << 1) + ((a & sh2_map->mask) & ~1));
		return (pd[0] << 16) | pd[1];
	}

	if ((a & 0xfffff000) == 0xc0000000) {
		// data array
		pd = (u16 *)Pico32xMem->data_array[sh2->is_slave]
			+ (a & 0xfff) / 2;
		return (pd[0] << 16) | pd[1];
	}
	if ((a & 0xdfffffc0) == 0x4000) {
		pd = &Pico32x.regs[(a & 0x3f) / 2];
		return (pd[0] << 16) | pd[1];
	}
	if ((a & 0xdffffe00) == 0x4200) {
		pd = &Pico32xMem->pal[(a & 0x1ff) / 2];
		return (pd[0] << 16) | pd[1];
	}

	return 0;
}

static void write_uint(unsigned char ctl, unsigned int v)
{
	fwrite(&ctl, 1, 1, f);
	fwrite(&v, sizeof(v), 1, f);
}

void do_sh2_trace(SH2 *current, int cycles)
{
	static int current_slave = -1;
	static u32 current_m68k_pc;
	SH2 *sh2o = &sh2ref[current->is_slave];
	u32 *regs_a = (void *)current;
	u32 *regs_o = (void *)sh2o;
	unsigned char v;
	u32 val;
	int i;

	if (f == NULL)
		f = fopen("tracelog", "wb");

	if (SekPc != current_m68k_pc) {
		current_m68k_pc = SekPc;
		write_uint(CTL_M68KPC, current_m68k_pc);
	}

	if (current->is_slave != current_slave) {
		current_slave = current->is_slave;
		v = CTL_MASTERSLAVE | current->is_slave;
		fwrite(&v, 1, 1, f);
	}

	for (i = 0; i < offsetof(SH2, read8_map) / 4; i++) {
		if (i == 17) // ppc
			continue;
		if (regs_a[i] != regs_o[i]) {
			write_uint(i, regs_a[i]);
			regs_o[i] = regs_a[i];
		}
	}

	if (current->ea != sh2o->ea) {
		write_uint(CTL_EA, current->ea);
		sh2o->ea = current->ea;
	}
	val = local_read32(current, current->ea);
	if (mem_val != val) {
		write_uint(CTL_EAVAL, val);
		mem_val = val;
	}
	write_uint(CTL_CYCLES, cycles);
}

static const char *regnames[] = {
	"r0",  "r1",  "r2",  "r3",
	"r4",  "r5",  "r6",  "r7",
	"r8",  "r9",  "r10", "r11",
	"r12", "r13", "r14", "r15",
	"pc",  "ppc", "pr",  "sr",
	"gbr", "vbr", "mach","macl",
};

static void dump_regs(SH2 *sh2)
{
	char csh2;
	int i;

	csh2 = sh2->is_slave ? 's' : 'm';
	for (i = 0; i < 16/2; i++)
		printf("%csh2 r%d: %08x r%02d: %08x\n", csh2,
			i, sh2->r[i], i+8, sh2->r[i+8]);
	printf("%csh2 PC: %08x  ,   %08x\n", csh2, sh2->pc, sh2->ppc);
	printf("%csh2 SR:      %03x  PR: %08x\n", csh2, sh2->sr, sh2->pr);
}

void do_sh2_cmp(SH2 *current)
{
	static int current_slave;
	static u32 current_val;
	SH2 *sh2o = &sh2ref[current->is_slave];
	u32 *regs_a = (void *)current;
	u32 *regs_o = (void *)sh2o;
	unsigned char code;
	int cycles_o = 666;
	u32 sr, val;
	int bad = 0;
	int cycles;
	int i, ret;

	if (f == NULL) {
		f = fopen("tracelog", "rb");
		sh2ref[1].is_slave = 1;
	}

	while (1) {
		ret = fread(&code, 1, 1, f);
		if (ret <= 0)
			break;
		if (code == CTL_CYCLES) {
			fread(&cycles_o, 1, 4, f);
			break;
		}

		switch (code) {
		case CTL_MASTERSLAVE:
		case CTL_MASTERSLAVE + 1:
			current_slave = code & 1;
			break;
		case CTL_EA:
			fread(&sh2o->ea, 4, 1, f);
			break;
		case CTL_EAVAL:
			fread(&current_val, 4, 1, f);
			break;
		case CTL_M68KPC:
			fread(&val, 4, 1, f);
			if (SekPc != val) {
				printf("m68k: %08x %08x\n", SekPc, val);
				bad = 1;
			}
			break;
		default:
			if (code < offsetof(SH2, read8_map) / 4)
				fread(regs_o + code, 4, 1, f);
			else {
				printf("invalid code: %02x\n", code);
				goto end;
			}
			break;
		}
	}

	if (ret <= 0) {
		printf("EOF?\n");
		goto end;
	}

	if (current->is_slave != current_slave) {
		printf("bad slave: %d %d\n", current->is_slave,
			current_slave);
		bad = 1;
	}

	for (i = 0; i < offsetof(SH2, read8_map) / 4; i++) {
		if (i == 17 || i == 19) // ppc, sr
			continue;
		if (regs_a[i] != regs_o[i]) {
			printf("bad %4s: %08x %08x\n",
				regnames[i], regs_a[i], regs_o[i]);
			bad = 1;
		}
	}

	sr = current->sr & 0x3f3;
	cycles = (signed int)current->sr >> 12;

	if (sr != sh2o->sr) {
		printf("bad SR:  %03x %03x\n", sr, sh2o->sr);
		bad = 1;
	}

	if (cycles != cycles_o) {
		printf("bad cycles: %d %d\n", cycles, cycles_o);
		bad = 1;
	}

	val = local_read32(current, sh2o->ea);
	if (val != current_val) {
		printf("bad val @%08x: %08x %08x\n", sh2o->ea, val, current_val);
		bad = 1;
	}

	if (!bad) {
		sh2o->ppc = current->pc;
		return;
	}

end:
	printf("--\n");
	dump_regs(sh2o);
	if (current->is_slave != current_slave)
		dump_regs(&sh2ref[current->is_slave ^ 1]);
	PDebugDumpMem();
	exit(1);
}

#endif // DRC_CMP
