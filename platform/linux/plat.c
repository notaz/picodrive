#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "../common/plat.h"


int plat_is_dir(const char *path)
{
	DIR *dir;
	if ((dir = opendir(path))) {
		closedir(dir);
		return 1;
	}
	return 0;
}

int plat_get_root_dir(char *dst, int len)
{
	extern char **g_argv;
	int j;

	strncpy(dst, g_argv[0], len);
	len -= 32; // reserve
	if (len < 0) len = 0;
	dst[len] = 0;
	for (j = strlen(dst); j > 0; j--)
		if (dst[j] == '/') { dst[j+1] = 0; break; }

	return j + 1;
}

#ifdef __GP2X__
/* Wiz has a borked gettimeofday().. */
#define plat_get_ticks_ms plat_get_ticks_ms_good
#define plat_get_ticks_us plat_get_ticks_us_good
#endif

unsigned int plat_get_ticks_ms(void)
{
	struct timeval tv;
	unsigned int ret;

	gettimeofday(&tv, NULL);

	ret = (unsigned)tv.tv_sec * 1000;
	/* approximate /= 1000 */
	ret += ((unsigned)tv.tv_usec * 4195) >> 22;

	return ret;
}

unsigned int plat_get_ticks_us(void)
{
	struct timeval tv;
	unsigned int ret;

	gettimeofday(&tv, NULL);

	ret = (unsigned)tv.tv_sec * 1000000;
	ret += (unsigned)tv.tv_usec;

	return ret;
}

void plat_sleep_ms(int ms)
{
	usleep(ms * 1000);
}

int plat_wait_event(int *fds_hnds, int count, int timeout_ms)
{
	struct timeval tv, *timeout = NULL;
	int i, ret, fdmax = -1;
	fd_set fdset;

	if (timeout_ms >= 0) {
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;
		timeout = &tv;
	}

	FD_ZERO(&fdset);
	for (i = 0; i < count; i++) {
		if (fds_hnds[i] > fdmax) fdmax = fds_hnds[i];
		FD_SET(fds_hnds[i], &fdset);
	}

	ret = select(fdmax + 1, &fdset, NULL, NULL, timeout);
	if (ret == -1)
	{
		perror("plat_wait_event: select failed");
		sleep(1);
		return -1;
	}

	if (ret == 0)
		return -1; /* timeout */

	ret = -1;
	for (i = 0; i < count; i++)
		if (FD_ISSET(fds_hnds[i], &fdset))
			ret = fds_hnds[i];

	return ret;
}

