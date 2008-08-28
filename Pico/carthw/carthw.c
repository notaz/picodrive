/*
 * Support for a few cart mappers.
 *
 * (c) Copyright 2008, Grazvydas "notaz" Ignotas
 * Free for non-commercial use.
 *
 *
 * I should better do some pointer stuff here. But as none of these bankswitch
 * while the game runs, memcpy will suffice.
 */

#include "../pico_int.h"


/* 12-in-1 and 4-in-1. Assuming >= 2MB ROMs here. */
static unsigned int carthw_12in1_baddr = 0;

static carthw_state_chunk carthw_12in1_state[] =
{
	{ CHUNK_CARTHW, sizeof(carthw_12in1_baddr), &carthw_12in1_baddr },
	{ 0,            0,                          NULL }
};

static unsigned int carthw_12in1_read16(unsigned int a, int realsize)
{
	// ??
	elprintf(EL_UIO, "12-in-1: read [%06x] @ %06x", a, SekPc);
	return 0;
}

static void carthw_12in1_write8(unsigned int a, unsigned int d, int realsize)
{
	int len;

	if (a < 0xA13000 || a >= 0xA13040) {
		/* 4-in-1 has Real Deal Boxing, which uses serial eeprom,
		 * but I really doubt that pirate cart had it */
		if (a != 0x200001)
			elprintf(EL_ANOMALY, "12-in-1: unexpected write [%06x] %02x @ %06x", a, d, SekPc);
		return;
	}

	carthw_12in1_baddr = a;
	a &= 0x3f; a <<= 16;
	len = Pico.romsize - a;
	if (len <= 0) {
		elprintf(EL_ANOMALY|EL_STATUS, "12-in-1: missing bank @ %06x", a);
		return;
	}

	memcpy(Pico.rom, Pico.rom + Pico.romsize + a, len);
}

static void carthw_12in1_reset(void)
{
	carthw_12in1_write8(0xA13000, 0, 0);
}

static void carthw_12in1_statef(void)
{
	carthw_12in1_write8(carthw_12in1_baddr, 0, 0);
}

void carthw_12in1_startup(void)
{
	void *tmp;

	elprintf(EL_STATUS, "12-in-1 mapper detected");

	tmp = realloc(Pico.rom, Pico.romsize * 2);
	if (tmp == NULL)
	{
		elprintf(EL_STATUS, "OOM");
		return;
	}
	Pico.rom = tmp;
	memcpy(Pico.rom + Pico.romsize, Pico.rom, Pico.romsize);

	PicoRead16Hook = carthw_12in1_read16;
	PicoWrite8Hook = carthw_12in1_write8;
	PicoResetHook  = carthw_12in1_reset;
	PicoLoadStateHook = carthw_12in1_statef;
	carthw_chunks     = carthw_12in1_state;
}


/* Realtec, based on TascoDLX doc
 * http://www.sharemation.com/TascoDLX/REALTEC%20Cart%20Mapper%20-%20description%20v1.txt
 */
static int realtec_bank = 0x80000000, realtec_size = 0x80000000;
static int realtec_romsize = 0;

static void carthw_realtec_write8(unsigned int a, unsigned int d, int realsize)
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
		if (realtec_size > realtec_romsize - realtec_bank || realtec_bank >= realtec_romsize)
		{
			elprintf(EL_ANOMALY, "realtec: bank too large / out of range?");
			return;
		}

		for (i = 0; i < 0x400000; i += realtec_size)
			memcpy(Pico.rom + i, Pico.rom + 0x400000 + realtec_bank, realtec_size);
	}
}

static void carthw_realtec_reset(void)
{
	int i;
	/* map boot code */
	for (i = 0; i < 0x400000; i += 0x2000)
		memcpy(Pico.rom + i, Pico.rom + 0x400000 + realtec_romsize - 0x2000, 0x2000);
	realtec_bank = realtec_size = 0x80000000;
}

void carthw_realtec_startup(void)
{
	void *tmp;

	elprintf(EL_STATUS, "Realtec mapper detected");

	realtec_romsize = Pico.romsize;
	Pico.romsize = 0x400000;
	tmp = realloc(Pico.rom, 0x400000 + realtec_romsize);
	if (tmp == NULL)
	{
		elprintf(EL_STATUS, "OOM");
		return;
	}
	Pico.rom = tmp;
	memcpy(Pico.rom + 0x400000, Pico.rom, realtec_romsize);

	PicoWrite8Hook = carthw_realtec_write8;
	PicoResetHook = carthw_realtec_reset;
}

/* Radica mapper, based on DevSter's info
 * http://devster.monkeeh.com/sega/radica/
 */
static unsigned int carthw_radica_baddr = 0;

static carthw_state_chunk carthw_radica_state[] =
{
	{ CHUNK_CARTHW, sizeof(carthw_radica_baddr), &carthw_radica_baddr },
	{ 0,            0,                           NULL }
};

static unsigned int carthw_radica_read16(unsigned int a, int realsize)
{
	if ((a & 0xffff80) != 0xa13000) {
		elprintf(EL_UIO, "radica: r16 %06x", a);
		return 0;
	}

	carthw_radica_baddr = a;
	a = (a & 0x7e) << 15;
	if (a >= Pico.romsize) {
		elprintf(EL_ANOMALY|EL_STATUS, "radica: missing bank @ %06x", a);
		return 0;
	}
	memcpy(Pico.rom, Pico.rom + Pico.romsize + a, Pico.romsize - a);

	return 0;
}

static void carthw_radica_statef(void)
{
	carthw_radica_read16(carthw_radica_baddr, 0);
}

static void carthw_radica_reset(void)
{
	memcpy(Pico.rom, Pico.rom + Pico.romsize, Pico.romsize);
}

void carthw_radica_startup(void)
{
	void *tmp;

	elprintf(EL_STATUS, "Radica mapper detected");

	tmp = realloc(Pico.rom, Pico.romsize * 2);
	if (tmp == NULL)
	{
		elprintf(EL_STATUS, "OOM");
		return;
	}
	Pico.rom = tmp;
	memcpy(Pico.rom + Pico.romsize, Pico.rom, Pico.romsize);

	PicoRead16Hook = carthw_radica_read16;
	PicoResetHook  = carthw_radica_reset;
	PicoLoadStateHook = carthw_radica_statef;
	carthw_chunks     = carthw_radica_state;
}


