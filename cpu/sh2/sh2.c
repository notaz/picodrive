#include <string.h>
#include <stddef.h>

#include "sh2.h"
#include "../debug.h"
#include "compiler.h"

#define I 0xf0

SH2 *sh2; // active sh2

int sh2_init(SH2 *sh2, int is_slave)
{
	int ret = 0;

	memset(sh2, 0, sizeof(*sh2));
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
	sh2->r[15] -= 4;
	p32x_sh2_write32(sh2->r[15], sh2->sr, sh2);	/* push SR onto stack */
	sh2->r[15] -= 4;
	p32x_sh2_write32(sh2->r[15], sh2->pc, sh2);	/* push PC onto stack */

	/* set I flags in SR */
	sh2->sr = (sh2->sr & ~I) | (level << 4);

	/* fetch PC */
	sh2->pc = p32x_sh2_read32(sh2->vbr + vector * 4, sh2);

	/* 13 cycles at best */
	sh2->cycles_done += 13;
//	sh2->icount -= 13;
}

void sh2_irl_irq(SH2 *sh2, int level, int nested_call)
{
	sh2->pending_irl = level;
	if (level < sh2->pending_int_irq)
		level = sh2->pending_int_irq;
	sh2->pending_level = level;

	if (!nested_call) {
		// not in memhandler, so handle this now (recompiler friendly)
		// do this to avoid missing irqs that other SH2 might clear
		if (level > ((sh2->sr >> 4) & 0x0f)) {
			int vector = sh2->irq_callback(sh2, level);
			sh2_do_irq(sh2, level, vector);
		}
	}
	else
		sh2->test_irq = 1;
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
	p[2] = sh2->cycles_aim;
	p[3] = sh2->cycles_done;
}

void sh2_unpack(SH2 *sh2, const unsigned char *buff)
{
	unsigned int *p;

	memcpy(sh2, buff, SH2_REG_SIZE);
	p = (void *)(buff + SH2_REG_SIZE);

	sh2->pending_int_irq = p[0];
	sh2->pending_int_vector = p[1];
	sh2->cycles_aim = p[2];
	sh2->cycles_done = p[3];
}

