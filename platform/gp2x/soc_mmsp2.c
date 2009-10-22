#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "soc.h"
#include "soc_mmsp2.h"
#include "plat_gp2x.h"
#include "../common/emu.h"
#include "../common/plat.h"
#include "../common/arm_utils.h"
#include "940ctl.h"

volatile unsigned short *gp2x_memregs;
volatile unsigned long  *gp2x_memregl;
extern void *gp2x_screens[4];
static int screensel = 0;

int memdev = -1;	/* used by code940 */
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

static char gamma_was_changed = 0;
static char cpuclk_was_changed = 0;
static unsigned short gp2x_screenaddr_old[4];
static unsigned short memtimex_old[2];
static unsigned short reg0910;

extern unsigned int plat_get_ticks_ms_good(void);
extern unsigned int plat_get_ticks_us_good(void);

/* video stuff */
static void gp2x_video_flip_(void)
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
static void gp2x_video_flip2_(void)
{
	unsigned short msw = (unsigned short)(gp2x_screenaddrs_use[screensel&1] >> 16);

  	gp2x_memregs[0x2910>>1] = msw;
  	gp2x_memregs[0x2914>>1] = msw;
	gp2x_memregs[0x290E>>1] = 0;
  	gp2x_memregs[0x2912>>1] = 0;

	// jump to other buffer:
	g_screen_ptr = gp2x_screens[++screensel&1];
}

static void gp2x_video_changemode_ll_(int bpp)
{
  	gp2x_memregs[0x28DA>>1] = (((bpp+1)/8)<<9)|0xAB; /*8/15/16/24bpp...*/
  	gp2x_memregs[0x290C>>1] = 320*((bpp+1)/8); /*line width in bytes*/
}

static void gp2x_video_setpalette_(int *pal, int len)
{
	unsigned short *g = (unsigned short *)pal;
	volatile unsigned short *memreg = &gp2x_memregs[0x295A>>1];

	gp2x_memregs[0x2958>>1] = 0;

	len *= 2;
	while (len--)
		*memreg = *g++;
}

static void gp2x_video_RGB_setscaling_(int ln_offs, int W, int H)
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

	if (gp2x_memregs[0x2800>>1]&0x100) //TV-Out
	{
		escalaw=489.0; // RGB Horiz TV (PAL, NTSC)
		if (gp2x_memregs[0x2818>>1]  == 287) //PAL
			escalah=274.0; // RGB Vert TV PAL
		else if (gp2x_memregs[0x2818>>1]  == 239) //NTSC
			escalah=331.0; // RGB Vert TV NTSC
	}

	// scale horizontal
	scalw = (unsigned short)((float)escalaw *(W/320.0));
	/* if there is no horizontal scaling, vertical doesn't work.
	 * Here is a nasty wrokaround... */
	if (H != 240 && W == 320) scalw--;
	gp2x_memregs[0x2906>>1]=scalw;
	// scale vertical
	gp2x_memregl[0x2908>>2]=(unsigned long)((float)escalah *bpp *(H/240.0));
}

static void gp2x_video_wait_vsync_(void)
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

/*
 * CPU clock
 * Fout = (m * Fin) / (p * 2^s)
 * m = MDIV+8, p = PDIV+2, s = SDIV
 *
 * m = (Fout * p * 2^s) / Fin
 */

#define SYS_CLK_FREQ 7372800

static void gp2x_set_cpuclk_(unsigned int mhz)
{
	unsigned int mdiv, pdiv, sdiv = 0;
	unsigned int v;
	int i;

	pdiv = 3;
	mdiv = (mhz * pdiv * 1000000) / SYS_CLK_FREQ;
	if (mdiv & ~0xff) {
		fprintf(stderr, "invalid cpuclk MHz: %u\n", mhz);
		return;
	}
	v = ((mdiv-8)<<8) | ((pdiv-2)<<2) | sdiv;
	gp2x_memregs[0x910>>1] = v;

	for (i = 0; i < 10000; i++)
		if (!(gp2x_memregs[0x902>>1] & 1))
			break;

	cpuclk_was_changed = 1;
}

/* RAM timings */
#define TIMING_CHECK(t, adj, mask) \
	t += adj; \
	if (t & ~mask) \
		goto bad

static void set_ram_timing_vals(int tCAS, int tRC, int tRAS, int tWR, int tMRD, int tRFC, int tRP, int tRCD)
{
	int i;
	TIMING_CHECK(tCAS, -2, 0x1);
	TIMING_CHECK(tRC,  -1, 0xf);
	TIMING_CHECK(tRAS, -1, 0xf);
	TIMING_CHECK(tWR,  -1, 0xf);
	TIMING_CHECK(tMRD, -1, 0xf);
	TIMING_CHECK(tRFC, -1, 0xf);
	TIMING_CHECK(tRP,  -1, 0xf);
	TIMING_CHECK(tRCD, -1, 0xf);

	/* get spend_cycles() into cache */
	spend_cycles(1);

	gp2x_memregs[0x3802>>1] = ((tMRD & 0xF) << 12) | ((tRFC & 0xF) << 8) | ((tRP & 0xF) << 4) | (tRCD & 0xF);
	gp2x_memregs[0x3804>>1] = 0x8000 | ((tCAS & 1) << 12) | ((tRC & 0xF) << 8) | ((tRAS & 0xF) << 4) | (tWR & 0xF);

	/* be sure we don't access the mem while it's being reprogrammed */
	spend_cycles(128*1024);
	for (i = 0; i < 8*1024; i++)
		if (!(gp2x_memregs[0x3804>>1] & 0x8000))
			break;

	printf("RAM timings set.\n");
	return;
bad:
	fprintf(stderr, "RAM timings invalid.\n");
}

static void set_ram_timings_(void)
{
	/* craigix: --cas 2 --trc 6 --tras 4 --twr 1 --tmrd 1 --trfc 1 --trp 2 --trcd 2 */
	set_ram_timing_vals(2, 6, 4, 1, 1, 1, 2, 2);
}

static void unset_ram_timings_(void)
{
	gp2x_memregs[0x3802>>1] = memtimex_old[0];
	gp2x_memregs[0x3804>>1] = memtimex_old[1] | 0x8000;
	printf("RAM timings reset to startup values.\n");
}

/* LCD refresh */
typedef struct
{
	unsigned short reg, valmask, val;
}
reg_setting;

/* 120.00 97/0/2/7|25/ 7/ 7/11/37 */
static const reg_setting lcd_rate_120[] =
{
	{ 0x0914, 0xffff, (97<<8)|(0<<2)|2 },	/* UPLLSETVREG */
	{ 0x0924, 0xff00, (2<<14)|(7<<8) },	/* DISPCSETREG */
	{ 0x281A, 0x00ff, 25 },			/* .HSWID(T2) */
	{ 0x281C, 0x00ff, 7 },			/* .HSSTR(T8) */
	{ 0x281E, 0x00ff, 7 },			/* .HSEND(T7) */
	{ 0x2822, 0x01ff, 11 },			/* .VSEND (T9) */
	{ 0x2826, 0x0ff0, 37<<4 },		/* .DESTR(T3) */
	{ 0, 0, 0 }
};

/* 100.00 96/0/2/7|29/25/53/15/37 */
static const reg_setting lcd_rate_100[] =
{
	{ 0x0914, 0xffff, (96<<8)|(0<<2)|2 },	/* UPLLSETVREG */
	{ 0x0924, 0xff00, (2<<14)|(7<<8) },	/* DISPCSETREG */
	{ 0x281A, 0x00ff, 29 },			/* .HSWID(T2) */
	{ 0x281C, 0x00ff, 25 },			/* .HSSTR(T8) */
	{ 0x281E, 0x00ff, 53 },			/* .HSEND(T7) */
	{ 0x2822, 0x01ff, 15 },			/* .VSEND (T9) */
	{ 0x2826, 0x0ff0, 37<<4 },		/* .DESTR(T3) */
	{ 0, 0, 0 }
};

static reg_setting lcd_rate_defaults[] =
{
	{ 0x0914, 0xffff, 0 },
	{ 0x0924, 0xff00, 0 },
	{ 0x281A, 0x00ff, 0 },
	{ 0x281C, 0x00ff, 0 },
	{ 0x281E, 0x00ff, 0 },
	{ 0x2822, 0x01ff, 0 },
	{ 0x2826, 0x0ff0, 0 },
	{ 0, 0, 0 }
};

static void get_reg_setting(reg_setting *set)
{
	for (; set->reg; set++)
	{
		unsigned short val = gp2x_memregs[set->reg >> 1];
		val &= set->valmask;
		set->val = val;
	}
}

static void set_reg_setting(const reg_setting *set)
{
	for (; set->reg; set++)
	{
		unsigned short val = gp2x_memregs[set->reg >> 1];
		val &= ~set->valmask;
		val |= set->val;
		gp2x_memregs[set->reg >> 1] = val;
	}
}

static void set_lcd_custom_rate_(int is_pal)
{
	if (gp2x_memregs[0x2800>>1] & 0x100) // tv-out
		return;

	printf("setting custom LCD refresh (%d Hz)... ", is_pal ? 100 : 120);
	fflush(stdout);

	set_reg_setting(is_pal ? lcd_rate_100 : lcd_rate_120);
	printf("done.\n");
}

static void unset_lcd_custom_rate_(void)
{
	printf("reset to prev LCD refresh.\n");
	set_reg_setting(lcd_rate_defaults);
}

static void set_lcd_gamma_(int g100, int A_SNs_curve)
{
	float gamma = (float) g100 / 100;
	int i;
	gamma = 1 / gamma;

	/* enable gamma */
	gp2x_memregs[0x2880>>1] &= ~(1<<12);

	gp2x_memregs[0x295C>>1] = 0;
	for (i = 0; i < 256; i++)
	{
		unsigned char g;
		unsigned short s;
		const unsigned short grey50=143, grey75=177, grey25=97;
		double blah;

		if (A_SNs_curve)
		{
			// The next formula is all about gaussian interpolation
			blah = ((  -128 * exp(-powf((float) i/64.0f + 2.0f , 2.0f))) +
				(   -64 * exp(-powf((float) i/64.0f + 1.0f , 2.0f))) +
				(grey25 * exp(-powf((float) i/64.0f - 1.0f , 2.0f))) +
				(grey50 * exp(-powf((float) i/64.0f - 2.0f , 2.0f))) +
				(grey75 * exp(-powf((float) i/64.0f - 3.0f , 2.0f))) +
				(   256 * exp(-powf((float) i/64.0f - 4.0f , 2.0f))) +
				(   320 * exp(-powf((float) i/64.0f - 5.0f , 2.0f))) +
				(   384 * exp(-powf((float) i/64.0f - 6.0f , 2.0f)))) / 1.772637;
			blah += 0.5;
		}
		else
		{
			blah = i;
		}

		g = (unsigned char)(255.0 * pow(blah/255.0, gamma));
		//printf("%d : %d\n", i, g);
		s = (g<<8) | g;
		gp2x_memregs[0x295E>>1]= s;
		gp2x_memregs[0x295E>>1]= g;
	}

	gamma_was_changed = 1;
}

static int gp2x_read_battery_(void)
{
	return -1; /* TODO? */
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

	/* save startup values: LCD refresh */
	get_reg_setting(lcd_rate_defaults);

	/* CPU and RAM timings */
	reg0910 = gp2x_memregs[0x0910>>1];
	memtimex_old[0] = gp2x_memregs[0x3802>>1];
	memtimex_old[1] = gp2x_memregs[0x3804>>1];

	/* touchscreen */
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

	/* code940 portion */
	sharedmem940_init();

	gp2x_video_flip = gp2x_video_flip_;
	gp2x_video_flip2 = gp2x_video_flip2_;
	gp2x_video_changemode_ll = gp2x_video_changemode_ll_;
	gp2x_video_setpalette = gp2x_video_setpalette_;
	gp2x_video_RGB_setscaling = gp2x_video_RGB_setscaling_;
	gp2x_video_wait_vsync = gp2x_video_wait_vsync_;

	gp2x_set_cpuclk = gp2x_set_cpuclk_;

	set_lcd_custom_rate = set_lcd_custom_rate_;
	unset_lcd_custom_rate = unset_lcd_custom_rate_;
	set_lcd_gamma = set_lcd_gamma_;

	set_ram_timings = set_ram_timings_;
	unset_ram_timings = unset_ram_timings_;
	gp2x_read_battery = gp2x_read_battery_;

	gp2x_get_ticks_ms = plat_get_ticks_ms_good;
	gp2x_get_ticks_us = plat_get_ticks_us_good;
}

void mmsp2_finish(void)
{
	reset940(1, 3);
	pause940(1);
	sharedmem940_finish();

	gp2x_video_RGB_setscaling_(0, 320, 240);
	gp2x_video_changemode_ll_(16);

	gp2x_memregs[0x290E>>1] = gp2x_screenaddr_old[0];
	gp2x_memregs[0x2910>>1] = gp2x_screenaddr_old[1];
	gp2x_memregs[0x2912>>1] = gp2x_screenaddr_old[2];
	gp2x_memregs[0x2914>>1] = gp2x_screenaddr_old[3];

	unset_lcd_custom_rate_();
	if (gamma_was_changed)
		set_lcd_gamma_(100, 0);
	unset_ram_timings_();
	if (cpuclk_was_changed)
		gp2x_memregs[0x910>>1] = reg0910;

	munmap(gp2x_screens[0], FRAMEBUFF_WHOLESIZE);
	munmap((void *)gp2x_memregs, 0x10000);
	close(memdev);
	if (touchdev >= 0)
		close(touchdev);
}

