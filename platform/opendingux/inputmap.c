#include <stdlib.h>
#include <SDL_keysym.h>

#include "../libpicofe/input.h"
#include "../libpicofe/in_sdl.h"
#include "../common/input_pico.h"

const struct in_default_bind in_sdl_defbinds[] = {
	{ SDLK_UP,	IN_BINDTYPE_PLAYER12, GBTN_UP },
	{ SDLK_DOWN,	IN_BINDTYPE_PLAYER12, GBTN_DOWN },
	{ SDLK_LEFT,	IN_BINDTYPE_PLAYER12, GBTN_LEFT },
	{ SDLK_RIGHT,	IN_BINDTYPE_PLAYER12, GBTN_RIGHT },
	{ SDLK_LSHIFT,	IN_BINDTYPE_PLAYER12, GBTN_A },
	{ SDLK_LALT,	IN_BINDTYPE_PLAYER12, GBTN_B },
	{ SDLK_LCTRL,	IN_BINDTYPE_PLAYER12, GBTN_C },
	{ SDLK_RETURN,	IN_BINDTYPE_PLAYER12, GBTN_START },
	{ SDLK_ESCAPE,	IN_BINDTYPE_EMU, PEVB_MENU },
	{ SDLK_TAB,		IN_BINDTYPE_EMU, PEVB_PICO_PPREV },
	{ SDLK_BACKSPACE,	IN_BINDTYPE_EMU, PEVB_PICO_PNEXT },
	{ SDLK_BACKSPACE,	IN_BINDTYPE_EMU, PEVB_STATE_SAVE },
	{ SDLK_TAB,		IN_BINDTYPE_EMU, PEVB_STATE_LOAD },
	{ SDLK_SPACE,	IN_BINDTYPE_EMU, PEVB_FF },
	{ 0, 0, 0 }
};

const struct menu_keymap in_sdl_key_map[] =
{
	{ SDLK_UP,	PBTN_UP },
	{ SDLK_DOWN,	PBTN_DOWN },
	{ SDLK_LEFT,	PBTN_LEFT },
	{ SDLK_RIGHT,	PBTN_RIGHT },
	{ SDLK_LCTRL,	PBTN_MOK },
	{ SDLK_LALT,	PBTN_MBACK },
	{ SDLK_SPACE,	PBTN_MA2 },
	{ SDLK_LSHIFT,	PBTN_MA3 },
	{ SDLK_TAB,	PBTN_L },
	{ SDLK_BACKSPACE,	PBTN_R },
};

const char * const in_sdl_key_names[SDLK_LAST] = {
	[SDLK_UP] = "UP",
	[SDLK_DOWN] = "DOWN",
	[SDLK_LEFT] = "LEFT",
	[SDLK_RIGHT] = "RIGHT",
	[SDLK_LCTRL] = "A",
	[SDLK_LALT] = "B",
#ifdef __GCW0__
	[SDLK_LSHIFT] = "X",
	[SDLK_SPACE] = "Y",
#else
	[SDLK_LSHIFT] = "Y",
	[SDLK_SPACE] = "X",
#endif
	[SDLK_RETURN] = "START",
	[SDLK_ESCAPE] = "SELECT",

#ifdef __RG350__
	[SDLK_HOME] = "POWER",

	[SDLK_TAB] = "L1",
	[SDLK_BACKSPACE] = "R1",
	[SDLK_PAGEUP] = "L2",
	[SDLK_PAGEDOWN] = "R2",
	[SDLK_KP_DIVIDE] = "L3",
	[SDLK_KP_PERIOD] = "R3",
#else
	[SDLK_TAB] = "L",
	[SDLK_BACKSPACE] = "R",
	[SDLK_POWER] = "POWER",
	[SDLK_PAUSE] = "LOCK",
#endif
};

const char *const *in_sdl_key_names_p = in_sdl_key_names;
