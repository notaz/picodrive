@ vim:filetype=armasm

.if 0
#include "cmn.h"
.endif

.global tcache
.global ssp_block_table
.global ssp_block_table_iram

@ translation cache buffer + pointer table
.data
.align 12 @ 4096
@.size tcache, SSP_TCACHE_SIZE
@.size ssp_block_table, SSP_BLOCKTAB_SIZE
@.size ssp_block_table_iram, SSP_BLOCKTAB_IRAM_SIZE
tcache:
 .space SSP_TCACHE_SIZE
ssp_block_table:
 .space SSP_BLOCKTAB_SIZE
ssp_block_table_iram:
 .space SSP_BLOCKTAB_IRAM_SIZE
 .space SSP_BLOCKTAB_ALIGN_SIZE


