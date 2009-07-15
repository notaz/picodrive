#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "soc.h"
#include "../common/emu.h"

gp2x_soc_t gp2x_soc = -1;

gp2x_soc_t soc_detect(void)
{
	volatile unsigned short *memregs;
	volatile unsigned int *memregl;
	int pollux_chipname[0x30/4 + 1];
	char *pollux_chipname_c = (char *)pollux_chipname;
	gp2x_soc_t ret = -1;
	int memdev;
	int i;

  	memdev = open("/dev/mem", O_RDONLY);
	if (memdev == -1)
	{
		perror("open(/dev/mem)");
		return -1;
	}

	memregs = mmap(0, 0x20000, PROT_READ, MAP_SHARED, memdev, 0xc0000000);
	if (memregs == MAP_FAILED)
	{
		perror("mmap(memregs)");
		close(memdev);
		return -1;
	}
	memregl = (volatile void *)memregs;

	if (memregs[0x1836>>1] == 0x2330)
	{
		printf("looks like this is MMSP2\n");
		ret = SOCID_MMSP2;
		goto out;
	}

	/* perform word reads. Byte reads might also work,
	 * but we don't want to play with that. */
	for (i = 0; i < 0x30; i += 4)
	{
		pollux_chipname[i >> 2] = memregl[(0x1f810 + i) >> 2];
	}
	pollux_chipname_c[0x30] = 0;

	for (i = 0; i < 0x30; i++)
	{
		unsigned char c = pollux_chipname_c[i];
		if (c < 0x20 || c > 0x7f)
			goto not_pollux_like;
	}

	printf("found pollux-like id: \"%s\"\n", pollux_chipname_c);

	if (strncmp(pollux_chipname_c, "MAGICEYES-LEAPFROG-LF1000", 25) ||
		strncmp(pollux_chipname_c, "MAGICEYES-POLLUX", 16))
	{
		ret = SOCID_POLLUX;
		goto out;
	}

not_pollux_like:
out:
	munmap((void *)memregs, 0x20000);
	close(memdev);
	gp2x_soc = ret;
	return ret;	
}

