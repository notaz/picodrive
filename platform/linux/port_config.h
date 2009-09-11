// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define NO_SYNC

#define CASE_SENSITIVE_FS 1 // CS filesystem
#define DONT_OPEN_MANY_FILES 0
#define REDUCE_IO_CALLS 0

#define SCREEN_SIZE_FIXED 0
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// draw.c
#define OVERRIDE_HIGHCOL 0

// draw2.c
#define START_ROW  0 // which row of tiles to start rendering at?
#define END_ROW   28 // ..end

// pico.c
#define CAN_HANDLE_240_LINES	1

#define SIMPLE_WRITE_SOUND	0
#define mix_32_to_16l_stereo_lvl mix_32_to_16l_stereo

#define EL_LOGMASK (EL_ANOMALY|EL_STATUS|EL_UIO|EL_IDLE|EL_32X)//|EL_VDPDMA|EL_HVCNT|EL_ASVDP)//|EL_SVP)
// EL_VDPDMA|EL_ASVDP|EL_SR) // |EL_BUSREQ|EL_Z80BNK)

//#define dprintf(f,...) printf("%05i:%03i: " f "\n",Pico.m.frame_count,Pico.m.scanline,##__VA_ARGS__)
#define dprintf(x...)

// platform
#define PATH_SEP      "/"
#define PATH_SEP_C    '/'
#define MENU_X2       0

#endif //PORT_CONFIG_H

