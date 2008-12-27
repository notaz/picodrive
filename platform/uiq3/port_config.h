// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define CASE_SENSITIVE_FS 0
#define DONT_OPEN_MANY_FILES 0
#define REDUCE_IO_CALLS 0
#define SIMPLE_WRITE_SOUND 0

// draw.c
#define OVERRIDE_HIGHCOL 0

// draw2.c
#define START_ROW  0 // which row of tiles to start rendering at?
#define END_ROW   28 // ..end

// pico.c
#define CAN_HANDLE_240_LINES	0 // for now

// logging emu events
#define EL_LOGMASK (EL_STATUS) // |EL_SVP|EL_ANOMALY)

//extern void dprintf(char *format, ...);
//#define dprintf(f,...) printf("%05i:%03i: " f "\n",Pico.m.frame_count,Pico.m.scanline,##__VA_ARGS__)
#define dprintf(x...)

// platform
#define PLAT_MAX_KEYS (256+19)
#define PLAT_HAVE_JOY 0
#define PATH_SEP      "\\"
#define PATH_SEP_C    '\\'

// engine/vid.cpp, also update BORDER_R in port_config.s
#define VID_BORDER_R  16

#endif // PORT_CONFIG_H
