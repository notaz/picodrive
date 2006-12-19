// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

void vidConvCpyRGB32  (void *to, void *from, int pixels);
void vidConvCpyRGB32sh(void *to, void *from, int pixels);
void vidConvCpyRGB32hi(void *to, void *from, int pixels);
void vidCpyM2_40col(void *dest, void *src);
void vidCpyM2_32col(void *dest, void *src);
void vidCpyM2_32col_nobord(void *dest, void *src);
void spend_cycles(int c); // utility
