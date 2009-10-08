#include <string.h>
#include "sh2.h"

#define I 0xf0

void sh2_init(SH2 *sh2, int is_slave)
{
	memset(sh2, 0, sizeof(*sh2));
	sh2->is_slave = is_slave;
}

void sh2_reset(SH2 *sh2)
{
	sh2->pc = p32x_sh2_read32(0, sh2->is_slave);
	sh2->r[15] = p32x_sh2_read32(4, sh2->is_slave);
	sh2->sr = I;
	sh2->vbr = 0;
	sh2->pending_int_irq = 0;
}

static void sh2_do_irq(SH2 *sh2, int level, int vector)
{
	sh2->irq_callback(sh2->is_slave, level);

	sh2->r[15] -= 4;
	p32x_sh2_write32(sh2->r[15], sh2->sr, sh2->is_slave);	/* push SR onto stack */
	sh2->r[15] -= 4;
	p32x_sh2_write32(sh2->r[15], sh2->pc, sh2->is_slave);	/* push PC onto stack */

	/* set I flags in SR */
	sh2->sr = (sh2->sr & ~I) | (level << 4);

	/* fetch PC */
	sh2->pc = p32x_sh2_read32(sh2->vbr + vector * 4, sh2->is_slave);

	/* 13 cycles at best */
	sh2->cycles_done += 13;
//	sh2->icount -= 13;
}

void sh2_irl_irq(SH2 *sh2, int level)
{
	sh2->pending_irl = level;
	if (level <= ((sh2->sr >> 4) & 0x0f))
		return;

	sh2_do_irq(sh2, level, 64 + level/2);
}

void sh2_internal_irq(SH2 *sh2, int level, int vector)
{
	sh2->pending_int_irq = level;
	sh2->pending_int_vector = vector;
	if (level <= ((sh2->sr >> 4) & 0x0f))
		return;

	sh2_do_irq(sh2, level, vector);
	sh2->pending_int_irq = 0; // auto-clear
}

