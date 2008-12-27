int  in_evdev_probe(void);
void in_evdev_free(void *drv_data);
int  in_evdev_bind_count(void);
int  in_evdev_update(void *drv_data, int *binds);
