
/*
void vidConvCpyRGB32 (void *to, void *from, int lines, int p240)
{
	unsigned short *ps = (unsigned short *) from;
	unsigned long  *pd = (unsigned long *) to;
	int x, y;
	int to_x = p240 ? 240 : 224;
	if(!p240) pd += 8;

	for(y = 0; y < lines; y++) // ps < ps_end; ps++)
		for(x = 0; x < to_x; x++, ps++)
			// Convert          0000bbb0 ggg0rrr0
			// to  ..0 rrr00000 ggg00000 bbb00000
			*(pd+y*256+x) = ((*ps&0x000F)<<20) | ((*ps&0x00F0)<<8) | ((*ps&0x0F00)>>4);
}
*/

// stubs
void vidConvCpyRGB32  (void *to, void *from, int pixels) {}
void vidConvCpyRGB32sh(void *to, void *from, int pixels) {}
void vidConvCpyRGB32hi(void *to, void *from, int pixels) {}

void vidConvCpy_90        (void *to, void *from, void *pal, int width) {}
void vidConvCpy_270       (void *to, void *from, void *pal, int width) {}
void vidConvCpy_center_0  (void *to, void *from, void *pal) {}
void vidConvCpy_center_180(void *to, void *from, void *pal) {}
void vidConvCpy_center2_40c_0  (void *to, void *from, void *pal, int lines) {}
void vidConvCpy_center2_40c_180(void *to, void *from, void *pal, int lines) {}
void vidConvCpy_center2_32c_0  (void *to, void *from, void *pal, int lines) {}
void vidConvCpy_center2_32c_180(void *to, void *from, void *pal, int lines) {}

void vidClear(void *to, int lines) {}

