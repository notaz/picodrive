#define SSP_TCACHE_SIZE         (512*1024)
#define SSP_BLOCKTAB_SIZE       (0x5090/2*4)
#define SSP_BLOCKTAB_IRAM_SIZE  (15*0x800/2*4)
#define SSP_BLOCKTAB_ALIGN_SIZE 3808
#define SSP_DRC_SIZE (SSP_TCACHE_SIZE + SSP_BLOCKTAB_SIZE + SSP_BLOCKTAB_IRAM_SIZE + SSP_BLOCKTAB_ALIGN_SIZE)

extern unsigned int tcache[SSP_TCACHE_SIZE/4];
extern unsigned int *ssp_block_table[SSP_BLOCKTAB_SIZE/4];
extern unsigned int *ssp_block_table_iram[15][0x800/2];

void drc_cmn_init(void);
void drc_cmn_cleanup(void);

