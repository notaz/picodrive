typedef enum
{
	READPNG_BG = 1,
	READPNG_FONT,
	READPNG_SELECTOR
}
readpng_what;

void readpng(void *dest, const char *fname, readpng_what what);

