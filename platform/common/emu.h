// (c) Copyright 2006-2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include "port_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EOPT_USE_SRAM     (1<<0)
#define EOPT_SHOW_FPS     (1<<1)
#define EOPT_EN_SOUND     (1<<2)
#define EOPT_GZIP_SAVES   (1<<3)

typedef struct _currentConfig_t {
	// char lastRomFile[512];
	int EmuOpt;		// LSb->MSb: use_sram, show_fps, enable_sound, gzip_saves,
					// squidgehack, no_save_cfg_on_exit, <unused>, 16_bit_mode
					// craigix_ram, confirm_save, show_cd_leds, confirm_load
					// A_SNs_gamma, perfect_vsync, giz_scanlines, giz_dblbuff
					// vsync_mode, show_clock, no_frame_limitter
	int s_PicoOpt;		// for old cfg files only
	int s_PsndRate;
	int s_PicoRegion;
	int s_PicoAutoRgnOrder;
	int s_PicoCDBuffers;
	int Frameskip;
	int CPUclock;
	int KeyBinds[PLAT_MAX_KEYS];
	int volume;
	int gamma;
#if PLAT_HAVE_JOY
	int JoyBinds[4][32];
#endif
	int scaling;  // gp2x: 0=center, 1=hscale, 2=hvscale, 3=hsoftscale; psp: bilinear filtering
	int rotation; // for UIQ
	float scale; // psp: screen scale
	float hscale32, hscale40; // psp: horizontal scale
	int gamma2;  // psp: black level
	int turbo_rate;
} currentConfig_t;

extern currentConfig_t currentConfig, defaultConfig;
extern char *PicoConfigFile;
extern int rom_loaded;
extern char noticeMsg[64];
extern int state_slot;
extern int config_slot, config_slot_current;
extern unsigned char *movie_data;
extern char loadedRomFName[512];		// last loaded ROM filename
extern int kb_combo_keys, kb_combo_acts;	// keys and actions which need button combos
extern int pico_inp_mode;


int   emu_ReloadRom(char *rom_fname);
int   emu_SaveLoadGame(int load, int sram);
int   emu_ReadConfig(int game, int no_defaults);
int   emu_WriteConfig(int game);
char *emu_GetSaveFName(int load, int is_sram, int slot);
int   emu_checkSaveFile(int slot);
void  emu_setSaveStateCbs(int gz);
void  emu_updateMovie(void);
int   emu_cdCheck(int *pregion, char *fname_in);
int   emu_findBios(int region, char **bios_file);
void  emu_textOut8 (int x, int y, const char *text);
void  emu_textOut16(int x, int y, const char *text);
char *emu_makeRomId(void);
void  emu_getGameName(char *str150);
void  emu_findKeyBindCombos(void);
void  emu_forcedFrame(int opts);
void  emu_changeFastForward(int set_on);
void  emu_RunEventsPico(unsigned int events);
void  emu_DoTurbo(int *pad, int acts);
void  emu_packConfig(void);
void  emu_unpackConfig(void);
void  emu_shutdownMCD(void);

extern const char * const keyNames[];
void  emu_prepareDefaultConfig(void);
void  emu_platformDebugCat(char *str);

#ifdef __cplusplus
} // extern "C"
#endif

