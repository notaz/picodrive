// (c) Copyright 2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#include <windows.h>

#include "giz.h"
#include "menu.h"
#include "emu.h"
#include "../common/menu.h"
#include "../common/emu.h"
#include "version.h"


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
	emu_ReadConfig(0, 0);
	giz_init(hInstance, hPrevInstance);
	emu_Init();
	menu_init();

	engineState = PGS_Menu;

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
					lprintf("PGS_ReloadRom == 0\n");
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
				lprintf("engine got into unknown state (%i), exitting\n", engineState);
				goto endloop;
		}
	}

	endloop:

	emu_Deinit();
	giz_deinit();

	return 0;
}
