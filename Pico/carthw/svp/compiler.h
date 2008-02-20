#define TCACHE_SIZE (1024*1024)

extern unsigned int tcache[];

void regfile_load(void);
void regfile_store(void);

int  ssp1601_dyn_startup(void);
void ssp1601_dyn_reset(ssp1601_t *ssp);
void ssp1601_dyn_run(int cycles);

