#ifndef __CPUCTRL_H__
#define __CPUCTRL_H__

extern void cpuctrl_init(void); /* call this at first */
extern void save_system_regs(void); /* save some registers */
extern void cpuctrl_deinit(void);
extern void set_display_clock_div(unsigned div);
extern void set_FCLK(unsigned MHZ); /* adjust the clock frequency (in Mhz units) */
extern void set_920_Div(unsigned short div); /* 0 to 7 divider (freq=FCLK/(1+div)) */
extern void set_DCLK_Div(unsigned short div); /* 0 to 7 divider (freq=FCLK/(1+div)) */
//extern void Disable_940(void); /* 940t down */

extern void set_RAM_Timings(int tRC, int tRAS, int tWR, int tMRD, int tRFC, int tRP, int tRCD);
extern void set_gamma(int g100);

typedef enum
{
	LCDR_60 = 0,	/* ~59.998Hz, has interlacing problems, kills USB host */
	LCDR_50,	/* 50Hz, has interlacing problems, kills USB host */
	LCDR_120_20,	/* ~60.10*2Hz, used by FCE Ultra */
	LCDR_100_02,	/* ~50.01*2Hz, used by FCE Ultra */
	LCDR_120,	/* 120Hz */
	LCDR_100,	/* 100Hz */
} lcd_rate_t;

extern void set_LCD_custom_rate(lcd_rate_t rate);
extern void unset_LCD_custom_rate(void);

#endif
