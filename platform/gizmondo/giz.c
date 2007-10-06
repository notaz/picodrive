#include <windows.h>
#include <stdio.h>
#include <version.h>

#include "giz.h"
#include "kgsdk/Framework.h"
#include "kgsdk/Framework2D.h"

#define LOG_FILE "log.log"

void *giz_screen = NULL;
static FILE *logf = NULL;

void lprintf_al(const char *fmt, ...)
{
	va_list vl;

	if (logf == NULL)
	{
		logf = fopen(LOG_FILE, "w");
		if (logf == NULL)
			return;
	}

	//if (strchr(fmt, '\n'))
	//	fprintf(logf, "%lu: ", GetTickCount());
	va_start(vl, fmt);
	vfprintf(logf, fmt, vl);
	va_end(vl);
	fflush(logf);
}

static void giz_log_close(void)
{
	if (logf != NULL)
	{
		fclose(logf);
		logf = NULL;
	}
}

void giz_init(HINSTANCE hInstance, HINSTANCE hPrevInstance)
{
	int ret;

	lprintf("PicoDrive v" VERSION " (c) notaz, 2006,2007\n");
	lprintf("%s %s\n\n", __DATE__, __TIME__);

	ret = Framework_Init(hInstance, hPrevInstance);
	if (!ret)
	{
		lprintf_al("Framework_Init() failed\n");
		exit(1);
	}
	ret = Framework2D_Init();
	if (!ret)
	{
		lprintf_al("Framework2D_Init() failed\n");
		exit(1);
	}

	// test screen
	giz_screen = Framework2D_LockBuffer(1);
	if (giz_screen == NULL)
	{
		lprintf_al("Framework2D_LockBuffer() failed\n");
		exit(1);
	}
	lprintf("Framework2D_LockBuffer returned %p\n", giz_screen);
	Framework2D_UnlockBuffer();
	giz_screen = NULL;
}

void giz_deinit(void)
{
	Framework2D_Close();
	Framework_Close();

	giz_log_close();
}

