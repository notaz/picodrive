// The SVP chip emulator

// (c) Copyright 2008, Grazvydas "notaz" Ignotas
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "../../PicoInt.h"

svp_t *svp = NULL;
int PicoSVPCycles = 1000; // cycles/line

static void PicoSVPReset(void)
{
	elprintf(EL_SVP, "SVP reset");

	memcpy(svp->iram_rom + 0x800, Pico.rom + 0x800, 0x20000 - 0x800);
	ssp1601_reset(&svp->ssp1601);
}


static void PicoSVPLine(int count)
{
	// ???
	ssp1601_run(PicoSVPCycles * count);

	// test mode
	//if (Pico.m.frame_count == 13) PicoPad[0] |= 0xff;
}


static int PicoSVPDma(unsigned int source, int len, unsigned short **srcp, unsigned short **limitp)
{
	if (source < Pico.romsize) // Rom
	{
		source -= 2;
		*srcp = (unsigned short *)(Pico.rom + (source&~1));
		*limitp = (unsigned short *)(Pico.rom + Pico.romsize);
		return 1;
	}
	else if ((source & 0xfe0000) == 0x300000)
	{
		elprintf(EL_VDPDMA|EL_SVP, "SVP DmaSlow from %06x, len=%i", source, len);
		source &= 0x1fffe;
		source -= 2;
		*srcp = (unsigned short *)(svp->dram + source);
		*limitp = (unsigned short *)(svp->dram + sizeof(svp->dram));
		return 1;
	}
	else
		elprintf(EL_VDPDMA|EL_SVP|EL_ANOMALY, "SVP FIXME unhandled DmaSlow from %06x, len=%i", source, len);

	return 0;
}


void PicoSVPInit(void)
{
	void *tmp;

	elprintf(EL_SVP, "SVP init");

	tmp = realloc(Pico.rom, 0x200000 + sizeof(*svp));
	if (tmp == NULL)
	{
		elprintf(EL_STATUS|EL_SVP, "OOM for SVP data");
		return;
	}

	Pico.rom = tmp;
	svp = (void *) ((char *)tmp + 0x200000);
	memset(svp, 0, sizeof(*svp));

	// init ok, setup hooks..
	PicoRead16Hook = PicoSVPRead16;
	PicoWrite8Hook = PicoSVPWrite8;
	PicoWrite16Hook = PicoSVPWrite16;
	PicoDmaHook = PicoSVPDma;
	PicoResetHook = PicoSVPReset;
	PicoLineHook = PicoSVPLine;
}

