#include "../PicoInt.h"

/* 12-in-1 */
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
		elprintf(EL_ANOMALY, "12-in-1: unexpected write [%06x] %02x @ %06x", a, d, SekPc);
	}

	a &= 0x3f; a <<= 16;
	len = Pico.romsize - a;
	if (len <= 0) {
		elprintf(EL_ANOMALY, "12-in-1: missing bank @ %06x", a);
		return;
	}

	memcpy(Pico.rom, Pico.rom + 0x200000 + a, len);
}

static void carthw_12in1_reset(void)
{
	carthw_12in1_write8(0xA13000, 0, 0);
}

void carthw_12in1_startup(void)
{
	void *tmp;

	elprintf(EL_STATUS, "12-in-1 mapper detected");

	tmp = realloc(Pico.rom, 0x200000 + 0x200000);
	if (tmp == NULL)
	{
		elprintf(EL_STATUS, "OOM");
		return;
	}
	memcpy(Pico.rom + 0x200000, Pico.rom, 0x200000);

	PicoRead16Hook = carthw_12in1_read16;
	PicoWrite8Hook = carthw_12in1_write8;
	PicoResetHook  = carthw_12in1_reset;
}

