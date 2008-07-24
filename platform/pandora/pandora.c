
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <errno.h>

#include "../gp2x/gp2x.h"
#include "../gp2x/usbjoy.h"
#include "../common/arm_utils.h"

static volatile unsigned int *memregs;
//static
int memdev = 0;
static int sounddev = -1, mixerdev = -1, touchdev = -1;
static int touchcal[7] = { 6203, 0, -1501397, 0, -4200, 16132680, 65536 };

//#define SCREEN_MAP_SIZE (((800*(480+11)*2)+0xfff)&~0xfff)
#define SCREEN_MAP_SIZE (800*480*2)
static void *screen;
void *gp2x_screen;


/* video stuff */
void gp2x_video_flip(void)
{
}

/* doulblebuffered flip */
void gp2x_video_flip2(void)
{
}


void gp2x_video_changemode2(int bpp)
{
}


void gp2x_video_changemode(int bpp)
{
}


void gp2x_video_setpalette(int *pal, int len)
{
}


void gp2x_video_RGB_setscaling(int ln_offs, int W, int H)
{
}


void gp2x_video_wait_vsync(void)
{
}


void gp2x_video_flush_cache(void)
{
//	flushcache(gp2x_screen, (char *)gp2x_screen + 320*240*2, 0);
}


void gp2x_memcpy_buffers(int buffers, void *data, int offset, int len)
{
}


void gp2x_memcpy_all_buffers(void *data, int offset, int len)
{
}


void gp2x_memset_all_buffers(int offset, int byte, int len)
{
	memset((char *)gp2x_screen + offset, byte, len);
}


void gp2x_pd_clone_buffer2(void)
{
}


unsigned long gp2x_joystick_read(int allow_usb_joy)
{
  	unsigned long value = 0;
	int i;

	if (allow_usb_joy && num_of_joys > 0) {
		// check the usb joy as well..
		gp2x_usbjoy_update();
		for (i = 0; i < num_of_joys; i++)
			value |= gp2x_usbjoy_check(i);
	}

	return value;
}

typedef struct ucb1x00_ts_event
{
	unsigned short pressure;
	unsigned short x;
	unsigned short y;
	unsigned short pad;
	struct timeval stamp;
} UCB1X00_TS_EVENT;

int gp2x_touchpad_read(int *x, int *y)
{
	UCB1X00_TS_EVENT event;
	static int zero_seen = 0;
	int retval;

	if (touchdev < 0) return -1;

	retval = read(touchdev, &event, sizeof(event));
	if (retval <= 0) {
		printf("touch read failed %i %i\n", retval, errno);
		return -1;
	}
	// this is to ignore the messed-up 4.1.x driver
	if (event.pressure == 0) zero_seen = 1;

	if (x) *x = (event.x * touchcal[0] + touchcal[2]) >> 16;
	if (y) *y = (event.y * touchcal[4] + touchcal[5]) >> 16;
	// printf("read %i %i %i\n", event.pressure, *x, *y);

	return zero_seen ? event.pressure : 0;
}


//static int s_oldrate = 0, s_oldbits = 0, s_oldstereo = 0;

void gp2x_start_sound(int rate, int bits, int stereo)
{
#if 0
	int frag = 0, bsize, buffers;

	// if no settings change, we don't need to do anything
	if (rate == s_oldrate && s_oldbits == bits && s_oldstereo == stereo) return;

	if (sounddev > 0) close(sounddev);
	sounddev = open("/dev/dsp", O_WRONLY|O_ASYNC);
	if (sounddev == -1)
		printf("open(\"/dev/dsp\") failed with %i\n", errno);

	ioctl(sounddev, SNDCTL_DSP_SETFMT, &bits);
	ioctl(sounddev, SNDCTL_DSP_SPEED,  &rate);
	ioctl(sounddev, SNDCTL_DSP_STEREO, &stereo);
	// calculate buffer size
	buffers = 16;
	bsize = rate / 32;
	if (rate > 22050) { bsize*=4; buffers*=2; } // 44k mode seems to be very demanding
	while ((bsize>>=1)) frag++;
	frag |= buffers<<16; // 16 buffers
	ioctl(sounddev, SNDCTL_DSP_SETFRAGMENT, &frag);
	usleep(192*1024);

	printf("gp2x_set_sound: %i/%ibit/%s, %i buffers of %i bytes\n",
		rate, bits, stereo?"stereo":"mono", frag>>16, 1<<(frag&0xffff));

	s_oldrate = rate; s_oldbits = bits; s_oldstereo = stereo;
#endif
}


void gp2x_sound_write(void *buff, int len)
{
//	write(sounddev, buff, len);
}

void gp2x_sound_sync(void)
{
//	ioctl(sounddev, SOUND_PCM_SYNC, 0);
}

void gp2x_sound_volume(int l, int r)
{
#if 0
 	l=l<0?0:l; l=l>255?255:l; r=r<0?0:r; r=r>255?255:r;
 	l<<=8; l|=r;
 	ioctl(mixerdev, SOUND_MIXER_WRITE_PCM, &l); /*SOUND_MIXER_WRITE_VOLUME*/
#endif
}


/* common */
void gp2x_init(void)
{
//	struct fb_fix_screeninfo fbfix;
	int fbdev;

	printf("entering init()\n"); fflush(stdout);

  	memdev = open("/dev/mem", O_RDWR);
	if (memdev == -1)
	{
		printf("open(\"/dev/mem\") failed with %i\n", errno);
		exit(1);
	}

	memregs = mmap(0, 0x01000000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0x48000000);
	if (memregs == MAP_FAILED)
	{
		printf("mmap(memregs) failed with %i\n", errno);
		exit(1);
	}

	fbdev = open("/dev/fb0", O_RDWR);
	if (fbdev == -1)
	{
		printf("open(\"/dev/fb0\") failed with %i\n", errno);
		exit(1);
	}

/*
	ret = ioctl(fbdev, FBIOGET_FSCREENINFO, &fbfix);
	if (ret == -1)
	{
		printf("ioctl(fbdev) failed with %i\n", errno);
		exit(1);
	}
*/
	screen = mmap(0, SCREEN_MAP_SIZE, PROT_WRITE|PROT_READ, MAP_SHARED, fbdev, 0);
	if (screen == MAP_FAILED)
	{
		printf("mmap(fbptr) failed with %i\n", errno);
		exit(1);
	}
	printf("fbptr %p\n", screen);
//	gp2x_screen = (char *)screen + 800*10*2-64;
	gp2x_screen = screen;


	// snd
  	mixerdev = open("/dev/mixer", O_RDWR);
	if (mixerdev == -1)
		printf("open(\"/dev/mixer\") failed with %i\n", errno);

	/* init usb joys -GnoStiC */
	gp2x_usbjoy_init();

	// touchscreen
	touchdev = open("/dev/touchscreen/wm97xx", O_RDONLY);
	if (touchdev >= 0) {
		FILE *pcf = fopen("/etc/pointercal", "r");
		if (pcf) {
			fscanf(pcf, "%d %d %d %d %d %d %d", &touchcal[0], &touchcal[1],
				&touchcal[2], &touchcal[3], &touchcal[4], &touchcal[5], &touchcal[6]);
			fclose(pcf);
		}
		printf("found touchscreen/wm97xx\n");
	}

	printf("exitting init()\n"); fflush(stdout);
}

void gp2x_deinit(void)
{
	//gp2x_video_changemode(15);

	munmap(screen, SCREEN_MAP_SIZE);
	munmap((void *)memregs, 0x10000);
	close(memdev);
	if (mixerdev >= 0) close(mixerdev);
	if (sounddev >= 0) close(sounddev);
	if (touchdev >= 0) close(touchdev);

	gp2x_usbjoy_deinit();

	printf("all done");
}

/* lprintf */
void lprintf(const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);
}


/* fake GP2X */
int crashed_940 = 0;

int readpng(void *dest, const char *fname, int what) { return -1; }
void set_gamma(int g100, int A_SNs_curve) {}
void set_FCLK(unsigned MHZ) {}
void set_LCD_custom_rate(int rate) {}
void unset_LCD_custom_rate(void) {}
void Pause940(int yes) {}
void Reset940(int yes, int bank) {}

