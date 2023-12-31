#include <stddef.h>

#include <ps2_joystick_driver.h>
#include <ps2_audio_driver.h>
#include <libpad.h>

#include "in_ps2.h"
#include "../libpicofe/input.h"
#include "../common/input_pico.h"
#include "../common/emu.h"

static struct in_default_bind in_ps2_defbinds[] =
{
	{ PAD_UP,          IN_BINDTYPE_PLAYER12, GBTN_UP },
	{ PAD_DOWN,        IN_BINDTYPE_PLAYER12, GBTN_DOWN },
	{ PAD_LEFT,        IN_BINDTYPE_PLAYER12, GBTN_LEFT },
	{ PAD_RIGHT,       IN_BINDTYPE_PLAYER12, GBTN_RIGHT },
	{ PAD_SQUARE,      IN_BINDTYPE_PLAYER12, GBTN_A },
	{ PAD_CROSS,       IN_BINDTYPE_PLAYER12, GBTN_B },
	{ PAD_CIRCLE,      IN_BINDTYPE_PLAYER12, GBTN_C },
	{ PAD_START,       IN_BINDTYPE_PLAYER12, GBTN_START },
	{ PAD_TRIANGLE,    IN_BINDTYPE_EMU, PEVB_SWITCH_RND },
	{ PAD_L1,          IN_BINDTYPE_EMU, PEVB_STATE_SAVE },
	{ PAD_R1,          IN_BINDTYPE_EMU, PEVB_STATE_LOAD },
	{ PAD_SELECT,      IN_BINDTYPE_EMU, PEVB_MENU },
	{ 0, 0, 0 }
};

void plat_init(void) 
{
    init_joystick_driver(false);
    in_ps2_init(in_ps2_defbinds);
    in_probe();
    init_audio_driver();
    // plat_get_data_dir(rom_fname_loaded, sizeof(rom_fname_loaded));
}

void plat_finish(void) {
    deinit_audio_driver();
    deinit_joystick_driver(false);
}