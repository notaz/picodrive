#define MENU_OPTIONS_GFX \
	mee_onoff     ("Vsync",                    MA_OPT2_VSYNC,         currentConfig.EmuOpt, EOPT_VSYNC),

#define MENU_OPTIONS_ADV \
	mee_onoff     ("SVP dynarec",              MA_OPT2_SVP_DYNAREC,   PicoOpt, POPT_EN_SVP_DRC), \
	mee_onoff     ("Status line in main menu", MA_OPT2_STATUS_LINE,   currentConfig.EmuOpt, EOPT_SHOW_RTC),

#define menu_main_plat_draw NULL
