// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <strings.h>
#include <linux/limits.h>

#include "../gp2x/gp2x.h"
#include "../gp2x/menu.h"
#include "../common/menu.h"
#include "../common/emu.h"
#include "../common/config.h"
#include "../gp2x/emu.h"
#include "../gp2x/version.h"


extern char *ext_menu, *ext_state;
extern int select_exits;
extern char *PicoConfigFile;
int mmuhack_status = 0; // TODO rm
char **g_argv;

void parse_cmd_line(int argc, char *argv[])
{
	int x, unrecognized = 0;

	for(x = 1; x < argc; x++)
	{
		if(argv[x][0] == '-')
		{
			if(strcasecmp(argv[x], "-menu") == 0) {
				if(x+1 < argc) { ++x; ext_menu = argv[x]; } /* External Frontend: Program Name */
			}
			else if(strcasecmp(argv[x], "-state") == 0) {
				if(x+1 < argc) { ++x; ext_state = argv[x]; } /* External Frontend: Arguments */
			}
			else if(strcasecmp(argv[x], "-config") == 0) {
				if(x+1 < argc) { ++x; PicoConfigFile = argv[x]; }
			}
			else if(strcasecmp(argv[x], "-selectexit") == 0) {
				select_exits = 1;
			}
			else {
				unrecognized = 1;
				break;
			}
		} else {
			/* External Frontend: ROM Name */
			FILE *f;
			strncpy(romFileName, argv[x], PATH_MAX);
			romFileName[PATH_MAX-1] = 0;
			f = fopen(romFileName, "rb");
			if (f) fclose(f);
			else unrecognized = 1;
			engineState = PGS_ReloadRom;
			break;
		}
	}

	if (unrecognized) {
		printf("\n\n\nPicoDrive v" VERSION " (c) notaz, 2006-2008\n");
		printf("usage: %s [options] [romfile]\n", argv[0]);
		printf( "options:\n"
				"-menu <menu_path> launch a custom program on exit instead of default gp2xmenu\n"
				"-state <param>    pass '-state param' to the menu program\n"
				"-config <file>    use specified config file instead of default 'picoconfig.bin'\n"
				"                  see currentConfig_t structure in emu.h for the file format\n"
				"-selectexit       pressing SELECT will exit the emu and start 'menu_path'\n");
	}
}


int main(int argc, char *argv[])
{
	g_argv = argv;

	emu_prepareDefaultConfig();
	emu_ReadConfig(0, 0);
	config_readlrom(PicoConfigFile);

	gp2x_init();
	emu_Init();
	menu_init();

	engineState = PGS_Menu;

	if (argc > 1)
		parse_cmd_line(argc, argv);

	for (;;)
	{
		switch (engineState)
		{
			case PGS_Menu:
				menu_loop();
				break;

			case PGS_ReloadRom:
				if (emu_ReloadRom())
					engineState = PGS_Running;
				else {
					printf("PGS_ReloadRom == 0\n");
					engineState = PGS_Menu;
				}
				break;

			case PGS_RestartRun:
				engineState = PGS_Running;

			case PGS_Running:
				emu_Loop();
				break;

			case PGS_Quit:
				goto endloop;

			default:
				printf("engine got into unknown state (%i), exitting\n", engineState);
				goto endloop;
		}
	}

	endloop:

	emu_Deinit();

	return 0;
}
