// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define NO_SYNC

// draw2.c
#define START_ROW  0 // which row of tiles to start rendering at?
#define END_ROW   28 // ..end

// pico.c
#define CAN_HANDLE_240_LINES	1

#define mix_32_to_16l_stereo_lvl mix_32_to_16l_stereo

#define EL_LOGMASK (EL_ANOMALY|EL_STATUS|EL_SRAMIO|EL_EEPROM|EL_UIO) // EL_VDPDMA|EL_ASVDP|EL_SR) // |EL_BUSREQ|EL_Z80BNK)

//#define dprintf(f,...) printf("%05i:%03i: " f "\n",Pico.m.frame_count,Pico.m.scanline,##__VA_ARGS__)
#define dprintf(x...)

#endif //PORT_CONFIG_H

