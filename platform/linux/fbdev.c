#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/matroxfb.h>

#include "fbdev.h"

#define FBDEV_MAX_BUFFERS 3

struct vout_fbdev {
	int	fd;
	void	*mem;
	size_t	mem_size;
	struct	fb_var_screeninfo fbvar_old;
	struct	fb_var_screeninfo fbvar_new;
	int	buffer_write;
	void	*buffers[FBDEV_MAX_BUFFERS];
	int	buffer_count;
};

void *vout_fbdev_flip(struct vout_fbdev *fbdev)
{
	int draw_buf;

	if (fbdev->buffer_count < 2)
		return fbdev->mem;

	draw_buf = fbdev->buffer_write;
	fbdev->buffer_write++;
	if (fbdev->buffer_write >= fbdev->buffer_count)
		fbdev->buffer_write = 0;

	fbdev->fbvar_new.yoffset = fbdev->fbvar_old.yres * draw_buf;

	ioctl(fbdev->fd, FBIOPAN_DISPLAY, &fbdev->fbvar_new);

	return fbdev->buffers[fbdev->buffer_write];
}

void vout_fbdev_wait_vsync(struct vout_fbdev *fbdev)
{
	int arg = 0;
	ioctl(fbdev->fd, FBIO_WAITFORVSYNC, &arg);
}

void vout_fbdev_clear(struct vout_fbdev *fbdev)
{
	memset(fbdev->mem, 0, fbdev->mem_size);
}

struct vout_fbdev *vout_fbdev_init(const char *fbdev_name, int *w, int *h, int no_dblbuf)
{
	struct vout_fbdev *fbdev;
	int i, ret;

	fbdev = calloc(1, sizeof(*fbdev));
	if (fbdev == NULL)
		return NULL;

	fbdev->fd = open(fbdev_name, O_RDWR);
	if (fbdev->fd == -1) {
		fprintf(stderr, "%s: ", fbdev_name);
		perror("open");
		goto fail_open;
	}

	ret = ioctl(fbdev->fd, FBIOGET_VSCREENINFO, &fbdev->fbvar_old);
	if (ret == -1) {
		perror("FBIOGET_VSCREENINFO ioctl");
		goto fail;
	}

	fbdev->fbvar_new = fbdev->fbvar_old;
	printf("%s: %ix%i@%d\n", fbdev_name, fbdev->fbvar_old.xres, fbdev->fbvar_old.yres,
		fbdev->fbvar_old.bits_per_pixel);
	*w = fbdev->fbvar_old.xres;
	*h = fbdev->fbvar_old.yres;
	fbdev->buffer_count = FBDEV_MAX_BUFFERS; // be optimistic
	if (no_dblbuf)
		fbdev->buffer_count = 1;

	if (fbdev->fbvar_new.bits_per_pixel != 16) {
		printf(" switching to 16bpp\n");
		fbdev->fbvar_new.bits_per_pixel = 16;
		ret = ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->fbvar_new);
		if (ret == -1) {
			perror("FBIOPUT_VSCREENINFO ioctl");
			goto fail;
		}
	}

	if (fbdev->fbvar_new.yres_virtual < fbdev->fbvar_old.yres * fbdev->buffer_count) {
		fbdev->fbvar_new.yres_virtual = fbdev->fbvar_old.yres * fbdev->buffer_count;
		ret = ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->fbvar_new);
		if (ret == -1) {
			fbdev->buffer_count = 1;
			fprintf(stderr, "Warning: failed to increase virtual resolution, "
					"doublebuffering disabled\n");
		}
	}

	fbdev->mem_size = *w * *h * 2 * fbdev->buffer_count;
	fbdev->mem = mmap(0, fbdev->mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev->fd, 0);
	if (fbdev->mem == MAP_FAILED && fbdev->buffer_count > 1) {
		fprintf(stderr, "Warning: can't map %zd bytes, doublebuffering disabled\n", fbdev->mem_size);
		fbdev->mem_size = *w * *h * 2;
		fbdev->buffer_count = 1;
		fbdev->mem = mmap(0, fbdev->mem_size, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev->fd, 0);
	}
	if (fbdev->mem == MAP_FAILED) {
		perror("mmap framebuffer");
		goto fail;
	}
	memset(fbdev->mem, 0, fbdev->mem_size);
	for (i = 0; i < fbdev->buffer_count; i++)
		fbdev->buffers[i] = (char *)fbdev->mem + i * *w * *h * 2;

	// some checks
	ret = 0;
	ret = ioctl(fbdev->fd, FBIO_WAITFORVSYNC, &ret);
	if (ret != 0)
		fprintf(stderr, "Warning: vsync doesn't seem to be supported\n");

	if (fbdev->buffer_count > 1) {
		fbdev->buffer_write = 0;
		fbdev->fbvar_new.yoffset = fbdev->fbvar_old.yres * (fbdev->buffer_count - 1);
		ret = ioctl(fbdev->fd, FBIOPAN_DISPLAY, &fbdev->fbvar_new);
		if (ret != 0) {
			fbdev->buffer_count = 1;
			fprintf(stderr, "Warning: can't pan display, doublebuffering disabled\n");
		}
	}

	printf("fbdev initialized.\n");
	return fbdev;

fail:
	close(fbdev->fd);
fail_open:
	free(fbdev);
	return NULL;
}

void vout_fbdev_finish(struct vout_fbdev *fbdev)
{
	ioctl(fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->fbvar_old);
	if (fbdev->mem != MAP_FAILED)
		munmap(fbdev->mem, fbdev->mem_size);
	if (fbdev->fd >= 0)
		close(fbdev->fd);
	fbdev->mem = NULL;
	fbdev->fd = -1;
	free(fbdev);
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
