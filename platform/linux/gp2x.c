/* faking/emulating gp2x by using gtk */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <gtk/gtk.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "../gp2x/emu.h"
//#include "../gp2x/gp2x.h"
#include "../gp2x/version.h"
#include "../common/emu.h"
#include "sndout_oss.h"

#include "log_io.h"

unsigned long current_keys = 0;
static int current_bpp = 8;
static int current_pal[256];
static const char *verstring = "PicoDrive " VERSION;
static int scr_changed = 0, scr_w = SCREEN_WIDTH, scr_h = SCREEN_HEIGHT;

// dummies
int mix_32_to_16l_level;

/* gtk */
struct gtk_global_struct
{
        GtkWidget *window;
        GtkWidget *pixmap1;
} gtk_items;


static gboolean delete_event (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	return FALSE;
}

static void destroy (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

/* faking GP2X pad */
enum  { GP2X_UP=0x1,       GP2X_LEFT=0x4,       GP2X_DOWN=0x10,  GP2X_RIGHT=0x40,
        GP2X_START=1<<8,   GP2X_SELECT=1<<9,    GP2X_L=1<<10,    GP2X_R=1<<11,
        GP2X_A=1<<12,      GP2X_B=1<<13,        GP2X_X=1<<14,    GP2X_Y=1<<15,
        GP2X_VOL_UP=1<<23, GP2X_VOL_DOWN=1<<22, GP2X_PUSH=1<<27 };

static gint key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	switch (event->hardware_keycode)
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

	return 0;
}

static gint key_release_event (GtkWidget *widget, GdkEventKey *event)
{
	switch (event->hardware_keycode)
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

	return 0;
}

static void size_allocate_event(GtkWidget *widget, GtkAllocation *allocation, gpointer user_data)
{
	// printf("%dx%d\n", allocation->width, allocation->height);
	if (scr_w != allocation->width - 2 || scr_h != allocation->height - 2) {
		scr_w = allocation->width - 2;
		scr_h = allocation->height - 2;
		scr_changed = 1;
	}
}

static void *gtk_threadf(void *targ)
{
	int argc = 0;
	char *argv[] = { "" };
	GtkWidget *box;
	sem_t *sem = targ;

	g_thread_init (NULL);
	gdk_threads_init ();
	gdk_set_locale ();
	gtk_init (&argc, (char ***) &argv);

	/* create new window */
	gtk_items.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (gtk_items.window), "delete_event",
			G_CALLBACK (delete_event), NULL);

	g_signal_connect (G_OBJECT (gtk_items.window), "destroy",
			G_CALLBACK (destroy), NULL);

	g_signal_connect (G_OBJECT (gtk_items.window), "key_press_event",
			G_CALLBACK (key_press_event), NULL);

	g_signal_connect (G_OBJECT (gtk_items.window), "key_release_event",
			G_CALLBACK (key_release_event), NULL);

	g_signal_connect (G_OBJECT (gtk_items.window), "size_allocate",
			G_CALLBACK (size_allocate_event), NULL);

	gtk_container_set_border_width (GTK_CONTAINER (gtk_items.window), 1);
	gtk_window_set_title ((GtkWindow *) gtk_items.window, verstring);

	box = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(box);
	gtk_container_add (GTK_CONTAINER (gtk_items.window), box);

	/* live pixmap */
	gtk_items.pixmap1 = gtk_image_new ();
	gtk_container_add (GTK_CONTAINER (box), gtk_items.pixmap1);
	gtk_widget_show (gtk_items.pixmap1);
	gtk_widget_set_size_request (gtk_items.pixmap1, 320, 240);

	gtk_widget_show  (gtk_items.window);

	sem_post(sem);

	gtk_main();

	printf("linux: gtk thread finishing\n");
	exit(1);

	return NULL;
}

static void gtk_initf(void)
{
	pthread_t gtk_thread;
	sem_t sem;
	sem_init(&sem, 0, 0);

	pthread_create(&gtk_thread, NULL, gtk_threadf, &sem);
	pthread_detach(gtk_thread);

	sem_wait(&sem);
	sem_close(&sem);
}

void finalize_image(guchar *pixels, gpointer data)
{
	free(pixels);
}

/* --- */

static void realloc_screen(void)
{
	void *old = g_screen_ptr;
	g_screen_width = scr_w;
	g_screen_height = scr_h;
	g_screen_ptr = calloc(g_screen_width * g_screen_height * 2, 1);
	free(old);
	scr_changed = 0;
}

void plat_init(void)
{
	printf("entering init()\n"); fflush(stdout);

	realloc_screen();
	memset(g_screen_ptr, 0, g_screen_width * g_screen_height * 2);

	// snd
	sndout_oss_init();

	gtk_initf();

	printf("exitting init()\n"); fflush(stdout);
}

void plat_finish(void)
{
	free(g_screen_ptr);
	sndout_oss_exit();
}

/* video */
void gp2x_video_flip(void)
{
	GdkPixbuf	*pixbuf;
	unsigned char	*image;
	int		pixel_count, i;

	pixel_count = g_screen_width * g_screen_height;

	gdk_threads_enter();

	image = malloc(pixel_count * 3);
	if (image == NULL)
	{
		gdk_threads_leave();
		return;
	}

	if (current_bpp == 8)
	{
		unsigned char *pixels = g_screen_ptr;
		int pix;

		for (i = 0; i < pixel_count; i++)
		{
			pix = current_pal[pixels[i]];
			image[3 * i + 0] = pix >> 16;
			image[3 * i + 1] = pix >>  8;
			image[3 * i + 2] = pix;
		}
	}
	else
	{
		unsigned short *pixels = g_screen_ptr;

		for (i = 0; i < pixel_count; i++)
		{
			/*  in:           rrrr rggg gggb bbbb */
			/* out: rrrr r000 gggg gg00 bbbb b000 */
			image[3 * i + 0] = (pixels[i] >> 8) & 0xf8;
			image[3 * i + 1] = (pixels[i] >> 3) & 0xfc;
			image[3 * i + 2] = (pixels[i] << 3);
		}
	}

	pixbuf = gdk_pixbuf_new_from_data (image, GDK_COLORSPACE_RGB,
			FALSE, 8, g_screen_width, g_screen_height,
			g_screen_width * 3, finalize_image, NULL);
	gtk_image_set_from_pixbuf (GTK_IMAGE (gtk_items.pixmap1), pixbuf);
	g_object_unref (pixbuf);

	gdk_threads_leave();

	if (scr_changed)
		realloc_screen();
}

void gp2x_video_flip2(void)
{
	gp2x_video_flip();
}

void gp2x_video_changemode(int bpp)
{
	current_bpp = bpp;
}

void gp2x_video_changemode2(int bpp)
{
	current_bpp = bpp;
}

void gp2x_video_setpalette(int *pal, int len)
{
	memcpy(current_pal, pal, len*4);
}

void gp2x_video_RGB_setscaling(int v_offs, int W, int H)
{
}

void gp2x_memset_all_buffers(int offset, int byte, int len)
{
	memset((char *)g_screen_ptr + offset, byte, len);
}

/* joy */
int gp2x_touchpad_read(int *x, int *y)
{
	return -1;
}

/* 940 */
int crashed_940 = 0;
void pause940(int yes)
{
}

void reset940(int yes, int bank)
{
}

void gp2x_video_wait_vsync(void)
{
}

void set_gamma(int g100, int A_SNs_curve)
{
}

void set_lcd_custom_rate(int rate)
{
}

void unset_lcd_custom_rate(void)
{
}

/* misc */
void spend_cycles(int c)
{
	usleep(c/200);
}

/* lprintf */
void lprintf(const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);
}

