#define TCACHE_SIZE (1024*1024)

extern unsigned int tcache[];

void ssp_regfile_load(void);
void ssp_regfile_store(void);
int  ssp_drc_entry(int cycles);
void ssp_drc_next(void);

void ssp_hle_800(void);

int  ssp1601_dyn_startup(void);
void ssp1601_dyn_reset(ssp1601_t *ssp);
void ssp1601_dyn_run(int cycles);

