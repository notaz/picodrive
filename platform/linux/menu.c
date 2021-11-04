// ------------ gfx options menu ------------

static const char *men_scaling_opts[] = { "OFF", "software", "hardware", NULL };
static const char *men_filter_opts[] = { "nearest", "smoother", "bilinear 1", "bilinear 2", NULL };
static const char *men_ghosting_opts[] = { "OFF", "weak", "normal", NULL };

static const char h_scale[] = "hardware scaling may not be working on some devices";
static const char h_ghost[] = "when active simulates inertia of the GG LCD display";

#define MENU_OPTIONS_GFX \
	mee_enum_h    ("Horizontal scaling",     MA_OPT_SCALING, currentConfig.scaling, men_scaling_opts, h_scale), \
	mee_enum_h    ("Vertical scaling",       MA_OPT_VSCALING, currentConfig.vscaling, men_scaling_opts, h_scale), \
	mee_enum_h    ("Scaler type",            MA_OPT3_FILTERING, currentConfig.filter, men_filter_opts, NULL), \
	mee_enum_h    ("Game Gear LCD ghosting", MA_SMSOPT_GHOSTING, currentConfig.ghosting, men_ghosting_opts, h_ghost), \

#define MENU_OPTIONS_ADV

void linux_menu_init(void)
{
}

