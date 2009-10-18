#include <stdio.h>
#if defined(__linux__) && defined(ARM)
#include <sys/mman.h>
#endif

#include "cmn.h"

u8 __attribute__((aligned(4096))) tcache[DRC_TCACHE_SIZE];


void drc_cmn_init(void)
{
#if defined(__linux__) && defined(ARM)
	void *tmp;

	tmp = mmap(tcache, DRC_TCACHE_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	printf("mmap tcache: %p, asked %p\n", tmp, tcache);
#endif

}

// TODO: add calls in core, possibly to cart.c?
void drc_cmn_cleanup(void)
{
#if defined(__linux__) && defined(ARM)
	int ret;
	ret = munmap(tcache, DRC_TCACHE_SIZE);
	printf("munmap tcache: %i\n", ret);
#endif
}

