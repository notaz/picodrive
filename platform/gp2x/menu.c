#include <time.h>
#include "soc.h"

static void menu_main_plat_draw(void)
{
	static time_t last_bat_read = 0;
	static int last_bat_val = -1;
	unsigned short *bp = g_screen_ptr;
	int bat_h = me_mfont_h * 2 / 3;
	int i, u, w, wfill, batt_val;
	struct tm *tmp;
	time_t ltime;
	char time_s[16];

	if (!(currentConfig.EmuOpt & EOPT_SHOW_RTC))
		return;

	ltime = time(NULL);
	tmp = gmtime(&ltime);
	strftime(time_s, sizeof(time_s), "%H:%M", tmp);

	text_out16(g_screen_width - me_mfont_w * 6, me_mfont_h + 2, time_s);

	if (ltime - last_bat_read > 10) {
		last_bat_read = ltime;
		last_bat_val = batt_val = gp2x_read_battery();
	}
	else
		batt_val = last_bat_val;

	if (batt_val < 0 || batt_val > 100)
		return;

	/* battery info */
	bp += (me_mfont_h * 2 + 2) * g_screen_width + g_screen_width - me_mfont_w * 3 - 3;
	for (i = 0; i < me_mfont_w * 2; i++)
		bp[i] = menu_text_color;
	for (i = 0; i < me_mfont_w * 2; i++)
		bp[i + g_screen_width * bat_h] = menu_text_color;
	for (i = 0; i <= bat_h; i++)
		bp[i * g_screen_width] =
		bp[i * g_screen_width + me_mfont_w * 2] = menu_text_color;
	for (i = 2; i < bat_h - 1; i++)
		bp[i * g_screen_width - 1] =
		bp[i * g_screen_width - 2] = menu_text_color;

	w = me_mfont_w * 2 - 1;
	wfill = batt_val * w / 100;
	for (u = 1; u < bat_h; u++)
		for (i = 0; i < wfill; i++)
			bp[(w - i) + g_screen_width * u] = menu_text_color;
}

// ------------ gfx options menu ------------

static const char *mgn_opt_scaling(menu_id id, int *offs)
{
	*offs = -13;
	switch (currentConfig.scaling) {
		default:               return "             OFF";
		case EOPT_SCALE_HW_H:  return "   hw horizontal";
		case EOPT_SCALE_HW_HV: return "hw horiz. + vert";
		case EOPT_SCALE_SW_H:  return "   sw horizontal";
	}
}

static const char *mgn_aopt_gamma(menu_id id, int *offs)
{
	sprintf(static_buff, "%i.%02i", currentConfig.gamma / 100, currentConfig.gamma%100);
	return static_buff;
}


#define MENU_OPTIONS_GFX \
	mee_range_cust("Scaling",                  MA_OPT_SCALING,        currentConfig.scaling, 0, 3, mgn_opt_scaling), \
	mee_onoff     ("Tearing Fix",              MA_OPT_TEARING_FIX,    currentConfig.EmuOpt, EOPT_WIZ_TEAR_FIX), \
	mee_range_cust("Gamma correction",         MA_OPT2_GAMMA,         currentConfig.gamma, 1, 300, mgn_aopt_gamma), \
	mee_onoff     ("A_SN's gamma curve",       MA_OPT2_A_SN_GAMMA,    currentConfig.EmuOpt, EOPT_A_SN_GAMMA), \
	mee_onoff     ("Vsync",                    MA_OPT2_VSYNC,         currentConfig.EmuOpt, EOPT_VSYNC),

#define MENU_OPTIONS_ADV \
	mee_onoff     ("Use second CPU for sound", MA_OPT_ARM940_SOUND,   PicoOpt, POPT_EXT_FM), \
	mee_onoff     ("RAM overclock",            MA_OPT2_RAMTIMINGS,    currentConfig.EmuOpt, EOPT_RAM_TIMINGS), \
	mee_onoff     ("MMU hack",                 MA_OPT2_SQUIDGEHACK,   currentConfig.EmuOpt, EOPT_MMUHACK), \
	mee_onoff     ("SVP dynarec",              MA_OPT2_SVP_DYNAREC,   PicoOpt, POPT_EN_SVP_DRC), \
	mee_onoff     ("Status line in main menu", MA_OPT2_STATUS_LINE,   currentConfig.EmuOpt, EOPT_SHOW_RTC),

