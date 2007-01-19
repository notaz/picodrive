
#ifndef __GP2X_H__
#define __GP2X_H__


void gp2x_init(void);
void gp2x_deinit(void);

/* video */
void gp2x_video_flip(void);
void gp2x_video_changemode(int bpp);
void gp2x_video_setpalette(int *pal, int len);
void gp2x_video_RGB_setscaling(int W, int H);
void gp2x_video_wait_vsync(void);
void gp2x_memcpy_all_buffers(void *data, int offset, int len);
void gp2x_memset_all_buffers(int offset, int byte, int len);

/* sound */
void gp2x_start_sound(int rate, int bits, int stereo);
void gp2x_sound_write(void *buff, int len);
void gp2x_sound_volume(int l, int r);

/* joy */
unsigned long gp2x_joystick_read(int allow_usb_joy);

/* 940 core */
void Pause940(int yes);
void Reset940(int yes, int bank);


extern void *gp2x_screen;
extern int memdev;


enum  { GP2X_UP=0x1,       GP2X_LEFT=0x4,       GP2X_DOWN=0x10,  GP2X_RIGHT=0x40,
        GP2X_START=1<<8,   GP2X_SELECT=1<<9,    GP2X_L=1<<10,    GP2X_R=1<<11,
        GP2X_A=1<<12,      GP2X_B=1<<13,        GP2X_X=1<<14,    GP2X_Y=1<<15,
        GP2X_VOL_UP=1<<23, GP2X_VOL_DOWN=1<<22, GP2X_PUSH=1<<27 };

#endif
