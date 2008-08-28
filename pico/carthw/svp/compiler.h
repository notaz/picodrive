#define SSP_TCACHE_SIZE         (512*1024)
#define SSP_BLOCKTAB_SIZE       (0x5090/2*4)
#define SSP_BLOCKTAB_IRAM_SIZE  (15*0x800/2*4)
#define SSP_BLOCKTAB_ALIGN_SIZE 3808
#define SSP_DRC_SIZE (SSP_TCACHE_SIZE + SSP_BLOCKTAB_SIZE + SSP_BLOCKTAB_IRAM_SIZE + SSP_BLOCKTAB_ALIGN_SIZE)

extern unsigned int tcache[SSP_TCACHE_SIZE/4];
extern unsigned int *ssp_block_table[0x5090/2];
extern unsigned int *ssp_block_table_iram[15][0x800/2];

int  ssp_drc_entry(int cycles);
void ssp_drc_next(void);
void ssp_drc_next_patch(void);
void ssp_drc_end(void);

void ssp_hle_800(void);
void ssp_hle_902(void);
void ssp_hle_07_6d6(void);
void ssp_hle_07_030(void);
void ssp_hle_07_036(void);
void ssp_hle_11_12c(void);
void ssp_hle_11_384(void);
void ssp_hle_11_38a(void);

int  ssp1601_dyn_startup(void);
void ssp1601_dyn_reset(ssp1601_t *ssp);
void ssp1601_dyn_run(int cycles);

