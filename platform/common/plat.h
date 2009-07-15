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

void emu_noticeMsgUpdated(void);
int  emu_getMainDir(char *dst, int len);
void menu_romload_prepare(const char *rom_name);
void menu_romload_end(void);

void plat_init(void);
void plat_finish(void);

/* menu: enter (switch bpp, etc), begin/end drawing */
void plat_video_menu_enter(int is_rom_loaded);
void plat_video_menu_begin(void);
void plat_video_menu_end(void);

int  plat_is_dir(const char *path);
int  plat_wait_event(int *fds_hnds, int count, int timeout_ms);
void plat_sleep_ms(int ms);

/* ms counter, to be used for time diff */
unsigned int  plat_get_ticks_ms(void);

const char   *plat_get_credits(void);

#ifdef __cplusplus
} // extern "C"
#endif

