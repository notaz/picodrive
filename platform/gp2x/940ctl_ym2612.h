void YM2612Init_940(int baseclock, int rate);
void YM2612ResetChip_940(void);
void YM2612UpdateOne_940(short *buffer, int length, int stereo);

int  YM2612Write_940(unsigned int a, unsigned int v);
unsigned char YM2612Read_940(void);

int  YM2612PicoTick_940(int n);
void YM2612PicoStateLoad_940(void);
