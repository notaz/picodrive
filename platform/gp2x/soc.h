typedef enum {
	SOCID_MMSP2 = 1,
	SOCID_POLLUX,
} gp2x_soc_t;

gp2x_soc_t soc_detect(void);

void mmsp2_init(void);
void mmsp2_finish(void);

void pollux_init(void);
void pollux_finish(void);

/* SoC specific functions */
void gp2x_video_flip(void);
void gp2x_video_flip2(void);
void gp2x_video_changemode_ll(int bpp);
void gp2x_video_setpalette(int *pal, int len);
void gp2x_video_RGB_setscaling(int ln_offs, int W, int H);
void gp2x_video_wait_vsync(void);

void gp2x_set_cpuclk(unsigned int mhz);

void set_lcd_custom_rate(int is_pal);
void unset_lcd_custom_rate(void);
void set_lcd_gamma(int g100, int A_SNs_curve);

void set_ram_timings(int tCAS, int tRC, int tRAS, int tWR, int tMRD, int tRFC, int tRP, int tRCD);
void unset_ram_timings(void);

