extern "C" {
	void vidConvCpyRGB444(void *to, void *from, int pixels);
	void vidConvCpyRGB565(void *to, void *from, int pixels);
	void vidConvCpyRGB32 (void *to, void *from, int pixels);

	// warning: the functions below will reboot the phone if used incorrectly!
	void vidConvCpyM2_16_90    (void *to, void *from, int width); // width is in blocks of 8 pixels
	void vidConvCpyM2_16_270   (void *to, void *from, int width);
	void vidConvCpyM2_RGB32_90 (void *to, void *from, int width);
	void vidConvCpyM2_RGB32_270(void *to, void *from, int width);
}
