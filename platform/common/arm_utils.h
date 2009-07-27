// (c) Copyright 2006-2009 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

void vidConvCpyRGB32  (void *to, void *from, int pixels);
void vidConvCpyRGB32sh(void *to, void *from, int pixels);
void vidConvCpyRGB32hi(void *to, void *from, int pixels);
void vidcpy_m2(void *dest, void *src, int m32col, int with_32c_border);
void vidcpy_m2_rot(void *dest, void *src, int m32col, int with_32c_border);
void spend_cycles(int c); // utility

void rotated_blit8 (void *dst, void *linesx4, int y, int is_32col);
void rotated_blit16(void *dst, void *linesx4, int y, int is_32col);
