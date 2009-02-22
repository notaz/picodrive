// (c) Copyright 2006-2007 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


extern char romFileName[];
extern int engineState;


void emu_Init(void);
void emu_Deinit(void);
void emu_Loop(void);
void emu_ResetGame(void);

void osd_text(int x, int y, const char *text);

