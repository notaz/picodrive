// (c) Copyright 2006-2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

typedef struct {
	// char lastRomFile[512];
	int EmuOpt;		// LSb->MSb: use_sram, show_fps, enable_sound, gzip_saves,
					// squidgehack, no_save_cfg_on_exit, <unused>, 16_bit_mode
					// craigix_ram, confirm_save, show_cd_leds, confirm_load
					// A_SNs_gamma, perfect_vsync, giz_scanlines, giz_dblbuff
					// vsync_mode, show_clock, no_frame_limitter
	int s_PicoOpt;		// for old cfg files only
	int s_PsndRate;
	int s_PicoRegion;
	int Frameskip;
	int CPUclock;
	int KeyBinds[32];
	int volume;
	int gamma;
	int JoyBinds[4][32];
	int s_PicoAutoRgnOrder;
	int s_PicoCDBuffers;
	int scaling; // gp2x: 0=center, 1=hscale, 2=hvscale, 3=hsoftscale; psp: bilinear filtering
	float scale; // psp: screen scale
	float hscale32, hscale40; // psp: horizontal scale
} currentConfig_t;

extern currentConfig_t currentConfig, defaultConfig;
extern char *PicoConfigFile;
extern int rom_loaded;
extern char noticeMsg[64];
extern int state_slot;
extern int config_slot, config_slot_current;
extern unsigned char *movie_data;
extern char lastRomFile[512];


int   emu_ReloadRom(void);
int   emu_SaveLoadGame(int load, int sram);
int   emu_ReadConfig(int game, int no_defaults);
int   emu_WriteConfig(int game);
char *emu_GetSaveFName(int load, int is_sram, int slot);
int   emu_checkSaveFile(int slot);
void  emu_setSaveStateCbs(int gz);
void  emu_updateMovie(void);
int   emu_cdCheck(int *pregion);
int   emu_findBios(int region, char **bios_file);
void  emu_textOut8 (int x, int y, const char *text);
void  emu_textOut16(int x, int y, const char *text);
char *emu_makeRomId(void);

extern const char *keyNames[];
void  emu_prepareDefaultConfig(void);
