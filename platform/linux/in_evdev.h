
#ifdef IN_EVDEV

int  in_evdev_probe(void);
void in_evdev_free(void *drv_data);
int  in_evdev_bind_count(void);
int  in_evdev_update(void *drv_data, int *binds);
void in_evdev_set_blocking(void *data, int y);
int  in_evdev_menu_translate(int keycode);
const char *in_evdev_get_key_name(int keycode);

#else

#define in_evdev_probe() -1
#define in_evdev_free(x)
#define in_evdev_bind_count() 0
#define in_evdev_update(x,y) 0
#define in_evdev_set_blocking(x,y)
#define in_evdev_menu_translate 0
#define in_evdev_get_key_name "Unkn"

#endif

int in_evdev_update_keycode(void **data, int count, int *which, int *is_down);

