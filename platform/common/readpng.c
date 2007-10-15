#include <stdio.h>
#include <string.h>
#include <png.h>
#include "readpng.h"
#include "lprintf.h"

#ifdef PSP
#define BG_WIDTH  480
#define BG_HEIGHT 272
#else
#define BG_WIDTH  320
#define BG_HEIGHT 240
#endif

void readpng(void *dest, const char *fname, readpng_what what)
{
	FILE *fp;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_bytepp row_ptr = NULL;

	if (dest == NULL || fname == NULL)
	{
		return;
	}

	fp = fopen(fname, "rb");
	if (fp == NULL)
	{
		lprintf(__FILE__ ": failed to open: %s\n", fname);
		return;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		lprintf(__FILE__ ": png_create_read_struct() failed\n");
		fclose(fp);
		return;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		lprintf(__FILE__ ": png_create_info_struct() failed\n");
		goto done;
	}

	// Start reading
	png_init_io(png_ptr, fp);
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_STRIP_ALPHA | PNG_TRANSFORM_PACKING, NULL);
	row_ptr = png_get_rows(png_ptr, info_ptr);
	if (row_ptr == NULL)
	{
		lprintf(__FILE__ ": png_get_rows() failed\n");
		goto done;
	}

	// lprintf("%s: %ix%i @ %ibpp\n", fname, (int)info_ptr->width, (int)info_ptr->height, info_ptr->pixel_depth);

	switch (what)
	{
		case READPNG_BG:
		{
			int height, width, h;
			unsigned short *dst = dest;
			if (info_ptr->pixel_depth != 24)
			{
				lprintf(__FILE__ ": bg image uses %ibpp, needed 24bpp\n", info_ptr->pixel_depth);
				break;
			}
			height = info_ptr->height;
			if (height > BG_HEIGHT) height = BG_HEIGHT;
			width = info_ptr->width;
			if (width > BG_WIDTH) width = BG_WIDTH;

			for (h = 0; h < height; h++)
			{
				unsigned char *src = row_ptr[h];
				int len = width;
				while (len--)
				{
#ifdef PSP
					*dst++ = ((src[2]&0xf8)<<8) | ((src[1]&0xf8)<<3) | (src[0] >> 3); // BGR
#else
					*dst++ = ((src[0]&0xf8)<<8) | ((src[1]&0xf8)<<3) | (src[2] >> 3); // RGB
#endif
					src += 3;
				}
				dst += BG_WIDTH - width;
			}
			break;
		}

		case READPNG_FONT:
		{
			int x, y, x1, y1;
			unsigned char *dst = dest;
			if (info_ptr->width != 128 || info_ptr->height != 160)
			{
				lprintf(__FILE__ ": unexpected font image size %ix%i, needed 128x160\n",
					(int)info_ptr->width, (int)info_ptr->height);
				break;
			}
			if (info_ptr->pixel_depth != 8)
			{
				lprintf(__FILE__ ": font image uses %ibpp, needed 8bpp\n", info_ptr->pixel_depth);
				break;
			}
			for (y = 0; y < 16; y++)
			{
				for (x = 0; x < 16; x++)
				{
					for (y1 = 0; y1 < 10; y1++)
					{
						unsigned char *src = row_ptr[y*10 + y1] + x*8;
						for (x1 = 8/2; x1 > 0; x1--, src+=2)
							*dst++ = ((src[0]^0xff) & 0xf0) | ((src[1]^0xff) >> 4);
					}
				}
			}
			break;
		}

		case READPNG_SELECTOR:
		{
			int x1, y1;
			unsigned char *dst = dest;
			if (info_ptr->width != 8 || info_ptr->height != 10)
			{
				lprintf(__FILE__ ": unexpected selector image size %ix%i, needed 8x10\n",
					(int)info_ptr->width, (int)info_ptr->height);
				break;
			}
			if (info_ptr->pixel_depth != 8)
			{
				lprintf(__FILE__ ": selector image uses %ibpp, needed 8bpp\n", info_ptr->pixel_depth);
				break;
			}
			for (y1 = 0; y1 < 10; y1++)
			{
				unsigned char *src = row_ptr[y1];
				for (x1 = 8/2; x1 > 0; x1--, src+=2)
					*dst++ = ((src[0]^0xff) & 0xf0) | ((src[1]^0xff) >> 4);
			}
			break;
		}
	}


done:
	png_destroy_read_struct(&png_ptr, info_ptr ? &info_ptr : NULL, (png_infopp)NULL);
	fclose(fp);
}


