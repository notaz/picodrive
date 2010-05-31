#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/matroxfb.h>

#include "../common/emu.h"
#include "fbdev.h"

#define FBDEV_MAX_BUFFERS 3

static int fbdev = -1;
static void *fbdev_mem = MAP_FAILED;
static int fbdev_mem_size;
static struct fb_var_screeninfo fbvar_old;
static struct fb_var_screeninfo fbvar_new;
static int fbdev_buffer_write;

void *fbdev_buffers[FBDEV_MAX_BUFFERS];
int fbdev_buffer_count;

void plat_video_flip(void)
{
	int draw_buf;

	if (fbdev_buffer_count < 2)
		return;

	draw_buf = fbdev_buffer_write;
	fbdev_buffer_write++;
	if (fbdev_buffer_write >= fbdev_buffer_count)
		fbdev_buffer_write = 0;

	fbvar_new.yoffset = fbvar_old.yres * draw_buf;
	g_screen_ptr = fbdev_buffers[fbdev_buffer_write];

	ioctl(fbdev, FBIOPAN_DISPLAY, &fbvar_new);
}

void plat_video_wait_vsync(void)
{
	int arg = 0;
	ioctl(fbdev, FBIO_WAITFORVSYNC, &arg);
}

int vout_fbdev_init(int *w, int *h)
{
	static const char *fbdev_name = "/dev/fb0";
	int i, ret;

	fbdev = open(fbdev_name, O_RDWR);
	if (fbdev == -1) {
		fprintf(stderr, "%s: ", fbdev_name);
		perror("open");
		return -1;
	}

	ret = ioctl(fbdev, FBIOGET_VSCREENINFO, &fbvar_old);
	if (ret == -1) {
		perror("FBIOGET_VSCREENINFO ioctl");
		goto fail;
	}

	fbvar_new = fbvar_old;
	printf("%s: %ix%i@%d\n", fbdev_name, fbvar_old.xres, fbvar_old.yres, fbvar_old.bits_per_pixel);
	*w = fbvar_old.xres;
	*h = fbvar_old.yres;
	fbdev_buffer_count = FBDEV_MAX_BUFFERS; // be optimistic

	if (fbvar_new.bits_per_pixel != 16) {
		printf(" switching to 16bpp\n");
		fbvar_new.bits_per_pixel = 16;
		ret = ioctl(fbdev, FBIOPUT_VSCREENINFO, &fbvar_new);
		if (ret == -1) {
			perror("FBIOPUT_VSCREENINFO ioctl");
			goto fail;
		}
	}

	if (fbvar_new.yres_virtual < fbvar_old.yres * fbdev_buffer_count) {
		fbvar_new.yres_virtual = fbvar_old.yres * fbdev_buffer_count;
		ret = ioctl(fbdev, FBIOPUT_VSCREENINFO, &fbvar_new);
		if (ret == -1) {
			fbdev_buffer_count = 1;
			fprintf(stderr, "Warning: failed to increase virtual resolution, "
					"doublebuffering disabled\n");
		}
	}

	fbdev_mem_size = *w * *h * 2 * fbdev_buffer_count;
	fbdev_mem = mmap(0, fbdev_mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev, 0);
	if (fbdev_mem == MAP_FAILED && fbdev_buffer_count > 1) {
		fprintf(stderr, "Warning: can't map %d bytes, doublebuffering disabled\n", fbdev_mem_size);
		fbdev_mem_size = *w * *h * 2;
		fbdev_buffer_count = 1;
		fbdev_mem = mmap(0, fbdev_mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev, 0);
	}
	if (fbdev_mem == MAP_FAILED) {
		perror("mmap framebuffer");
		goto fail;
	}
	memset(fbdev_mem, 0, fbdev_mem_size);
	for (i = 0; i < fbdev_buffer_count; i++)
		fbdev_buffers[i] = (char *)fbdev_mem + i * *w * *h * 2;
	g_screen_ptr = fbdev_buffers[0];

	// some checks
	ret = 0;
	ret = ioctl(fbdev, FBIO_WAITFORVSYNC, &ret);
	if (ret != 0)
		fprintf(stderr, "Warning: vsync doesn't seem to be supported\n");

	if (fbdev_buffer_count > 1) {
		fbdev_buffer_write = 0;
		fbvar_new.yoffset = fbvar_old.yres * (fbdev_buffer_count - 1);
		ret = ioctl(fbdev, FBIOPAN_DISPLAY, &fbvar_new);
		if (ret != 0) {
			fbdev_buffer_count = 1;
			fprintf(stderr, "Warning: can't pan display, doublebuffering disabled\n");
		}
	}

	printf("fbdev initialized.\n");
	return 0;

fail:
	close(fbdev);
	return -1;
}

void vout_fbdev_finish(void)
{
	ioctl(fbdev, FBIOPUT_VSCREENINFO, &fbvar_old);
	if (fbdev_mem != MAP_FAILED)
		munmap(fbdev_mem, fbdev_mem_size);
	if (fbdev >= 0)
		close(fbdev);
	fbdev_mem = NULL;
	fbdev = -1;
}

#if 0
void *g_screen_ptr;
int main()
{
	int w, h;
	vout_fbdev_init(&w, &h);
	//while (1)
	{
		memset(g_screen_ptr, 0xff, fbdev_mem_size / 2);
		plat_video_wait_vsync();
		plat_video_flip();
		memset(g_screen_ptr, 0x00, fbdev_mem_size / 2);
		usleep(8000);
//		plat_video_wait_vsync();
		plat_video_flip();
	}
}
#endif
