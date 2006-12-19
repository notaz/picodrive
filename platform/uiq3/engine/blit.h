// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

extern "C"
{
	void vidConvCpyRGB32  (void *to, void *from, int pixels);
	void vidConvCpyRGB32sh(void *to, void *from, int pixels);
	void vidConvCpyRGB32hi(void *to, void *from, int pixels);

	void vidConvCpy_90        (void *to, void *from, void *pal, int width);
	void vidConvCpy_270       (void *to, void *from, void *pal, int width);
	void vidConvCpy_center_0  (void *to, void *from, void *pal);
	void vidConvCpy_center_180(void *to, void *from, void *pal);
	void vidConvCpy_center2_40c_0  (void *to, void *from, void *pal, int lines);
	void vidConvCpy_center2_40c_180(void *to, void *from, void *pal, int lines);
	void vidConvCpy_center2_32c_0  (void *to, void *from, void *pal, int lines);
	void vidConvCpy_center2_32c_180(void *to, void *from, void *pal, int lines);

	void vidClear(void *to, int lines);
}
