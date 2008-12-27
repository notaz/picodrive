#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>

#include "../common/input.h"
#include "event.h"

#define BIT(x) (keybits[(x)/sizeof(keybits[0])/8] & \
	(1 << ((x) & (sizeof(keybits[0])*8-1))))

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

