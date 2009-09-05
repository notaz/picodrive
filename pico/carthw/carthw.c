/*
 * Support for a few cart mappers.
 *
 * (c) Copyright 2008-2009, Grazvydas "notaz" Ignotas
 * Free for non-commercial use.
 *
 */

#include "../pico_int.h"
#include "../memory.h"


/* Common *-in-1 pirate mapper.
 * Switches banks based on addr lines when /TIME is set.
 * TODO: verify
 */
static unsigned int carthw_Xin1_baddr = 0;

static void carthw_Xin1_do(u32 a, int mask, int shift)
{
	int len;

	carthw_Xin1_baddr = a;
	a &= mask;
	a <<= shift;
	len = Pico.romsize - a;
	if (len <= 0) {
		elprintf(EL_ANOMALY|EL_STATUS, "X-in-1: missing bank @ %06x", a);
		return;
	}

	len = (len + M68K_BANK_MASK) & ~M68K_BANK_MASK;
	cpu68k_map_set(m68k_read8_map,  0x000000, len - 1, Pico.rom + a, 0);
	cpu68k_map_set(m68k_read16_map, 0x000000, len - 1, Pico.rom + a, 0);
}

static carthw_state_chunk carthw_Xin1_state[] =
{
	{ CHUNK_CARTHW, sizeof(carthw_Xin1_baddr), &carthw_Xin1_baddr },
	{ 0,            0,                         NULL }
};

// TODO: test a0, reads, w16
static void carthw_Xin1_write8(u32 a, u32 d)
{
	if ((a & 0xffff00) != 0xa13000) {
		PicoWrite8_io(a, d);
		return;
	}

	carthw_Xin1_do(a, 0x3f, 16);
}

static void carthw_Xin1_mem_setup(void)
{
	cpu68k_map_set(m68k_write8_map, 0xa10000, 0xa1ffff, carthw_Xin1_write8, 1);
}

static void carthw_Xin1_reset(void)
{
	carthw_Xin1_write8(0xa13000, 0);
}

static void carthw_Xin1_statef(void)
{
	carthw_Xin1_write8(carthw_Xin1_baddr, 0);
}

void carthw_Xin1_startup(void)
{
	elprintf(EL_STATUS, "X-in-1 mapper startup");

	PicoCartMemSetup  = carthw_Xin1_mem_setup;
	PicoResetHook     = carthw_Xin1_reset;
	PicoLoadStateHook = carthw_Xin1_statef;
	carthw_chunks     = carthw_Xin1_state;
}


/* Realtec, based on TascoDLX doc
 * http://www.sharemation.com/TascoDLX/REALTEC%20Cart%20Mapper%20-%20description%20v1.txt
 */
static int realtec_bank = 0x80000000, realtec_size = 0x80000000;

static void carthw_realtec_write8(u32 a, u32 d)
{
	int i, bank_old = realtec_bank, size_old = realtec_size;

	if (a == 0x400000)
	{
		realtec_bank &= 0x0e0000;
		realtec_bank |= 0x300000 & (d << 19);
		if (realtec_bank != bank_old)
			elprintf(EL_ANOMALY, "write [%06x] %02x @ %06x", a, d, SekPc);
	}
	else if (a == 0x402000)
	{
		realtec_size = (d << 17) & 0x3e0000;
		if (realtec_size != size_old)
			elprintf(EL_ANOMALY, "write [%06x] %02x @ %06x", a, d, SekPc);
	}
	else if (a == 0x404000)
	{
		realtec_bank &= 0x300000;
		realtec_bank |= 0x0e0000 & (d << 17);
		if (realtec_bank != bank_old)
			elprintf(EL_ANOMALY, "write [%06x] %02x @ %06x", a, d, SekPc);
	}
	else
		elprintf(EL_ANOMALY, "realtec: unexpected write [%06x] %02x @ %06x", a, d, SekPc);

	if (realtec_bank >= 0 && realtec_size >= 0 &&
		(realtec_bank != bank_old || realtec_size != size_old))
	{
		elprintf(EL_ANOMALY, "realtec: new bank %06x, size %06x", realtec_bank, realtec_size, SekPc);
		if (realtec_size > Pico.romsize - realtec_bank)
		{
			elprintf(EL_ANOMALY, "realtec: bank too large / out of range?");
			return;
		}

		for (i = 0; i < 0x400000; i += realtec_size) {
			cpu68k_map_set(m68k_read8_map,  i, realtec_size - 1, Pico.rom + realtec_bank, 0);
			cpu68k_map_set(m68k_read16_map, i, realtec_size - 1, Pico.rom + realtec_bank, 0);
		}
	}
}

static void carthw_realtec_reset(void)
{
	int i;

	/* map boot code */
	for (i = 0; i < 0x400000; i += M68K_BANK_SIZE) {
		cpu68k_map_set(m68k_read8_map,  i, i + M68K_BANK_SIZE - 1, Pico.rom + Pico.romsize, 0);
		cpu68k_map_set(m68k_read16_map, i, i + M68K_BANK_SIZE - 1, Pico.rom + Pico.romsize, 0);
	}
	cpu68k_map_set(m68k_write8_map, 0x400000, 0x400000 + M68K_BANK_SIZE - 1, carthw_realtec_write8, 1);
	realtec_bank = realtec_size = 0x80000000;
}

void carthw_realtec_startup(void)
{
	void *tmp;
	int i;

	elprintf(EL_STATUS, "Realtec mapper startup");

	// allocate additional bank for boot code
	// (we know those ROMs have aligned size)
	tmp = realloc(Pico.rom, Pico.romsize + M68K_BANK_SIZE);
	if (tmp == NULL)
	{
		elprintf(EL_STATUS, "OOM");
		return;
	}
	Pico.rom = tmp;

	// create bank for boot code
	for (i = 0; i < M68K_BANK_SIZE; i += 0x2000)
		memcpy(Pico.rom + Pico.romsize + i, Pico.rom + Pico.romsize - 0x2000, 0x2000);

	PicoResetHook = carthw_realtec_reset;
}

/* Radica mapper, based on DevSter's info
 * http://devster.monkeeh.com/sega/radica/
 * XXX: mostly the same as X-in-1, merge?
 */
static u32 carthw_radica_read16(u32 a)
{
	if ((a & 0xffff00) != 0xa13000)
		return PicoRead16_io(a);

	carthw_Xin1_do(a, 0x7e, 15);

	return 0;
}

static void carthw_radica_mem_setup(void)
{
	cpu68k_map_set(m68k_read16_map, 0xa10000, 0xa1ffff, carthw_radica_read16, 1);
}

static void carthw_radica_statef(void)
{
	carthw_radica_read16(carthw_Xin1_baddr);
}

static void carthw_radica_reset(void)
{
	carthw_radica_read16(0xa13000);
}

void carthw_radica_startup(void)
{
	elprintf(EL_STATUS, "Radica mapper startup");

	PicoCartMemSetup  = carthw_radica_mem_setup;
	PicoResetHook     = carthw_radica_reset;
	PicoLoadStateHook = carthw_radica_statef;
	carthw_chunks     = carthw_Xin1_state;
}

