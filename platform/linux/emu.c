/*
 * PicoDrive
 * (C) notaz, 2006-2010
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

#include "../libpicofe/menu.h"
#include "../libpicofe/plat.h"
#include "../common/emu.h"
#include "../common/arm_utils.h"
#include "../common/version.h"

#include <pico/pico_int.h>


const char *renderer_names[] = { "16bit accurate", " 8bit accurate", " 8bit fast", NULL };
const char *renderer_names32x[] = { "accurate", "faster", "fastest", NULL };
enum renderer_types { RT_16BIT, RT_8BIT_ACC, RT_8BIT_FAST, RT_COUNT };

static int out_x, out_y;
static int out_w, out_h;

void pemu_prep_defconfig(void)
{
}

void pemu_validate_config(void)
{
#if !defined(__arm__) && !defined(__aarch64__) && !defined(__mips__) && !defined(__riscv__) &&  !defined(__riscv) && !defined(__powerpc__) && !defined(__ppc__) && !defined(__i386__) && !defined(__x86_64__)
	PicoIn.opt &= ~POPT_EN_DRC;
#endif
}

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

static void draw_cd_leds(void)
{
	int led_reg, pitch, scr_offs, led_offs;
	led_reg = Pico_mcd->s68k_regs[0];

	pitch = g_screen_ppitch;
	led_offs = 4;
	scr_offs = pitch * 2 + 4;

#define p(x) px[(x)*2 >> 2] = px[((x)*2 >> 2) + 1]
	// 16-bit modes
	uint32_t *px = (uint32_t *)((short *)g_screen_ptr + scr_offs);
	uint32_t col_g = (led_reg & 2) ? 0x06000600 : 0;
	uint32_t col_r = (led_reg & 1) ? 0xc000c000 : 0;
	p(pitch*0) = p(pitch*1) = p(pitch*2) = col_g;
	p(pitch*0 + led_offs) = p(pitch*1 + led_offs) = p(pitch*2 + led_offs) = col_r;
#undef p
}

static unsigned short *get_16bit_start(unsigned short *buf)
{
	// center the output on the screen
	int offs = (g_screen_height-240)/2 * g_screen_ppitch + (g_screen_width-320)/2;
	return buf + offs;
}

void pemu_finalize_frame(const char *fps, const char *notice)
{
	if (!is_16bit_mode()) {
		// convert the 8 bit CLUT output to 16 bit RGB
		unsigned short *pd = (unsigned short *)g_screen_ptr +
					out_y * g_screen_ppitch + out_x;
		unsigned char *ps = Pico.est.Draw2FB + 328*out_y + 8;
		unsigned short *pal = Pico.est.HighPal;
		int i, x;

		pd = get_16bit_start(pd);
		PicoDrawUpdateHighPal();
		for (i = 0; i < out_h; i++, ps += 8) {
			for (x = 0; x < out_w; x++)
				*pd++ = pal[*ps++];
			pd += g_screen_ppitch - out_w;
			ps += 320 - out_w;
		}
	}

	if (notice)
		emu_osd_text16(4, g_screen_height - 8, notice);
	if (currentConfig.EmuOpt & EOPT_SHOW_FPS)
		emu_osd_text16(g_screen_width - 60, g_screen_height - 8, fps);
	if ((PicoIn.AHW & PAHW_MCD) && (currentConfig.EmuOpt & EOPT_EN_CD_LEDS))
		draw_cd_leds();
}

void plat_video_set_buffer(void *buf)
{
	if (is_16bit_mode())
		PicoDrawSetOutBuf(get_16bit_start(buf), g_screen_ppitch * 2);
}

static void apply_renderer(void)
{
	switch (get_renderer()) {
	case RT_16BIT:
		PicoIn.opt &= ~POPT_ALT_RENDERER;
		PicoIn.opt &= ~POPT_DIS_32C_BORDER;
		PicoDrawSetOutFormat(PDF_RGB555, 0);
		PicoDrawSetOutBuf(get_16bit_start(g_screen_ptr), g_screen_ppitch * 2);
		break;
	case RT_8BIT_ACC:
		PicoIn.opt &= ~POPT_ALT_RENDERER;
		PicoIn.opt |=  POPT_DIS_32C_BORDER;
		PicoDrawSetOutFormat(PDF_8BIT, 0);
		PicoDrawSetOutBuf(Pico.est.Draw2FB, 328);
		break;
	case RT_8BIT_FAST:
		PicoIn.opt |=  POPT_ALT_RENDERER;
		PicoIn.opt |=  POPT_DIS_32C_BORDER;
		PicoDrawSetOutFormat(PDF_NONE, 0);
		break;
	}

	if (PicoIn.AHW & PAHW_32X)
		PicoDrawSetOutBuf(get_16bit_start(g_screen_ptr), g_screen_ppitch * 2);

	Pico.m.dirtyPal = 1;
}

void plat_video_toggle_renderer(int change, int is_menu)
{
	change_renderer(change);

	if (!is_menu) {
		apply_renderer();

		if (PicoIn.AHW & PAHW_32X)
			emu_status_msg(renderer_names32x[get_renderer()]);
		else
			emu_status_msg(renderer_names[get_renderer()]);
	}
}

void plat_status_msg_clear(void)
{
	plat_video_clear_status();
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
}

void pemu_forced_frame(int no_scale, int do_emu)
{
	unsigned short *pd = get_16bit_start(g_screen_ptr);

	PicoIn.opt &= ~POPT_DIS_32C_BORDER;
	PicoDrawSetCallbacks(NULL, NULL);
	Pico.m.dirtyPal = 1;

	emu_cmn_forced_frame(no_scale, do_emu, pd);

	g_menubg_src_ptr = g_screen_ptr;
}

void pemu_sound_start(void)
{
	emu_sound_start();
}

void plat_debug_cat(char *str)
{
}

void emu_video_mode_change(int start_line, int line_count, int is_32cols)
{
	// clear whole screen in all buffers
	if (!is_16bit_mode())
		memset32(Pico.est.Draw2FB, 0xe0e0e0e0, (320+8) * (8+240+8) / 4);
	plat_video_clear_buffers();

	out_y = start_line; out_x = (is_32cols ? 32 : 0);
	out_h = line_count; out_w = (is_32cols ? 256:320);
}

void pemu_loop_prep(void)
{
	apply_renderer();
	plat_video_clear_buffers();
}

void pemu_loop_end(void)
{
	/* do one more frame for menu bg */
	pemu_forced_frame(0, 1);
}

void plat_wait_till_us(unsigned int us_to)
{
	unsigned int now;

	now = plat_get_ticks_us();

	while ((signed int)(us_to - now) > 512)
	{
		usleep(1024);
		now = plat_get_ticks_us();
	}
}

void *plat_mem_get_for_drc(size_t size)
{
#ifdef MAP_JIT
	// newer versions of OSX, IOS or TvOS need this
	return plat_mmap(0, size, 1, 0);
#else
	return NULL;
#endif
}
