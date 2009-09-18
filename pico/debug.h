
char *PDebugMain(void);
char *PDebug32x(void);
char *PDebugSpriteList(void);
void PDebugShowSpriteStats(unsigned short *screen, int stride);
void PDebugShowPalette(unsigned short *screen, int stride);
void PDebugShowSprite(unsigned short *screen, int stride, int which);
void PDebugDumpMem(void);
void PDebugZ80Frame(void);
void PDebugCPUStep(void);

