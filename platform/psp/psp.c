#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>

#include "psp.h"
#include "../common/lprintf.h"

PSP_MODULE_INFO("PicoDrive", 0, 1, 34);

void *psp_screen = PSP_VRAM_BASE0;
static int current_screen = 0; /* front bufer */


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

void psp_video_flip(void)
{
	sceDisplaySetFrameBuf(psp_screen, 512, PSP_DISPLAY_PIXEL_FORMAT_565, PSP_DISPLAY_SETBUF_NEXTFRAME);
	current_screen ^= 1;
	psp_screen = current_screen ? PSP_VRAM_BASE1 : PSP_VRAM_BASE0;
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

unsigned int psp_pad_read(void)
{
	SceCtrlData pad;
	sceCtrlReadBufferPositive(&pad, 1);

	return pad.Buttons;
}

