// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define CASE_SENSITIVE_FS 1 // CS filesystem
#define DONT_OPEN_MANY_FILES 0
#define REDUCE_IO_CALLS 0
#define SIMPLE_WRITE_SOUND 0

#define SCREEN_SIZE_FIXED 1
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// draw2.c
#define START_ROW  0 // which row of tiles to start rendering at?
#define END_ROW   28 // ..end

// pico.c
#define CAN_HANDLE_240_LINES	1

// logging emu events
#define EL_LOGMASK (EL_STATUS)
// (EL_STATUS|EL_ANOMALY|EL_UIO|EL_SRAMIO|EL_INTS|EL_CDPOLL|EL_IDLE)

//#define dprintf(f,...) printf("%05i:%03i: " f "\n",Pico.m.frame_count,Pico.m.scanline,##__VA_ARGS__)
#define dprintf(x...)

// platform
#define PATH_SEP      "/"
#define PATH_SEP_C    '/'
#define MENU_X2       0

#endif //PORT_CONFIG_H
