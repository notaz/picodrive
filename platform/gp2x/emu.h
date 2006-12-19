// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.



// engine states
enum TPicoGameState {
	PGS_Paused = 1,
	PGS_Running,
	PGS_Quit,
	PGS_KeyConfig,
	PGS_ReloadRom,
	PGS_Menu,
};

typedef struct {
	char lastRomFile[512];
	int EmuOpt;		// LSb->MSb: use_sram, show_fps, enable_sound, gzip_saves,
					// squidgehack, save_cfg_on_exit, <unused>, 16_bit_mode
					// craigix_ram, confirm_save
	int PicoOpt;  // used for config saving only, see Pico.h
	int PsndRate; // ditto
	int PicoRegion; // ditto
	int Frameskip;
	int CPUclock;
	int KeyBinds[32];
	int volume;
	int gamma;
	int JoyBinds[4][32];
} currentConfig_t;

extern char romFileName[];
extern int engineState;
extern currentConfig_t currentConfig;


int  emu_ReloadRom(void);
void emu_Init(void);
void emu_Deinit(void);
int  emu_SaveLoadGame(int load, int sram);
void emu_Loop(void);
void emu_ResetGame(void);
int  emu_ReadConfig(int game);
int  emu_WriteConfig(int game);
