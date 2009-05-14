
#ifndef __GP2X_H__
#define __GP2X_H__


void gp2x_init(void);
void gp2x_deinit(void);

/* video */
void gp2x_video_flip(void);
void gp2x_video_flip2(void);
void gp2x_video_changemode(int bpp);
void gp2x_video_changemode2(int bpp);
void gp2x_video_setpalette(int *pal, int len);
void gp2x_video_RGB_setscaling(int ln_offs, int W, int H);
void gp2x_video_wait_vsync(void);
void gp2x_video_flush_cache(void);
void gp2x_memcpy_buffers(int buffers, void *data, int offset, int len);
void gp2x_memcpy_all_buffers(void *data, int offset, int len);
void gp2x_memset_all_buffers(int offset, int byte, int len);
void gp2x_pd_clone_buffer2(void);

/* input */
int gp2x_touchpad_read(int *x, int *y);

/* 940 core */
void Pause940(int yes);
void Reset940(int yes, int bank);


extern int memdev;


#endif
