// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

// draw2.c
#define START_ROW  1 // which row of tiles to start rendering at?
#define END_ROW   27 // ..end

// pico.c
#define CAN_HANDLE_240_LINES	0

// common debug
#if defined(__DEBUG_PRINT)
void dprintf(char *format, ...);
#else
#define dprintf(x...)
#endif


#endif //PORT_CONFIG_H
