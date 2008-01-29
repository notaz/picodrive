// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

// draw2.c
#define START_ROW  0 // which row of tiles to start rendering at?
#define END_ROW   28 // ..end

#define SIMPLE_WRITE_SOUND	1
#define mix_32_to_16l_stereo_lvl mix_32_to_16l_stereo

// pico.c
#define CAN_HANDLE_240_LINES	0

#define dprintf
#define strcasecmp stricmp

#endif //PORT_CONFIG_H
