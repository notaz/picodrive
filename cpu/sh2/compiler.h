int  sh2_drc_init(SH2 *sh2);
void sh2_drc_finish(SH2 *sh2);
void sh2_drc_wcheck_ram(uint32_t a, unsigned len, SH2 *sh2);
void sh2_drc_wcheck_da(uint32_t a, unsigned len, SH2 *sh2);

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
#define OF_BASIC_LOOP (1 << 7) // pinnable loop without any branches in it

#define OF_IDLE_LOOP  (1 << 2)
#define OF_DELAY_LOOP (2 << 2)
#define OF_POLL_LOOP  (3 << 2)

unsigned short scan_block(uint32_t base_pc, int is_slave,
		unsigned char *op_flags, uint32_t *end_pc,
		uint32_t *base_literals, uint32_t *end_literals);

#if defined(DRC_SH2) && defined(__GNUC__)
// direct access to some host CPU registers used by the DRC 
// XXX MUST match definitions for SHR_SR in cpu/drc/emit_*.c
#if defined(__arm__)
#define	DRC_SR_REG	"r10"
#elif defined(__aarch64__)
#define	DRC_SR_REG	"r28"
#elif defined(__mips__)
#define	DRC_SR_REG	"s6"
#elif defined(__riscv__) || defined(__riscv)
#define	DRC_SR_REG	"s11"
#elif defined(__i386__)
#define	DRC_SR_REG	"edi"
#elif defined(__x86_64__)
#define	DRC_SR_REG	"ebx"
#endif
#endif

#ifdef DRC_SR_REG
extern void REGPARM(1) (*sh2_drc_save_sr)(SH2 *sh2);
extern void REGPARM(1) (*sh2_drc_restore_sr)(SH2 *sh2);

#define	DRC_DECLARE_SR	register int32_t sh2_sr asm(DRC_SR_REG)
#define DRC_SAVE_SR(sh2) \
    if (likely((sh2->state & (SH2_STATE_RUN|SH2_STATE_SLEEP)) == SH2_STATE_RUN)) \
        sh2_drc_save_sr(sh2)
#define DRC_RESTORE_SR(sh2) \
    if (likely((sh2->state & (SH2_STATE_RUN|SH2_STATE_SLEEP)) == SH2_STATE_RUN)) \
        sh2_drc_restore_sr(sh2)
#else
#define	DRC_DECLARE_SR
#define DRC_SAVE_SR(sh2)
#define DRC_RESTORE_SR(sh2)
#endif
