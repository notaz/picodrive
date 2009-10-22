typedef enum {
	SOCID_MMSP2 = 1,
	SOCID_POLLUX,
} gp2x_soc_t;

gp2x_soc_t soc_detect(void);

void mmsp2_init(void);
void mmsp2_finish(void);

void pollux_init(void);
void pollux_finish(void);

void dummy_init(void);
void dummy_finish(void);

/* SoC specific functions */
extern void (*gp2x_video_flip)(void);
extern void (*gp2x_video_flip2)(void);
/* negative bpp means rotated mode (for Wiz) */
extern void (*gp2x_video_changemode_ll)(int bpp);
extern void (*gp2x_video_setpalette)(int *pal, int len);
extern void (*gp2x_video_RGB_setscaling)(int ln_offs, int W, int H);
extern void (*gp2x_video_wait_vsync)(void);

extern void (*gp2x_set_cpuclk)(unsigned int mhz);

extern void (*set_lcd_custom_rate)(int is_pal);
extern void (*unset_lcd_custom_rate)(void);
extern void (*set_lcd_gamma)(int g100, int A_SNs_curve);

extern void (*set_ram_timings)(void);
extern void (*unset_ram_timings)(void);
extern int  (*gp2x_read_battery)(void);

/* gettimeofday is not suitable for Wiz, at least fw 1.1 or lower */
extern unsigned int (*gp2x_get_ticks_ms)(void);
extern unsigned int (*gp2x_get_ticks_us)(void);
