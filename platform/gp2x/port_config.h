// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define CASE_SENSITIVE_FS 1 // CS filesystem
#define DONT_OPEN_MANY_FILES 0

// draw.c
#define OVERRIDE_HIGHCOL 0

// draw2.c
#define START_ROW  0 // which row of tiles to start rendering at?
#define END_ROW   28 // ..end

// pico.c
#define CAN_HANDLE_240_LINES	1

// logging emu events
#define EL_LOGMASK 0 // (EL_STATUS|EL_ANOMALY|EL_UIO|EL_SRAMIO) // xffff

//#define dprintf(f,...) printf("%05i:%03i: " f "\n",Pico.m.frame_count,Pico.m.scanline,##__VA_ARGS__)
#define dprintf(x...)

#endif //PORT_CONFIG_H
