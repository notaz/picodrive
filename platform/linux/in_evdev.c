#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>

#include "../common/common.h"
#include "../common/input.h"
#include "in_evdev.h"

#define BIT(x) (keybits[(x)/sizeof(keybits[0])/8] & \
	(1 << ((x) & (sizeof(keybits[0])*8-1))))

static const char *in_evdev_keys[KEY_MAX + 1] = {
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


int in_evdev_probe(void)
{
	int i;

	for (i = 0;; i++)
	{
		int u, ret, fd, keybits[KEY_MAX/sizeof(int)];
		int support = 0, count = 0;
		char name[64];

		snprintf(name, sizeof(name), "/dev/input/event%d", i);
		fd = open(name, O_RDONLY|O_NONBLOCK);
		if (fd == -1)
			break;

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
		for (u = 0; u < KEY_MAX; u++) {
			if (BIT(u) && u != KEY_POWER && u != KEY_SLEEP)
				count++;
		}

		if (count == 0)
			goto skip;

		strcpy(name, "evdev:");
		ioctl(fd, EVIOCGNAME(sizeof(name)-6), name+6);
		printf("in_evdev: found \"%s\" with %d events (type %08x)\n",
			name+6, count, support);
		in_register(name, IN_DRVID_EVDEV, (void *)fd);
		continue;

skip:
		close(fd);
	}

	return 0;
}

void in_evdev_free(void *drv_data)
{
	close((int)drv_data);
}

int in_evdev_bind_count(void)
{
	return 512;
}

int in_evdev_update(void *drv_data, int *binds)
{
	struct input_event ev[16];
	int keybits[KEY_MAX/sizeof(int)];
	int fd = (int)drv_data;
	int result = 0, changed = 0;
	int rd, ret, u;

	while (1) {
		rd = read(fd, ev, sizeof(ev));
		if (rd < (int)sizeof(ev[0])) {
			if (errno != EAGAIN)
				perror("in_evdev: read failed");
			break;
		}

		changed = 1;
	}

/*
	if (!changed)
		return 0;
*/
	ret = ioctl(fd, EVIOCGKEY(sizeof(keybits)), keybits);
	if (ret == -1) {
		perror("in_evdev: ioctl failed");
		return 0;
	}

	printf("#%d: ", fd);
	for (u = 0; u < KEY_MAX; u++) {
		if (BIT(u)) {
			printf(" %d", u);
			result |= binds[u];
		}
	}
	printf("\n");

	return result;
}

void in_evdev_set_blocking(void *data, int y)
{
	long flags;
	int ret;

	flags = (long)fcntl((int)data, F_GETFL);
	if ((int)flags == -1) {
		perror("in_evdev: F_GETFL fcntl failed");
		return;
	}
	if (y)
		flags &= ~O_NONBLOCK;
	else
		flags |=  O_NONBLOCK;
	ret = fcntl((int)data, F_SETFL, flags);
	if (ret == -1)
		perror("in_evdev: F_SETFL fcntl failed");
}

int in_evdev_update_keycode(void **data, int dcount, int *which, int *is_down)
{
	const int *fds = (const int *)data;
	int i, fdmax = -1;

	for (i = 0; i < dcount; i++)
		if (fds[i] > fdmax) fdmax = fds[i];

	while (1)
	{
		struct input_event ev[64];
		int ret, fd = -1, rd;
		fd_set fdset;

		FD_ZERO(&fdset);
		for (i = 0; i < dcount; i++)
			FD_SET(fds[i], &fdset);

		ret = select(fdmax + 1, &fdset, NULL, NULL, NULL);
		if (ret == -1)
		{
			perror("in_evdev: select failed");
			sleep(1);
			return 0;
		}

		for (i = 0; i < dcount; i++)
			if (FD_ISSET(fds[i], &fdset))
				*which = i, fd = fds[i];

		rd = read(fd, ev, sizeof(ev[0]) * 64);
		if (rd < (int) sizeof(ev[0])) {
			perror("in_evdev: error reading");
			sleep(1);
			return 0;
		}

		for (i = 0; i < rd / sizeof(ev[0]); i++)
		{
			if (ev[i].type != EV_KEY || ev[i].value < 0 || ev[i].value > 1)
				continue;

			if (is_down)
				*is_down = ev[i].value;
			return ev[i].code;
		}
	}
}

int in_evdev_menu_translate(int keycode)
{
	switch (keycode) {
		/* keyboards */
		case KEY_UP:	return PBTN_UP;
		case KEY_DOWN:	return PBTN_DOWN;
		case KEY_LEFT:	return PBTN_LEFT;
		case KEY_RIGHT:	return PBTN_RIGHT;
		case KEY_ENTER:	return PBTN_EAST;
		case KEY_ESC:	return PBTN_SOUTH;
		default:	return 0;
	}
}

const char *in_evdev_get_key_name(int keycode)
{
	const char *name = NULL;
	if (keycode >= 0 && keycode < KEY_MAX)
		name = in_evdev_keys[keycode];
	if (name == NULL)
		name = "Unkn";
	
	return name;
}

