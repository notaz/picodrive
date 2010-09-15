/* dummy code for qemu testing, etc */
#include <stdlib.h>

#include "soc.h"
#include "../common/emu.h"

extern void *gp2x_screens[4];

extern unsigned int plat_get_ticks_ms_good(void);
extern unsigned int plat_get_ticks_us_good(void);

/* video stuff */
static void gp2x_video_flip_(void)
{
}

/* doulblebuffered flip */
static void gp2x_video_flip2_(void)
{
}

static void gp2x_video_changemode_ll_(int bpp)
{
}

static void gp2x_video_setpalette_(int *pal, int len)
{
}

static void gp2x_video_RGB_setscaling_(int ln_offs, int W, int H)
{
}

static void gp2x_video_wait_vsync_(void)
{
}

/* RAM timings */
static void set_ram_timings_(void)
{
}

static void unset_ram_timings_(void)
{
}

/* LCD refresh */
static void set_lcd_custom_rate_(int is_pal)
{
}

static void unset_lcd_custom_rate_(void)
{
}

static void set_lcd_gamma_(int g100, int A_SNs_curve)
{
}

static int gp2x_read_battery_(void)
{
	return 0;
}

void dummy_init(void)
{
	int i;
	g_screen_ptr = malloc(320 * 240 * 2);
	for (i = 0; i < array_size(gp2x_screens); i++)
		gp2x_screens[i] = g_screen_ptr;

	gp2x_video_flip = gp2x_video_flip_;
	gp2x_video_flip2 = gp2x_video_flip2_;
	gp2x_video_changemode_ll = gp2x_video_changemode_ll_;
	gp2x_video_setpalette = gp2x_video_setpalette_;
	gp2x_video_RGB_setscaling = gp2x_video_RGB_setscaling_;
	gp2x_video_wait_vsync = gp2x_video_wait_vsync_;

	set_lcd_custom_rate = set_lcd_custom_rate_;
	unset_lcd_custom_rate = unset_lcd_custom_rate_;
	set_lcd_gamma = set_lcd_gamma_;

	set_ram_timings = set_ram_timings_;
	unset_ram_timings = unset_ram_timings_;
	gp2x_read_battery = gp2x_read_battery_;

	gp2x_get_ticks_ms = plat_get_ticks_ms_good;
	gp2x_get_ticks_us = plat_get_ticks_us_good;
}

void dummy_finish(void)
{
	free(gp2x_screens[0]);
}

