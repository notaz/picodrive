#include <stddef.h>

#include <kernel.h>
#include <ps2_joystick_driver.h>
#include <ps2_audio_driver.h>
#include <libpad.h>

#include "in_ps2.h"
#include "../libpicofe/input.h"
#include "../libpicofe/plat.h"
#include "../common/input_pico.h"
#include "../common/emu.h"

#include <pico/pico_int.h>

#define OSD_FPS_X 220

static int osd_buf_cnt, osd_cdleds;

static int out_x, out_y;
static int out_w, out_h;
static float hscale, vscale;

static struct in_default_bind in_ps2_defbinds[] =
{
	{ PAD_UP,          IN_BINDTYPE_PLAYER12, GBTN_UP },
	{ PAD_DOWN,        IN_BINDTYPE_PLAYER12, GBTN_DOWN },
	{ PAD_LEFT,        IN_BINDTYPE_PLAYER12, GBTN_LEFT },
	{ PAD_RIGHT,       IN_BINDTYPE_PLAYER12, GBTN_RIGHT },
	{ PAD_SQUARE,      IN_BINDTYPE_PLAYER12, GBTN_A },
	{ PAD_CROSS,       IN_BINDTYPE_PLAYER12, GBTN_B },
	{ PAD_CIRCLE,      IN_BINDTYPE_PLAYER12, GBTN_C },
	{ PAD_START,       IN_BINDTYPE_PLAYER12, GBTN_START },
	{ PAD_TRIANGLE,    IN_BINDTYPE_EMU, PEVB_SWITCH_RND },
	{ PAD_L1,          IN_BINDTYPE_EMU, PEVB_STATE_SAVE },
	{ PAD_R1,          IN_BINDTYPE_EMU, PEVB_STATE_LOAD },
	{ PAD_SELECT,      IN_BINDTYPE_EMU, PEVB_MENU },
	{ 0, 0, 0 }
};

const char *renderer_names[] = { "16bit accurate", " 8bit accurate", " 8bit fast", NULL };
const char *renderer_names32x[] = { "accurate", "faster", "fastest", NULL };
enum renderer_types { RT_16BIT, RT_8BIT_ACC, RT_8BIT_FAST, RT_COUNT };

#define is_16bit_mode() \
	(currentConfig.renderer == RT_16BIT || (PicoIn.AHW & PAHW_32X))

static int get_renderer(void)
{
	if (PicoIn.AHW & PAHW_32X)
		return currentConfig.renderer32x;
	else
		return currentConfig.renderer;
}

static void change_renderer(int diff)
{
	int *r;
	if (PicoIn.AHW & PAHW_32X)
		r = &currentConfig.renderer32x;
	else
		r = &currentConfig.renderer;
	*r += diff;

	if      (*r >= RT_COUNT)
		*r = 0;
	else if (*r < 0)
		*r = RT_COUNT - 1;
}

static void apply_renderer(void)
{
	PicoIn.opt &= ~(POPT_ALT_RENDERER|POPT_EN_SOFTSCALE);
	PicoIn.opt |= POPT_DIS_32C_BORDER;

	switch (get_renderer()) {
	case RT_16BIT:
		PicoDrawSetOutFormat(PDF_RGB555, 0);
		break;
	case RT_8BIT_ACC:
		PicoDrawSetOutFormat(PDF_8BIT, 0);
		break;
	case RT_8BIT_FAST:
		PicoIn.opt |=  POPT_ALT_RENDERER;
		PicoDrawSetOutFormat(PDF_NONE, 0);
		break;
	}
}

static void osd_text(int x, const char *text)
{

}

static void blit_osd(void)
{

}

static void cd_leds(void)
{

}

static void blit_cdleds(void)
{

}

void blitscreen_clut(void)
{

    blit_osd();
	blit_cdleds();
}

static void draw_pico_ptr(void)
{
	// unsigned char *p = (unsigned char *)g_screen_ptr + 8;

	// // only if pen enabled and for 8bit mode
	// if (pico_inp_mode == 0 || is_16bit_mode()) return;

	// p += 512 * (pico_pen_y + PICO_PEN_ADJUST_Y);
	// p += pico_pen_x + PICO_PEN_ADJUST_X;
	// if (!(Pico.video.reg[12]&1) && !(PicoIn.opt & POPT_DIS_32C_BORDER))
	// 	p += 32;

	// p[  -1] = 0xe0; p[   0] = 0xf0; p[   1] = 0xe0;
	// p[ 511] = 0xf0; p[ 512] = 0xf0; p[ 513] = 0xf0;
	// p[1023] = 0xe0; p[1024] = 0xf0; p[1025] = 0xe0;
}

static void vidResetMode(void) {}

void pemu_sound_start(void) {}
void pemu_sound_stop(void) {}

/* set default configuration values */
void pemu_prep_defconfig(void)
{
	defaultConfig.s_PsndRate = 44100;
	defaultConfig.s_PicoCDBuffers = 64;
	defaultConfig.filter = EOPT_FILTER_BILINEAR; // bilinear filtering
	defaultConfig.scaling = EOPT_SCALE_43;
	defaultConfig.vscaling = EOPT_VSCALE_FULL;
	defaultConfig.renderer = RT_8BIT_ACC;
	defaultConfig.renderer32x = RT_8BIT_ACC;
	defaultConfig.EmuOpt |= EOPT_SHOW_RTC;
}

/* check configuration for inconsistencies */
void pemu_validate_config(void)
{
	if (currentConfig.gamma < -4 || currentConfig.gamma >  16)
		currentConfig.gamma = 0;
	if (currentConfig.gamma2 < 0 || currentConfig.gamma2 > 2)
		currentConfig.gamma2 = 0;
}

/* finalize rendering a frame */
void pemu_finalize_frame(const char *fps, const char *notice)
{
	int emu_opt = currentConfig.EmuOpt;

	if (PicoIn.AHW & PAHW_PICO)
		draw_pico_ptr();

	osd_buf_cnt = 0;
	if (notice)      osd_text(4, notice);
	if (emu_opt & 2) osd_text(OSD_FPS_X, fps);

	osd_cdleds = 0;
	if ((emu_opt & 0x400) && (PicoIn.AHW & PAHW_MCD))
		cd_leds();

	FlushCache(WRITEBACK_DCACHE);
}

void plat_init(void) 
{
    init_joystick_driver(false);
    in_ps2_init(in_ps2_defbinds);
    in_probe();
    init_audio_driver();
    // plat_get_data_dir(rom_fname_loaded, sizeof(rom_fname_loaded));
}

void plat_finish(void) {
    deinit_audio_driver();
    deinit_joystick_driver(false);
}

/* display emulator status messages before holding emulation */
void plat_status_msg_busy_first(const char *msg)
{
	plat_status_msg_busy_next(msg);
}

void plat_status_msg_busy_next(const char *msg)
{
	plat_status_msg_clear();
	pemu_finalize_frame("", msg);
	plat_video_flip();
	emu_status_msg("");
	reset_timing = 1;
}

/* clear status message area */
void plat_status_msg_clear(void)
{
	// not needed since the screen buf is cleared through the GU
}

/* change the audio volume setting */
void plat_update_volume(int has_changed, int is_up)
{
}

/* prepare for MD screen mode change */
void emu_video_mode_change(int start_line, int line_count, int start_col, int col_count)
{
	int h43 = (col_count  >= 192 ? 320 : col_count); // ugh, mind GG...
	int v43 = (line_count >= 192 ? Pico.m.pal ? 240 : 224 : line_count);

	out_y = start_line; out_x = start_col;
	out_h = line_count; out_w = col_count;

	if (col_count == 248) // mind aspect ratio when blanking 1st column
		col_count = 256;

	switch (currentConfig.vscaling) {
	case EOPT_VSCALE_FULL:
		line_count = v43;
		vscale = (float)270/line_count;
		break;
	case EOPT_VSCALE_NOBORDER:
		vscale = (float)270/line_count;
		break;
	default:
		vscale = 1;
		break;
	}

	switch (currentConfig.scaling) {
	case EOPT_SCALE_43:
		hscale = (vscale*h43)/col_count;
		break;
	case EOPT_SCALE_STRETCH:
		hscale = (vscale*h43/2 + 480/2)/col_count;
		break;
	case EOPT_SCALE_WIDE:
		hscale = (float)480/col_count;
		break;
	default:
		hscale = vscale;
		break;
	}

	vidResetMode();
}

/* render one frame in RGB */
void pemu_forced_frame(int no_scale, int do_emu)
{
	Pico.m.dirtyPal = 1;

	if (!no_scale)
		no_scale = currentConfig.scaling == EOPT_SCALE_NONE;
	emu_cmn_forced_frame(no_scale, do_emu, g_screen_ptr);
}

/* change the platform output rendering */
void plat_video_toggle_renderer(int change, int is_menu_call)
{
	change_renderer(change);

	if (is_menu_call)
		return;

	apply_renderer();
	vidResetMode();
	rendstatus_old = -1;

	if (PicoIn.AHW & PAHW_32X)
		emu_status_msg(renderer_names32x[get_renderer()]);
	else
		emu_status_msg(renderer_names[get_renderer()]);
}

/* prepare for emulator output rendering */
void plat_video_loop_prepare(void) 
{
	apply_renderer();
	vidResetMode();
}

/* prepare for entering the emulator loop */
void pemu_loop_prep(void)
{
}

/* terminate the emulator loop */
void pemu_loop_end(void)
{
}