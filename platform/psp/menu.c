
static const char h_scale43[]	= "Scales low and high res to 4:3 screen size.\n"
				  "For 240 line PAL use Scale factor 1.12";
static const char h_scalefull[] = "Scales low and high res to full screen.\n"
				  "For 240 line PAL use scaling 1.12, 1.6, 1.28";
#define MENU_OPTIONS_GFX \
	mee_cust("Scale factor",                    MA_OPT3_SCALE,    mh_scale, ms_scale), \
	mee_cust("Hor. scale (for low res. games)", MA_OPT3_HSCALE32, mh_scale, ms_scale), \
	mee_cust("Hor. scale (for hi res. games)",  MA_OPT3_HSCALE40, mh_scale, ms_scale), \
	mee_onoff("Bilinear filtering",             MA_OPT3_FILTERING, currentConfig.scaling, 1), \
	mee_range("Gamma adjustment",               MA_OPT3_GAMMAA,    currentConfig.gamma, -4, 16), \
	mee_range("Black level",                    MA_OPT3_BLACKLVL,  currentConfig.gamma2,  0,  2), \
	mee_onoff("wait for vsync",                 MA_OPT3_VSYNC,     currentConfig.EmuOpt, EOPT_VSYNC), \
	mee_cust_nosave("Set to unscaled centered", MA_OPT3_PRES_NOSCALE, mh_preset_scale, NULL), \
	mee_cust_nosave("Set to 4:3 scaled",        MA_OPT3_PRES_SCALE43, mh_preset_scale, NULL), \
	mee_cust_nosave("Set to fullscreen",        MA_OPT3_PRES_FULLSCR, mh_preset_scale, NULL), \

#define MENU_OPTIONS_ADV


static const char *ms_scale(int id, int *offs)
{
	float val = 0;
	switch (id) {
	case MA_OPT3_SCALE:	val = currentConfig.scale; break;
	case MA_OPT3_HSCALE32:	val = currentConfig.hscale32; break;
	case MA_OPT3_HSCALE40:	val = currentConfig.hscale40; break;
	}
	sprintf(static_buff, "%.2f", val);
	return static_buff;
}

static int mh_scale(int id, int keys)
{
	float *val = NULL;
	switch (id) {
	case MA_OPT3_SCALE:	val = &currentConfig.scale; break;
	case MA_OPT3_HSCALE32:	val = &currentConfig.hscale32; break;
	case MA_OPT3_HSCALE40:	val = &currentConfig.hscale40; break;
	}
	if (keys & PBTN_LEFT)	*val += -0.01;
	if (keys & PBTN_RIGHT)	*val += +0.01;
	if (*val < 0.01)	*val  = +0.01;
	return 0;
}


static int mh_preset_scale(int id, int keys)
{
	switch (id) {
	case MA_OPT3_PRES_NOSCALE:
		currentConfig.scale = 1.0;
		currentConfig.hscale32 = 1.0;
		currentConfig.hscale40 = 1.0;
		break;
	case MA_OPT3_PRES_SCALE43:
		// parameters for 224 lines; for 240 lines scale = 1.125
		// moreover, H32 and H40 had the same width on a TV.
		currentConfig.scale = 1.2;
		currentConfig.hscale32 = 1.25;
		currentConfig.hscale40 = 1.0;
		break;
	case MA_OPT3_PRES_FULLSCR:
		// uses width 460 to avoid some ugly moiree effects
		currentConfig.scale = 1.2;
		currentConfig.hscale32 = 1.5;
		currentConfig.hscale40 = 1.2;
		break;
	}
	return 0;
}

static menu_entry e_menu_gfx_options[];

void psp_menu_init(void)
{
	int i;
	for (i = 0; e_menu_gfx_options[i].name; i++) {
		switch (e_menu_gfx_options[i].id) {
		case MA_OPT3_PRES_SCALE43: e_menu_gfx_options[i].help = h_scale43; break;
		case MA_OPT3_PRES_FULLSCR: e_menu_gfx_options[i].help = h_scalefull; break;
		}
	}
}
