#ifndef __SH2_H__
#define __SH2_H__

// pico memhandlers
// XXX: move somewhere else
unsigned int p32x_sh2_read8(unsigned int a, int id);
unsigned int p32x_sh2_read16(unsigned int a, int id);
unsigned int p32x_sh2_read32(unsigned int a, int id);
void p32x_sh2_write8(unsigned int a, unsigned int d, int id);
void p32x_sh2_write16(unsigned int a, unsigned int d, int id);
void p32x_sh2_write32(unsigned int a, unsigned int d, int id);


typedef struct
{
	unsigned int	r[16];
	unsigned int	ppc;
	unsigned int	pc;
	unsigned int	pr;
	unsigned int	sr;
	unsigned int	gbr, vbr;
	unsigned int	mach, macl;

	unsigned int	ea;
	unsigned int	delay;
	unsigned int	test_irq;

	int	pending_irl;
	int	pending_int_irq;	// internal irq
	int	pending_int_vector;
	void	(*irq_callback)(int id, int level);
	int	is_slave;

	int		icount;		// cycles left in current timeslice
	unsigned int	cycles_aim;	// subtract sh2_icount to get global counter
	unsigned int	cycles_done;
} SH2;

extern SH2 *sh2; // active sh2

void sh2_init(SH2 *sh2, int is_slave);
void sh2_reset(SH2 *sh2);
void sh2_irl_irq(SH2 *sh2, int level);
void sh2_internal_irq(SH2 *sh2, int level, int vector);

void sh2_execute(SH2 *sh2, int cycles);

#endif /* __SH2_H__ */
