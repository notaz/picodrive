// (c) Copyright 2006,2007 notaz, All rights reserved.


void menu_init(void);
void text_out16(int x, int y, const char *texto, ...);
void smalltext_out16(int x, int y, const char *texto, int color);
void smalltext_out16_lim(int x, int y, const char *texto, int color, int max);
void menu_draw_selection(int x, int y, int w);

extern char menuErrorMsg[64];


typedef enum
{
	MB_NONE = 1,		/* no auto processing */
	MB_ONOFF,		/* ON/OFF setting */
	MB_RANGE,		/* [min-max] setting */
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
	MA_OPT_ACC_TIMING,
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
} menu_id;

typedef struct
{
	char *name;
	menu_behavior beh;
	menu_id id;
	void *var;		/* for on-off settings */
	int mask;
	signed char min;	/* for ranged integer settings, to be sign-extended */
	signed char max;
	char enabled;
} menu_entry;


typedef void (me_draw_custom_f)(const menu_entry *entry, int x, int y, void *param);

int     me_id2offset(const menu_entry *entries, int count, menu_id id);
void    me_enable(menu_entry *entries, int count, menu_id id, int enable);
int     me_count_enabled(const menu_entry *entries, int count);
menu_id me_index2id(const menu_entry *entries, int count, int index);
void    me_draw(const menu_entry *entries, int count, int x, int y, me_draw_custom_f *cust_draw, void *param);
int     me_process(menu_entry *entries, int count, menu_id id, int is_next);


