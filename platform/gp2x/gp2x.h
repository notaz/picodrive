#ifndef __GP2X_H__
#define __GP2X_H__

/* video */
void gp2x_video_changemode(int bpp);
void gp2x_memcpy_all_buffers(void *data, int offset, int len);
void gp2x_memset_all_buffers(int offset, int byte, int len);

/* input */
int gp2x_touchpad_read(int *x, int *y);

#endif
