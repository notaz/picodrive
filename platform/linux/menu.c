#include "../libpicofe/gp2x/plat_gp2x.h"

// ------------ gfx options menu ------------


const char *men_scaling_opts[] = { "OFF", "ON", NULL };

#define MENU_OPTIONS_GFX \
	// mee_enum      ("screen scaling",           MA_OPT_SCALING,        currentConfig.scaling, men_scaling_opts), \

#define MENU_OPTIONS_ADV

void linux_menu_init(void)
{
}

