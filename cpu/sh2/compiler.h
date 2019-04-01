int  sh2_drc_init(SH2 *sh2);
void sh2_drc_finish(SH2 *sh2);
void sh2_drc_wcheck_ram(unsigned int a, int val, int cpuid);
void sh2_drc_wcheck_da(unsigned int a, int val, int cpuid);

#ifdef DRC_SH2
void sh2_drc_mem_setup(SH2 *sh2);
void sh2_drc_flush_all(void);
void sh2_drc_frame(void);
#else
#define sh2_drc_mem_setup(x)
#define sh2_drc_flush_all()
#define sh2_drc_frame()
#endif

#define BLOCK_INSN_LIMIT 1024

/* op_flags */
#define OF_DELAY_OP   (1 << 0)
#define OF_BTARGET    (1 << 1)
#define OF_T_SET      (1 << 2) // T is known to be set
#define OF_T_CLEAR    (1 << 3) // ... clear
#define OF_B_IN_DS    (1 << 4)

void scan_block(unsigned int base_pc, int is_slave,
		unsigned char *op_flags, unsigned int *end_pc,
		unsigned int *end_literals);

#if defined(DRC_SH2)
// direct access to some host CPU registers used by the DRC
// XXX MUST match definitions in cpu/sh2/compiler.c
#if defined(__arm__)
#define	DRC_SR_REG	r10
#elif defined(__i386__)
#define	DRC_SR_REG	edi
#elif defined(__x86_64__)
#define	DRC_SR_REG	ebx
#else
#warning "direct DRC register access not available for this host"
#endif

#ifdef DCR_SR_REG
#define	DRC_DECLARE_SR	register int sh2_sr asm(#DCR_SR_REG)
#define DRC_SAVE_SR(sh2) \
    if ((sh2->state & (SH2_STATE_RUN|SH2_STATE_BUSY)) == SH2_STATE_RUN) \
        sh2->sr = sh2_sr;
#define DRC_RESTORE_SR(sh2) \
    if ((sh2->state & (SH2_STATE_RUN|SH2_STATE_BUSY)) == SH2_STATE_RUN) \
        sh2_sr = sh2->sr;
#else
#define	DRC_DECLARE_SR
#define DRC_SAVE_SR(sh2)
#define DRC_RESTORE_SR(sh2)
#endif
#endif
