#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "soc.h"

void (*gp2x_video_flip)(void);
void (*gp2x_video_flip2)(void);
void (*gp2x_video_changemode_ll)(int bpp);
void (*gp2x_video_setpalette)(int *pal, int len);
void (*gp2x_video_RGB_setscaling)(int ln_offs, int W, int H);
void (*gp2x_video_wait_vsync)(void);

void (*gp2x_set_cpuclk)(unsigned int mhz);

void (*set_lcd_custom_rate)(int is_pal);
void (*unset_lcd_custom_rate)(void);
void (*set_lcd_gamma)(int g100, int A_SNs_curve);

void (*set_ram_timings)(void);
void (*unset_ram_timings)(void);
int  (*gp2x_read_battery)(void);

unsigned int (*gp2x_get_ticks_ms)(void);
unsigned int (*gp2x_get_ticks_us)(void);


gp2x_soc_t soc_detect(void)
{
	volatile unsigned short *memregs;
	volatile unsigned int *memregl;
	static gp2x_soc_t ret = -2;
	int pollux_chipname[0x30/4 + 1];
	char *pollux_chipname_c = (char *)pollux_chipname;
	int memdev;
	int i;

	if (ret != -2)
		/* already detected */
		return ret;

  	memdev = open("/dev/mem", O_RDONLY);
	if (memdev == -1)
	{
		perror("open(/dev/mem)");
		ret = -1;
		return -1;
	}

	memregs = mmap(0, 0x20000, PROT_READ, MAP_SHARED, memdev, 0xc0000000);
	if (memregs == MAP_FAILED)
	{
		perror("mmap(memregs)");
		close(memdev);
		ret = -1;
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
	return ret;	
}

