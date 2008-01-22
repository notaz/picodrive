#include "../../PicoInt.h"

svp_t *svp = NULL;

static void PicoSVPReset(void)
{
	elprintf(EL_SVP, "SVP reset");

	memcpy(svp->iram_rom + 0x800, Pico.rom + 0x800, 0x20000 - 0x800);
	ssp1601_reset(&svp->ssp1601);
}


static void PicoSVPLine(void)
{
	// ???
	// OSC_NTSC / 3.0 / 60.0 / 262.0 ~= 1139
	// OSC_PAL  / 3.0 / 50.0 / 312.0 ~= 1137
	ssp1601_run(800);

	// test mode
	//if (Pico.m.frame_count == 13) PicoPad[0] |= 0xff;
	// pushing start
	//if (Pico.m.frame_count & 4) PicoPad[0] |= 0x80;
}


static int PicoSVPDma(unsigned int source, int len, unsigned short **srcp, unsigned short **limitp)
{
	if ((source & 0xfe0000) == 0x300000)
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

