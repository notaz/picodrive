#include <stdio.h>
#if defined(__linux__) && defined(ARM)
#include <sys/mman.h>
#endif

#include "cmn.h"

#ifndef ARM
unsigned int tcache[SSP_TCACHE_SIZE/4];
unsigned int *ssp_block_table[0x5090/2];
unsigned int *ssp_block_table_iram[15][0x800/2];
char ssp_align[SSP_BLOCKTAB_ALIGN_SIZE];
#endif


void drc_cmn_init(void)
{
#if defined(__linux__) && defined(ARM)
	void *tmp;

	tmp = mmap(tcache, SSP_DRC_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	printf("mmap tcache: %p, asked %p\n", tmp, tcache);
#endif

}

// TODO: add calls in core, possibly to cart.c?
void drc_cmn_cleanup(void)
{
#if defined(__linux__) && defined(ARM)
	int ret;
	ret = munmap(tcache, SSP_DRC_SIZE);
	printf("munmap tcache: %i\n", ret);
#endif
}

