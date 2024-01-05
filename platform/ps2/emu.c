#include <stddef.h>
#include <malloc.h>

#include <kernel.h>
#include <ps2_joystick_driver.h>
#include <ps2_audio_driver.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <gsInline.h>

#include "in_ps2.h"
#include "../libpicofe/input.h"
#include "../libpicofe/plat.h"
#include "../libpicofe/menu.h"
#include "../common/input_pico.h"
#include "../common/emu.h"

#include <pico/pico_int.h>

#define OSD_FPS_X 220

/* turn black GS Screen */
#define GS_BLACK GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x80)
/* Generic tint color */
#define GS_TEXT GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80)

static int osd_buf_cnt, osd_cdleds;

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

static GSGLOBAL *gsGlobal;

static GSTEXTURE *g_menuscreen;
static GSPRIMUVPOINT *g_menuscreen_vertices;

static GSTEXTURE *g_screen;
static GSPRIMUVPOINT *g_screen_vertices;

static GSTEXTURE *osd;
static uint32_t osd_vertices_count;
static GSPRIMUVPOINT *osd_vertices;

static GSTEXTURE *cdleds;
static GSPRIMUVPOINT *cdleds_vertices;

static int vsync_sema_id;
static int32_t vsync_callback_id;
static uint8_t vsync; /* 0 (Disabled), 1 (Enabled), 2 (Dynamic) */

#define is_16bit_mode() \
	(currentConfig.renderer == RT_16BIT || (PicoIn.AHW & PAHW_32X))

static int vsync_handler(void)
{
   iSignalSema(vsync_sema_id);

   ExitHandler();
   return 0;
}

static void set_g_menuscreen_values()
{
    if (g_menuscreen != NULL) {
        free(g_menuscreen->Mem);
        free(g_menuscreen);
        free(g_menubg_ptr);
        free(g_menuscreen_vertices);
    }
    g_menuscreen = (GSTEXTURE *)calloc(1, sizeof(GSTEXTURE));
    size_t g_menuscreenSize = gsKit_texture_size_ee(gsGlobal->Width, gsGlobal->Height, GS_PSM_CT16);
    g_menuscreen->Width = gsGlobal->Width;
    g_menuscreen->Height = gsGlobal->Height;
    g_menuscreen->PSM = GS_PSM_CT16;
    g_menuscreen->Mem = malloc(g_menuscreenSize);

    g_menubg_ptr = (uint8_t *)malloc(g_menuscreenSize);

    g_menuscreen_w = g_menuscreen->Width;
    g_menuscreen_h  = g_menuscreen->Height; 
    g_menuscreen_pp = g_menuscreen->Width;
    g_menuscreen_ptr = g_menuscreen->Mem;

    g_menubg_src_w = g_menuscreen->Width;
    g_menubg_src_h  = g_menuscreen->Height;
    g_menubg_src_pp = g_menuscreen->Width;

    g_menuscreen_vertices = (GSPRIMUVPOINT *)calloc(2, sizeof(GSPRIMUVPOINT));
    
    g_menuscreen_vertices[0].xyz2 = vertex_to_XYZ2(gsGlobal, 0, 0, 2);
	g_menuscreen_vertices[0].uv = vertex_to_UV(g_menuscreen, 0, 0);
	g_menuscreen_vertices[0].rgbaq = color_to_RGBAQ(0x80, 0x80, 0x80, 0x80, 0);

    g_menuscreen_vertices[1].xyz2 = vertex_to_XYZ2(gsGlobal, g_menuscreen->Width, g_menuscreen->Height, 2);
    g_menuscreen_vertices[1].uv = vertex_to_UV(g_menuscreen, g_menuscreen->Width, g_menuscreen->Height);
    g_menuscreen_vertices[1].rgbaq = color_to_RGBAQ(0x80, 0x80, 0x80, 0x80, 0);
}

void set_g_screen_values() {
    if (g_screen != NULL) {
        free(g_screen->Mem);
        free(g_screen);
        free(g_screen_vertices);
    }
    g_screen = (GSTEXTURE *)calloc(1, sizeof(GSTEXTURE));
    size_t g_screenSize = gsKit_texture_size_ee(328, 256, GS_PSM_CT16);
    g_screen->Width = 328;
    g_screen->Height = 256;
    g_screen->PSM = GS_PSM_CT16;
    g_screen->Mem = (uint32_t *)malloc(g_screenSize);

    g_screen_width = 328;
    g_screen_height = 256; 
    g_screen_ppitch = 328;
    g_screen_ptr = g_screen->Mem;

    g_screen_vertices = (GSPRIMUVPOINT *)calloc(2, sizeof(GSPRIMUVPOINT));
    
    g_screen_vertices[0].xyz2 = vertex_to_XYZ2(gsGlobal, 0, 0, 0);
    g_screen_vertices[0].uv = vertex_to_UV(g_screen, 0, 0);
    g_screen_vertices[0].rgbaq = color_to_RGBAQ(0x80, 0x80, 0x80, 0x80, 0);

    g_screen_vertices[1].xyz2 = vertex_to_XYZ2(gsGlobal, gsGlobal->Width, gsGlobal->Height, 0);
    g_screen_vertices[1].uv = vertex_to_UV(g_screen, g_screen->Width, g_screen->Height);
    g_screen_vertices[1].rgbaq = color_to_RGBAQ(0x80, 0x80, 0x80, 0x80, 0);

    if (is_16bit_mode())
		PicoDrawSetOutBuf(g_screen_ptr, g_screen_ppitch * 2);
	else
		PicoDrawSetOutBuf(g_screen_ptr, g_screen_ppitch);
}

void set_cdleds_values() {
    if (cdleds != NULL) {
        free(cdleds->Mem);
        free(cdleds);
        free(cdleds_vertices);
    }
    cdleds = (GSTEXTURE *)calloc(1, sizeof(GSTEXTURE));
    size_t cdledsSize = gsKit_texture_size_ee(14, 5, GS_PSM_CT16);
    cdleds->Width = 14;
    cdleds->Height = 5;
    cdleds->PSM = GS_PSM_CT16;
    cdleds->Mem = (uint32_t *)malloc(cdledsSize);

    cdleds_vertices = (GSPRIMUVPOINT *)calloc(2, sizeof(GSPRIMUVPOINT));
    
    cdleds_vertices[0].xyz2 = vertex_to_XYZ2(gsGlobal, 4, 1, 1);
    cdleds_vertices[0].uv = vertex_to_UV(cdleds, 0, 0);
    cdleds_vertices[0].rgbaq = color_to_RGBAQ(0x80, 0x80, 0x80, 0x80, 0);

    cdleds_vertices[1].xyz2 = vertex_to_XYZ2(gsGlobal, cdleds->Width, cdleds->Height, 1);
    cdleds_vertices[1].uv = vertex_to_UV(cdleds, cdleds->Width, cdleds->Height);
    cdleds_vertices[1].rgbaq = color_to_RGBAQ(0x80, 0x80, 0x80, 0x80, 0);
}

void set_osd_values() {
    if (osd != NULL) {
        free(osd->Mem);
        free(osd);
        free(osd_vertices);
    }
    osd = (GSTEXTURE *)calloc(1, sizeof(GSTEXTURE));
    size_t osdSize = gsKit_texture_size_ee(512, 8, GS_PSM_CT16);
    osd->Width = 512;
    osd->Height = 8;
    osd->PSM = GS_PSM_CT16;
    osd->Mem = (uint32_t *)malloc(osdSize);

    osd_vertices_count = 2;
    osd_vertices = (GSPRIMUVPOINT *)calloc(osd_vertices_count, sizeof(GSPRIMUVPOINT));
    
    osd_vertices[0].xyz2 = vertex_to_XYZ2(gsGlobal, 0, 0, 1);
    osd_vertices[0].uv = vertex_to_UV(osd, 0, 0);
    osd_vertices[0].rgbaq = color_to_RGBAQ(0x80, 0x80, 0x80, 0x80, 0);

    osd_vertices[1].xyz2 = vertex_to_XYZ2(gsGlobal, osd->Width, osd->Height, 1);
    osd_vertices[1].uv = vertex_to_UV(osd, osd->Width, osd->Height);
    osd_vertices[1].rgbaq = color_to_RGBAQ(0x80, 0x80, 0x80, 0x80, 0);
}

static void video_init(void)
{
    ee_sema_t sema;

    sema.init_count = 0;
    sema.max_count  = 1;
    sema.option     = 0;

    vsync_sema_id   = CreateSema(&sema);

    gsGlobal = gsKit_init_global();
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Height = 448;

    gsGlobal->PSM  = GS_PSM_CT24;
    gsGlobal->PSMZ = GS_PSMZ_16S;
    gsGlobal->ZBuffering = GS_SETTING_OFF;
    gsGlobal->DoubleBuffering = GS_SETTING_ON;
    gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
    gsGlobal->Dithering = GS_SETTING_OFF;


    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_set_clamp(gsGlobal, GS_CMODE_REPEAT);

    gsKit_vram_clear(gsGlobal);
    gsKit_init_screen(gsGlobal);
    gsKit_TexManager_init(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);
    gsKit_clear(gsGlobal, GS_BLACK);
    vsync = 0;
    vsync_callback_id = gsKit_add_vsync_handler(vsync_handler);

    set_g_menuscreen_values();
}

static void video_deinit(void)
{
    free(g_menuscreen->Mem);
    free(g_menuscreen);
    free(g_menuscreen_vertices);
    free(g_menubg_ptr);

    gsKit_clear(gsGlobal, GS_BLACK);
    gsKit_vram_clear(gsGlobal);
    gsKit_deinit_global(gsGlobal);
    gsKit_remove_vsync_handler(vsync_callback_id);

    if (vsync_sema_id >= 0)
        DeleteSema(vsync_sema_id);
}

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
    // int len = strlen(text) * 8;
	// int *p, h;
	// void *tmp = g_screen_ptr;
    // printf("osd_text, text: %s\n", text);

	// g_screen_ptr = osd_buf;
	// for (h = 0; h < 8; h++) {
	// 	p = (int *) (osd_buf+x+512*h);
	// 	p = (int *) ((int)p & ~3); // align
	// 	memset32_uncached(p, 0, len/2);
	// }
	// emu_text_out16(x, 0, text);
	// g_screen_ptr = tmp;

	// osd_buf_x[osd_buf_cnt] = x;
	// osd_buf_l[osd_buf_cnt] = len;
	// osd_buf_cnt ++;
}

static void blit_screen(void)
{
    gsKit_TexManager_invalidate(gsGlobal, g_screen);

    gsKit_TexManager_bind(gsGlobal, g_screen);
    gskit_prim_list_sprite_texture_uv_3d(
        gsGlobal, 
        g_screen,
        2,
        g_screen_vertices
    );
}

static void blit_osd(void)
{

}

static void cd_leds(void)
{
    unsigned int reg, col_g, col_r, *p;
    gsKit_TexManager_invalidate(gsGlobal, cdleds);

	reg = Pico_mcd->s68k_regs[0];

	p = (unsigned int *)cdleds->Mem;
	col_g = (reg & 2) ? 0x06000600 : 0;
	col_r = (reg & 1) ? 0x00180018 : 0;
	*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += 512/2 - 12/2;
	*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r; p += 512/2 - 12/2;
	*p++ = col_g; *p++ = col_g; p+=2; *p++ = col_r; *p++ = col_r;

	osd_cdleds = 1;
}

static void blit_cdleds(void)
{
    if (!osd_cdleds) return;

    gsKit_TexManager_bind(gsGlobal, cdleds);
    gskit_prim_list_sprite_texture_uv_3d(
        gsGlobal, 
        cdleds, 
        2,
        cdleds_vertices
    );
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

/* Copy of gsKit_sync_flip, but without the 'flip' */
static void gsKit_sync(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame)
      WaitSema(vsync_sema_id);

   while (PollSema(vsync_sema_id) >= 0);
}

/* Copy of gsKit_sync_flip, but without the 'sync' */
static void gsKit_flip(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame)
   {
      if (gsGlobal->DoubleBuffering == GS_SETTING_ON)
      {
         GS_SET_DISPFB2(gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] / 8192,
                        gsGlobal->Width / 64, gsGlobal->PSM, 0, 0);

         gsGlobal->ActiveBuffer ^= 1;
      }
   }

   gsKit_setactive(gsGlobal);
}

static void flipScreen()
{
	gsKit_queue_exec(gsGlobal);
	gsKit_finish();
    gsKit_flip(gsGlobal);

	gsKit_TexManager_nextFrame(gsGlobal);
    gsKit_clear(gsGlobal, GS_BLACK);
}


/* display a completed frame buffer and prepare a new render buffer */
void plat_video_flip(void)
{
    blit_screen();
    blit_osd();
	blit_cdleds();
    flipScreen();
}

/* wait for start of vertical blanking */
void plat_video_wait_vsync(void)
{
    gsKit_sync(gsGlobal);
}

/* switch from emulation display to menu display */
void plat_video_menu_enter(int is_rom_loaded)
{
}

/* start rendering a menu screen */
void plat_video_menu_begin(void)
{
    gsKit_TexManager_invalidate(gsGlobal, g_menuscreen);
}

/* display a completed menu screen */
void plat_video_menu_end(void)
{
    gsKit_TexManager_bind(gsGlobal, g_menuscreen);
    gskit_prim_list_sprite_texture_uv_3d(
        gsGlobal, 
        g_menuscreen, 
        2, 
        g_menuscreen_vertices
    );
    flipScreen(1);
}

/* terminate menu display */
void plat_video_menu_leave(void)
{
}

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
	defaultConfig.renderer = RT_16BIT;
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
    video_init();
    init_joystick_driver(false);
    in_ps2_init(in_ps2_defbinds);
    in_probe();
    init_audio_driver();
    plat_get_data_dir(rom_fname_loaded, sizeof(rom_fname_loaded));
}

void plat_finish(void) {
    deinit_audio_driver();
    deinit_joystick_driver(false);
    video_deinit();
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
	// not needed since the screen buf is cleared through the gskit_clear
}

/* change the audio volume setting */
void plat_update_volume(int has_changed, int is_up) {}

/* prepare for MD screen mode change */
void emu_video_mode_change(int start_line, int line_count, int start_col, int col_count)
{
	int h43 = (col_count  >= 192 ? 320 : col_count); // ugh, mind GG...
	int v43 = (line_count >= 192 ? Pico.m.pal ? 240 : 224 : line_count);

	if (col_count == 248) // mind aspect ratio when blanking 1st column
		col_count = 256;
    
    g_screen_vertices[0].uv = vertex_to_UV(g_screen, start_col, start_line);
    g_screen_vertices[1].uv = vertex_to_UV(g_screen, col_count, line_count);

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
    set_g_screen_values();
    set_cdleds_values();
}

/* terminate the emulator loop */
void pemu_loop_end(void)
{
    pemu_sound_stop();
}