#define TCACHE_SIZE (1024*1024)

extern unsigned int tcache[];

int  ssp_drc_entry(int cycles);
void ssp_drc_next(void);
void ssp_drc_next_patch(void);
void ssp_drc_end(void);

void ssp_hle_800(void);
void ssp_hle_902(void);

int  ssp1601_dyn_startup(void);
void ssp1601_dyn_reset(ssp1601_t *ssp);
void ssp1601_dyn_run(int cycles);

