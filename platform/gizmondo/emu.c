#include <windows.h>

#include "emu.h"

char romFileName[MAX_PATH];
int engineState;
currentConfig_t currentConfig;

unsigned char *rom_data = NULL;
unsigned char *PicoDraw2FB = NULL;  // temporary buffer for alt renderer
int state_slot = 0;
int reset_timing = 0;
int config_slot = 0, config_slot_current = 0;




int  emu_ReloadRom(void){}
void emu_Init(void){}
void emu_Deinit(void){}
int  emu_SaveLoadGame(int load, int sram){}
void emu_Loop(void){}
void emu_ResetGame(void){}
int  emu_ReadConfig(int game, int no_defaults){}
int  emu_WriteConfig(int game){}
char *emu_GetSaveFName(int load, int is_sram, int slot){}
int  emu_check_save_file(int slot){}
void emu_set_save_cbs(int gz){}
void emu_forced_frame(void){}
int  emu_cd_check(int *pregion){}
int  find_bios(int region, char **bios_file){}
void scaling_update(void){}

