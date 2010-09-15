#ifndef __GP2X_H__
#define __GP2X_H__

extern int default_cpu_clock;

/* video */
void gp2x_video_changemode(int bpp);
void gp2x_memcpy_all_buffers(void *data, int offset, int len);
void gp2x_memset_all_buffers(int offset, int byte, int len);
void gp2x_make_fb_bufferable(int yes);

/* input */
int gp2x_touchpad_read(int *x, int *y);

/* misc */
enum {
	GP2X_DEV_GP2X = 1,
	GP2X_DEV_WIZ,
	GP2X_DEV_CAANOO,
};
extern int gp2x_dev_id;
extern int gp2x_current_bpp;

unsigned int plat_get_ticks_ms_good(void);
unsigned int plat_get_ticks_us_good(void);

void gp2x_menu_init(void);

#endif
