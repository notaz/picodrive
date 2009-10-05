// (c) Copyright 2006-2009 notaz, All rights reserved.

typedef enum
{
	MB_NONE = 1,		/* no auto processing */
	MB_OPT_ONOFF,		/* ON/OFF setting */
	MB_OPT_RANGE,		/* [min-max] setting */
	MB_OPT_CUSTOM,		/* custom value */
	MB_OPT_CUSTONOFF,
	MB_OPT_CUSTRANGE,
} menu_behavior;

typedef enum
{
	MA_NONE = 1,
	MA_MAIN_RESUME_GAME,
	MA_MAIN_SAVE_STATE,
	MA_MAIN_LOAD_STATE,
	MA_MAIN_RESET_GAME,
	MA_MAIN_LOAD_ROM,
	MA_MAIN_OPTIONS,
	MA_MAIN_CONTROLS,
	MA_MAIN_CREDITS,
	MA_MAIN_PATCHES,
	MA_MAIN_EXIT,
	MA_OPT_RENDERER,
	MA_OPT_SCALING,
	MA_OPT_ACC_SPRITES,
	MA_OPT_SHOW_FPS,
	MA_OPT_FRAMESKIP,
	MA_OPT_ENABLE_SOUND,
	MA_OPT_SOUND_QUALITY,
	MA_OPT_ARM940_SOUND,
	MA_OPT_6BUTTON_PAD,
	MA_OPT_REGION,
	MA_OPT_SRAM_STATES,
	MA_OPT_CONFIRM_STATES,
	MA_OPT_SAVE_SLOT,
	MA_OPT_CPU_CLOCKS,
	MA_OPT_SCD_OPTS,
	MA_OPT_ADV_OPTS,
	MA_OPT_DISP_OPTS,	/* psp */
	MA_OPT_SAVECFG,
	MA_OPT_SAVECFG_GAME,
	MA_OPT_LOADCFG,
	MA_OPT_INTERLACED,	/* giz */
	MA_OPT_ROTATION,	/* uiq */
	MA_OPT_TEARING_FIX,	/* wiz */
	MA_OPT2_GAMMA,
	MA_OPT2_A_SN_GAMMA,
	MA_OPT2_DBLBUFF,	/* giz */
	MA_OPT2_VSYNC,
	MA_OPT2_ENABLE_Z80,
	MA_OPT2_ENABLE_YM2612,
	MA_OPT2_ENABLE_SN76496,
	MA_OPT2_GZIP_STATES,
	MA_OPT2_NO_LAST_ROM,
	MA_OPT2_RAMTIMINGS,	/* gp2x */
	MA_OPT2_SQUIDGEHACK,	/* gp2x */
	MA_OPT2_STATUS_LINE,	/* psp */
	MA_OPT2_NO_FRAME_LIMIT,	/* psp */
	MA_OPT2_SVP_DYNAREC,
	MA_OPT2_NO_SPRITE_LIM,
	MA_OPT2_NO_IDLE_LOOPS,
	MA_OPT2_DONE,
	MA_OPT3_SCALE,		/* psp (all OPT3) */
	MA_OPT3_HSCALE32,
	MA_OPT3_HSCALE40,
	MA_OPT3_PRES_NOSCALE,
	MA_OPT3_PRES_SCALE43,
	MA_OPT3_PRES_FULLSCR,
	MA_OPT3_FILTERING,
	MA_OPT3_VSYNC,
	MA_OPT3_GAMMAA,
	MA_OPT3_BLACKLVL,
	MA_OPT3_DONE,
	MA_CDOPT_TESTBIOS_USA,
	MA_CDOPT_TESTBIOS_EUR,
	MA_CDOPT_TESTBIOS_JAP,
	MA_CDOPT_LEDS,
	MA_CDOPT_CDDA,
	MA_CDOPT_PCM,
	MA_CDOPT_READAHEAD,
	MA_CDOPT_SAVERAM,
	MA_CDOPT_SCALEROT_CHIP,
	MA_CDOPT_BETTER_SYNC,
	MA_CDOPT_DONE,
	MA_32XOPT_ENABLE_32X,
	MA_32XOPT_PWM,
	MA_CTRL_PLAYER1,
	MA_CTRL_PLAYER2,
	MA_CTRL_EMU,
	MA_CTRL_TURBO_RATE,
	MA_CTRL_DEV_FIRST,
	MA_CTRL_DEV_NEXT,
	MA_CTRL_DONE,
} menu_id;

typedef struct
{
	const char *name;
	menu_behavior beh;
	menu_id id;
	void *var;		/* for on-off/range settings */
	int mask;		/* bit to toggle for on/off */
	signed short min;	/* for ranged integer settings, to be sign-extended */
	signed short max;
	int enabled:1;
	int need_to_save:1;
	int selectable:1;
	int (*handler)(menu_id id, int keys);
	const char * (*generate_name)(menu_id id, int *offs);
} menu_entry;

#define mee_handler_id(name, id, handler) \
	{ name, MB_NONE, id, NULL, 0, 0, 0, 1, 0, 1, handler, NULL }

#define mee_handler(name, handler) \
	mee_handler_id(name, MA_NONE, handler)

#define mee_handler_mkname_id(id, handler, name_func) \
	{ "", MB_NONE, id, NULL, 0, 0, 0, 1, 0, 1, handler, name_func }

#define mee_label(name) \
	{ name, MB_NONE, MA_NONE, NULL, 0, 0, 0, 1, 0, 0, NULL, NULL }

#define mee_label_mk(id, name_func) \
	{ "", MB_NONE, id, NULL, 0, 0, 0, 1, 0, 0, NULL, name_func }

#define mee_onoff(name, id, var, mask) \
	{ name, MB_OPT_ONOFF, id, &(var), mask, 0, 0, 1, 1, 1, NULL, NULL }

#define mee_range(name, id, var, min, max) \
	{ name, MB_OPT_RANGE, id, &(var), 0, min, max, 1, 1, 1, NULL, NULL }

#define mee_cust(name, id, handler, name_func) \
	{ name, MB_OPT_CUSTOM, id, NULL, 0, 0, 0, 1, 1, 1, handler, name_func }

#define mee_onoff_cust(name, id, var, mask, name_func) \
	{ name, MB_OPT_CUSTONOFF, id, &(var), mask, 0, 0, 1, 1, 1, NULL, name_func }

#define mee_range_cust(name, id, var, min, max, name_func) \
	{ name, MB_OPT_CUSTRANGE, id, &(var), 0, min, max, 1, 1, 1, NULL, name_func }

#define mee_end \
	{ NULL, 0, 0, NULL, 0, 0, 0, 0, 0, 0, NULL, NULL }

typedef struct
{
	char *name;
	int mask;
} me_bind_action;

extern me_bind_action me_ctrl_actions[15];
extern me_bind_action emuctrl_actions[];	// platform code

void menu_init(void);
void menu_plat_setup(int is_wiz);
void text_out16(int x, int y, const char *texto, ...);
void me_update_msg(const char *msg);

void menu_romload_prepare(const char *rom_name);
void menu_romload_end(void);

void menu_loop(void);
int  menu_loop_tray(void);

menu_entry *me_list_get_first(void);
menu_entry *me_list_get_next(void);

void menu_darken_bg(void *dst, int pixels, int darker);

