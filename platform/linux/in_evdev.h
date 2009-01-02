
int  in_evdev_update(void *drv_data, int *binds);
int  in_evdev_update_keycode(void **data, int count, int *which, int *is_down, int timeout_ms);
void in_evdev_init(void *vdrv);

