// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define CASE_SENSITIVE_FS 0
#define DONT_OPEN_MANY_FILES 1 // work around the stupid PSP ~10 open file limit
#define REDUCE_IO_CALLS 1      // another workaround
#define SIMPLE_WRITE_SOUND 0

#define SCREEN_SIZE_FIXED 1
#define SCREEN_WIDTH  512
#define SCREEN_HEIGHT 272

// draw.c
#define USE_BGR555 1

// draw2.c
#define START_ROW  0 // which row of tiles to start rendering at?
#define END_ROW   28 // ..end
#define DRAW2_OVERRIDE_LINE_WIDTH 512

// pico.c
extern void blit1(void);
#define DRAW_FINISH_FUNC blit1
#define CAN_HANDLE_240_LINES	1

// logging emu events
#define EL_LOGMASK (EL_STATUS|EL_IDLE) // (EL_STATUS|EL_ANOMALY|EL_UIO|EL_SRAMIO) // xffff

//#define dprintf(f,...) printf("%05i:%03i: " f "\n",Pico.m.frame_count,Pico.m.scanline,##__VA_ARGS__)
#define dprintf(x...)

// platform
#define PATH_SEP      "/"
#define PATH_SEP_C    '/'
#define MENU_X2       0

#endif //PORT_CONFIG_H
