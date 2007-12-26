#include "../../PicoInt.h"

svp_t *svp = NULL;

static void PicoSVPReset(void)
{
	elprintf(EL_SVP, "SVP reset");

	ssp1601_reset(&svp->ssp1601);
}


static void PicoSVPLine(void)
{
	// ???
	// OSC_NTSC / 3.0 / 60.0 / 262.0 ~= 1139
	// OSC_PAL  / 3.0 / 50.0 / 312.0 ~= 1137
	ssp1601_run(100);
	exit(1);
}


static int PicoSVPDma(unsigned int source, unsigned short **srcp, unsigned short **limitp)
{
	if ((source & 0xfe0000) == 0x300000)
	{
		elprintf(EL_VDPDMA|EL_SVP, "SVP DmaSlow from %06x", source);
		source &= 0x1fffe;
		*srcp = (unsigned short *)(svp->ram + source);
		*limitp = (unsigned short *)(svp->ram + sizeof(svp->ram));
		return 1;
	}

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

