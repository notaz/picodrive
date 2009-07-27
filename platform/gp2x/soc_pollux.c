#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include "soc.h"
#include "plat_gp2x.h"
#include "../common/emu.h"
#include "../common/arm_utils.h"
#include "pollux_set.h"

static volatile unsigned short *memregs;
static volatile unsigned long  *memregl;
static int memdev = -1;

extern void *gp2x_screens[4];

#define fb_buf_count 4
static unsigned int fb_paddr[fb_buf_count];
static int fb_work_buf;
static int fbdev = -1;

static char cpuclk_was_changed = 0;
static unsigned short memtimex_old[2];
static unsigned int pllsetreg0;
static int last_pal_setting = 0;


/* video stuff */
static void pollux_video_flip(int buf_count)
{
	memregl[0x406C>>2] = fb_paddr[fb_work_buf];
	memregl[0x4058>>2] |= 0x10;
	fb_work_buf++;
	if (fb_work_buf >= buf_count)
		fb_work_buf = 0;
	g_screen_ptr = gp2x_screens[fb_work_buf];
}

static void gp2x_video_flip_(void)
{
	pollux_video_flip(fb_buf_count);
}

/* doulblebuffered flip */
static void gp2x_video_flip2_(void)
{
	pollux_video_flip(2);
}

static void gp2x_video_changemode_ll_(int bpp)
{
	static int prev_bpp = 0;
	int code = 0, bytes = 2;
	int rot_cmd[2] = { 0, 0 };
	unsigned int r;
	int ret;

	if (bpp == prev_bpp)
		return;
	prev_bpp = bpp;

	printf("changemode: %dbpp rot=%d\n", abs(bpp), bpp < 0);

	/* negative bpp means rotated mode */
	rot_cmd[0] = (bpp < 0) ? 6 : 5;
	ret = ioctl(fbdev, _IOW('D', 90, int[2]), rot_cmd);
	if (ret < 0)
		perror("rot ioctl failed");
	memregl[0x4004>>2] = (bpp < 0) ? 0x013f00ef : 0x00ef013f;
	memregl[0x4000>>2] |= 1 << 3;

	/* the above ioctl resets LCD timings, so set them here */
	set_lcd_custom_rate(last_pal_setting);

	switch (abs(bpp))
	{
		case 8:
			code = 0x443a;
			bytes = 1;
			break;

		case 15:
		case 16:
			code = 0x4432;
			bytes = 2;
			break;

		default:
			printf("unhandled bpp request: %d\n", abs(bpp));
			return;
	}

	memregl[0x405c>>2] = bytes;
	memregl[0x4060>>2] = bytes * (bpp < 0 ? 240 : 320);

	r = memregl[0x4058>>2];
	r = (r & 0xffff) | (code << 16) | 0x10;
	memregl[0x4058>>2] = r;
}

static void gp2x_video_setpalette_(int *pal, int len)
{
	/* pollux palette is 16bpp only.. */
	int i;
	for (i = 0; i < len; i++)
	{
		int c = pal[i];
		c = ((c >> 8) & 0xf800) | ((c >> 5) & 0x07c0) | ((c >> 3) & 0x001f);
		memregl[0x4070>>2] = (i << 24) | c;
	}
}

static void gp2x_video_RGB_setscaling_(int ln_offs, int W, int H)
{
	/* maybe a job for 3d hardware? */
}

static void gp2x_video_wait_vsync_(void)
{
	while (!(memregl[0x308c>>2] & (1 << 10)));
		spend_cycles(128);
	memregl[0x308c>>2] |= 1 << 10;
}

/* CPU clock */
static void gp2x_set_cpuclk_(unsigned int mhz)
{
	char buff[24];
	snprintf(buff, sizeof(buff), "cpuclk=%u", mhz);
	pollux_set(memregs, buff);

	cpuclk_was_changed = 1;
}

/* misc */
static void pollux_set_fromenv(const char *env_var)
{
	const char *set_string;
	set_string = getenv(env_var);
	if (set_string)
		pollux_set(memregs, set_string);
	else
		printf("env var %s not defined.\n", env_var);
}

/* RAM timings */
static void set_ram_timings_(void)
{
	pollux_set_fromenv("POLLUX_RAM_TIMINGS");
}

static void unset_ram_timings_(void)
{
	int i;

	memregs[0x14802>>1] = memtimex_old[0];
	memregs[0x14804>>1] = memtimex_old[1] | 0x8000;

	for (i = 0; i < 0x100000; i++)
		if (!(memregs[0x14804>>1] & 0x8000))
			break;

	printf("RAM timings reset to startup values.\n");
}

/* LCD refresh */
static void set_lcd_custom_rate_(int is_pal)
{
	char buff[32];

	snprintf(buff, sizeof(buff), "POLLUX_LCD_TIMINGS_%s", is_pal ? "PAL" : "NTSC");
	pollux_set_fromenv(buff);
	last_pal_setting = is_pal;
}

static void unset_lcd_custom_rate_(void)
{
}

static void set_lcd_gamma_(int g100, int A_SNs_curve)
{
	/* hm, the LCD possibly can do it (but not POLLUX) */
}

void pollux_init(void)
{
	struct fb_fix_screeninfo fbfix;
	int i, ret;

	memdev = open("/dev/mem", O_RDWR);
	if (memdev == -1) {
		perror("open(/dev/mem) failed");
		exit(1);
	}

	memregs	= mmap(0, 0x20000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);
	if (memregs == MAP_FAILED) {
		perror("mmap(memregs) failed");
		exit(1);
	}
	memregl = (volatile void *)memregs;

	fbdev = open("/dev/fb0", O_RDWR);
	if (fbdev < 0) {
		perror("can't open fbdev");
		exit(1);
	}

	ret = ioctl(fbdev, FBIOGET_FSCREENINFO, &fbfix);
	if (ret == -1) {
		perror("ioctl(fbdev) failed");
		exit(1);
	}

	printf("framebuffer: \"%s\" @ %08lx\n", fbfix.id, fbfix.smem_start);
	fb_paddr[0] = fbfix.smem_start;

	gp2x_screens[0] = mmap(0, 320*240*2*fb_buf_count, PROT_READ|PROT_WRITE,
			MAP_SHARED, memdev, fb_paddr[0]);
	if (gp2x_screens[0] == MAP_FAILED)
	{
		perror("mmap(gp2x_screens) failed");
		exit(1);
	}
	memset(gp2x_screens[0], 0, 320*240*2*fb_buf_count);

	printf("  %p -> %08x\n", gp2x_screens[0], fb_paddr[0]);
	for (i = 1; i < fb_buf_count; i++)
	{
		fb_paddr[i] = fb_paddr[i-1] + 320*240*2;
		gp2x_screens[i] = (char *)gp2x_screens[i-1] + 320*240*2;
		printf("  %p -> %08x\n", gp2x_screens[i], fb_paddr[i]);
	}
	fb_work_buf = 0;
	g_screen_ptr = gp2x_screens[0];

	pllsetreg0 = memregl[0xf004];
	memtimex_old[0] = memregs[0x14802>>1];
	memtimex_old[1] = memregs[0x14804>>1];

	gp2x_video_flip = gp2x_video_flip_;
	gp2x_video_flip2 = gp2x_video_flip2_;
	gp2x_video_changemode_ll = gp2x_video_changemode_ll_;
	gp2x_video_setpalette = gp2x_video_setpalette_;
	gp2x_video_RGB_setscaling = gp2x_video_RGB_setscaling_;
	gp2x_video_wait_vsync = gp2x_video_wait_vsync_;

	gp2x_set_cpuclk = gp2x_set_cpuclk_;

	set_lcd_custom_rate = set_lcd_custom_rate_;
	unset_lcd_custom_rate = unset_lcd_custom_rate_;
	set_lcd_gamma = set_lcd_gamma_;

	set_ram_timings = set_ram_timings_;
	unset_ram_timings = unset_ram_timings_;
}

void pollux_finish(void)
{
	/* switch to default fb mem, turn portrait off */
	memregl[0x406C>>2] = fb_paddr[0];
	memregl[0x4058>>2] |= 0x10;
	close(fbdev);

	gp2x_video_changemode_ll_(16);
	unset_ram_timings_();
	if (cpuclk_was_changed) {
		memregl[0xf004>>2] = pllsetreg0;
		memregl[0xf07c>>2] |= 0x8000;
	}

	munmap((void *)memregs, 0x20000);
	close(memdev);
}

