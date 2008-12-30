enum {
	IN_DRVID_EVDEV = 1,
};

/* to be called by drivers */
void in_register(const char *nname, int drv_id, void *drv_data);

void in_init(void);
void in_probe(void);
int  in_update(void);
void in_set_blocking(int is_blocking);
int  in_update_keycode(int *dev_id, int *is_down);
int  in_update_menu(void);
const char *in_get_key_name(int dev_id, int keycode);
