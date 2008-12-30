
#ifdef IN_EVDEV

int  in_evdev_probe(void);
void in_evdev_free(void *drv_data);
int  in_evdev_bind_count(void);
int  in_evdev_update(void *drv_data, int *binds);

#else

#define in_evdev_probe() -1
#define in_evdev_free(x)
#define in_evdev_bind_count() 0
#define in_evdev_update(x,y) 0

#endif

int in_evdev_update_menu(void **data, int count);

