// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

extern char menuErrorMsg[40];

void gp2x_text_out8  (int x, int y, char *texto, ...);
void gp2x_text_out15 (int x, int y, char *text);
void gp2x_text_out8_2(int x, int y, char *texto, int color);
void menu_loop(void);

#define CONFIGURABLE_KEYS \
	(GP2X_UP|GP2X_DOWN|GP2X_LEFT|GP2X_RIGHT|GP2X_A|GP2X_B|GP2X_X|GP2X_Y| \
		GP2X_START|GP2X_SELECT|GP2X_L|GP2X_R|GP2X_PUSH|GP2X_VOL_UP|GP2X_VOL_DOWN)

