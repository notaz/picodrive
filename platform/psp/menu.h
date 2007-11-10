// (c) Copyright 2006,2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

void menu_loop(void);
int  menu_loop_tray(void);
void menu_romload_prepare(const char *rom_name);
void menu_romload_end(void);


#define CONFIGURABLE_KEYS (BTN_UP|BTN_LEFT|BTN_RIGHT|BTN_DOWN|BTN_L|BTN_R|BTN_TRIANGLE|BTN_CIRCLE|BTN_X|BTN_SQUARE|BTN_START| \
	BTN_NUB_UP|BTN_NUB_RIGHT|BTN_NUB_DOWN|BTN_NUB_LEFT|BTN_NOTE)

