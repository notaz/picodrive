int  sh2_drc_init(SH2 *sh2);
void sh2_drc_finish(SH2 *sh2);
void sh2_drc_mem_setup(SH2 *sh2);
void sh2_drc_flush_all(void);
void sh2_drc_wcheck_ram(unsigned int a, int val, int cpuid);
void sh2_drc_wcheck_da(unsigned int a, int val, int cpuid);

#define BLOCK_CYCLE_LIMIT 128

#define OP_FLAGS(pc) op_flags[((pc) - (base_pc)) / 2]
#define OF_DELAY_OP (1 << 0)
#define OF_TARGET   (1 << 1)

void scan_block(unsigned int base_pc, int is_slave,
		unsigned char *op_flags, unsigned int *end_pc);
