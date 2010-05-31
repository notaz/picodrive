#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "../linux/sndout_oss.h"
#include "../linux/fbdev.h"
#include "../linux/oshide.h"
#include "../common/emu.h"

void plat_early_init(void)
{
}

void plat_init(void)
{
	int ret, w, h;

	oshide_init();

	ret = vout_fbdev_init(&w, &h);
	if (ret != 0) {
		fprintf(stderr, "couldn't init framebuffer\n");
		exit(1);
	}

	if (w != g_screen_width || h != g_screen_height) {
		fprintf(stderr, "%dx%d not supported\n", w, h);
		vout_fbdev_finish();
		exit(1);
	}

	// snd
	sndout_oss_init();
}

void plat_finish(void)
{
	sndout_oss_exit();
	vout_fbdev_finish();
	oshide_finish();

	printf("all done\n");
}

/* lprintf */
void lprintf(const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);
}

