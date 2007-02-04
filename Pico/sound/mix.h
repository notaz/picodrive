
void memcpy32(int *dest, int *src, int count);
void memset32(int *dest, int c, int count);
//void mix_32_to_32(int *dest, int *src, int count);
void mix_16h_to_32(int *dest, short *src, int count);
void mix_16h_to_32_s1(int *dest, short *src, int count);
void mix_16h_to_32_s2(int *dest, short *src, int count);
void mix_32_to_16l_stereo(short *dest, int *src, int count);
void mix_32_to_16_mono(short *dest, int *src, int count);

