// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#ifdef __cplusplus
extern "C" {
#endif

int sound_timers_and_dac(int raster);
int sound_render(int offset, int length);

//int YM2612PicoTick(int n);

// z80 functionality wrappers
void z80_init();
void z80_resetCycles();
void z80_int();
int  z80_run(int cycles);
void z80_exit();

#ifdef __cplusplus
} // End of extern "C"
#endif
