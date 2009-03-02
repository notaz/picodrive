#include <stdlib.h>
#include <string.h>

#include "../common/common.h"
#include "../common/input.h"
#include "in_gp2x.h"

#define IN_PREFIX "gp2x:"
#define IN_GP2X_NBUTTONS 32

extern volatile unsigned short *gp2x_memregs; /* from minimal library rlyeh */

enum  { BTN_UP = 0,      BTN_LEFT = 2,      BTN_DOWN = 4,  BTN_RIGHT = 6,
        BTN_START = 8,   BTN_SELECT = 9,    BTN_L = 10,    BTN_R = 11,
        BTN_A = 12,      BTN_B = 13,        BTN_X = 14,    BTN_Y = 15,
        BTN_VOL_UP = 23, BTN_VOL_DOWN = 22, BTN_PUSH = 27 };

static const char * const in_gp2x_prefix = IN_PREFIX;
static const char * const in_gp2x_keys[IN_GP2X_NBUTTONS] = {
	[0 ... IN_GP2X_NBUTTONS-1] = NULL,
	[BTN_UP]    = "UP",    [BTN_LEFT]   = "LEFT",   [BTN_DOWN] = "DOWN", [BTN_RIGHT] = "RIGHT",
	[BTN_START] = "START", [BTN_SELECT] = "SELECT", [BTN_L]    = "L",    [BTN_R]     = "R",
	[BTN_A]     = "A",     [BTN_B]      = "B",      [BTN_X]    = "X",    [BTN_Y]     = "Y",
	[BTN_VOL_DOWN]= "VOL DOWN",                     [BTN_VOL_UP] = "VOL UP",
	[BTN_PUSH] = "PUSH"
};


static void in_gp2x_probe(void)
{
	in_register(IN_PREFIX "GP2X pad", IN_DRVID_GP2X, -1, (void *)1);
}

static int in_gp2x_get_bind_count(void)
{
	return IN_GP2X_NBUTTONS;
}

static int in_gp2x_get_gpio_bits(void)
{
#ifndef FAKE_IN_GP2X
	int value;
	value = gp2x_memregs[0x1198>>1] & 0xff; // GPIO M
	if (value == 0xFD) value = 0xFA;
	if (value == 0xF7) value = 0xEB;
	if (value == 0xDF) value = 0xAF;
	if (value == 0x7F) value = 0xBE;
	value |= gp2x_memregs[0x1184>>1] & 0xFF00; // GPIO C
	value |= gp2x_memregs[0x1186>>1] << 16; // GPIO D
	value = ~value & 0x08c0ff55;

	return value;
#else
	extern int current_keys;
	return current_keys;
#endif
}

/* returns bitfield of binds of pressed buttons */
int in_gp2x_update(void *drv_data, int *binds)
{
	int i, value, ret = 0;

	value = in_gp2x_get_gpio_bits();

	for (i = 0; value; i++) {
		if (value & 1)
			ret |= binds[i];
		value >>= 1;
	}

	return ret;
}

int in_gp2x_update_keycode(void *data, int *is_down)
{
	static int old_val = 0;
	int val, diff, i;

	val = in_gp2x_get_gpio_bits();
	diff = val ^ old_val;
	if (diff == 0)
		return -1;

	/* take one bit only */
	for (i = 0; i < sizeof(diff)*8; i++)
		if (diff & (1<<i))
			break;

	old_val ^= 1 << i;

	if (is_down)
		*is_down = !!(val & (1<<i));
	return i;
}

static int in_gp2x_menu_translate(int keycode)
{
	switch (keycode) {
		case BTN_UP:	return PBTN_UP;
		case BTN_LEFT:	return PBTN_LEFT;
		case BTN_DOWN:	return PBTN_DOWN;
		case BTN_RIGHT:	return PBTN_RIGHT;
		case BTN_B:	return PBTN_MOK;
		case BTN_X:	return PBTN_MBACK;
		case BTN_SELECT:return PBTN_MENU;
		case BTN_L:	return PBTN_L;
		case BTN_R:	return PBTN_R;
		default:	return 0;
	}
}

static int in_gp2x_get_key_code(const char *key_name)
{
	int i;

	for (i = 0; i < IN_GP2X_NBUTTONS; i++) {
		const char *k = in_gp2x_keys[i];
		if (k != NULL && strcasecmp(k, key_name) == 0)
			return i;
	}

	return -1;
}

static const char *in_gp2x_get_key_name(int keycode)
{
	const char *name = NULL;
	if (keycode >= 0 && keycode < IN_GP2X_NBUTTONS)
		name = in_gp2x_keys[keycode];
	if (name == NULL)
		name = "Unkn";
	
	return name;
}

static const struct {
	short code;
	short bit;
} in_gp2x_def_binds[] =
{
	/* MXYZ SACB RLDU */
	{ BTN_UP,	0 },
	{ BTN_DOWN,	1 },
	{ BTN_LEFT,	2 },
	{ BTN_RIGHT,	3 },
	{ BTN_X,	4 },	/* B */
	{ BTN_B,	5 },	/* C */
	{ BTN_A,	6 },	/* A */
	{ BTN_START,	7 },
};

#define DEF_BIND_COUNT (sizeof(in_gp2x_def_binds) / sizeof(in_gp2x_def_binds[0]))

static void in_gp2x_get_def_binds(int *binds)
{
	int i;

	for (i = 0; i < DEF_BIND_COUNT; i++)
		binds[in_gp2x_def_binds[i].code] = 1 << in_gp2x_def_binds[i].bit;
}

/* remove binds of missing keys, count remaining ones */
static int in_gp2x_clean_binds(void *drv_data, int *binds)
{
	int i, count = 0;

	for (i = 0; i < IN_GP2X_NBUTTONS; i++) {
		if (in_gp2x_keys[i] == NULL)
			binds[i] = binds[i + IN_GP2X_NBUTTONS] = 0;
		if (binds[i])
			count++;
	}

	return count;

}

void in_gp2x_init(void *vdrv)
{
	in_drv_t *drv = vdrv;

	drv->prefix = in_gp2x_prefix;
	drv->probe = in_gp2x_probe;
	drv->get_bind_count = in_gp2x_get_bind_count;
	drv->get_def_binds = in_gp2x_get_def_binds;
	drv->clean_binds = in_gp2x_clean_binds;
	drv->menu_translate = in_gp2x_menu_translate;
	drv->get_key_code = in_gp2x_get_key_code;
	drv->get_key_name = in_gp2x_get_key_name;
	drv->update_keycode = in_gp2x_update_keycode;
}

