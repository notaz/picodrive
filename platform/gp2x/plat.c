#include <string.h>

#include "gp2x.h"
#include "../common/plat.h"
#include "../common/readpng.h"
#include "../common/menu.h"


void plat_video_menu_enter(int is_rom_loaded)
{
	if (is_rom_loaded)
	{
		// darken the active framebuffer
		memset(gp2x_screen, 0, 320*8*2);
		menu_darken_bg((char *)gp2x_screen + 320*8*2, 320*224, 1);
		memset((char *)gp2x_screen + 320*232*2, 0, 320*8*2);
	}
	else
	{
		// should really only happen once, on startup..
		readpng(gp2x_screen, "skin/background.png", READPNG_BG);
	}

	// copy to buffer2
	gp2x_memcpy_buffers((1<<2), gp2x_screen, 0, 320*240*2);

	// switch to 16bpp
	gp2x_video_changemode2(16);
	gp2x_video_RGB_setscaling(0, 320, 240);
	gp2x_video_flip2();
}

void plat_video_menu_begin(void)
{
	gp2x_pd_clone_buffer2();
}

void plat_video_menu_end(void)
{
	gp2x_video_flush_cache();
	gp2x_video_flip2();
}

