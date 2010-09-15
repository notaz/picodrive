#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "plat_gp2x.h"
#include "soc.h"
#include "warm.h"
#include "../common/plat.h"
#include "../common/readpng.h"
#include "../common/menu.h"
#include "../common/emu.h"
#include "../linux/sndout_oss.h"

#include <pico/pico.h>

/* GP2X local */
int default_cpu_clock;
int gp2x_dev_id;
int gp2x_current_bpp;
void *gp2x_screens[4];

void gp2x_video_changemode(int bpp)
{
	gp2x_video_changemode_ll(bpp);

	gp2x_current_bpp = bpp < 0 ? -bpp : bpp;
}

static void gp2x_memcpy_buffers(int buffers, void *data, int offset, int len)
{
	char *dst;
	if (buffers & (1<<0)) { dst = (char *)gp2x_screens[0] + offset; if (dst != data) memcpy(dst, data, len); }
	if (buffers & (1<<1)) { dst = (char *)gp2x_screens[1] + offset; if (dst != data) memcpy(dst, data, len); }
	if (buffers & (1<<2)) { dst = (char *)gp2x_screens[2] + offset; if (dst != data) memcpy(dst, data, len); }
	if (buffers & (1<<3)) { dst = (char *)gp2x_screens[3] + offset; if (dst != data) memcpy(dst, data, len); }
}

void gp2x_memcpy_all_buffers(void *data, int offset, int len)
{
	gp2x_memcpy_buffers(0xf, data, offset, len);
}

void gp2x_memset_all_buffers(int offset, int byte, int len)
{
	memset((char *)gp2x_screens[0] + offset, byte, len);
	memset((char *)gp2x_screens[1] + offset, byte, len);
	memset((char *)gp2x_screens[2] + offset, byte, len);
	memset((char *)gp2x_screens[3] + offset, byte, len);
}

void gp2x_make_fb_bufferable(int yes)
{
	int ret = 0;
	
	yes = yes ? 1 : 0;
	ret |= warm_change_cb_range(WCB_B_BIT, yes, gp2x_screens[0], 320*240*2);
	ret |= warm_change_cb_range(WCB_B_BIT, yes, gp2x_screens[1], 320*240*2);
	ret |= warm_change_cb_range(WCB_B_BIT, yes, gp2x_screens[2], 320*240*2);
	ret |= warm_change_cb_range(WCB_B_BIT, yes, gp2x_screens[3], 320*240*2);

	if (ret)
		fprintf(stderr, "could not make fb buferable.\n");
	else
		printf("made fb buferable.\n");
}

/* common */
void plat_video_menu_enter(int is_rom_loaded)
{
	if (gp2x_current_bpp != 16 || gp2x_dev_id == GP2X_DEV_WIZ) {
		/* try to switch nicely avoiding glitches */
		gp2x_video_wait_vsync();
		memset(gp2x_screens[0], 0, 320*240*2);
		memset(gp2x_screens[1], 0, 320*240*2);
		gp2x_video_flip2(); // might flip to fb2/3
		gp2x_video_flip2(); // ..so we do it again
		// gp2x_video_wait_vsync();
	}

	// switch to 16bpp
	gp2x_video_changemode_ll(16);
	gp2x_video_RGB_setscaling(0, 320, 240);
}

void plat_video_menu_begin(void)
{
	memcpy(g_screen_ptr, gp2x_screens[2], 320*240*2);
	g_menuscreen_ptr = g_screen_ptr;
}

void plat_video_menu_end(void)
{
	// FIXME
	// gp2x_video_flush_cache();
	gp2x_video_flip2();
}

void plat_early_init(void)
{
	gp2x_soc_t soc;
	FILE *f;

	soc = soc_detect();
	switch (soc)
	{
	case SOCID_MMSP2:
		default_cpu_clock = 200;
		gp2x_dev_id = GP2X_DEV_GP2X;
		break;
	case SOCID_POLLUX:
		default_cpu_clock = 533;
		f = fopen("/dev/accel", "rb");
		if (f) {
			printf("detected Caanoo\n");
			gp2x_dev_id = GP2X_DEV_CAANOO;
			fclose(f);
		}
		else {
			printf("detected Wiz\n");
			gp2x_dev_id = GP2X_DEV_WIZ;
		}
		break;
	default:
		printf("could not recognize SoC, running in dummy mode.\n");
		break;
	}

	gp2x_menu_init();
}

void plat_init(void)
{
	gp2x_soc_t soc;

	soc = soc_detect();
	switch (soc)
	{
	case SOCID_MMSP2:
		mmsp2_init();
		break;
	case SOCID_POLLUX:
		pollux_init();
		break;
	default:
		dummy_init();
		break;
	}

	warm_init();

	gp2x_memset_all_buffers(0, 0, 320*240*2);

	// use buffer2 for menubg to save mem (using only buffers 0, 1 in menu)
	g_menubg_ptr = gp2x_screens[2];

	// snd
	sndout_oss_init();
}

void plat_finish(void)
{
	gp2x_soc_t soc;

	warm_finish();

	soc = soc_detect();
	switch (soc)
	{
	case SOCID_MMSP2:
		mmsp2_finish();
		break;
	case SOCID_POLLUX:
		pollux_finish();
		break;
	default:
		dummy_finish();
		break;
	}

	sndout_oss_exit();
}

void lprintf(const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);
}

