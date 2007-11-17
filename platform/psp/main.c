// (c) Copyright 2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <string.h>
#include "psp.h"
#include "emu.h"
#include "menu.h"
#include "mp3.h"
#include "../common/menu.h"
#include "../common/emu.h"
#include "../common/lprintf.h"
#include "version.h"

#define GPROF 0
#define GCOV 0

#if GPROF
#include <pspprof.h>
#endif

#if GCOV
#include <stdio.h>
#include <stdlib.h>

void dummy(void)
{
	engineState = atoi(romFileName);
	setbuf(NULL, NULL);
	getenv(NULL);
}
#endif

int main()
{
	lprintf("\nPicoDrive v" VERSION " " __DATE__ " " __TIME__ "\n");
	psp_init();
	emu_ReadConfig(0, 0);
	emu_Init();
	menu_init();
	// moved to emu_Loop(), after CPU clock change..
	//mp3_init();

	engineState = PGS_Menu;

	for (;;)
	{
		switch (engineState)
		{
			case PGS_Menu:
#if !GPROF
				menu_loop();
#else
				strcpy(romFileName, currentConfig.lastRomFile);
				engineState = PGS_ReloadRom;
#endif
				break;

			case PGS_ReloadRom:
				if (emu_ReloadRom()) {
					engineState = PGS_Running;
					if (mp3_last_error != 0)
						engineState = PGS_Menu; // send to menu to display mp3 error
				} else {
					lprintf("PGS_ReloadRom == 0\n");
					engineState = PGS_Menu;
				}
				break;

			case PGS_RestartRun:
				engineState = PGS_Running;

			case PGS_Running:
				emu_Loop();
#if GPROF
				goto endloop;
#endif
				break;

			case PGS_Quit:
				goto endloop;

			default:
				lprintf("engine got into unknown state (%i), exitting\n", engineState);
				goto endloop;
		}
	}

	endloop:

	mp3_deinit();
	emu_Deinit();
#if GPROF
	gprof_cleanup();
#endif
#if !GCOV
	psp_finish();
#endif

	return 0;
}

