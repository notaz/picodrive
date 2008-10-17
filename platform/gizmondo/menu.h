// (c) Copyright 2006,2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

void menu_loop(void);
int  menu_loop_tray(void);
void menu_romload_prepare(const char *rom_name);
void menu_romload_end(void);


#define CONFIGURABLE_KEYS \
	(PBTN_UP|PBTN_DOWN|PBTN_LEFT|PBTN_RIGHT|PBTN_STOP|PBTN_PLAY|PBTN_FWD|PBTN_REW| \
		PBTN_L|PBTN_R|PBTN_VOLUME|PBTN_BRIGHTNESS|PBTN_ALARM)

