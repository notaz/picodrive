#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <errno.h>

#include "event.h"

#define NUM_DEVS	8
#define NUM_KEYS_DOWN	16

#define BIT(x) (keybits[(x)/sizeof(keybits[0])/8] & \
	(1 << ((x) & (sizeof(keybits[0])*8-1))))

static int event_fds[NUM_DEVS];
static int event_fd_count = 0;

int in_event_init(void)
{
	int i;

	in_event_exit();

	for (i = 0; event_fd_count < NUM_DEVS; i++)
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
			printf("in_event: ioctl failed on %s\n", name);
			goto skip;
		}

		if (!(support & (1 << EV_KEY)))
			goto skip;

		ret = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits);
		if (ret == -1) {
			printf("in_event: ioctl failed on %s\n", name);
			goto skip;
		}

		printf("%s: %08x\n", name, support);

		/* check for interesting keys */
		for (u = 0; u < KEY_MAX; u++) {
			if (BIT(u) && u != KEY_POWER)
				count++;
		}

		if (count == 0)
			goto skip;

		ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		printf("event: %d: using \"%s\" with %d events\n",
			event_fd_count, name, count);
		event_fds[event_fd_count++] = fd;
		continue;

skip:
		close(fd);
	}

	printf("event: %d devices found.\n", event_fd_count);
	return 0;
}

void in_event_exit(void)
{
	for (; event_fd_count > 0; event_fd_count--)
		close(event_fds[event_fd_count - 1]);
}

int in_event_update(int binds[512])
{
	struct input_event ev[16];
	int d, rd, ret;
	int result = 0;

	for (d = 0; d < event_fd_count; d++)
	{
		int keybits[KEY_MAX/sizeof(int)];
		int fd = event_fds[d];
		int u, changed = 0;

		while (1) {
			rd = read(fd, ev, sizeof(ev));
			if (rd < (int)sizeof(ev[0])) {
				if (errno != EAGAIN)
					perror("event: read failed");
				break;
			}

			changed = 1;
		}

		if (!changed)
			continue;

		ret = ioctl(fd, EVIOCGKEY(sizeof(keybits)), keybits);
		if (ret == -1) {
			printf("in_event: ioctl failed on %d\n", d);
			continue;
		}

		for (u = 0; u < KEY_MAX; u++) {
			if (BIT(u)) {
				printf(" %d", u);
				result |= binds[u];
			}
		}
		printf("\n");
	}

	return result;
}

int main()
{
	in_event_init();

	while (1) {
		int b[512];
		in_event_update(b);
		sleep(1);
	}

	return 0;
}

