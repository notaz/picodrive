
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

#include "../gp2x/gp2x.h"
#include "../linux/sndout_oss.h"
#include "../common/arm_linux.h"

static volatile unsigned int *memregs = MAP_FAILED;
//static
int memdev = 0;
static int fbdev = -1;

#define SCREEN_MAP_SIZE (800*480*2)
static void *screen = MAP_FAILED;
void *gp2x_screen;


/* video stuff */
void gp2x_video_flip(void)
{
}

/* doulblebuffered flip */
void gp2x_video_flip2(void)
{
}


void gp2x_video_changemode2(int bpp)
{
}


void gp2x_video_changemode(int bpp)
{
}


void gp2x_video_setpalette(int *pal, int len)
{
}


void gp2x_video_RGB_setscaling(int ln_offs, int W, int H)
{
}


void gp2x_video_wait_vsync(void)
{
}

void gp2x_video_flush_cache(void)
{
//	cache_flush_d_inval_i(gp2x_screen, (char *)gp2x_screen + 320*240*2, 0);
}

void gp2x_memcpy_buffers(int buffers, void *data, int offset, int len)
{
}


void gp2x_memcpy_all_buffers(void *data, int offset, int len)
{
}


void gp2x_memset_all_buffers(int offset, int byte, int len)
{
	memset((char *)gp2x_screen + offset, byte, len);
}


void gp2x_pd_clone_buffer2(void)
{
	memset(gp2x_screen, 0, 800*480*2);
}

// FIXME
#if 0
static int touchcal[7] = { 6203, 0, -1501397, 0, -4200, 16132680, 65536 };

typedef struct ucb1x00_ts_event
{
	unsigned short pressure;
	unsigned short x;
	unsigned short y;
	unsigned short pad;
	struct timeval stamp;
} UCB1X00_TS_EVENT;

int gp2x_touchpad_read(int *x, int *y)
{
	UCB1X00_TS_EVENT event;
	static int zero_seen = 0;
	int retval;

	if (touchdev < 0) return -1;

	retval = read(touchdev, &event, sizeof(event));
	if (retval <= 0) {
		printf("touch read failed %i %i\n", retval, errno);
		return -1;
	}
	// this is to ignore the messed-up 4.1.x driver
	if (event.pressure == 0) zero_seen = 1;

	if (x) *x = (event.x * touchcal[0] + touchcal[2]) >> 16;
	if (y) *y = (event.y * touchcal[4] + touchcal[5]) >> 16;
	// printf("read %i %i %i\n", event.pressure, *x, *y);

	return zero_seen ? event.pressure : 0;
}
#else
int gp2x_touchpad_read(int *x, int *y) { return -1; }
#endif

/* common */
void gp2x_init(void)
{
	printf("entering init()\n"); fflush(stdout);

  	memdev = open("/dev/mem", O_RDWR);
	if (memdev == -1)
	{
		perror("open(\"/dev/mem\")");
		exit(1);
	}
/*
	memregs = mmap(0, 0x01000000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0x48000000);
	if (memregs == MAP_FAILED)
	{
		printf("mmap(memregs) failed with %i\n", errno);
		exit(1);
	}
*/
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
	gp2x_screen = screen;

	// snd
	sndout_oss_init();

	printf("exitting init()\n"); fflush(stdout);
}

void gp2x_deinit(void)
{
	if (screen != MAP_FAILED)
		munmap(screen, SCREEN_MAP_SIZE);
	if (memregs != MAP_FAILED)
		munmap((void *)memregs, 0x10000);
	close(memdev);
	if (fbdev >= 0)    close(fbdev);

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
int crashed_940 = 0;

void set_gamma(int g100, int A_SNs_curve) {}
void set_FCLK(unsigned MHZ) {}
void set_LCD_custom_rate(int rate) {}
void unset_LCD_custom_rate(void) {}
void Pause940(int yes) {}
void Reset940(int yes, int bank) {}

