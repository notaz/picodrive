#include <stdio.h>
#ifdef __linux__
#include <sys/mman.h>
#endif

#include "cmn.h"

u8 __attribute__((aligned(4096))) tcache[DRC_TCACHE_SIZE];


void drc_cmn_init(void)
{
#ifdef __linux__
	void *tmp;

	tmp = mmap(tcache, DRC_TCACHE_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	printf("mmap tcache: %p, asked %p\n", tmp, tcache);
#endif
}

void drc_cmn_cleanup(void)
{
#ifdef __linux__
	int ret;
	ret = munmap(tcache, DRC_TCACHE_SIZE);
	printf("munmap tcache: %i\n", ret);
#endif
}

