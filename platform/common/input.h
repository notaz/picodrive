#define IN_MAX_DEVS 10

/* unified menu keys */
#define PBTN_UP    (1 <<  0)
#define PBTN_DOWN  (1 <<  1)
#define PBTN_LEFT  (1 <<  2)
#define PBTN_RIGHT (1 <<  3)

#define PBTN_MOK   (1 <<  4)
#define PBTN_MBACK (1 <<  5)
#define PBTN_MA2   (1 <<  6)	/* menu action 2 */
#define PBTN_MA3   (1 <<  7)

#define PBTN_L     (1 <<  8)
#define PBTN_R     (1 <<  9)

#define PBTN_MENU  (1 << 10)

/* ui events */
#define PEVB_VOL_DOWN   30
#define PEVB_VOL_UP     29
#define PEVB_STATE_LOAD 28
#define PEVB_STATE_SAVE 27
#define PEVB_SWITCH_RND 26
#define PEVB_SSLOT_PREV 25
#define PEVB_SSLOT_NEXT 24
#define PEVB_MENU       23
#define PEVB_FF         22
#define PEVB_PICO_PNEXT 21
#define PEVB_PICO_PPREV 20
#define PEVB_PICO_SWINP 19

#define PEV_VOL_DOWN    (1 << PEVB_VOL_DOWN)
#define PEV_VOL_UP      (1 << PEVB_VOL_UP)
#define PEV_STATE_LOAD  (1 << PEVB_STATE_LOAD)
#define PEV_STATE_SAVE  (1 << PEVB_STATE_SAVE)
#define PEV_SWITCH_RND  (1 << PEVB_SWITCH_RND)
#define PEV_SSLOT_PREV  (1 << PEVB_SSLOT_PREV)
#define PEV_SSLOT_NEXT  (1 << PEVB_SSLOT_NEXT)
#define PEV_MENU        (1 << PEVB_MENU)
#define PEV_FF          (1 << PEVB_FF)
#define PEV_PICO_PNEXT  (1 << PEVB_PICO_PNEXT)
#define PEV_PICO_PPREV  (1 << PEVB_PICO_PPREV)
#define PEV_PICO_SWINP  (1 << PEVB_PICO_SWINP)

#define PEV_MASK 0x7ff80000


enum {
	IN_DRVID_UNKNOWN = 0,
	IN_DRVID_GP2X,
	IN_DRVID_EVDEV,
	IN_DRVID_COUNT,
};

enum {
	IN_INFO_BIND_COUNT = 0,
	IN_INFO_DOES_COMBOS,
};

typedef struct {
	const char *prefix;
	void (*probe)(void);
	void (*free)(void *drv_data);
	int  (*get_bind_count)(void);
	void (*get_def_binds)(int *binds);
	int  (*clean_binds)(void *drv_data, int *binds);
	void (*set_blocking)(void *data, int y);
	int  (*update_keycode)(void *drv_data, int *is_down);
	int  (*menu_translate)(int keycode);
	int  (*get_key_code)(const char *key_name);
	const char * (*get_key_name)(int keycode);
} in_drv_t;


/* to be called by drivers */
void in_register(const char *nname, int drv_id, int fd_hnd, void *drv_data, int combos);
void in_combos_find(int *binds, int last_key, int *combo_keys, int *combo_acts);
int  in_combos_do(int keys, int *binds, int last_key, int combo_keys, int combo_acts);

void in_init(void);
void in_probe(void);
int  in_update(void);
void in_set_blocking(int is_blocking);
int  in_update_keycode(int *dev_id, int *is_down, int timeout_ms);
int  in_menu_wait_any(int timeout_ms);
int  in_menu_wait(int interesting, int autorep_delay_ms);
int  in_get_dev_info(int dev_id, int what);
void in_config_start(void);
int  in_config_parse_dev(const char *dev_name);
int  in_config_bind_key(int dev_id, const char *key, int mask);
void in_config_end(void);
int  in_bind_key(int dev_id, int keycode, int mask, int force_unbind);
void in_debug_dump(void);

const int  *in_get_dev_binds(int dev_id);
const int  *in_get_dev_def_binds(int dev_id);
const char *in_get_dev_name(int dev_id, int must_be_active, int skip_pfix);
const char *in_get_key_name(int dev_id, int keycode);
