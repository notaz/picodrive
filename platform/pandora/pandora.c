#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <errno.h>

#include "../linux/sndout_oss.h"
#include "../common/arm_linux.h"
#include "../common/emu.h"
#include "pandora.h"

static int fbdev = -1;

#define SCREEN_MAP_SIZE (800*480*2)
static void *screen = MAP_FAILED;

/* common */
void pnd_init(void)
{
	printf("entering init()\n"); fflush(stdout);

	fbdev = open("/dev/fb0", O_RDWR);
	if (fbdev == -1)
	{
		perror("open(\"/dev/fb0\")");
		exit(1);
	}

	screen = mmap(0, SCREEN_MAP_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev, 0);
	if (screen == MAP_FAILED)
	{
		perror("mmap(fbptr)");
		exit(1);
	}
	printf("fbptr %p\n", screen);
	g_screen_ptr = screen;

	// snd
	sndout_oss_init();

	printf("exitting init()\n"); fflush(stdout);
}

void pnd_exit(void)
{
	if (screen != MAP_FAILED)
		munmap(screen, SCREEN_MAP_SIZE);
	if (fbdev >= 0)
		close(fbdev);

	sndout_oss_exit();

	printf("all done");
}

/* lprintf */
void lprintf(const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);
}


/* fake GP2X */
/*
void set_gamma(int g100, int A_SNs_curve) {}
void set_FCLK(unsigned MHZ) {}
void set_LCD_custom_rate(int rate) {}
void unset_LCD_custom_rate(void) {}
void Pause940(int yes) {}
void Reset940(int yes, int bank) {}
*/
