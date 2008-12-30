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
	switch (drv_id) {
	case IN_DRVID_EVDEV:
		count = in_evdev_bind_count();
		break;
	}
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
		switch (dev->drv_id) {
		case IN_DRVID_EVDEV:
			in_evdev_free(dev->drv_data);
			break;
		}
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
			switch (in_devices[i].drv_id) {
			case IN_DRVID_EVDEV:
				result |= in_evdev_update(in_devices[i].drv_data, in_devices[i].binds);
				break;
			}
		}
	}

	return result;
}

static void **in_collect_drvdata(int drv_id, int *count)
{
	static void *data[IN_MAX_DEVS];
	int i;

	for (*count = i = 0; i < in_dev_count; i++) {
		if (in_devices[i].drv_id == drv_id && in_devices[i].probed)
			data[(*count)++] = in_devices[i].drv_data;
	}

	return data;
}

void in_set_blocking(int is_blocking)
{
	int i;

	for (i = 0; i < in_dev_count; i++) {
		if (in_devices[i].probed) {
			switch (in_devices[i].drv_id) {
			case IN_DRVID_EVDEV:
				in_evdev_set_blocking(in_devices[i].drv_data, is_blocking);
				break;
			}
		}
	}
}

/* 
 * update with wait for a press, return keycode
 * only can use 1 drv here..
 */
int in_update_keycode(int *dev_id, int *is_down)
{
	int result = 0;
#ifdef IN_EVDEV
	void **data;
	int i, id = 0, count = 0;

	data = in_collect_drvdata(IN_DRVID_EVDEV, &count);
	if (count == 0) {
		/* don't deadlock, fail */
		printf("input: failed to find devices to read\n");
		exit(1);
	}

	result = in_evdev_update_keycode(data, count, &id, is_down);

	if (dev_id != NULL) {
		for (i = id; i < in_dev_count; i++) {
			if (in_devices[i].drv_data == data[id]) {
				*dev_id = i;
				break;
			}
		}
	}
#else
#error no menu read handlers
#endif

	return result;
}

/* 
 * same as above, only return bitfield of BTN_*
 */
int in_update_menu(void)
{
	static int keys_active = 0;
	int keys_old = keys_active;

	while (1)
	{
		int code, is_down = 0;
		code = in_update_keycode(NULL, &is_down);
#ifdef IN_EVDEV
		code = in_evdev_menu_translate(code);
#endif
		if (code == 0) continue;

		if (is_down)
			keys_active |=  code;
		else
			keys_active &= ~code;

		if (keys_old != keys_active)
			break;
	}

	return keys_active;
}

const char *in_get_key_name(int dev_id, int keycode)
{
	if (dev_id < 0 || dev_id >= IN_MAX_DEVS)
		return "Unkn0";
	switch (in_devices[dev_id].drv_id) {
	case IN_DRVID_EVDEV:
		return in_evdev_get_key_name(keycode);
	}

	return "Unkn1";
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

	in_set_blocking(1);

#if 1
	while (1) {
		int dev = 0, down;
		ret = in_update_keycode(&dev, &down);
		printf("#%i: %i %i (%s)\n", dev, down, ret, in_get_key_name(dev, ret));
	}
#else
	while (1) {
		ret = in_update_menu();
		printf("%08x\n", ret);
	}
#endif

	return 0;
}

