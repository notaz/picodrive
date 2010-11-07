// (c) Copyright 2006-2009 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/omapfb.h>

#include "../common/emu.h"
#include "../common/menu.h"
#include "../common/plat.h"
#include "../common/arm_utils.h"
#include "../common/input.h"
#include "../linux/sndout_oss.h"
#include "../linux/fbdev.h"
#include "plat.h"
#include "asm_utils.h"
#include "version.h"

#include <pico/pico_int.h>

#include <linux/input.h>

static struct vout_fbdev *main_fb, *layer_fb;
static int g_layer_x, g_layer_y;
static int g_layer_w = 320, g_layer_h = 240;
static int g_osd_fps_x, g_osd_y, doing_bg_frame;

static const char pnd_script_base[] = "sudo -n /usr/pandora/scripts";
static short __attribute__((aligned(4))) sndBuffer[2*44100/50];
static unsigned char __attribute__((aligned(4))) fb_copy[g_screen_width * g_screen_height * 2];
static void *temp_frame;
unsigned char *PicoDraw2FB;
const char *renderer_names[] = { NULL };
const char *renderer_names32x[] = { NULL };

static const char * const pandora_gpio_keys[KEY_MAX + 1] = {
	[0 ... KEY_MAX] = NULL,
	[KEY_UP]	= "Up",
	[KEY_LEFT]	= "Left",
	[KEY_RIGHT]	= "Right",
	[KEY_DOWN]	= "Down",
	[KEY_HOME]	= "A",
	[KEY_PAGEDOWN]	= "X",
	[KEY_END]	= "B",
	[KEY_PAGEUP]	= "Y",
	[KEY_RIGHTSHIFT]= "L",
	[KEY_RIGHTCTRL]	= "R",
	[KEY_LEFTALT]	= "Start",
	[KEY_LEFTCTRL]	= "Select",
	[KEY_MENU]	= "Pandora",
};

static int get_cpu_clock(void)
{
	FILE *f;
	int ret = 0;
	f = fopen("/proc/pandora/cpu_mhz_max", "r");
	if (f) {
		fscanf(f, "%d", &ret);
		fclose(f);
	}
	return ret;
}

void pemu_prep_defconfig(void)
{
	defaultConfig.EmuOpt |= EOPT_VSYNC|EOPT_16BPP;
	defaultConfig.s_PicoOpt |= POPT_EN_MCD_GFX|POPT_EN_MCD_PSYNC;
	defaultConfig.scaling = SCALE_2x2_3x2;
}

void pemu_validate_config(void)
{
	currentConfig.CPUclock = get_cpu_clock();
}

static void osd_text(int x, int y, const char *text)
{
	int len = strlen(text)*8;
	int i, h;

	len++;
	for (h = 0; h < 8; h++) {
		unsigned short *p;
		p = (unsigned short *)g_screen_ptr + x + g_screen_width*(y + h);
		for (i = len; i; i--, p++)
			*p = (*p>>2) & 0x39e7;
	}
	emu_text_out16(x, y, text);
}

static void draw_cd_leds(void)
{
	int old_reg;
	old_reg = Pico_mcd->s68k_regs[0];

	if (0) {
		// 8-bit modes
		unsigned int col_g = (old_reg & 2) ? 0xc0c0c0c0 : 0xe0e0e0e0;
		unsigned int col_r = (old_reg & 1) ? 0xd0d0d0d0 : 0xe0e0e0e0;
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*2+ 4) =
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*3+ 4) =
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*4+ 4) = col_g;
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*2+12) =
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*3+12) =
		*(unsigned int *)((char *)g_screen_ptr + g_screen_width*4+12) = col_r;
	} else {
		// 16-bit modes
		unsigned int *p = (unsigned int *)((short *)g_screen_ptr + g_screen_width*2+4);
		unsigned int col_g = (old_reg & 2) ? 0x06000600 : 0;
		unsigned int col_r = (old_reg & 1) ? 0xc000c000 : 0;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += g_screen_width/2 - 12/2;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += g_screen_width/2 - 12/2;
		*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r;
	}
}

static int emuscan(unsigned int num)
{
	DrawLineDest = (unsigned short *)g_screen_ptr + num * g_screen_width;

	return 0;
}

void pemu_finalize_frame(const char *fps, const char *notice)
{
	if (notice && notice[0])
		osd_text(2, g_osd_y, notice);
	if (fps && fps[0] && (currentConfig.EmuOpt & EOPT_SHOW_FPS))
		osd_text(g_osd_fps_x, g_osd_y, fps);
	if ((PicoAHW & PAHW_MCD) && (currentConfig.EmuOpt & EOPT_EN_CD_LEDS))
		draw_cd_leds();
}

void plat_video_flip(void)
{
	g_screen_ptr = vout_fbdev_flip(layer_fb);
}

void plat_video_toggle_renderer(int change, int is_menu)
{
}

void plat_video_menu_enter(int is_rom_loaded)
{
}

void plat_video_menu_begin(void)
{
}

void plat_video_menu_end(void)
{
	g_menuscreen_ptr = vout_fbdev_flip(main_fb);
}

void plat_video_wait_vsync(void)
{
	vout_fbdev_wait_vsync(main_fb);
}

void plat_status_msg_clear(void)
{
	vout_fbdev_clear_lines(layer_fb, g_osd_y, 8);
}

void plat_status_msg_busy_next(const char *msg)
{
	plat_status_msg_clear();
	pemu_finalize_frame("", msg);
	plat_video_flip();
	emu_status_msg("");
	reset_timing = 1;
}

void plat_status_msg_busy_first(const char *msg)
{
	plat_status_msg_busy_next(msg);
}

void plat_update_volume(int has_changed, int is_up)
{
	static int prev_frame = 0, wait_frames = 0;
	int vol = currentConfig.volume;

	if (has_changed)
	{
		if (is_up) {
			if (vol < 99) vol++;
		} else {
			if (vol >  0) vol--;
		}
		wait_frames = 0;
		sndout_oss_setvol(vol, vol);
		currentConfig.volume = vol;
		emu_status_msg("VOL: %02i", vol);
		prev_frame = Pico.m.frame_count;
	}
}

static void make_bg(int no_scale)
{
	unsigned short *s = (void *)fb_copy;
	int x, y;

	memset32(g_menubg_src_ptr, 0, g_menuscreen_w * g_menuscreen_h * 2 / 4);

	if (!no_scale && g_menuscreen_w >= 640 && g_menuscreen_h >= 480) {
		unsigned int t, *d = g_menubg_src_ptr;
		d += (g_menuscreen_h / 2 - 480 / 2) * g_menuscreen_w / 2;
		d += (g_menuscreen_w / 2 - 640 / 2) / 2;
		for (y = 0; y < 240; y++, s += 320, d += g_menuscreen_w*2/2) {
			for (x = 0; x < 320; x++) {
				t = s[x];
				t |= t << 16;
				d[x] = d[x + g_menuscreen_w / 2] = t;
			}
		}
		return;
	}

	if (g_menuscreen_w >= 320 && g_menuscreen_h >= 240) {
		unsigned short *d = g_menubg_src_ptr;
		d += (g_menuscreen_h / 2 - 240 / 2) * g_menuscreen_w;
		d += (g_menuscreen_w / 2 - 320 / 2);
		for (y = 0; y < 240; y++, s += 320, d += g_menuscreen_w)
			memcpy(d, s, 320*2);
		return;
	}
}

void pemu_forced_frame(int no_scale, int do_emu)
{
	doing_bg_frame = 1;
	emu_cmn_forced_frame(no_scale, do_emu);
	doing_bg_frame = 0;

	// making a copy because enabling the layer clears it's mem
	memcpy32((void *)fb_copy, g_screen_ptr, sizeof(fb_copy) / 4);
	make_bg(no_scale);
}

static void oss_write_nonblocking(int len)
{
	// sndout_oss_can_write() is not reliable, only use with no_frmlimit
	if ((currentConfig.EmuOpt & EOPT_NO_FRMLIMIT) && !sndout_oss_can_write(len))
		return;

	sndout_oss_write_nb(PsndOut, len);
}

void pemu_sound_start(void)
{
	PsndOut = NULL;

	if (currentConfig.EmuOpt & EOPT_EN_SOUND)
	{
		int is_stereo = (PicoOpt & POPT_EN_STEREO) ? 1 : 0;

		PsndRerate(Pico.m.frame_count ? 1 : 0);

		/*
		 * for 44k stereo, we do 1470 samples/emu_frame
		 * OMAP driver does power of 2 buffers, so we need at least 4K buffer.
		 * The most we can lag is 1K samples, size of OMAP's McBSP FIFO,
		 * with 2K sample buffer we might sometimes lag more than that,
		 * thus causing underflows.
		 */
		printf("starting audio: %i len: %i stereo: %i, pal: %i\n",
			PsndRate, PsndLen, is_stereo, Pico.m.pal);
		sndout_oss_start(PsndRate, is_stereo, 2);
		//sndout_oss_setvol(currentConfig.volume, currentConfig.volume);
		PicoWriteSound = oss_write_nonblocking;
		plat_update_volume(0, 0);
		memset(sndBuffer, 0, sizeof(sndBuffer));
		PsndOut = sndBuffer;
	}
}

void pemu_sound_stop(void)
{
	sndout_oss_stop();
}

void pemu_sound_wait(void)
{
	// don't need to do anything, writes will block by themselves
}

void plat_debug_cat(char *str)
{
}

static int pnd_setup_layer_(int fd, int enabled, int x, int y, int w, int h)
{
	struct omapfb_plane_info pi;
	struct omapfb_mem_info mi;
	int ret;

	ret = ioctl(fd, OMAPFB_QUERY_PLANE, &pi);
	if (ret != 0) {
		perror("QUERY_PLANE");
		return -1;
	}

	ret = ioctl(fd, OMAPFB_QUERY_MEM, &mi);
	if (ret != 0) {
		perror("QUERY_MEM");
		return -1;
	}

	/* must disable when changing stuff */
	if (pi.enabled) {
		pi.enabled = 0;
		ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
		if (ret != 0)
			perror("SETUP_PLANE");
	}

	mi.size = 320*240*2*4;
	ret = ioctl(fd, OMAPFB_SETUP_MEM, &mi);
	if (ret != 0) {
		perror("SETUP_MEM");
		return -1;
	}

	pi.pos_x = x;
	pi.pos_y = y;
	pi.out_width = w;
	pi.out_height = h;
	pi.enabled = enabled;

	ret = ioctl(fd, OMAPFB_SETUP_PLANE, &pi);
	if (ret != 0) {
		perror("SETUP_PLANE");
		return -1;
	}

	return 0;
}

int pnd_setup_layer(int enabled, int x, int y, int w, int h)
{
	return pnd_setup_layer_(vout_fbdev_get_fd(layer_fb), enabled, x, y, w, h);
}

void pnd_restore_layer_data(void)
{
	short *t = (short *)fb_copy + 320*240 / 2 + 160;

	// right now this is used by menu, which wants to preview something
	// so try to get something on the layer.
	if ((t[0] | t[5] | t[13]) == 0)
		memset32((void *)fb_copy, 0x07000700, sizeof(fb_copy) / 4);

	memcpy32(g_screen_ptr, (void *)fb_copy, 320*240*2 / 4);
	plat_video_flip();
}

static void apply_filter(int which)
{
	char buf[128];
	int i;

	if (pnd_filter_list == NULL)
		return;

	for (i = 0; i < which; i++)
		if (pnd_filter_list[i] == NULL)
			return;

	if (pnd_filter_list[i] == NULL)
		return;

	snprintf(buf, sizeof(buf), "%s/op_videofir.sh %s", pnd_script_base, pnd_filter_list[i]);
	system(buf);
}

void emu_video_mode_change(int start_line, int line_count, int is_32cols)
{
	int fb_w = 320, fb_h = 240, fb_left = 0, fb_right = 0, fb_top = 0, fb_bottom = 0;

	if (doing_bg_frame)
		return;

	PicoDrawSetOutFormat(PDF_RGB555, 1);
	PicoDrawSetCallbacks(emuscan, NULL);

	if (is_32cols) {
		fb_w = 256;
		fb_left = fb_right = 32;
	}

	switch (currentConfig.scaling) {
	case SCALE_1x1:
		g_layer_w = fb_w;
		g_layer_h = fb_h;
		break;
	case SCALE_2x2_3x2:
		g_layer_w = fb_w * (is_32cols ? 3 : 2);
		g_layer_h = fb_h * 2;
		break;
	case SCALE_2x2_2x2:
		g_layer_w = fb_w * 2;
		g_layer_h = fb_h * 2;
		break;
	case SCALE_FULLSCREEN:
		g_layer_w = 800;
		g_layer_h = 480;
		break;
	case SCALE_CUSTOM:
		g_layer_x = g_layer_cx;
		g_layer_y = g_layer_cy;
		g_layer_w = g_layer_cw;
		g_layer_h = g_layer_ch;
		break;
	}

	if (currentConfig.scaling != SCALE_CUSTOM) {
		// center the layer
		g_layer_x = 800 / 2 - g_layer_w / 2;
		g_layer_y = 480 / 2 - g_layer_h / 2;
	}

	switch (currentConfig.scaling) {
	case SCALE_FULLSCREEN:
	case SCALE_CUSTOM:
		fb_top = start_line;
		fb_h = line_count;
		break;
	}
	g_osd_fps_x = is_32cols ? 232 : 264;
	g_osd_y = fb_top + fb_h - 8;

	pnd_setup_layer(1, g_layer_x, g_layer_y, g_layer_w, g_layer_h);
	vout_fbdev_resize(layer_fb, fb_w, fb_h, fb_left, fb_right, fb_top, fb_bottom, 0);
	vout_fbdev_clear(layer_fb);
	plat_video_flip();
}

void pemu_loop_prep(void)
{
	static int pal_old = -1;
	static int filter_old = -1;
	char buf[128];

	if (currentConfig.CPUclock != get_cpu_clock()) {
		snprintf(buf, sizeof(buf), "unset DISPLAY; echo y | %s/op_cpuspeed.sh %d",
			 pnd_script_base, currentConfig.CPUclock);
		system(buf);
	}

	if (Pico.m.pal != pal_old) {
		snprintf(buf, sizeof(buf), "%s/op_lcdrate.sh %d",
			 pnd_script_base, Pico.m.pal ? 50 : 60);
		system(buf);
		pal_old = Pico.m.pal;
	}

	if (currentConfig.filter != filter_old) {
		apply_filter(currentConfig.filter);
		filter_old = currentConfig.filter;
	}

	// make sure there is no junk left behind the layer
	memset32(g_menuscreen_ptr, 0, g_menuscreen_w * g_menuscreen_h * 2 / 4);
	g_menuscreen_ptr = vout_fbdev_flip(main_fb);

	// emu_video_mode_change will call pnd_setup_layer()

	// dirty buffers better go now than during gameplay
	sync();
	sleep(0);

	pemu_sound_start();
}

void pemu_loop_end(void)
{
	pemu_sound_stop();

	/* do one more frame for menu bg */
	pemu_forced_frame(0, 1);

	pnd_setup_layer(0, g_layer_x, g_layer_y, g_layer_w, g_layer_h);
}

void plat_wait_till_us(unsigned int us_to)
{
	unsigned int now;
	signed int diff;

	now = plat_get_ticks_us();

	// XXX: need to check NOHZ
	diff = (signed int)(us_to - now);
	if (diff > 10000) {
		//printf("sleep %d\n", us_to - now);
		usleep(diff * 15 / 16);
		now = plat_get_ticks_us();
		//printf(" wake %d\n", (signed)(us_to - now));
	}
/*
	while ((signed int)(us_to - now) > 512) {
		spend_cycles(1024);
		now = plat_get_ticks_us();
	}
*/
}

const char *plat_get_credits(void)
{
	return "PicoDrive v" VERSION " (c) notaz, 2006-2010\n\n\n"
		"Credits:\n"
		"fDave: Cyclone 68000 core,\n"
		"      base code of PicoDrive\n"
		"Reesy & FluBBa: DrZ80 core\n"
		"MAME devs: YM2612 and SN76496 cores\n"
		"Pandora team: Pandora\n"
		"Inder, ketchupgun: graphics\n"
		"\n"
		"special thanks (for docs, ideas):\n"
		" Charles MacDonald, Haze,\n"
		" Stephane Dallongeville,\n"
		" Lordus, Exophase, Rokas,\n"
		" Nemesis, Tasco Deluxe";
}

#include "../linux/oshide.h"

void plat_early_init(void)
{
}

void plat_init(void)
{
	const char *main_fb_name, *layer_fb_name;
	int fd, ret, w, h;

	main_fb_name = getenv("FBDEV_MAIN");
	if (main_fb_name == NULL)
		main_fb_name = "/dev/fb0";

	layer_fb_name = getenv("FBDEV_LAYER");
	if (layer_fb_name == NULL)
		layer_fb_name = "/dev/fb1";

	// must set the layer up first to be able to use it
	fd = open(layer_fb_name, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "%s: ", layer_fb_name);
		perror("open");
		exit(1);
	}

	ret = pnd_setup_layer_(fd, 0, g_layer_x, g_layer_y, g_layer_w, g_layer_h);
	close(fd);
	if (ret != 0) {
		fprintf(stderr, "failed to set up layer, exiting.\n");
		exit(1);
	}

	oshide_init();

	w = h = 0;
	main_fb = vout_fbdev_init(main_fb_name, &w, &h, 0);
	if (main_fb == NULL) {
		fprintf(stderr, "couldn't init fb: %s\n", main_fb_name);
		exit(1);
	}

	g_menuscreen_w = w;
	g_menuscreen_h = h;
	g_menuscreen_ptr = vout_fbdev_flip(main_fb);

	w = 320; h = 240;
	layer_fb = vout_fbdev_init(layer_fb_name, &w, &h, 0);
	if (layer_fb == NULL) {
		fprintf(stderr, "couldn't init fb: %s\n", layer_fb_name);
		goto fail0;
	}

	if (w != g_screen_width || h != g_screen_height) {
		fprintf(stderr, "%dx%d not supported on %s\n", w, h, layer_fb_name);
		goto fail1;
	}
	g_screen_ptr = vout_fbdev_flip(layer_fb);

	temp_frame = calloc(g_menuscreen_w * g_menuscreen_h * 2, 1);
	if (temp_frame == NULL) {
		fprintf(stderr, "OOM\n");
		goto fail1;
	}
	g_menubg_ptr = temp_frame;
	g_menubg_src_ptr = temp_frame;
	PicoDraw2FB = temp_frame;

	sndout_oss_init();
	pnd_menu_init();

	in_set_config(in_name_to_id("evdev:gpio-keys"), IN_CFG_KEY_NAMES,
		      pandora_gpio_keys, sizeof(pandora_gpio_keys));
	return;

fail1:
	vout_fbdev_finish(layer_fb);
fail0:
	vout_fbdev_finish(main_fb);
	exit(1);
}

void plat_finish(void)
{
	sndout_oss_exit();
	vout_fbdev_finish(main_fb);
	oshide_finish();

	printf("all done\n");
}

