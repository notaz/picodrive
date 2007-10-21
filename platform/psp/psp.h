#include <pspctrl.h>

void psp_init(void);
void psp_finish(void);

void psp_msleep(int ms);

// vram usage map:
// 000000-044000 fb0
// 044000-088000 fb1
// 088000-0cc000 depth (?)
// 0cc000-0??000 stuff

#define VRAMOFFS_FB0    ((void *) 0x00000000)
#define VRAMOFFS_FB1    ((void *) 0x00044000)
#define VRAMOFFS_DEPTH  ((void *) 0x00088000)

#define VRAM_FB0        ((void *) 0x44000000)
#define VRAM_FB1        ((void *) 0x44044000)
#define VRAM_STUFF      ((void *) 0x440cc000)

#define VRAM_CACHED_STUFF   ((void *) 0x040cc000)

#define GU_CMDLIST_SIZE (16*1024) // TODO: adjust

extern unsigned int guCmdList[GU_CMDLIST_SIZE];

void *psp_video_get_active_fb(void);
void  psp_video_switch_to_single(void);
void  psp_video_flip(int wait_vsync);
extern void *psp_screen;

unsigned int psp_pad_read(int blocking);

int psp_get_cpu_clock(void);
int psp_set_cpu_clock(int clock);

/* shorter btn names */
#define BTN_UP       PSP_CTRL_UP
#define BTN_LEFT     PSP_CTRL_LEFT
#define BTN_RIGHT    PSP_CTRL_RIGHT
#define BTN_DOWN     PSP_CTRL_DOWN
#define BTN_L        PSP_CTRL_LTRIGGER
#define BTN_R        PSP_CTRL_RTRIGGER
#define BTN_TRIANGLE PSP_CTRL_TRIANGLE
#define BTN_CIRCLE   PSP_CTRL_CIRCLE
#define BTN_X        PSP_CTRL_CROSS
#define BTN_SQUARE   PSP_CTRL_SQUARE
#define BTN_SELECT   PSP_CTRL_SELECT
#define BTN_START    PSP_CTRL_START
#define BTN_NOTE     PSP_CTRL_NOTE

