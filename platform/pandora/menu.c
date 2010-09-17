#include "plat.h"

static const char *men_scaler[] = { "1x1, 1x1", "2x2, 3x2", "2x2, 2x2", "fullscreen", "custom", NULL };
static const char h_scaler[]    = "Scalers for 40 and 32 column modes\n"
				  "(320 and 256 pixel wide horizontal)";
static const char h_cscaler[]   = "Displays the scaler layer, you can resize it\n"
				  "using d-pad or move it using R+d-pad";
static const char *men_dummy[] = { NULL };
char **pnd_filter_list;
int g_layer_cx = 80, g_layer_cy = 0;
int g_layer_cw = 640, g_layer_ch = 480;

static int menu_loop_cscaler(menu_id id, int keys)
{
	unsigned int inp;

	currentConfig.scaling = SCALE_CUSTOM;

	pnd_setup_layer(1, g_layer_cx, g_layer_cy, g_layer_cw, g_layer_ch);
	pnd_restore_layer_data();

	for (;;)
	{
		menu_draw_begin(0);
		memset(g_menuscreen_ptr, 0, g_menuscreen_w * g_menuscreen_h * 2);
		text_out16(2, 480 - 18, "%dx%d | d-pad to resize, R+d-pad to move", g_layer_cw, g_layer_ch);
		menu_draw_end();

		inp = in_menu_wait(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_R|PBTN_MOK|PBTN_MBACK, 40);
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

#define MENU_OPTIONS_GFX \
	mee_enum_h    ("Scaler",                   MA_OPT_SCALING,        currentConfig.scaling, \
	                                                                  men_scaler, h_scaler), \
	mee_enum      ("Filter",                   MA_OPT3_FILTERING,     currentConfig.filter, men_dummy), \
	mee_onoff     ("Vsync",                    MA_OPT2_VSYNC,         currentConfig.EmuOpt, EOPT_VSYNC), \
	mee_cust_h    ("Setup custom scaler",      MA_NONE,               menu_loop_cscaler, NULL, h_cscaler), \
	mee_range_hide("layer_x",                  MA_OPT3_LAYER_X,       g_layer_cx, 0, 640), \
	mee_range_hide("layer_y",                  MA_OPT3_LAYER_Y,       g_layer_cy, 0, 420), \
	mee_range_hide("layer_w",                  MA_OPT3_LAYER_W,       g_layer_cw, 160, 800), \
	mee_range_hide("layer_h",                  MA_OPT3_LAYER_H,       g_layer_ch,  60, 480), \

#define MENU_OPTIONS_ADV \
	mee_onoff     ("SVP dynarec",              MA_OPT2_SVP_DYNAREC,   PicoOpt, POPT_EN_SVP_DRC), \
	mee_onoff     ("Status line in main menu", MA_OPT2_STATUS_LINE,   currentConfig.EmuOpt, EOPT_SHOW_RTC),

#define menu_main_plat_draw NULL

#include <dirent.h>
#include <errno.h>

static menu_entry e_menu_gfx_options[];
static menu_entry e_menu_options[];
static menu_entry e_menu_keyconfig[];

void pnd_menu_init(void)
{
	struct dirent *ent;
	int i, count = 0;
	char **mfilters;
	char buff[64];
	DIR *dir;

	dir = opendir("/etc/pandora/conf/dss_fir");
	if (dir == NULL) {
		perror("filter opendir");
		return;
	}

	while (1) {
		errno = 0;
		ent = readdir(dir);
		if (ent == NULL) {
			if (errno != 0)
				perror("readdir");
			break;
		}
		if (strstr(ent->d_name, "_up_h"))
			count++;
	}

	if (count == 0)
		return;

	mfilters = calloc(count + 1, sizeof(mfilters[0]));
	if (mfilters == NULL)
		return;

	rewinddir(dir);
	for (i = 0; (ent = readdir(dir)); ) {
		char *pos;
		size_t len;

		pos = strstr(ent->d_name, "_up_h");
		if (pos == NULL)
			continue;

		len = pos - ent->d_name;
		if (len > sizeof(buff) - 1)
			continue;

		strncpy(buff, ent->d_name, len);
		buff[len] = 0;
		mfilters[i] = strdup(buff);
		if (mfilters[i] != NULL)
			i++;
	}
	closedir(dir);

	i = me_id2offset(e_menu_gfx_options, MA_OPT3_FILTERING);
	e_menu_gfx_options[i].data = (void *)mfilters;
	pnd_filter_list = mfilters;

	i = me_id2offset(e_menu_options, MA_OPT_CPU_CLOCKS);
	e_menu_options[i].name = "Max CPU clock";

	me_enable(e_menu_keyconfig, MA_CTRL_DEADZONE, 0);
}

