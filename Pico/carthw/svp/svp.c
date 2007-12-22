#include "../../PicoInt.h"

svp_t *svp = NULL;

void PicoSVPInit(void)
{
	void *tmp;

	elprintf(0xffff, "SVP init");

	tmp = realloc(Pico.rom, 0x200000 + sizeof(*svp));
	if (tmp == NULL)
	{
		elprintf(EL_STATUS, "OOM for SVP data");
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
}


void PicoSVPReset(void)
{
	elprintf(0xffff, "SVP reset");

	ssp1601_reset(&svp->ssp1601);
}


int PicoSVPDma(unsigned int source, unsigned short **srcp, unsigned short **limitp)
{
	if ((source & 0xfe0000) == 0x300000)
	{
		elprintf(EL_VDPDMA|0xffff, "SVP DmaSlow from %06x", source);
		source &= 0x1fffe;
		*srcp = (unsigned short *)(svp->ram + source);
		*limitp = (unsigned short *)(svp->ram + sizeof(svp->ram));
		return 1;
	}

	return 0;
}

