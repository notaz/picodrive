#define IN_DRVID_EVDEV 1

/* to be called by drivers */
void in_register(const char *nname, int drv_id, void *drv_data);

void in_init(void);
void in_probe(void);
int  in_update(void);
