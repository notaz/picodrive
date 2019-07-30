int  sh2_drc_init(SH2 *sh2);
void sh2_drc_finish(SH2 *sh2);
void sh2_drc_wcheck_ram(unsigned int a, int val, SH2 *sh2);
void sh2_drc_wcheck_da(unsigned int a, int val, SH2 *sh2);

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
#define OF_LOOP       (3 << 2) // NONE, IDLE, DELAY, POLL loop
#define OF_B_IN_DS    (1 << 4)
#define OF_DELAY_INSN (1 << 5) // DT, (TODO ADD+CMP?)
#define OF_POLL_INSN  (1 << 6) // MOV @(...),Rn (no post increment), TST @(...)

#define OF_IDLE_LOOP  (1 << 2)
#define OF_DELAY_LOOP (2 << 2)
#define OF_POLL_LOOP  (3 << 2)

unsigned short scan_block(unsigned int base_pc, int is_slave,
		unsigned char *op_flags, unsigned int *end_pc,
		unsigned int *base_literals, unsigned int *end_literals);

#if defined(DRC_SH2)
// direct access to some host CPU registers used by the DRC
// XXX MUST match definitions in cpu/sh2/compiler.c
#if defined(__arm__)
#define	DRC_SR_REG	r10
#elif defined(__aarch64__)
#define	DRC_SR_REG	r22
#elif defined(__mips__)
#define	DRC_SR_REG	s6
#elif defined(__i386__)
#define	DRC_SR_REG	edi
#elif defined(__x86_64__)
#define	DRC_SR_REG	ebx
#else
#warning "direct DRC register access not available for this host"
#endif
#endif

#ifdef DRC_SR_REG
#define	__DRC_DECLARE_SR(SR)	register int sh2_sr asm(#SR)
#define	_DRC_DECLARE_SR(SR)	__DRC_DECLARE_SR(SR)
#define	DRC_DECLARE_SR	_DRC_DECLARE_SR(DRC_SR_REG)
#define DRC_SAVE_SR(sh2) \
    if ((sh2->state & (SH2_STATE_RUN|SH2_STATE_SLEEP)) == SH2_STATE_RUN) \
        sh2->sr = sh2_sr;
#define DRC_RESTORE_SR(sh2) \
    if ((sh2->state & (SH2_STATE_RUN|SH2_STATE_SLEEP)) == SH2_STATE_RUN) \
        sh2_sr = sh2->sr;
#else
#define	DRC_DECLARE_SR
#define DRC_SAVE_SR(sh2)
#define DRC_RESTORE_SR(sh2)
#endif
