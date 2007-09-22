// (c) Copyright 2006,2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

void menu_loop(void);
int  menu_loop_tray(void);
void menu_romload_prepare(const char *rom_name);
void menu_romload_end(void);

#define CONFIGURABLE_KEYS \
	(GP2X_UP|GP2X_DOWN|GP2X_LEFT|GP2X_RIGHT|GP2X_A|GP2X_B|GP2X_X|GP2X_Y| \
		GP2X_START|GP2X_SELECT|GP2X_L|GP2X_R|GP2X_PUSH|GP2X_VOL_UP|GP2X_VOL_DOWN)

