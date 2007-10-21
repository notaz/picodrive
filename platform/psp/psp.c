#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspdisplay.h>
#include <psppower.h>
#include <pspgu.h>

#include "psp.h"
#include "../common/lprintf.h"

PSP_MODULE_INFO("PicoDrive", 0, 1, 34);

unsigned int __attribute__((aligned(16))) guCmdList[GU_CMDLIST_SIZE];

void *psp_screen = VRAM_FB0;
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
	sceDisplaySetFrameBuf(VRAM_FB1, 512, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
	current_screen = 1;
	psp_screen = VRAM_FB0;

	/* gu */
	sceGuInit();

	sceGuStart(GU_DIRECT, guCmdList);
	sceGuDrawBuffer(GU_PSM_5650, VRAMOFFS_FB0, 512); // point to back fb?
	sceGuDispBuffer(480, 272, VRAMOFFS_FB1, 512);
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	sceGuDepthBuffer(VRAMOFFS_DEPTH, 512);
	sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
	sceGuViewport(2048, 2048, 480, 272);
	sceGuDepthRange(0xc350, 0x2710);
	sceGuScissor(0, 0, 480, 272);
	sceGuEnable(GU_SCISSOR_TEST);
//	sceGuAlphaFunc(GU_GREATER, 0, 0xff);
//	sceGuEnable(GU_ALPHA_TEST);
//	sceGuDepthFunc(GU_ALWAYS); // GU_GEQUAL);
//	sceGuEnable(GU_DEPTH_TEST);

	sceGuDepthMask(0xffff);
	sceGuDisable(GU_DEPTH_TEST);

	sceGuFrontFace(GU_CW);
//	sceGuShadeModel(GU_SMOOTH);
//	sceGuEnable(GU_CULL_FACE);
	sceGuEnable(GU_TEXTURE_2D);
//	sceGuEnable(GU_CLIP_PLANES);
	sceGuTexMode(GU_PSM_5650, 0, 0, 0);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuTexFilter(GU_NEAREST, GU_NEAREST);
//	sceGuAmbientColor(0xffffffff);
//	sceGuEnable(GU_BLEND);
//	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuFinish();
	sceGuSync(0, 0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);


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
	sceDisplaySetFrameBuf(psp_screen, 512, PSP_DISPLAY_PIXEL_FORMAT_565,
		wait_vsync ? PSP_DISPLAY_SETBUF_IMMEDIATE : PSP_DISPLAY_SETBUF_NEXTFRAME);
	current_screen ^= 1;
	psp_screen = current_screen ? VRAM_FB0 : VRAM_FB1;
}

void *psp_video_get_active_fb(void)
{
	return current_screen ? VRAM_FB1 : VRAM_FB0;
}

void psp_video_switch_to_single(void)
{
	psp_screen = VRAM_FB0;
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

int psp_get_cpu_clock(void)
{
	return scePowerGetCpuClockFrequencyInt();
}

int psp_set_cpu_clock(int clock)
{
	int ret = scePowerSetClockFrequency(clock, clock, clock/2);
	if (ret != 0) lprintf("failed to set clock: %i\n", ret);

	return ret;
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


