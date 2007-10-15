#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspdisplay.h>
#include <pspgu.h>

#include "psp.h"
#include "../common/lprintf.h"

PSP_MODULE_INFO("PicoDrive", 0, 1, 34);

void *psp_screen = PSP_VRAM_BASE0;
static int current_screen = 0; /* front bufer */

static SceUID logfd = -1;

/* Exit callback */
static int exit_callback(int arg1, int arg2, void *common)
{
	sceKernelExitGame();
	return 0;
}

/* Callback thread */
static int callback_thread(SceSize args, void *argp)
{
	int cbid;

	lprintf("callback_thread started with id %i\n", sceKernelGetThreadId());

	cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);

	sceKernelSleepThreadCB();

	return 0;
}

void psp_init(void)
{
	int thid;

	lprintf("entered psp_init, threadId %i\n", sceKernelGetThreadId());

	thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
	if (thid >= 0)
	{
		sceKernelStartThread(thid, 0, 0);
	}

	/* video */
	sceDisplaySetMode(0, 480, 272);
	sceDisplaySetFrameBuf(PSP_VRAM_BASE1, 512, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
	current_screen = 1;
	psp_screen = PSP_VRAM_BASE0;

	/* gu */
	sceGuInit();

	/* input */
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(0);
}

void psp_finish(void)
{
	sceGuTerm();

	//sceKernelSleepThread();
	sceKernelExitGame();
}

void psp_video_flip(int wait_vsync)
{
	if (wait_vsync) sceDisplayWaitVblankStart();
	sceDisplaySetFrameBuf(psp_screen, 512, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
	current_screen ^= 1;
	psp_screen = current_screen ? PSP_VRAM_BASE0 : PSP_VRAM_BASE1;
}

void *psp_video_get_active_fb(void)
{
	return current_screen ? PSP_VRAM_BASE1 : PSP_VRAM_BASE0;
}

void psp_video_switch_to_single(void)
{
	psp_screen = PSP_VRAM_BASE0;
	sceDisplaySetFrameBuf(psp_screen, 512, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
	current_screen = 0;
}

void psp_msleep(int ms)
{
	sceKernelDelayThread(ms * 1000);
}

unsigned int psp_pad_read(int blocking)
{
	SceCtrlData pad;
	if (blocking)
	     sceCtrlReadBufferPositive(&pad, 1);
	else sceCtrlPeekBufferPositive(&pad, 1);

	return pad.Buttons;
}

/* alt logging */
#define LOG_FILE "log.log"

void lprintf_f(const char *fmt, ...)
{
	va_list vl;
	char buff[256];

	if (logfd < 0)
	{
		logfd = sceIoOpen(LOG_FILE, PSP_O_WRONLY|PSP_O_APPEND, 0777);
		if (logfd < 0)
			return;
	}

	va_start(vl, fmt);
	vsnprintf(buff, sizeof(buff), fmt, vl);
	va_end(vl);

	sceIoWrite(logfd, buff, strlen(buff));
//sceKernelDelayThread(200 * 1000);
sceIoClose(logfd);
logfd = -1;
}


