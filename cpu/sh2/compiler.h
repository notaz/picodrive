int  sh2_drc_init(SH2 *sh2);
void sh2_drc_finish(SH2 *sh2);
void sh2_drc_mem_setup(SH2 *sh2);
void sh2_drc_flush_all(void);
void sh2_drc_wcheck_ram(unsigned int a, int val, int cpuid);
void sh2_drc_wcheck_da(unsigned int a, int val, int cpuid);

