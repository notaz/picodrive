#ifdef __cplusplus
extern "C" {
#endif

/* stuff to be implemented by platform code */
extern char cpu_clk_name[];
/* TODO rename all these */
extern const char * const keyNames[]; // TODO rm
void  emu_prepareDefaultConfig(void);
void  emu_platformDebugCat(char *str);
void  emu_forcedFrame(int opts);
void  emu_startSound(void);
void  emu_endSound(void);
void  emu_waitSound(void);
void  emu_ResetGame(void); // TODO mv rm?
void  emu_Init(void);
void  emu_Deinit(void);
void  emu_Loop(void);

int  emu_getMainDir(char *dst, int len);
void menu_romload_prepare(const char *rom_name);
void menu_romload_end(void);

void plat_early_init(void);
void plat_init(void);
void plat_finish(void);

/* to be used while emulation is starting or running */
void plat_status_msg(const char *format, ...);

/* used before things blocking for a while (these funcs redraw on return) */
void plat_status_msg_busy_first(const char *msg);
void plat_status_msg_busy_next(const char *msg);

/* menu: enter (switch bpp, etc), begin/end drawing */
void plat_video_menu_enter(int is_rom_loaded);
void plat_video_menu_begin(void);
void plat_video_menu_end(void);

void plat_video_toggle_renderer(void);
void plat_validate_config(void);
void plat_update_volume(int has_changed, int is_up);

int  plat_is_dir(const char *path);
int  plat_wait_event(int *fds_hnds, int count, int timeout_ms);
void plat_sleep_ms(int ms);

/* ms counter, to be used for time diff */
unsigned int  plat_get_ticks_ms(void);

const char   *plat_get_credits(void);

#ifdef __cplusplus
} // extern "C"
#endif

