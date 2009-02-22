/* stuff to be implemented by platform code */

#ifdef __cplusplus
extern "C" {
#endif

extern const char * const keyNames[];
void  emu_prepareDefaultConfig(void);
void  emu_platformDebugCat(char *str);
void  emu_forcedFrame(int opts);
void  emu_startSound(void);
void  emu_endSound(void);
void  emu_waitSound(void);

/* menu: enter (switch bpp, etc), begin/end drawing */
void plat_video_menu_enter(int is_rom_loaded);
void plat_video_menu_begin(void);
void plat_video_menu_end(void);

#ifdef __cplusplus
} // extern "C"
#endif

