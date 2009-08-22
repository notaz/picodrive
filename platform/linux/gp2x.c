/* faking/emulating gp2x by using xlib */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <semaphore.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "../gp2x/version.h"
#include "../common/emu.h"
#include "../common/menu.h"
#include "../common/readpng.h"
#include "sndout_oss.h"

#include "log_io.h"

unsigned long current_keys = 0;
static int current_bpp = 8;
static int current_pal[256];
static const char *verstring = "PicoDrive " VERSION;
static int scr_changed = 0, scr_w = SCREEN_WIDTH, scr_h = SCREEN_HEIGHT;
void *gp2x_screens[4];

// dummies
int mix_32_to_16l_level;
int crashed_940 = 0;
int default_cpu_clock = 123;
void *gp2x_memregs = NULL;

/* faking GP2X pad */
enum  { GP2X_UP=0x1,       GP2X_LEFT=0x4,       GP2X_DOWN=0x10,  GP2X_RIGHT=0x40,
        GP2X_START=1<<8,   GP2X_SELECT=1<<9,    GP2X_L=1<<10,    GP2X_R=1<<11,
        GP2X_A=1<<12,      GP2X_B=1<<13,        GP2X_X=1<<14,    GP2X_Y=1<<15,
        GP2X_VOL_UP=1<<23, GP2X_VOL_DOWN=1<<22, GP2X_PUSH=1<<27 };

static void key_press_event(int keycode)
{
	switch (keycode)
	{
		case 111:
		case 0x62: current_keys |= GP2X_UP;    break;
		case 116:
		case 0x68: current_keys |= GP2X_DOWN;  break;
		case 113:
		case 0x64: current_keys |= GP2X_LEFT;  break;
		case 114:
		case 0x66: current_keys |= GP2X_RIGHT; break;
		case 0x24: current_keys |= GP2X_START; break; // enter
		case 0x23: current_keys |= GP2X_SELECT;break; // ]
		case 0x34: current_keys |= GP2X_A;     break; // z
		case 0x35: current_keys |= GP2X_X;     break; // x
		case 0x36: current_keys |= GP2X_B;     break; // c
		case 0x37: current_keys |= GP2X_Y;     break; // v
		case 0x27: current_keys |= GP2X_L;     break; // s
		case 0x28: current_keys |= GP2X_R;     break; // d
		case 0x29: current_keys |= GP2X_PUSH;  break; // f
		case 0x18: current_keys |= GP2X_VOL_DOWN;break; // q
		case 0x19: current_keys |= GP2X_VOL_UP;break; // w
		case 0x2d: log_io_clear(); break; // k
		case 0x2e: log_io_dump();  break; // l
		case 0x17: { // tab
			extern int PicoReset(void);
			PicoReset();
			break;
		}
	}
}

static void key_release_event(int keycode)
{
	switch (keycode)
	{
		case 111:
		case 0x62: current_keys &= ~GP2X_UP;    break;
		case 116:
		case 0x68: current_keys &= ~GP2X_DOWN;  break;
		case 113:
		case 0x64: current_keys &= ~GP2X_LEFT;  break;
		case 114:
		case 0x66: current_keys &= ~GP2X_RIGHT; break;
		case 0x24: current_keys &= ~GP2X_START; break; // enter
		case 0x23: current_keys &= ~GP2X_SELECT;break; // ]
		case 0x34: current_keys &= ~GP2X_A;     break; // z
		case 0x35: current_keys &= ~GP2X_X;     break; // x
		case 0x36: current_keys &= ~GP2X_B;     break; // c
		case 0x37: current_keys &= ~GP2X_Y;     break; // v
		case 0x27: current_keys &= ~GP2X_L;     break; // s
		case 0x28: current_keys &= ~GP2X_R;     break; // d
		case 0x29: current_keys &= ~GP2X_PUSH;  break; // f
		case 0x18: current_keys &= ~GP2X_VOL_DOWN;break; // q
		case 0x19: current_keys &= ~GP2X_VOL_UP;break; // w
	}
}

/* --- */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Display *xlib_display;
static Window xlib_window;
static XImage *ximage;

static void ximage_realloc(Display *display, Visual *visual)
{
	void *xlib_screen;

	XLockDisplay(xlib_display);

	if (ximage != NULL)
		XDestroyImage(ximage);
	ximage = NULL;

	xlib_screen = calloc(scr_w * scr_h, 4);
	if (xlib_screen != NULL)
		ximage = XCreateImage(display, visual, 24, ZPixmap, 0,
				xlib_screen, scr_w, scr_h, 32, 0);
	if (ximage == NULL)
		fprintf(stderr, "failed to alloc ximage\n");

	XUnlockDisplay(xlib_display);
}

static void xlib_update(void)
{
	Status xstatus;

	XLockDisplay(xlib_display);

	xstatus = XPutImage(xlib_display, xlib_window, DefaultGC(xlib_display, 0), ximage,
		0, 0, 0, 0, g_screen_width, g_screen_height);
	if (xstatus != 0)
		fprintf(stderr, "XPutImage %d\n", xstatus);

	XUnlockDisplay(xlib_display);
}

static void *xlib_threadf(void *targ)
{
	unsigned int width, height, display_width, display_height;
	sem_t *sem = targ;
	XTextProperty windowName;
	Window win;
	XEvent report;
	Display *display;
	Visual *visual;
	int screen;

	XInitThreads();

	xlib_display = display = XOpenDisplay(NULL);
	if (display == NULL)
	{
		fprintf(stderr, "cannot connect to X server %s\n",
				XDisplayName(NULL));
		sem_post(sem);
		return NULL;
	}

	visual = DefaultVisual(display, 0);
	if (visual->class != TrueColor)
	{
		fprintf(stderr, "cannot handle non true color visual\n");
		XCloseDisplay(display);
		sem_post(sem);
		return NULL;
	}

	printf("X vendor: %s, rel: %d, display: %s, protocol ver: %d.%d\n", ServerVendor(display),
		VendorRelease(display), DisplayString(display), ProtocolVersion(display),
		ProtocolRevision(display));

	screen = DefaultScreen(display);

	ximage_realloc(display, visual);
	sem_post(sem);

	display_width = DisplayWidth(display, screen);
	display_height = DisplayHeight(display, screen);

	xlib_window = win = XCreateSimpleWindow(display,
			RootWindow(display, screen),
			display_width / 2 - scr_w / 2,
			display_height / 2 - scr_h / 2,
			scr_w + 2, scr_h + 2, 1,
			BlackPixel(display, screen),
			BlackPixel(display, screen));

	XStringListToTextProperty((char **)&verstring, 1, &windowName);
	XSetWMName(display, win, &windowName);

	XSelectInput(display, win, ExposureMask |
			KeyPressMask |
			ButtonPressMask |
			StructureNotifyMask);

	XMapWindow(display, win);

	while (1)
	{
		XNextEvent(display, &report);
		switch (report.type)
		{
			case Expose:
				while (XCheckTypedEvent(display, Expose, &report))
					;
				xlib_update();
				break;

			case ConfigureNotify:
				width = report.xconfigure.width;
				height = report.xconfigure.height;
				if (scr_w != width - 2 || scr_h != height - 2) {
					scr_w = width - 2;
					scr_h = height - 2;
					scr_changed = 1;
				}
				break;

			case ButtonPress:
				break;

			case KeyPress:
				key_press_event(report.xkey.keycode);
				break;

			case KeyRelease:
				key_release_event(report.xkey.keycode);
				break;

			default:
				break;
		}
	}
}

static void xlib_init(void)
{
	pthread_t x_thread;
	sem_t xlib_sem;

	sem_init(&xlib_sem, 0, 0);

	pthread_create(&x_thread, NULL, xlib_threadf, &xlib_sem);
	pthread_detach(x_thread);

	sem_wait(&xlib_sem);
	sem_destroy(&xlib_sem);
}

/* --- */

static void realloc_screen(void)
{
	void *old = g_screen_ptr;
	int i;
	g_screen_width = scr_w;
	g_screen_height = scr_h;
	g_screen_ptr = calloc(g_screen_width * g_screen_height * 2, 1);
	free(old);
	scr_changed = 0;

	for (i = 0; i < 4; i++)
		gp2x_screens[i] = g_screen_ptr;
}

/* gp2x/emu.c stuff, most to be rm'd */
static void gp2x_video_flip_(void)
{
	unsigned int *image;
	int pixel_count, i;

	if (ximage == NULL)
		return;

	pixel_count = g_screen_width * g_screen_height;
	image = (void *)ximage->data;

	if (current_bpp == 8)
	{
		unsigned char *pixels = g_screen_ptr;
		int pix;

		for (i = 0; i < pixel_count; i++)
		{
			pix = current_pal[pixels[i]];
			image[i] = pix;
		}
	}
	else
	{
		unsigned short *pixels = g_screen_ptr;

		for (i = 0; i < pixel_count; i++)
		{
			/*  in:           rrrr rggg gggb bbbb */
			/* out: rrrr r000 gggg gg00 bbbb b000 */
			image[i]  = (pixels[i] << 8) & 0xf80000;
			image[i] |= (pixels[i] << 5) & 0x00fc00;
			image[i] |= (pixels[i] << 3) & 0x0000f8;
		}
	}
	xlib_update();

	if (scr_changed) {
		realloc_screen();
		ximage_realloc(xlib_display, DefaultVisual(xlib_display, 0));
	}
}

static void gp2x_video_changemode_ll_(int bpp)
{
	current_bpp = bpp;
}

static void gp2x_video_setpalette_(int *pal, int len)
{
	memcpy(current_pal, pal, len*4);
}

void gp2x_memcpy_all_buffers(void *data, int offset, int len)
{
}

void gp2x_memset_all_buffers(int offset, int byte, int len)
{
	memset((char *)g_screen_ptr + offset, byte, len);
}

void gp2x_video_changemode(int bpp)
{
	gp2x_video_changemode_ll_(bpp);
}

void gp2x_make_fb_bufferable(int yes)
{
}

int soc_detect(void)
{
	return 0;
}

/* plat */
static char menu_bg_buffer[320*240*2];
char cpu_clk_name[16] = "GP2X CPU clocks";

void plat_video_menu_enter(int is_rom_loaded)
{
	if (is_rom_loaded)
	{
		// darken the active framebuffer
		memset(g_screen_ptr, 0, 320*8*2);
		menu_darken_bg((char *)g_screen_ptr + 320*8*2, 320*224, 1);
		memset((char *)g_screen_ptr + 320*232*2, 0, 320*8*2);
	}
	else
	{
		char buff[256];

		// should really only happen once, on startup..
		emu_make_path(buff, "skin/background.png", sizeof(buff));
		if (readpng(g_screen_ptr, buff, READPNG_BG) < 0)
			memset(g_screen_ptr, 0, 320*240*2);
	}

	memcpy(menu_bg_buffer, g_screen_ptr, 320*240*2);

	// switch to 16bpp
	gp2x_video_changemode_ll_(16);
	gp2x_video_flip_();
}

void plat_video_menu_begin(void)
{
	memcpy(g_screen_ptr, menu_bg_buffer, 320*240*2);
}

void plat_video_menu_end(void)
{
	gp2x_video_flip_();
}

void plat_validate_config(void)
{
//	PicoOpt &= ~POPT_EXT_FM;
}

void plat_early_init(void)
{
}

void plat_init(void)
{
	realloc_screen();
	memset(g_screen_ptr, 0, g_screen_width * g_screen_height * 2);

	// snd
	sndout_oss_init();

	xlib_init();
}

void plat_finish(void)
{
	free(g_screen_ptr);
	sndout_oss_exit();
}

/* nasty */
static void do_nothing()
{
}

void *gp2x_video_flip = gp2x_video_flip_;
void *gp2x_video_flip2 = gp2x_video_flip_;
void *gp2x_video_changemode_ll = gp2x_video_changemode_ll_;
void *gp2x_video_setpalette = gp2x_video_setpalette_;

void *gp2x_video_RGB_setscaling = do_nothing;
void *gp2x_video_wait_vsync = do_nothing;
void *gp2x_set_cpuclk = do_nothing;
void *set_lcd_custom_rate = do_nothing;
void *unset_lcd_custom_rate = do_nothing;
void *set_lcd_gamma = do_nothing;
void *set_ram_timings = do_nothing;
void *unset_ram_timings = do_nothing;

/* joy */
int gp2x_touchpad_read(int *x, int *y)
{
	return -1;
}

/* misc */
void spend_cycles(int c)
{
	usleep(c/200);
}

int mp3_get_bitrate(FILE *f, int size)
{
	return 128;
}

void mp3_start_play(FILE *f, int pos)
{
}

void mp3_update(int *buffer, int length, int stereo)
{
}

/* lprintf */
void lprintf(const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);
}

