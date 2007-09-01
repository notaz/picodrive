/* faking/emulating gp2x.c by using gtk */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <gtk/gtk.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <fcntl.h>
#include <errno.h>

#include "../gp2x/emu.h"
#include "../gp2x/gp2x.h"
#include "../gp2x/usbjoy.h"
#include "../gp2x/version.h"

#include "log_io.h"

void *gp2x_screen;
static int current_bpp = 8;
static int current_pal[256];
static unsigned long current_keys = 0;
static int sounddev = 0, mixerdev = 0;
static const char *verstring = "PicoDrive " VERSION;

// dummies
char *ext_menu = 0, *ext_state = 0;

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

static gint key_press_event (GtkWidget *widget, GdkEventKey *event)
{
	switch (event->hardware_keycode)
	{
		case 0x62: current_keys |= GP2X_UP;    break;
		case 0x68: current_keys |= GP2X_DOWN;  break;
		case 0x64: current_keys |= GP2X_LEFT;  break;
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
	}

	return 0;
}

static gint key_release_event (GtkWidget *widget, GdkEventKey *event)
{
	switch (event->hardware_keycode)
	{
		case 0x62: current_keys &= ~GP2X_UP;    break;
		case 0x68: current_keys &= ~GP2X_DOWN;  break;
		case 0x64: current_keys &= ~GP2X_LEFT;  break;
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

static void *gtk_threadf(void *none)
{
	gtk_main();

	printf("linux: gtk thread finishing\n");
	engineState = PGS_Quit;

	return NULL;
}

static void gtk_initf(void)
{
	int argc = 0;
	char *argv[] = { "" };
	GtkWidget *box;
	pthread_t gtk_thread;

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

	gtk_container_set_border_width (GTK_CONTAINER (gtk_items.window), 2);
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

	// pthread_mutex_init (&thr_mutex, NULL);
	// pthread_mutex_lock (&thr_mutex);
	// pthread_mutex_init (&scanner_muttex, NULL);

	pthread_create(&gtk_thread, NULL, gtk_threadf, NULL);
}

void finalize_image(guchar *pixels, gpointer data)
{
	free(pixels);
}

/* --- */

void gp2x_init(void)
{
	printf("entering init()\n"); fflush(stdout);

	gp2x_screen = malloc(320*240*2 + 320*2);
	memset(gp2x_screen, 0, 320*240*2 + 320*2);

	// snd
  	mixerdev = open("/dev/mixer", O_RDWR);
	if (mixerdev == -1)
		printf("open(\"/dev/mixer\") failed with %i\n", errno);

	gtk_initf();

	gp2x_usbjoy_init();

	printf("exitting init()\n"); fflush(stdout);
}

void gp2x_deinit(void)
{
	free(gp2x_screen);
	if (sounddev > 0) close(sounddev);
	close(mixerdev);
	gp2x_usbjoy_deinit();
}

/* video */
void gp2x_video_flip(void)
{
	GdkPixbuf	*pixbuf;
	unsigned char	*image;
	int		i;

	gdk_threads_enter();

	image = malloc (320*240*3);
	if (image == NULL)
	{
		gdk_threads_leave();
		return;
	}

	if (current_bpp == 8)
	{
		unsigned char *pixels = gp2x_screen;
		int pix;

		for (i = 0; i < 320*240; i++)
		{
			pix = current_pal[pixels[i]];
			image[3 * i + 0] = pix >> 16;
			image[3 * i + 1] = pix >>  8;
			image[3 * i + 2] = pix;
		}
	}
	else
	{
		unsigned short *pixels = gp2x_screen;

		for (i = 0; i < 320*240; i++)
		{
			/*  in:           rrrr rggg gggb bbbb */
			/* out: rrrr r000 gggg gg00 bbbb b000 */
			image[3 * i + 0] = (pixels[i] >> 8) & 0xf8;
			image[3 * i + 1] = (pixels[i] >> 3) & 0xfc;
			image[3 * i + 2] = (pixels[i] << 3);
		}
	}

	pixbuf = gdk_pixbuf_new_from_data (image, GDK_COLORSPACE_RGB,
			FALSE, 8, 320, 240, 320*3, finalize_image, NULL);
	gtk_image_set_from_pixbuf (GTK_IMAGE (gtk_items.pixmap1), pixbuf);
	g_object_unref (pixbuf);

	gdk_threads_leave();
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

void gp2x_video_flush_cache(void)
{
}

void gp2x_video_RGB_setscaling(int v_offs, int W, int H)
{
}

void gp2x_memcpy_buffers(int buffers, void *data, int offset, int len)
{
	if ((char *)gp2x_screen + offset != data)
		memcpy((char *)gp2x_screen + offset, data, len);
}

void gp2x_memcpy_all_buffers(void *data, int offset, int len)
{
	memcpy((char *)gp2x_screen + offset, data, len);
}


void gp2x_memset_all_buffers(int offset, int byte, int len)
{
	memset((char *)gp2x_screen + offset, byte, len);
}

void gp2x_pd_clone_buffer2(void)
{
	memset(gp2x_screen, 0, 320*240*2);
}

/* sound */
static int s_oldrate = 0, s_oldbits = 0, s_oldstereo = 0;

void gp2x_start_sound(int rate, int bits, int stereo)
{
	int frag = 0, bsize, buffers;

	// if no settings change, we don't need to do anything
	if (rate == s_oldrate && s_oldbits == bits && s_oldstereo == stereo) return;

	if (sounddev > 0) close(sounddev);
	sounddev = open("/dev/dsp", O_WRONLY|O_ASYNC);
	if (sounddev == -1)
		printf("open(\"/dev/dsp\") failed with %i\n", errno);

	ioctl(sounddev, SNDCTL_DSP_SPEED,  &rate);
	ioctl(sounddev, SNDCTL_DSP_SETFMT, &bits);
	ioctl(sounddev, SNDCTL_DSP_STEREO, &stereo);
	// calculate buffer size
	buffers = 16;
	bsize = rate / 32;
	if (rate > 22050) { bsize*=4; buffers*=2; } // 44k mode seems to be very demanding
	while ((bsize>>=1)) frag++;
	frag |= buffers<<16; // 16 buffers
	ioctl(sounddev, SNDCTL_DSP_SETFRAGMENT, &frag);
	printf("gp2x_set_sound: %i/%ibit/%s, %i buffers of %i bytes\n",
		rate, bits, stereo?"stereo":"mono", frag>>16, 1<<(frag&0xffff));

	s_oldrate = rate; s_oldbits = bits; s_oldstereo = stereo;
}

void gp2x_sound_write(void *buff, int len)
{
	write(sounddev, buff, len);
}

void gp2x_sound_sync(void)
{
	ioctl(sounddev, SOUND_PCM_SYNC, 0);
}

void gp2x_sound_volume(int l, int r)
{
 	l=l<0?0:l; l=l>255?255:l; r=r<0?0:r; r=r>255?255:r;
 	l<<=8; l|=r;
 	ioctl(mixerdev, SOUND_MIXER_WRITE_PCM, &l); /*SOUND_MIXER_WRITE_VOLUME*/
}

/* joy */
unsigned long gp2x_joystick_read(int allow_usb_joy)
{
	unsigned long value = current_keys;
	int i;

	if (allow_usb_joy && num_of_joys > 0) {
		// check the usb joy as well..
		gp2x_usbjoy_update();
		for (i = 0; i < num_of_joys; i++)
			value |= gp2x_usbjoy_check(i);
	}

	return value;
}

/* 940 */
int crashed_940 = 0;
void Pause940(int yes)
{
}

void Reset940(int yes, int bank)
{
}

/* faking gp2x cpuctrl.c */
void cpuctrl_init(void)
{
}

void cpuctrl_deinit(void)
{
}

void set_FCLK(unsigned MHZ)
{
}

void Disable_940(void)
{
}

void gp2x_video_wait_vsync(void)
{
}

void set_RAM_Timings(int tRC, int tRAS, int tWR, int tMRD, int tRFC, int tRP, int tRCD)
{
}

void set_gamma(int g100, int A_SNs_curve)
{
}

void set_LCD_custom_rate(int rate)
{
}

void unset_LCD_custom_rate(void)
{
}

/* squidgehack.c */
int mmuhack(void)
{
	return 0;
}


int mmuunhack(void)
{
	return 0;
}


/* misc */
void spend_cycles(int c)
{
	usleep(c/200);
}



