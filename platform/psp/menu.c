
// TODO scaling configuration non-functional ATM

static const char *men_scaler[] = { "unscaled", "4:3", "fullscreen", NULL };
#if 0
static const char h_cscaler40[]   = "Configures the custom scaler for wide resolution";
static const char h_cscaler32[]   = "Configures the custom scaler for narrow resolution";

static int menu_loop_cscaler(int id, int keys)
{
	unsigned int inp;

	currentConfig.scaling = SCALE_CUSTOM;

	pnd_setup_layer(1, g_layer_cx, g_layer_cy, g_layer_cw, g_layer_ch);
	pnd_restore_layer_data();

	for (;;)
	{
		menu_draw_begin(0, 1);
		menuscreen_memset_lines(g_menuscreen_ptr, 0, g_menuscreen_h);
		text_out16(2, 480 - 18, "%dx%d | d-pad to resize, R+d-pad to move", g_layer_cw, g_layer_ch);
		menu_draw_end();

		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT
				   |PBTN_R|PBTN_MOK|PBTN_MBACK, NULL, 40);
		if (inp & PBTN_UP)    g_layer_cy--;
		if (inp & PBTN_DOWN)  g_layer_cy++;
		if (inp & PBTN_LEFT)  g_layer_cx--;
		if (inp & PBTN_RIGHT) g_layer_cx++;
		if (!(inp & PBTN_R)) {
			if (inp & PBTN_UP)    g_layer_ch += 2;
			if (inp & PBTN_DOWN)  g_layer_ch -= 2;
			if (inp & PBTN_LEFT)  g_layer_cw += 2;
			if (inp & PBTN_RIGHT) g_layer_cw -= 2;
		}
		if (inp & (PBTN_MOK|PBTN_MBACK))
			break;

		if (inp & (PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT)) {
			if (g_layer_cx < 0)   g_layer_cx = 0;
			if (g_layer_cx > 640) g_layer_cx = 640;
			if (g_layer_cy < 0)   g_layer_cy = 0;
			if (g_layer_cy > 420) g_layer_cy = 420;
			if (g_layer_cw < 160) g_layer_cw = 160;
			if (g_layer_ch < 60)  g_layer_ch = 60;
			if (g_layer_cx + g_layer_cw > 800)
				g_layer_cw = 800 - g_layer_cx;
			if (g_layer_cy + g_layer_ch > 480)
				g_layer_ch = 480 - g_layer_cy;
			pnd_setup_layer(1, g_layer_cx, g_layer_cy, g_layer_cw, g_layer_ch);
		}
	}

	pnd_setup_layer(0, g_layer_cx, g_layer_cy, g_layer_cw, g_layer_ch);

	return 0;
}
#endif

#define MENU_OPTIONS_GFX \
	mee_enum      ("Scaler",                   MA_OPT_SCALING,        currentConfig.scaling, \
	                                                                  men_scaler), \
	mee_onoff     ("Vsync",                    MA_OPT3_VSYNC,         currentConfig.EmuOpt, EOPT_VSYNC), \
	/* mee_cust_h    ("Setup custom scaler",      MA_NONE,               menu_loop_cscaler, NULL, h_cscaler), \*/

#define MENU_OPTIONS_ADV

