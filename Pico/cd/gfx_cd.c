// TODO...

#include "../PicoInt.h"


static void gfx_cd_start(void)
{
		dprintf("gfx_cd_start()");
	if (Pico_mcd->s68k_regs[0x33] & (1<<1))
	{
		dprintf("gfx_cd irq 1");
		SekInterruptS68k(1);
	}
}

void gfx_cd_update(void)
{
}


unsigned int gfx_cd_read(unsigned int a)
{
	dprintf("gfx_cd_read(%x)", a);

//	switch (a) {
//		case 2:
//			return;
	return 0;
}

void gfx_cd_write(unsigned int a, unsigned int d)
{
	dprintf("gfx_cd_write(%x, %04x)", a, d);

	switch (a) {
		case 0x66:
			if (Pico_mcd->s68k_regs[3]&4) return; // can't do tanformations in 1M mode
			gfx_cd_start();
			return;
	}
}

