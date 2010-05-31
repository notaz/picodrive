static const char *men_scaler[] = { "1x1, 1x1", "2x2, 3x2", "2x2, 2x2", NULL };
static const char h_scaler[]    = "Scalers for 40 and 32 column modes\n"
				  "(320 and 256 pixel wide horizontal)";

#define MENU_OPTIONS_GFX \
	mee_onoff     ("Vsync",                    MA_OPT2_VSYNC,         currentConfig.EmuOpt, EOPT_VSYNC), \
	mee_enum_h    ("Scaler",                   MA_OPT_SCALING,        currentConfig.scaling, \
	                                                                  men_scaler, h_scaler),

#define MENU_OPTIONS_ADV \
	mee_onoff     ("SVP dynarec",              MA_OPT2_SVP_DYNAREC,   PicoOpt, POPT_EN_SVP_DRC), \
	mee_onoff     ("Status line in main menu", MA_OPT2_STATUS_LINE,   currentConfig.EmuOpt, EOPT_SHOW_RTC),

#define menu_main_plat_draw NULL
