
/* svp */
#include "svp/ssp16.h"

typedef struct {
	unsigned char iram_rom[0x20000]; // IRAM (0-0x7ff) and program ROM (0x800-0x1ffff)
	unsigned char dram[0x20000];
	ssp1601_t ssp1601;
} svp_t;

extern svp_t *svp;

void PicoSVPInit(void);

unsigned int PicoSVPRead16(unsigned int a, int realsize);
void PicoSVPWrite8 (unsigned int a, unsigned int d, int realsize);
void PicoSVPWrite16(unsigned int a, unsigned int d, int realsize);

