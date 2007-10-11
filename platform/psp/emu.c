#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syslimits.h> // PATH_MAX

#include "../../Pico/PicoInt.h"

char romFileName[PATH_MAX];
unsigned char *PicoDraw2FB;  // temporary buffer for alt renderer ( (8+320)*(8+240+8) )
int engineState;


void emu_noticeMsgUpdated(void)
{
}

void emu_getMainDir(char *dst, int len)
{
}

void emu_stateCb(const char *str)
{
}

void emu_setDefaultConfig(void)
{
}

void emu_forcedFrame(void)
{
}

void emu_Init(void)
{
	// make dirs for saves, cfgs, etc.
	mkdir("mds", 0777);
	mkdir("srm", 0777);
	mkdir("brm", 0777);
	mkdir("cfg", 0777);

	PicoInit();
//	PicoMessage = emu_msg_cb;
//	PicoMCDopenTray = emu_msg_tray_open;
//	PicoMCDcloseTray = menu_loop_tray;
}

void emu_Deinit(void)
{
	// save SRAM
/*	if ((currentConfig.EmuOpt & 1) && SRam.changed) {
		emu_SaveLoadGame(0, 1);
		SRam.changed = 0;
	}

	if (!(currentConfig.EmuOpt & 0x20)) {
		FILE *f = fopen(PicoConfigFile, "r+b");
		if (!f) emu_WriteConfig(0);
		else {
			// if we already have config, reload it, except last ROM
			fseek(f, sizeof(currentConfig.lastRomFile), SEEK_SET);
			fread(&currentConfig.EmuOpt, 1, sizeof(currentConfig) - sizeof(currentConfig.lastRomFile), f);
			fseek(f, 0, SEEK_SET);
			fwrite(&currentConfig, 1, sizeof(currentConfig), f);
			fflush(f);
			fclose(f);
		}
	}
*/
	PicoExit();
}

void emu_ResetGame(void)
{
	PicoReset(0);
	//reset_timing = 1;
}

