typedef enum {
	SOCID_MMSP2 = 1,
	SOCID_POLLUX,
} gp2x_soc_t;

extern gp2x_soc_t gp2x_soc;

gp2x_soc_t soc_detect(void);

void mmsp2_init(void);
void mmsp2_finish(void);

void pollux_init(void);
void pollux_finish(void);

void gp2x_video_flip(void);
void gp2x_video_flip2(void);
void gp2x_video_changemode_ll(int bpp);
void gp2x_video_setpalette(int *pal, int len);
void gp2x_video_RGB_setscaling(int ln_offs, int W, int H);
void gp2x_video_wait_vsync(void);
