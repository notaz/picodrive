
/* svp */
#include "svp/ssp16.h"

typedef struct {
	unsigned char ram[0x20000];
	// TODO: IRAM?
	ssp1601_t ssp1601;
} svp_t;

extern svp_t *svp;

void PicoSVPInit(void);
void PicoSVPReset(void);

unsigned int PicoSVPRead16(unsigned int a, int realsize);
void PicoSVPWrite8 (unsigned int a, unsigned int d, int realsize);
void PicoSVPWrite16(unsigned int a, unsigned int d, int realsize);

int PicoSVPDma(unsigned int source, unsigned short **srcp, unsigned short **limitp);

