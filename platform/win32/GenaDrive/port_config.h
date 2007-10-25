// port specific settings

#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

// draw2.c
#define START_ROW  0 // which row of tiles to start rendering at?
#define END_ROW   28 // ..end

// pico.c
#define CAN_HANDLE_240_LINES	0

#ifdef __cplusplus
extern "C" {
#endif

// common debug
int dprintf (char *format, ...);
int dprintf2(char *format, ...);

#ifdef __cplusplus
} // End of extern "C"
#endif

#endif //PORT_CONFIG_H
