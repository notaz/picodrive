#include <pspctrl.h>

void psp_init(void);
void psp_finish(void);

void psp_msleep(int ms);

// vram usage map:
// 000000-044000 fb0
// 044000-088000 fb1
// 088000-0cc000 depth (?)
// 0cc000-126000 emu draw buffers: 512*240 + 512*240*2

#define VRAMOFFS_FB0    0x00000000
#define VRAMOFFS_FB1    0x00044000
#define VRAMOFFS_DEPTH  0x00088000
#define VRAMOFFS_STUFF  0x000cc000

#define VRAM_FB0        ((void *) (0x44000000+VRAMOFFS_FB0))
#define VRAM_FB1        ((void *) (0x44000000+VRAMOFFS_FB1))
#define VRAM_STUFF      ((void *) (0x44000000+VRAMOFFS_STUFF))

#define VRAM_CACHED_STUFF   ((void *) (0x04000000+VRAMOFFS_STUFF))

#define GU_CMDLIST_SIZE (16*1024)

extern unsigned int guCmdList[GU_CMDLIST_SIZE];

void *psp_video_get_active_fb(void);
void  psp_video_switch_to_single(void);
void  psp_video_flip(int wait_vsync);
extern void *psp_screen;

unsigned int psp_pad_read(int blocking);

int psp_get_cpu_clock(void);
int psp_set_cpu_clock(int clock);

char *psp_get_status_line(void);

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
#define BTN_NOTE     PSP_CTRL_NOTE // doesn't seem to work?

/* fake 'nub' btns */
#define BTN_NUB_UP    (1 << 28)
#define BTN_NUB_RIGHT (1 << 29)
#define BTN_NUB_DOWN  (1 << 30)
#define BTN_NUB_LEFT  (1 << 31)

