// (c) Copyright 2006-2009 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "menu.h"
#include "emu.h"
#include "config.h"
#include "input.h"
#include "plat.h"
#include <version.h>


extern char *PicoConfigFile;
static int load_state_slot = -1;
char **g_argv;

void parse_cmd_line(int argc, char *argv[])
{
	int x, unrecognized = 0;

	for (x = 1; x < argc; x++)
	{
		if (argv[x][0] == '-')
		{
			if (strcasecmp(argv[x], "-config") == 0) {
				if (x+1 < argc) { ++x; PicoConfigFile = argv[x]; }
			}
			else if (strcasecmp(argv[x], "-loadstate") == 0) {
				if (x+1 < argc) { ++x; load_state_slot = atoi(argv[x]); }
			}
			else {
				unrecognized = 1;
				break;
			}
		} else {
			/* External Frontend: ROM Name */
			FILE *f;
			strncpy(rom_fname_reload, argv[x], sizeof(rom_fname_reload));
			rom_fname_reload[sizeof(rom_fname_reload) - 1] = 0;
			f = fopen(rom_fname_reload, "rb");
			if (f) fclose(f);
			else unrecognized = 1;
			engineState = PGS_ReloadRom;
			break;
		}
	}

	if (unrecognized) {
		printf("\n\n\nPicoDrive v" VERSION " (c) notaz, 2006-2009\n");
		printf("usage: %s [options] [romfile]\n", argv[0]);
		printf("options:\n"
			" -config <file>    use specified config file instead of default 'config.cfg'\n"
			" -loadstate <num>  if ROM is specified, try loading slot <num>\n");
	}
}


int main(int argc, char *argv[])
{
	g_argv = argv;

	plat_early_init();

	/* in_init() must go before config, config accesses in_ fwk */
	in_init();
	pemu_prep_defconfig();
	emu_read_config(0, 0);
	config_readlrom(PicoConfigFile);

	plat_init();
	in_probe();
	in_debug_dump();

	emu_init();
	menu_init();

	engineState = PGS_Menu;

	if (argc > 1)
		parse_cmd_line(argc, argv);

	if (engineState == PGS_ReloadRom)
	{
		if (emu_reload_rom(rom_fname_reload)) {
			engineState = PGS_Running;
			if (load_state_slot >= 0) {
				state_slot = load_state_slot;
				emu_save_load_game(1, 0);
			}
		}
	}

	for (;;)
	{
		switch (engineState)
		{
			case PGS_Menu:
				menu_loop();
				break;

			case PGS_ReloadRom:
				if (emu_reload_rom(rom_fname_reload))
					engineState = PGS_Running;
				else {
					printf("PGS_ReloadRom == 0\n");
					engineState = PGS_Menu;
				}
				break;

			case PGS_RestartRun:
				engineState = PGS_Running;

			case PGS_Running:
				emu_loop();
				break;

			case PGS_Quit:
				goto endloop;

			default:
				printf("engine got into unknown state (%i), exitting\n", engineState);
				goto endloop;
		}
	}

	endloop:

	emu_finish();
	plat_finish();

	return 0;
}
