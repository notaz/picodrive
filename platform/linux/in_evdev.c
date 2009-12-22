#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>

#include "../common/input.h"
#include "in_evdev.h"

typedef struct {
	int fd;
	int *kbits;
	int abs_lzone;
	int abs_rzone;
	int abs_tzone;
	int abs_bzone;
	int abs_lastx;
	int abs_lasty;
} in_evdev_t;

#ifndef KEY_CNT
#define KEY_CNT (KEY_MAX + 1)
#endif

#define KEYBITS_BIT(x) (keybits[(x)/sizeof(keybits[0])/8] & \
	(1 << ((x) & (sizeof(keybits[0])*8-1))))

#define KEYBITS_BIT_SET(x) (keybits[(x)/sizeof(keybits[0])/8] |= \
	(1 << ((x) & (sizeof(keybits[0])*8-1))))

#define KEYBITS_BIT_CLEAR(x) (keybits[(x)/sizeof(keybits[0])/8] &= \
	~(1 << ((x) & (sizeof(keybits[0])*8-1))))

static const char * const in_evdev_prefix = "evdev:";
static const char * const in_evdev_keys[KEY_CNT] = {
	[0 ... KEY_MAX] = NULL,
	[KEY_RESERVED] = "Reserved",		[KEY_ESC] = "Esc",
	[KEY_1] = "1",				[KEY_2] = "2",
	[KEY_3] = "3",				[KEY_4] = "4",
	[KEY_5] = "5",				[KEY_6] = "6",
	[KEY_7] = "7",				[KEY_8] = "8",
	[KEY_9] = "9",				[KEY_0] = "0",
	[KEY_MINUS] = "Minus",			[KEY_EQUAL] = "Equal",
	[KEY_BACKSPACE] = "Backspace",		[KEY_TAB] = "Tab",
	[KEY_Q] = "Q",				[KEY_W] = "W",
	[KEY_E] = "E",				[KEY_R] = "R",
	[KEY_T] = "T",				[KEY_Y] = "Y",
	[KEY_U] = "U",				[KEY_I] = "I",
	[KEY_O] = "O",				[KEY_P] = "P",
	[KEY_LEFTBRACE] = "LeftBrace",		[KEY_RIGHTBRACE] = "RightBrace",
	[KEY_ENTER] = "Enter",			[KEY_LEFTCTRL] = "LeftControl",
	[KEY_A] = "A",				[KEY_S] = "S",
	[KEY_D] = "D",				[KEY_F] = "F",
	[KEY_G] = "G",				[KEY_H] = "H",
	[KEY_J] = "J",				[KEY_K] = "K",
	[KEY_L] = "L",				[KEY_SEMICOLON] = "Semicolon",
	[KEY_APOSTROPHE] = "Apostrophe",	[KEY_GRAVE] = "Grave",
	[KEY_LEFTSHIFT] = "LeftShift",		[KEY_BACKSLASH] = "BackSlash",
	[KEY_Z] = "Z",				[KEY_X] = "X",
	[KEY_C] = "C",				[KEY_V] = "V",
	[KEY_B] = "B",				[KEY_N] = "N",
	[KEY_M] = "M",				[KEY_COMMA] = "Comma",
	[KEY_DOT] = "Dot",			[KEY_SLASH] = "Slash",
	[KEY_RIGHTSHIFT] = "RightShift",	[KEY_KPASTERISK] = "KPAsterisk",
	[KEY_LEFTALT] = "LeftAlt",		[KEY_SPACE] = "Space",
	[KEY_CAPSLOCK] = "CapsLock",		[KEY_F1] = "F1",
	[KEY_F2] = "F2",			[KEY_F3] = "F3",
	[KEY_F4] = "F4",			[KEY_F5] = "F5",
	[KEY_F6] = "F6",			[KEY_F7] = "F7",
	[KEY_F8] = "F8",			[KEY_F9] = "F9",
	[KEY_F10] = "F10",			[KEY_NUMLOCK] = "NumLock",
	[KEY_SCROLLLOCK] = "ScrollLock",	[KEY_KP7] = "KP7",
	[KEY_KP8] = "KP8",			[KEY_KP9] = "KP9",
	[KEY_KPMINUS] = "KPMinus",		[KEY_KP4] = "KP4",
	[KEY_KP5] = "KP5",			[KEY_KP6] = "KP6",
	[KEY_KPPLUS] = "KPPlus",		[KEY_KP1] = "KP1",
	[KEY_KP2] = "KP2",			[KEY_KP3] = "KP3",
	[KEY_KP0] = "KP0",			[KEY_KPDOT] = "KPDot",
	[KEY_ZENKAKUHANKAKU] = "Zenkaku/Hankaku", [KEY_102ND] = "102nd",
	[KEY_F11] = "F11",			[KEY_F12] = "F12",
	[KEY_KPJPCOMMA] = "KPJpComma",		[KEY_KPENTER] = "KPEnter",
	[KEY_RIGHTCTRL] = "RightCtrl",		[KEY_KPSLASH] = "KPSlash",
	[KEY_SYSRQ] = "SysRq",			[KEY_RIGHTALT] = "RightAlt",
	[KEY_LINEFEED] = "LineFeed",		[KEY_HOME] = "Home",
	[KEY_UP] = "Up",			[KEY_PAGEUP] = "PageUp",
	[KEY_LEFT] = "Left",			[KEY_RIGHT] = "Right",
	[KEY_END] = "End",			[KEY_DOWN] = "Down",
	[KEY_PAGEDOWN] = "PageDown",		[KEY_INSERT] = "Insert",
	[KEY_DELETE] = "Delete",		[KEY_MACRO] = "Macro",
	[KEY_HELP] = "Help",			[KEY_MENU] = "Menu",
	[KEY_COFFEE] = "Coffee",		[KEY_DIRECTION] = "Direction",
	[BTN_0] = "Btn0",			[BTN_1] = "Btn1",
	[BTN_2] = "Btn2",			[BTN_3] = "Btn3",
	[BTN_4] = "Btn4",			[BTN_5] = "Btn5",
	[BTN_6] = "Btn6",			[BTN_7] = "Btn7",
	[BTN_8] = "Btn8",			[BTN_9] = "Btn9",
	[BTN_LEFT] = "LeftBtn",			[BTN_RIGHT] = "RightBtn",
	[BTN_MIDDLE] = "MiddleBtn",		[BTN_SIDE] = "SideBtn",
	[BTN_EXTRA] = "ExtraBtn",		[BTN_FORWARD] = "ForwardBtn",
	[BTN_BACK] = "BackBtn",			[BTN_TASK] = "TaskBtn",
	[BTN_TRIGGER] = "Trigger",		[BTN_THUMB] = "ThumbBtn",
	[BTN_THUMB2] = "ThumbBtn2",		[BTN_TOP] = "TopBtn",
	[BTN_TOP2] = "TopBtn2",			[BTN_PINKIE] = "PinkieBtn",
	[BTN_BASE] = "BaseBtn",			[BTN_BASE2] = "BaseBtn2",
	[BTN_BASE3] = "BaseBtn3",		[BTN_BASE4] = "BaseBtn4",
	[BTN_BASE5] = "BaseBtn5",		[BTN_BASE6] = "BaseBtn6",
	[BTN_DEAD] = "BtnDead",			[BTN_A] = "BtnA",
	[BTN_B] = "BtnB",			[BTN_C] = "BtnC",
	[BTN_X] = "BtnX",			[BTN_Y] = "BtnY",
	[BTN_Z] = "BtnZ",			[BTN_TL] = "BtnTL",
	[BTN_TR] = "BtnTR",			[BTN_TL2] = "BtnTL2",
	[BTN_TR2] = "BtnTR2",			[BTN_SELECT] = "BtnSelect",
	[BTN_START] = "BtnStart",		[BTN_MODE] = "BtnMode",
	[BTN_THUMBL] = "BtnThumbL",		[BTN_THUMBR] = "BtnThumbR",
	[BTN_TOUCH] = "Touch",			[BTN_STYLUS] = "Stylus",
	[BTN_STYLUS2] = "Stylus2",		[BTN_TOOL_DOUBLETAP] = "Tool Doubletap",
	[BTN_TOOL_TRIPLETAP] = "Tool Tripletap", [BTN_GEAR_DOWN] = "WheelBtn",
	[BTN_GEAR_UP] = "Gear up",		[KEY_OK] = "Ok",
};


static void in_evdev_probe(void)
{
	long keybits[KEY_CNT / sizeof(long) / 8];
	long absbits[(ABS_MAX+1) / sizeof(long) / 8];
	int i;

	// the kernel might support and return less keys then we know about,
	// so make sure the buffers are clear.
	memset(keybits, 0, sizeof(keybits));
	memset(absbits, 0, sizeof(absbits));

	for (i = 0;; i++)
	{
		int support = 0, count = 0;
		in_evdev_t *dev;
		int u, ret, fd;
		char name[64];

		snprintf(name, sizeof(name), "/dev/input/event%d", i);
		fd = open(name, O_RDONLY|O_NONBLOCK);
		if (fd == -1) {
			if (errno == EACCES)
				continue;	/* maybe we can access next one */
			break;
		}

		/* check supported events */
		ret = ioctl(fd, EVIOCGBIT(0, sizeof(support)), &support);
		if (ret == -1) {
			printf("in_evdev: ioctl failed on %s\n", name);
			goto skip;
		}

		if (!(support & (1 << EV_KEY)))
			goto skip;

		ret = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);
		if (ret == -1) {
			printf("in_evdev: ioctl failed on %s\n", name);
			goto skip;
		}

		/* check for interesting keys */
		for (u = 0; u < KEY_CNT; u++) {
			if (KEYBITS_BIT(u) && u != KEY_POWER &&
					u != KEY_SLEEP && u != BTN_TOUCH)
				count++;
		}

		if (count == 0)
			goto skip;

		dev = calloc(1, sizeof(*dev));
		if (dev == NULL)
			goto skip;

		ret = ioctl(fd, EVIOCGKEY(sizeof(keybits)), keybits);
		if (ret == -1) {
			printf("Warning: EVIOCGKEY not supported, will have to track state\n");
			dev->kbits = calloc(KEY_CNT, sizeof(int));
			if (dev->kbits == NULL) {
				free(dev);
				goto skip;
			}
		}

		/* check for abs too */
		if (support & (1 << EV_ABS)) {
			struct input_absinfo ainfo;
			int dist;
			ret = ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits);
			if (ret == -1)
				goto no_abs;
			if (absbits[0] & (1 << ABS_X)) {
				ret = ioctl(fd, EVIOCGABS(ABS_X), &ainfo);
				if (ret == -1)
					goto no_abs;
				dist = ainfo.maximum - ainfo.minimum;
				dev->abs_lzone = ainfo.minimum + dist / 4;
				dev->abs_rzone = ainfo.maximum - dist / 4;
			}
			if (absbits[0] & (1 << ABS_Y)) {
				ret = ioctl(fd, EVIOCGABS(ABS_Y), &ainfo);
				if (ret == -1)
					goto no_abs;
				dist = ainfo.maximum - ainfo.minimum;
				dev->abs_tzone = ainfo.minimum + dist / 4;
				dev->abs_bzone = ainfo.maximum - dist / 4;
			}
		}

no_abs:
		dev->fd = fd;
		strcpy(name, in_evdev_prefix);
		ioctl(fd, EVIOCGNAME(sizeof(name)-6), name+6);
		printf("in_evdev: found \"%s\" with %d events (type %08x)\n",
			name+6, count, support);
		in_register(name, IN_DRVID_EVDEV, fd, dev, KEY_CNT, 0);
		continue;

skip:
		close(fd);
	}
}

static void in_evdev_free(void *drv_data)
{
	in_evdev_t *dev = drv_data;
	if (dev == NULL)
		return;
	close(dev->fd);
	free(dev);
}

static int in_evdev_get_bind_count(void)
{
	return KEY_CNT;
}

static void or_binds(const int *binds, int key, int *result)
{
	int t;
	for (t = 0; t < IN_BINDTYPE_COUNT; t++)
		result[t] |= binds[IN_BIND_OFFS(key, t)];
}

/* ORs result with binds of pressed buttons
 * XXX: should measure performance hit of this func, might need to optimize */
int in_evdev_update(void *drv_data, const int *binds, int *result)
{
	struct input_event ev[16];
	struct input_absinfo ainfo;
	int keybits_[KEY_CNT / sizeof(int)];
	int *keybits = keybits_;
	in_evdev_t *dev = drv_data;
	int rd, ret, u;

	if (dev->kbits == NULL) {
		ret = ioctl(dev->fd, EVIOCGKEY(sizeof(keybits_)), keybits_);
		if (ret == -1) {
			perror("in_evdev: ioctl failed");
			return -1;
		}
	}
	else {
		keybits = dev->kbits;
		while (1) {
			rd = read(dev->fd, ev, sizeof(ev));
			if (rd < (int)sizeof(ev[0])) {
				if (errno != EAGAIN)
					perror("in_evdev: read failed");
				break;
			}
			for (u = 0; u < rd / sizeof(ev[0]); u++) {
				if (ev[u].type != EV_KEY)
					continue;
				else if (ev[u].value == 1)
					KEYBITS_BIT_SET(ev[u].code);
				else if (ev[u].value == 0)
					KEYBITS_BIT_CLEAR(ev[u].code);
			}
		}
	}

	for (u = 0; u < KEY_CNT; u++) {
		if (KEYBITS_BIT(u))
			or_binds(binds, u, result);
	}

	/* map X and Y absolute to UDLR */
	if (dev->abs_lzone != 0) {
		ret = ioctl(dev->fd, EVIOCGABS(ABS_X), &ainfo);
		if (ret != -1) {
			if (ainfo.value < dev->abs_lzone) or_binds(binds, KEY_LEFT, result);
			if (ainfo.value > dev->abs_rzone) or_binds(binds, KEY_RIGHT, result);
		}
	}
	if (dev->abs_tzone != 0) {
		ret = ioctl(dev->fd, EVIOCGABS(ABS_Y), &ainfo);
		if (ret != -1) {
			if (ainfo.value < dev->abs_tzone) or_binds(binds, KEY_UP, result);
			if (ainfo.value > dev->abs_bzone) or_binds(binds, KEY_DOWN, result);
		}
	}

	return 0;
}

static void in_evdev_set_blocking(void *drv_data, int y)
{
	in_evdev_t *dev = drv_data;
	long flags;
	int ret;

	flags = (long)fcntl(dev->fd, F_GETFL);
	if ((int)flags == -1) {
		perror("in_evdev: F_GETFL fcntl failed");
		return;
	}

	if (flags & O_NONBLOCK) {
		/* flush the event queue */
		struct input_event ev;
		do {
			ret = read(dev->fd, &ev, sizeof(ev));
		}
		while (ret == sizeof(ev));
	}

	if (y)
		flags &= ~O_NONBLOCK;
	else
		flags |=  O_NONBLOCK;
	ret = fcntl(dev->fd, F_SETFL, flags);
	if (ret == -1)
		perror("in_evdev: F_SETFL fcntl failed");
}

static int in_evdev_update_keycode(void *data, int *is_down)
{
	in_evdev_t *dev = data;
	struct input_event ev;
	int rd;

	if (is_down != NULL)
		*is_down = 0;

	rd = read(dev->fd, &ev, sizeof(ev));
	if (rd < (int) sizeof(ev)) {
		if (errno != EAGAIN) {
			perror("in_evdev: error reading");
			sleep(1);
		}
		return -1;
	}

	if (ev.type == EV_KEY) {
		if (ev.value < 0 || ev.value > 1)
			return -1;
		if (is_down != NULL)
			*is_down = ev.value;
		return ev.code;
	}
	else if (ev.type == EV_ABS)
	{
		int down = 0;
		if (dev->abs_lzone != 0 && ev.code == ABS_X) {
			if (ev.value < dev->abs_lzone) {
				down = 1;
				dev->abs_lastx = KEY_LEFT;
			}
			else if (ev.value > dev->abs_rzone) {
				down = 1;
				dev->abs_lastx = KEY_RIGHT;
			}
			if (is_down != NULL)
				*is_down = down;
			return dev->abs_lastx;
		}
		if (dev->abs_tzone != 0 && ev.code == ABS_Y) {
			if (ev.value < dev->abs_tzone) {
				down = 1;
				dev->abs_lasty = KEY_UP;
			}
			else if (ev.value > dev->abs_bzone) {
				down = 1;
				dev->abs_lasty = KEY_DOWN;
			}
			if (is_down != NULL)
				*is_down = down;
			return dev->abs_lasty;
		}
	}

	return -1;
}

static const struct {
	short key;
	short pbtn;
} key_pbtn_map[] =
{
	{ KEY_UP,	PBTN_UP },
	{ KEY_DOWN,	PBTN_DOWN },
	{ KEY_LEFT,	PBTN_LEFT },
	{ KEY_RIGHT,	PBTN_RIGHT },
	{ KEY_ENTER,	PBTN_MOK },
	{ KEY_KP2,	PBTN_MOK },
	{ BTN_TRIGGER,	PBTN_MOK },
	{ KEY_ESC,	PBTN_MBACK },
	{ KEY_KP3,	PBTN_MBACK },
	{ BTN_THUMB,	PBTN_MBACK },
	{ KEY_A,	PBTN_MA2 },
	{ KEY_KP4,	PBTN_MA2 },
	{ KEY_S,	PBTN_MA3 },
	{ KEY_KP1,	PBTN_MA3 },
	{ KEY_BACKSLASH,  PBTN_MENU },
	{ KEY_LEFTCTRL,   PBTN_MENU },
	{ BTN_TL,	  PBTN_L },
	{ KEY_LEFTBRACE,  PBTN_L },
	{ BTN_TR,	  PBTN_R },
	{ KEY_RIGHTBRACE, PBTN_R },
};

#define KEY_PBTN_MAP_SIZE (sizeof(key_pbtn_map) / sizeof(key_pbtn_map[0]))

static int in_evdev_menu_translate(int keycode)
{
	int i;
	if (keycode < 0)
	{
		/* menu -> kc */
		keycode = -keycode;
		for (i = 0; i < KEY_PBTN_MAP_SIZE; i++)
			if (key_pbtn_map[i].pbtn == keycode)
				return key_pbtn_map[i].key;
	}
	else
	{
		for (i = 0; i < KEY_PBTN_MAP_SIZE; i++)
			if (key_pbtn_map[i].key == keycode)
				return key_pbtn_map[i].pbtn;
	}

	return 0;
}

static int in_evdev_get_key_code(const char *key_name)
{
	int i;

	for (i = 0; i < KEY_CNT; i++) {
		const char *k = in_evdev_keys[i];
		if (k != NULL && strcasecmp(k, key_name) == 0)
			return i;
	}

	return -1;
}

static const char *in_evdev_get_key_name(int keycode)
{
	const char *name = NULL;
	if (keycode >= 0 && keycode <= KEY_MAX)
		name = in_evdev_keys[keycode];
	if (name == NULL)
		name = "Unkn";
	
	return name;
}

static const struct {
	short code;
	char btype;
	char bit;
} in_evdev_def_binds[] =
{
	/* MXYZ SACB RLDU */
	{ KEY_UP,	IN_BINDTYPE_PLAYER12, 0 },
	{ KEY_DOWN,	IN_BINDTYPE_PLAYER12, 1 },
	{ KEY_LEFT,	IN_BINDTYPE_PLAYER12, 2 },
	{ KEY_RIGHT,	IN_BINDTYPE_PLAYER12, 3 },
	{ KEY_S,	IN_BINDTYPE_PLAYER12, 4 },	/* B */
	{ KEY_KP3,	IN_BINDTYPE_PLAYER12, 4 },
	{ KEY_D,	IN_BINDTYPE_PLAYER12, 5 },	/* C */
	{ KEY_KP2,	IN_BINDTYPE_PLAYER12, 5 },
	{ KEY_A,	IN_BINDTYPE_PLAYER12, 6 },	/* A */
	{ KEY_KP4,	IN_BINDTYPE_PLAYER12, 6 },
	{ KEY_ENTER,	IN_BINDTYPE_PLAYER12, 7 },
	{ KEY_LEFTALT,	IN_BINDTYPE_PLAYER12, 7 },
	{ BTN_TL,	IN_BINDTYPE_EMU, PEVB_STATE_LOAD },
	{ BTN_TR,	IN_BINDTYPE_EMU, PEVB_STATE_SAVE },
	{ KEY_LEFTCTRL, IN_BINDTYPE_EMU, PEVB_MENU },
};

#define DEF_BIND_COUNT (sizeof(in_evdev_def_binds) / sizeof(in_evdev_def_binds[0]))

static void in_evdev_get_def_binds(int *binds)
{
	int i;

	for (i = 0; i < DEF_BIND_COUNT; i++)
		binds[IN_BIND_OFFS(in_evdev_def_binds[i].code, in_evdev_def_binds[i].btype)] =
			1 << in_evdev_def_binds[i].bit;
}

/* remove binds of missing keys, count remaining ones */
static int in_evdev_clean_binds(void *drv_data, int *binds, int *def_binds)
{
	int keybits[KEY_CNT / sizeof(int)];
	in_evdev_t *dev = drv_data;
	int i, t, ret, offs, count = 0;

	ret = ioctl(dev->fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);
	if (ret == -1) {
		perror("in_evdev: ioctl failed");
		memset(keybits, 0xff, sizeof(keybits)); /* mark all as good */
	}

	if (dev->abs_lzone != 0) {
		KEYBITS_BIT_SET(KEY_LEFT);
		KEYBITS_BIT_SET(KEY_RIGHT);
	}
	if (dev->abs_tzone != 0) {
		KEYBITS_BIT_SET(KEY_UP);
		KEYBITS_BIT_SET(KEY_DOWN);
	}

	for (i = 0; i < KEY_CNT; i++) {
		for (t = 0; t < IN_BINDTYPE_COUNT; t++) {
			offs = IN_BIND_OFFS(i, t);
			if (!KEYBITS_BIT(i))
				binds[offs] = def_binds[offs] = 0;
			if (binds[offs])
				count++;
		}
	}

	return count;
}

void in_evdev_init(void *vdrv)
{
	in_drv_t *drv = vdrv;

	drv->prefix = in_evdev_prefix;
	drv->probe = in_evdev_probe;
	drv->free = in_evdev_free;
	drv->get_bind_count = in_evdev_get_bind_count;
	drv->get_def_binds = in_evdev_get_def_binds;
	drv->clean_binds = in_evdev_clean_binds;
	drv->set_blocking = in_evdev_set_blocking;
	drv->update_keycode = in_evdev_update_keycode;
	drv->menu_translate = in_evdev_menu_translate;
	drv->get_key_code = in_evdev_get_key_code;
	drv->get_key_name = in_evdev_get_key_name;
}

