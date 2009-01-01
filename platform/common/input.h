#define IN_MAX_DEVS 10

enum {
	IN_DRVID_UNKNOWN = 0,
	IN_DRVID_EVDEV,
	IN_DRVID_COUNT
};

typedef struct {
	const char *prefix;
	void (*probe)(void);
	void (*free)(void *drv_data);
	int  (*get_bind_count)(void);
	void (*get_def_binds)(int *binds);
	int  (*clean_binds)(void *drv_data, int *binds);
	void (*set_blocking)(void *data, int y);
	int  (*menu_translate)(int keycode);
	int  (*get_key_code)(const char *key_name);
	const char * (*get_key_name)(int keycode);
} in_drv_t;


/* to be called by drivers */
void in_register(const char *nname, int drv_id, void *drv_data);

void in_init(void);
void in_probe(void);
int  in_update(void);
void in_set_blocking(int is_blocking);
int  in_update_keycode(int *dev_id, int *is_down);
int  in_update_menu(void);
int  in_get_dev_bind_count(int dev_id);
void in_config_start(void);
int  in_config_parse_dev(const char *dev_name);
int  in_config_bind_key(int dev_id, const char *key, int mask);
void in_config_end(void);
int  in_bind_key(int dev_id, int keycode, int mask);
void in_debug_dump(void);

const int  *in_get_dev_binds(int dev_id);
const int  *in_get_dev_def_binds(int dev_id);
const char *in_get_dev_name(int dev_id);
const char *in_get_key_name(int dev_id, int keycode);
