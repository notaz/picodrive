#ifndef __SH2_H__
#define __SH2_H__

typedef struct
{
	unsigned int	r[16];		// 00
	unsigned int	pc;		// 40
	unsigned int	ppc;
	unsigned int	pr;
	unsigned int	sr;
	unsigned int	gbr, vbr;	// 50
	unsigned int	mach, macl;	// 58

	// common
	const void	*read8_map;	// 60
	const void	*read16_map;
	const void	**write8_tab;
	const void	**write16_tab;

	// drc stuff
	int		drc_tmp;	// 70

	// interpreter stuff
	int		icount;		// cycles left in current timeslice
	unsigned int	ea;
	unsigned int	delay;
	unsigned int	test_irq;

	int	pending_level;		// MAX(pending_irl, pending_int_irq)
	int	pending_irl;
	int	pending_int_irq;	// internal irq
	int	pending_int_vector;
	void	(*irq_callback)(int id, int level);
	int	is_slave;

	unsigned int	cycles_aim;	// subtract sh2_icount to get global counter
	unsigned int	cycles_done;
} SH2;

extern SH2 *sh2; // active sh2. XXX: consider removing

int  sh2_init(SH2 *sh2, int is_slave);
void sh2_finish(SH2 *sh2);
void sh2_reset(SH2 *sh2);
void sh2_irl_irq(SH2 *sh2, int level);
void sh2_internal_irq(SH2 *sh2, int level, int vector);
void sh2_do_irq(SH2 *sh2, int level, int vector);

void sh2_execute(SH2 *sh2, int cycles);

// pico memhandlers
// XXX: move somewhere else
#if !defined(REGPARM) && defined(__i386__) 
#define REGPARM(x) __attribute__((regparm(x)))
#else
#define REGPARM(x)
#endif

unsigned int REGPARM(2) p32x_sh2_read8(unsigned int a, SH2 *sh2);
unsigned int REGPARM(2) p32x_sh2_read16(unsigned int a, SH2 *sh2);
unsigned int REGPARM(2) p32x_sh2_read32(unsigned int a, SH2 *sh2);
void REGPARM(3) p32x_sh2_write8(unsigned int a, unsigned int d, SH2 *sh2);
void REGPARM(3) p32x_sh2_write16(unsigned int a, unsigned int d, SH2 *sh2);
void REGPARM(3) p32x_sh2_write32(unsigned int a, unsigned int d, SH2 *sh2);

#endif /* __SH2_H__ */
