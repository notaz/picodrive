typedef enum
{
	READPNG_BG = 1,
	READPNG_FONT,
	READPNG_SELECTOR,
	READPNG_320_24,
	READPNG_480_24
}
readpng_what;

#ifdef __cplusplus
extern "C" {
#endif

int readpng(void *dest, const char *fname, readpng_what what);

#ifdef __cplusplus
}
#endif
