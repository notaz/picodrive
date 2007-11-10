#include "psp.h"
#include "emu.h"
#include "menu.h"
#include "../common/menu.h"
#include "../common/emu.h"
#include "../common/lprintf.h"
#include "version.h"

int main()
{
	lprintf("\nPicoDrive v" VERSION " " __DATE__ " " __TIME__ "\n");
	psp_init();

	emu_ReadConfig(0, 0);
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
	psp_finish();

	return 0;
}

