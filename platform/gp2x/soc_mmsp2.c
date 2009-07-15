#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "soc.h"
#include "soc_mmsp2.h"
#include "gp2x.h"
#include "../common/emu.h"
#include "../common/arm_utils.h"

volatile unsigned short *gp2x_memregs;
volatile unsigned long  *gp2x_memregl;
extern void *gp2x_screens[4];
static int screensel = 0;

int memdev = 0;	/* used by code940 */
static int touchdev = -1;
static int touchcal[7] = { 6203, 0, -1501397, 0, -4200, 16132680, 65536 };

#define FRAMEBUFF_SIZE  0x30000
#define FRAMEBUFF_WHOLESIZE (FRAMEBUFF_SIZE*4) // 320*240*2 + some more
#define FRAMEBUFF_ADDR0 (0x4000000 - FRAMEBUFF_WHOLESIZE)
#define FRAMEBUFF_ADDR1 (FRAMEBUFF_ADDR0 + FRAMEBUFF_SIZE)
#define FRAMEBUFF_ADDR2 (FRAMEBUFF_ADDR1 + FRAMEBUFF_SIZE)
#define FRAMEBUFF_ADDR3 (FRAMEBUFF_ADDR2 + FRAMEBUFF_SIZE)

static const int gp2x_screenaddrs[4] = { FRAMEBUFF_ADDR0, FRAMEBUFF_ADDR1, FRAMEBUFF_ADDR2, FRAMEBUFF_ADDR3 };
static int gp2x_screenaddrs_use[4];
static unsigned short gp2x_screenaddr_old[4];


/* video stuff */
void gp2x_video_flip(void)
{
	unsigned short lsw = (unsigned short) gp2x_screenaddrs_use[screensel&3];
	unsigned short msw = (unsigned short)(gp2x_screenaddrs_use[screensel&3] >> 16);

  	gp2x_memregs[0x2910>>1] = msw;
  	gp2x_memregs[0x2914>>1] = msw;
	gp2x_memregs[0x290E>>1] = lsw;
  	gp2x_memregs[0x2912>>1] = lsw;

	// jump to other buffer:
	g_screen_ptr = gp2x_screens[++screensel&3];
}

/* doulblebuffered flip */
void gp2x_video_flip2(void)
{
	unsigned short msw = (unsigned short)(gp2x_screenaddrs_use[screensel&1] >> 16);

  	gp2x_memregs[0x2910>>1] = msw;
  	gp2x_memregs[0x2914>>1] = msw;
	gp2x_memregs[0x290E>>1] = 0;
  	gp2x_memregs[0x2912>>1] = 0;

	// jump to other buffer:
	g_screen_ptr = gp2x_screens[++screensel&1];
}

void gp2x_video_changemode_ll(int bpp)
{
  	gp2x_memregs[0x28DA>>1] = (((bpp+1)/8)<<9)|0xAB; /*8/15/16/24bpp...*/
  	gp2x_memregs[0x290C>>1] = 320*((bpp+1)/8); /*line width in bytes*/
}

void gp2x_video_setpalette(int *pal, int len)
{
	unsigned short *g = (unsigned short *)pal;
	volatile unsigned short *memreg = &gp2x_memregs[0x295A>>1];

	gp2x_memregs[0x2958>>1] = 0;

	len *= 2;
	while (len--)
		*memreg = *g++;
}

// TV Compatible function //
void gp2x_video_RGB_setscaling(int ln_offs, int W, int H)
{
	float escalaw, escalah;
	int bpp = (gp2x_memregs[0x28DA>>1]>>9)&0x3;
	unsigned short scalw;

	// set offset
	gp2x_screenaddrs_use[0] = gp2x_screenaddrs[0] + ln_offs * 320 * bpp;
	gp2x_screenaddrs_use[1] = gp2x_screenaddrs[1] + ln_offs * 320 * bpp;
	gp2x_screenaddrs_use[2] = gp2x_screenaddrs[2] + ln_offs * 320 * bpp;
	gp2x_screenaddrs_use[3] = gp2x_screenaddrs[3] + ln_offs * 320 * bpp;

	escalaw = 1024.0; // RGB Horiz LCD
	escalah = 320.0; // RGB Vert LCD

	if(gp2x_memregs[0x2800>>1]&0x100) //TV-Out
	{
		escalaw=489.0; // RGB Horiz TV (PAL, NTSC)
		if (gp2x_memregs[0x2818>>1]  == 287) //PAL
			escalah=274.0; // RGB Vert TV PAL
		else if (gp2x_memregs[0x2818>>1]  == 239) //NTSC
			escalah=331.0; // RGB Vert TV NTSC
	}

	// scale horizontal
	scalw = (unsigned short)((float)escalaw *(W/320.0));
	/* if there is no horizontal scaling, vertical doesn't work. Here is a nasty wrokaround... */
	if (H != 240 && W == 320) scalw--;
	gp2x_memregs[0x2906>>1]=scalw;
	// scale vertical
	gp2x_memregl[0x2908>>2]=(unsigned long)((float)escalah *bpp *(H/240.0));
}

void gp2x_video_wait_vsync(void)
{
	unsigned short v = gp2x_memregs[0x1182>>1];
	while (!((v ^ gp2x_memregs[0x1182>>1]) & 0x10))
		spend_cycles(1024);
}

/* 940 */
void pause940(int yes)
{
	if (yes)
		gp2x_memregs[0x0904>>1] &= 0xFFFE;
	else
		gp2x_memregs[0x0904>>1] |= 1;
}

void reset940(int yes, int bank)
{
	gp2x_memregs[0x3B48>>1] = ((yes&1) << 7) | (bank & 0x03);
}


/* these are not quite MMSP2 related,
 * more to GP2X F100/F200 consoles themselves. */
typedef struct ucb1x00_ts_event
{
	unsigned short pressure;
	unsigned short x;
	unsigned short y;
	unsigned short pad;
	struct timeval stamp;
} UCB1X00_TS_EVENT;

int gp2x_touchpad_read(int *x, int *y)
{
	UCB1X00_TS_EVENT event;
	static int zero_seen = 0;
	int retval;

	if (touchdev < 0) return -1;

	retval = read(touchdev, &event, sizeof(event));
	if (retval <= 0) {
		perror("touch read failed");
		return -1;
	}
	// this is to ignore the messed-up 4.1.x driver
	if (event.pressure == 0) zero_seen = 1;

	if (x) *x = (event.x * touchcal[0] + touchcal[2]) >> 16;
	if (y) *y = (event.y * touchcal[4] + touchcal[5]) >> 16;
	// printf("read %i %i %i\n", event.pressure, *x, *y);

	return zero_seen ? event.pressure : 0;
}

static void proc_set(const char *path, const char *val)
{
	FILE *f;
	char tmp[16];

	f = fopen(path, "w");
	if (f == NULL) {
		printf("failed to open: %s\n", path);
		return;
	}

	fprintf(f, "0\n");
	fclose(f);

	printf("\"%s\" is set to: ", path);
	f = fopen(path, "r");
	if (f == NULL) {
		printf("(open failed)\n");
		return;
	}

	fgets(tmp, sizeof(tmp), f);
	printf("%s", tmp);
	fclose(f);
}


void mmsp2_init(void)
{
	int i;

  	memdev = open("/dev/mem", O_RDWR);
	if (memdev == -1)
	{
		perror("open(\"/dev/mem\")");
		exit(1);
	}

	gp2x_memregs = mmap(0, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);
	if (gp2x_memregs == MAP_FAILED)
	{
		perror("mmap(memregs)");
		exit(1);
	}
	gp2x_memregl = (unsigned long *) gp2x_memregs;

	gp2x_memregs[0x2880>>1] &= ~0x383; // disable cursor, subpict, osd, video layers

  	gp2x_screens[0] = mmap(0, FRAMEBUFF_WHOLESIZE, PROT_WRITE, MAP_SHARED,
		memdev, gp2x_screenaddrs[0]);
	if (gp2x_screens[0] == MAP_FAILED)
	{
		perror("mmap(g_screen_ptr)");
		exit(1);
	}
	printf("framebuffers:\n");
	printf("  %08x -> %p\n", gp2x_screenaddrs[0], gp2x_screens[0]);
	for (i = 1; i < 4; i++)
	{
		gp2x_screens[i] = (char *) gp2x_screens[i - 1] + FRAMEBUFF_SIZE;
		printf("  %08x -> %p\n", gp2x_screenaddrs[i], gp2x_screens[i]);
	}

	g_screen_ptr = gp2x_screens[0];
	screensel = 0;

	gp2x_screenaddr_old[0] = gp2x_memregs[0x290E>>1];
	gp2x_screenaddr_old[1] = gp2x_memregs[0x2910>>1];
	gp2x_screenaddr_old[2] = gp2x_memregs[0x2912>>1];
	gp2x_screenaddr_old[3] = gp2x_memregs[0x2914>>1];

	memcpy(gp2x_screenaddrs_use, gp2x_screenaddrs, sizeof(gp2x_screenaddrs));

	// touchscreen
	touchdev = open("/dev/touchscreen/wm97xx", O_RDONLY);
	if (touchdev >= 0) {
		FILE *pcf = fopen("/etc/pointercal", "r");
		if (pcf) {
			fscanf(pcf, "%d %d %d %d %d %d %d", &touchcal[0], &touchcal[1],
				&touchcal[2], &touchcal[3], &touchcal[4], &touchcal[5], &touchcal[6]);
			fclose(pcf);
		}
		printf("found touchscreen/wm97xx\n");
	}

	/* disable Linux read-ahead */
	proc_set("/proc/sys/vm/max-readahead", "0\n");
	proc_set("/proc/sys/vm/min-readahead", "0\n");
}

void mmsp2_finish(void)
{
	reset940(1, 3);
	pause940(1);

	gp2x_memregs[0x290E>>1] = gp2x_screenaddr_old[0];
	gp2x_memregs[0x2910>>1] = gp2x_screenaddr_old[1];
	gp2x_memregs[0x2912>>1] = gp2x_screenaddr_old[2];
	gp2x_memregs[0x2914>>1] = gp2x_screenaddr_old[3];

	munmap(gp2x_screens[0], FRAMEBUFF_WHOLESIZE);
	munmap((void *)gp2x_memregs, 0x10000);
	close(memdev);
	if (touchdev >= 0)
		close(touchdev);
}

