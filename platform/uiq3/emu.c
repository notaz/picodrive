#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../common/emu.h"
#include "../common/config.h"
#include "../common/menu.h"
#include "Pico/PicoInt.h"

const char * const keyNames[] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

int emu_getMainDir(char *dst, int len)
{
	strcpy(dst, "D:\\other\\PicoDrive\\");
	return strlen(dst);
}

void emu_Init(void)
{
	int ret;

	// make dirs for saves, cfgs, etc.
	ret = mkdir("D:\\other\\PicoDrive", 0777);
	if (ret == 0)
	{
		mkdir("D:\\other\\PicoDrive\\mds", 0777);
		mkdir("D:\\other\\PicoDrive\\srm", 0777);
		mkdir("D:\\other\\PicoDrive\\brm", 0777);
	}

	emu_prepareDefaultConfig();
	config_readlrom("D:\\other\\PicoDrive\\config.cfg");
	emu_ReadConfig(0, 0);
	//PicoInit();
}

void emu_Deinit(void)
{
	// saves volume and last ROM
	emu_WriteConfig(0);
	//PicoExit();
}

void menu_romload_prepare(const char *rom_name)
{
}

void menu_romload_end(void)
{
}

void emu_prepareDefaultConfig(void)
{
	memset(&defaultConfig, 0, sizeof(defaultConfig));
	defaultConfig.EmuOpt    = 0x1d | 0x680; // | confirm_save, cd_leds, 16bit rend
	defaultConfig.s_PicoOpt = 0x0f | POPT_EN_MCD_PCM|POPT_EN_MCD_CDDA|POPT_EN_SVP_DRC|POPT_ACC_SPRITES;
	defaultConfig.s_PsndRate = 22050;
	defaultConfig.s_PicoRegion = 0; // auto
	defaultConfig.s_PicoAutoRgnOrder = 0x184; // US, EU, JP
	defaultConfig.s_PicoCDBuffers = 0;
	defaultConfig.Frameskip = -1; // auto
	defaultConfig.volume = 80;
	defaultConfig.scaling = 0;
	defaultConfig.KeyBinds[0xd5] = 1<<26; // back
}

/* used by config engine only, not actual menus */
menu_entry opt_entries[] =
{
	{ NULL,                        MB_NONE,  MA_OPT_RENDERER,      NULL, 0, 0, 0, 1, 1 },
	{ "Scaling",                   MB_RANGE, MA_OPT_SCALING,       &currentConfig.scaling,     0, 0, 2, 1, 1 },
	{ "Rotation",                  MB_RANGE, MA_OPT_ROTATION,      &currentConfig.rotation,    0, 0, 3, 1, 1 },
	{ "Accurate sprites",          MB_ONOFF, MA_OPT_ACC_SPRITES,   &PicoOpt, 0x080, 0, 0, 0, 1 },
	{ "Show FPS",                  MB_ONOFF, MA_OPT_SHOW_FPS,      &currentConfig.EmuOpt,  0x002, 0, 0, 1, 1 },
	{ NULL,                        MB_RANGE, MA_OPT_FRAMESKIP,     &currentConfig.Frameskip, 0, -1, 16, 1, 1 },
	{ "Enable sound",              MB_ONOFF, MA_OPT_ENABLE_SOUND,  &currentConfig.EmuOpt,  0x004, 0, 0, 1, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_SOUND_QUALITY, NULL, 0, 0, 0, 1, 1 },
	{ NULL,                        MB_NONE,  MA_OPT_REGION,        NULL, 0, 0, 0, 1, 1 },
	{ "Use SRAM/BRAM savestates",  MB_ONOFF, MA_OPT_SRAM_STATES,   &currentConfig.EmuOpt,  0x001, 0, 0, 1, 1 },
};

#define OPT_ENTRY_COUNT (sizeof(opt_entries) / sizeof(opt_entries[0]))
const int opt_entry_count = OPT_ENTRY_COUNT;

menu_entry opt2_entries[] =
{
	{ "Disable sprite limit",      MB_ONOFF, MA_OPT2_NO_SPRITE_LIM, &PicoOpt, 0x40000, 0, 0, 1, 1 },
	{ "Emulate Z80",               MB_ONOFF, MA_OPT2_ENABLE_Z80,    &PicoOpt, 0x00004, 0, 0, 1, 1 },
	{ "Emulate YM2612 (FM)",       MB_ONOFF, MA_OPT2_ENABLE_YM2612, &PicoOpt, 0x00001, 0, 0, 1, 1 },
	{ "Emulate SN76496 (PSG)",     MB_ONOFF, MA_OPT2_ENABLE_SN76496,&PicoOpt, 0x00002, 0, 0, 1, 1 },
	{ "gzip savestates",           MB_ONOFF, MA_OPT2_GZIP_STATES,   &currentConfig.EmuOpt, 0x0008, 0, 0, 1, 1 },
	{ "SVP dynarec",               MB_ONOFF, MA_OPT2_SVP_DYNAREC,   &PicoOpt, 0x20000, 0, 0, 1, 1 },
	{ "Disable idle loop patching",MB_ONOFF, MA_OPT2_NO_IDLE_LOOPS, &PicoOpt, 0x80000, 0, 0, 1, 1 },
};

#define OPT2_ENTRY_COUNT (sizeof(opt2_entries) / sizeof(opt2_entries[0]))
const int opt2_entry_count = OPT2_ENTRY_COUNT;

menu_entry cdopt_entries[] =
{
	{ "CD LEDs",                   MB_ONOFF, MA_CDOPT_LEDS,         &currentConfig.EmuOpt, 0x0400, 0, 0, 1, 1 },
	{ "CDDA audio",                MB_ONOFF, MA_CDOPT_CDDA,         &PicoOpt, 0x0800, 0, 0, 1, 1 },
	{ "PCM audio",                 MB_ONOFF, MA_CDOPT_PCM,          &PicoOpt, 0x0400, 0, 0, 1, 1 },
	{ NULL,                        MB_NONE,  MA_CDOPT_READAHEAD,    NULL, 0, 0, 0, 1, 1 },
	{ "SaveRAM cart",              MB_ONOFF, MA_CDOPT_SAVERAM,      &PicoOpt, 0x8000, 0, 0, 1, 1 },
	{ "Scale/Rot. fx (slow)",      MB_ONOFF, MA_CDOPT_SCALEROT_CHIP,&PicoOpt, 0x1000, 0, 0, 1, 1 },
	{ "Better sync (slow)",        MB_ONOFF, MA_CDOPT_BETTER_SYNC,  &PicoOpt, 0x2000, 0, 0, 1, 1 },
};

#define CDOPT_ENTRY_COUNT (sizeof(cdopt_entries) / sizeof(cdopt_entries[0]))
const int cdopt_entry_count = CDOPT_ENTRY_COUNT;

menu_entry ctrlopt_entries[] =
{
	{ "6 button pad",              MB_ONOFF, MA_OPT_6BUTTON_PAD,   &PicoOpt, 0x020, 0, 0, 1, 1 },
	{ "Turbo rate",                MB_RANGE, MA_CTRL_TURBO_RATE,   &currentConfig.turbo_rate, 0, 1, 30, 1, 1 },
};

#define CTRLOPT_ENTRY_COUNT (sizeof(ctrlopt_entries) / sizeof(ctrlopt_entries[0]))
const int ctrlopt_entry_count = CTRLOPT_ENTRY_COUNT;

me_bind_action emuctrl_actions[] =
{
	{ "Load State     ", 1<<28 },
	{ "Save State     ", 1<<27 },
	{ "Pause Emu      ", 1<<26 },
	{ "Switch Renderer", 1<<25 },
	{ "Prev save slot ", 1<<23 },
	{ "Next save slot ", 1<<22 },
	{ "Volume down    ", 1<<21 },
	{ "Volume up      ", 1<<20 },
	{ NULL,              0     }
};


