// (c) Copyright 2006,2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

void menu_init(void);
void text_out16(int x, int y, const char *texto, ...);
void menu_loop(void);
int  menu_loop_tray(void);
void menu_romload_prepare(const char *rom_name);
void menu_romload_end(void);

// TODO: add more?
#define CONFIGURABLE_KEYS \
	(BTN_UP|BTN_DOWN|BTN_LEFT|BTN_RIGHT|BTN_STOP|BTN_PLAY|BTN_FWD|BTN_REW| \
		BTN_L|BTN_R)

