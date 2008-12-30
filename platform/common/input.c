#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input.h"
#include "../linux/in_evdev.h"

typedef struct
{
	int drv_id;
	void *drv_data;
	int *binds;
	char *name;
	int probed:1;
	int ignore:1;
} in_dev_t;

#define IN_MAX_DEVS 10

static in_dev_t in_devices[IN_MAX_DEVS];
static int in_dev_count = 0;

static int in_bind_count(int drv_id)
{
	int count = 0;
	if (drv_id == IN_DRVID_EVDEV)
		count = in_evdev_bind_count();
	if (count <= 0)
		printf("input: failed to get bind count for drv %d\n", drv_id);

	return count;
}

static int *in_alloc_binds(int drv_id)
{
	int count, *ret;

	count = in_bind_count(drv_id);
	if (count <= 0) {
		printf("input: failed to get bind count for drv %d\n", drv_id);
		return NULL;
	}

	ret = malloc(count * sizeof(*ret));
	return ret;
}

static void in_free(in_dev_t *dev)
{
	if (dev->probed) {
		if (dev->drv_id == IN_DRVID_EVDEV)
			in_evdev_free(dev->drv_data);
	}
	dev->probed = 0;
	dev->drv_data = NULL;
	free(dev->name);
	dev->name = NULL;
	free(dev->binds);
	dev->binds = NULL;
}

/* to be called by drivers */
void in_register(const char *nname, int drv_id, void *drv_data)
{
	int i, dupe_count = 0, *binds;
	char name[256], *name_end, *tmp;

	strncpy(name, nname, sizeof(name));
	name[sizeof(name)-12] = 0;
	name_end = name + strlen(name);

	for (i = 0; i < in_dev_count; i++)
	{
		if (in_devices[i].name == NULL)
			continue;
		if (strcmp(in_devices[i].name, name) == 0)
		{
			if (in_devices[i].probed) {
				dupe_count++;
				sprintf(name_end, " [%d]", dupe_count);
				continue;
			}
			goto update;
		}
	}

	if (i >= IN_MAX_DEVS)
	{
		/* try to find unused device */
		for (i = 0; i < IN_MAX_DEVS; i++)
			if (!in_devices[i].probed) break;
		if (i >= IN_MAX_DEVS) {
			printf("input: too many devices, can't add %s\n", name);
			return;
		}
		in_free(&in_devices[i]);
	}

	tmp = strdup(name);
	if (tmp == NULL)
		return;

	binds = in_alloc_binds(drv_id);
	if (binds == NULL) {
		free(tmp);
		return;
	}

	in_devices[i].name = tmp;
	in_devices[i].binds = binds;
	if (i + 1 > in_dev_count)
		in_dev_count = i + 1;

	printf("input: new device #%d \"%s\"\n", i, name);
update:
	in_devices[i].probed = 1;
	in_devices[i].drv_id = drv_id;
	in_devices[i].drv_data = drv_data;
}

void in_probe(void)
{
	int i;
	for (i = 0; i < in_dev_count; i++)
		in_devices[i].probed = 0;

	in_evdev_probe();

	/* get rid of devs without binds and probes */
	for (i = 0; i < in_dev_count; i++) {
		if (!in_devices[i].probed && in_devices[i].binds == NULL) {
			in_dev_count--;
			if (i < in_dev_count) {
				free(in_devices[i].name);
				memmove(&in_devices[i], &in_devices[i+1],
					(in_dev_count - i) * sizeof(in_devices[0]));
			}
		}
	}
}

void in_clear_binds(const char *devname)
{
/*	int count;

	count = in_bind_count(drv_id);
	if (count <= 0) {
		printf("input: failed to get bind count for drv %d\n", dev->drv_id);
		return NULL;
	}
*/
}

int in_update(void)
{
	int i, result = 0;

	for (i = 0; i < in_dev_count; i++) {
		if (in_devices[i].probed && in_devices[i].binds != NULL) {
			if (in_devices[i].drv_id == IN_DRVID_EVDEV)
				result |= in_evdev_update(in_devices[i].drv_data, in_devices[i].binds);
		}
	}

	return result;
}

/* 
 * update with wait for a press, return bitfield of BTN_*
 * only can use 1 drv here..
 */
int in_update_menu(void)
{
	int result = 0;
#ifdef IN_EVDEV
	void *data[IN_MAX_DEVS];
	int i, count = 0;

	for (i = 0; i < in_dev_count; i++) {
		if (in_devices[i].probed)
			data[count++] = in_devices[i].drv_data;
	}

	if (count == 0) {
		/* don't deadlock, fail */
		printf("input: failed to find devices to read\n");
		exit(1);
	}

	result = in_evdev_update_menu(data, count);
#else
#error no menu read handlers
#endif

	return result;
}

void in_init(void)
{
	memset(in_devices, 0, sizeof(in_devices));
	in_dev_count = 0;
}

int main(void)
{
	int ret;

	in_init();
	in_probe();

	while (1) {
		ret = in_update_menu();
		printf("%08x\n", ret);
		sleep(1);
	}

	return 0;
}

