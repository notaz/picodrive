#include <pspctrl.h>

void psp_init(void);
void psp_finish(void);

void psp_msleep(int ms);

#define PSP_VRAM_BASE0 ((void *) 0x44000000)
#define PSP_VRAM_BASE1 ((void *) 0x44044000)

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

