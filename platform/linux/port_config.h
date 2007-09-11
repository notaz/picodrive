// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define CPU_CALL
#define NO_SYNC

// draw2.c
#define START_ROW  0 // which row of tiles to start rendering at?
#define END_ROW   28 // ..end

// pico.c
#define CAN_HANDLE_240_LINES	1

#define EL_LOGMASK (EL_ANOMALY|EL_STATUS|EL_VDPDMA|EL_ASVDP|EL_SR) // |EL_BUSREQ|EL_Z80BNK)

//#define dprintf(f,...) printf("%05i:%03i: " f "\n",Pico.m.frame_count,Pico.m.scanline,##__VA_ARGS__)
#define dprintf(x...)

#endif //PORT_CONFIG_H

