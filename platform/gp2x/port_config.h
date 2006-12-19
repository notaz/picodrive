// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define CPU_CALL

// draw2.c
#define START_ROW  0 // which row of tiles to start rendering at?
#define END_ROW   28 // ..end

// pico.c
#define CAN_HANDLE_240_LINES	1

//#define dprintf(f,...) printf(f"\n",##__VA_ARGS__)
#define dprintf(x...)

#endif //PORT_CONFIG_H
