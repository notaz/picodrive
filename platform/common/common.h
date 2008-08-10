// platform specific things for common menu code

#ifdef __GP2X__
#include "../gp2x/gp2x.h"

#define BTN_UP    GP2X_UP
#define BTN_DOWN  GP2X_DOWN
#define BTN_LEFT  GP2X_LEFT
#define BTN_RIGHT GP2X_RIGHT

#define BTN_NORTH GP2X_Y
#define BTN_SOUTH GP2X_X
#define BTN_WEST  GP2X_A
#define BTN_EAST  GP2X_B
#define BTN_L     GP2X_L
#define BTN_R     GP2X_R

unsigned long wait_for_input(unsigned long interesting);
void gp2x_pd_clone_buffer2(void);
void menu_darken_bg(void *dst, int pixels, int darker);
void menu_flip(void);

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define SCREEN_BUFFER gp2x_screen

#define read_buttons(which) \
	wait_for_input(which)
#define read_buttons_async(which) \
	(gp2x_joystick_read(0) & (which))
#define menu_draw_begin() \
	gp2x_pd_clone_buffer2()
#define clear_screen() \
	memset(gp2x_screen, 0, 320*240*2)
#define darken_screen() \
	menu_darken_bg(gp2x_screen, 320*240, 0)
#define menu_draw_end() \
	menu_flip()

// ------------------------------------

#elif defined(__GIZ__)

#include "../gizmondo/giz.h"

#define BTN_NORTH BTN_STOP
#define BTN_SOUTH BTN_PLAY
#define BTN_WEST  BTN_REW
#define BTN_EAST  BTN_FWD

unsigned long wait_for_input(unsigned int interesting);
void menu_draw_begin(int use_bgbuff);
void menu_darken_bg(void *dst, const void *src, int pixels, int darker);
void menu_draw_end(void);

#define SCREEN_WIDTH  321
#define SCREEN_HEIGHT 240
#define SCREEN_BUFFER ((giz_screen != NULL) ? giz_screen : menu_screen)
extern void *menu_screen;
extern void *giz_screen;

#define read_buttons(which) \
	wait_for_input(which)
#define read_buttons_async(which) 0
#define menu_draw_begin() \
	menu_draw_begin(1)
#define clear_screen() \
	memset(SCREEN_BUFFER, 0, SCREEN_WIDTH*SCREEN_HEIGHT*2)
#define darken_screen() \
	menu_darken_bg(menu_screen, menu_screen, SCREEN_WIDTH*SCREEN_HEIGHT, 0)

// ------------------------------------

#elif defined(PSP)

#include "../psp/psp.h"

#define BTN_NORTH BTN_TRIANGLE
#define BTN_SOUTH BTN_X
#define BTN_WEST  BTN_SQUARE
#define BTN_EAST  BTN_CIRCLE

unsigned long wait_for_input(unsigned int interesting, int is_key_config);
void menu_draw_begin(void);
void menu_darken_bg(void *dst, const void *src, int pixels, int darker);
void menu_draw_end(void);

#define SCREEN_WIDTH  512
#define SCREEN_HEIGHT 272
#define SCREEN_BUFFER psp_screen

#define read_buttons(which) \
	wait_for_input(which, 0)
#define read_buttons_async(which) \
	(psp_pad_read(0) & (which))
#define clear_screen() \
	memset(SCREEN_BUFFER, 0, SCREEN_WIDTH*SCREEN_HEIGHT*2)
#define darken_screen() \
	menu_darken_bg(psp_screen, psp_screen, SCREEN_WIDTH*SCREEN_HEIGHT, 0)

// ------------------------------------

#elif defined(PANDORA)

// TODO

#include "../gp2x/gp2x.h"

#define BTN_UP    0
#define BTN_DOWN  0
#define BTN_LEFT  0
#define BTN_RIGHT 0

#define BTN_NORTH 0
#define BTN_SOUTH 0
#define BTN_WEST  0
#define BTN_EAST  0
#define BTN_L     0
#define BTN_R     0

unsigned long wait_for_input(unsigned long interesting);
void gp2x_pd_clone_buffer2(void);
void menu_darken_bg(void *dst, int pixels, int darker);
void menu_flip(void);

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 480
#define SCREEN_BUFFER gp2x_screen

#define read_buttons(which) \
	wait_for_input(which)
#define read_buttons_async(which) \
	(gp2x_joystick_read(0) & (which))
#define menu_draw_begin() \
	gp2x_pd_clone_buffer2()
#define clear_screen() \
	memset(gp2x_screen, 0, 800*480*2)
#define darken_screen() \
	menu_darken_bg(gp2x_screen, 800*480, 0)
#define menu_draw_end() \
	menu_flip()

#endif
