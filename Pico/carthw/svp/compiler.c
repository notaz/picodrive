// 187 blocks, 12072 bytes
// 14 IRAM blocks

#include "../../PicoInt.h"
#include "compiler.h"

static unsigned int *block_table[0x5090/2];
static unsigned int *block_table_iram[15][0x800/2];
static unsigned int *tcache_ptr = NULL;

static int nblocks = 0;
static int iram_context = 0;

#ifndef ARM
#define DUMP_BLOCK 0x341e
unsigned int tcache[512*1024];
void regfile_load(void){}
void regfile_store(void){}
#endif

#define EMBED_INTERPRETER
#define ssp1601_reset ssp1601_reset_local
#define ssp1601_run ssp1601_run_local

#define GET_PC() rPC
#define GET_PPC_OFFS() (GET_PC()*2 - 2)
#define SET_PC(d) { rPC = d; }		/* must return to dispatcher after this */
//#define GET_PC() (PC - (unsigned short *)svp->iram_rom)
//#define GET_PPC_OFFS() ((unsigned int)PC - (unsigned int)svp->iram_rom - 2)
//#define SET_PC(d) PC = (unsigned short *)svp->iram_rom + d

#include "ssp16.c"
#include "gen_arm.c"

// -----------------------------------------------------

// ld d, s
static void op00(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	PC = ((unsigned short *)(void *)&op) + 1; /* FIXME: needed for interpreter */
	if (op == 0) return; // nop
	if (op == ((SSP_A<<4)|SSP_P)) { // A <- P
		// not sure. MAME claims that only hi word is transfered.
		read_P(); // update P
		rA32 = rP.v;
	}
	else
	{
		tmpv = REG_READ(op & 0x0f);
		REG_WRITE((op & 0xf0) >> 4, tmpv);
	}
}

// ld d, (ri)
static void op01(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr1_read(op); REG_WRITE((op & 0xf0) >> 4, tmpv);
}

// ld (ri), s
static void op02(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = REG_READ((op & 0xf0) >> 4); ptr1_write(op, tmpv);
}

// ldi d, imm
static void op04(unsigned int op, unsigned int imm)
{
	REG_WRITE((op & 0xf0) >> 4, imm);
}

// ld d, ((ri))
static void op05(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr2_read(op); REG_WRITE((op & 0xf0) >> 4, tmpv);
}

// ldi (ri), imm
static void op06(unsigned int op, unsigned int imm)
{
	ptr1_write(op, imm);
}

// ld adr, a
static void op07(unsigned int op, unsigned int imm)
{
	ssp->RAM[op & 0x1ff] = rA;
}

// ld d, ri
static void op09(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = rIJ[(op&3)|((op>>6)&4)]; REG_WRITE((op & 0xf0) >> 4, tmpv);
}

// ld ri, s
static void op0a(unsigned int op, unsigned int imm)
{
	rIJ[(op&3)|((op>>6)&4)] = REG_READ((op & 0xf0) >> 4);
}

// ldi ri, simm (also op0d op0e op0f)
static void op0c(unsigned int op, unsigned int imm)
{
	rIJ[(op>>8)&7] = op;
}

// call cond, addr
static void op24(unsigned int op, unsigned int imm)
{
	int cond = 0;
	do {
		COND_CHECK
		if (cond) { int new_PC = imm; write_STACK(GET_PC()); SET_PC(new_PC); }
	}
	while (0);
}

// ld d, (a)
static void op25(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ((unsigned short *)svp->iram_rom)[rA]; REG_WRITE((op & 0xf0) >> 4, tmpv);
}

// bra cond, addr
static void op26(unsigned int op, unsigned int imm)
{
	do
	{
		int cond = 0;
		COND_CHECK
		if (cond) SET_PC(imm);
	}
	while (0);
}

// mod cond, op
static void op48(unsigned int op, unsigned int imm)
{
	do
	{
		int cond = 0;
		COND_CHECK
		if (cond) {
			switch (op & 7) {
				case 2: rA32 = (signed int)rA32 >> 1; break; // shr (arithmetic)
				case 3: rA32 <<= 1; break; // shl
				case 6: rA32 = -(signed int)rA32; break; // neg
				case 7: if ((int)rA32 < 0) rA32 = -(signed int)rA32; break; // abs
				default: elprintf(EL_SVP|EL_ANOMALY, "ssp FIXME: unhandled mod %i @ %04x",
							 op&7, GET_PPC_OFFS());
			}
			UPD_ACC_ZN // ?
		}
	}
	while(0);
}

// mpys?
static void op1b(unsigned int op, unsigned int imm)
{
	read_P(); // update P
	rA32 -= rP.v;			// maybe only upper word?
	UPD_ACC_ZN			// there checking flags after this
	rX = ptr1_read_(op&3, 0, (op<<1)&0x18); // ri (maybe rj?)
	rY = ptr1_read_((op>>4)&3, 4, (op>>3)&0x18); // rj
}

// mpya (rj), (ri), b
static void op4b(unsigned int op, unsigned int imm)
{
	read_P(); // update P
	rA32 += rP.v; // confirmed to be 32bit
	UPD_ACC_ZN // ?
	rX = ptr1_read_(op&3, 0, (op<<1)&0x18); // ri (maybe rj?)
	rY = ptr1_read_((op>>4)&3, 4, (op>>3)&0x18); // rj
}

// mld (rj), (ri), b
static void op5b(unsigned int op, unsigned int imm)
{
	rA32 = 0;
	rST &= 0x0fff; // ?
	rX = ptr1_read_(op&3, 0, (op<<1)&0x18); // ri (maybe rj?)
	rY = ptr1_read_((op>>4)&3, 4, (op>>3)&0x18); // rj
}

// OP a, s
static void op10(unsigned int op, unsigned int imm)
{
	do
	{
		unsigned int tmpv;
		OP_CHECK32(OP_SUBA32); tmpv = REG_READ(op & 0x0f); OP_SUBA(tmpv);
	}
	while(0);
}

static void op30(unsigned int op, unsigned int imm)
{
	do
	{
		unsigned int tmpv;
		OP_CHECK32(OP_CMPA32); tmpv = REG_READ(op & 0x0f); OP_CMPA(tmpv);
	}
	while(0);
}

static void op40(unsigned int op, unsigned int imm)
{
	do
	{
		unsigned int tmpv;
		OP_CHECK32(OP_ADDA32); tmpv = REG_READ(op & 0x0f); OP_ADDA(tmpv);
	}
	while(0);
}

static void op50(unsigned int op, unsigned int imm)
{
	do
	{
		unsigned int tmpv;
		OP_CHECK32(OP_ANDA32); tmpv = REG_READ(op & 0x0f); OP_ANDA(tmpv);
	}
	while(0);
}

static void op60(unsigned int op, unsigned int imm)
{
	do
	{
		unsigned int tmpv;
		OP_CHECK32(OP_ORA32 ); tmpv = REG_READ(op & 0x0f); OP_ORA (tmpv);
	}
	while(0);
}

static void op70(unsigned int op, unsigned int imm)
{
	do
	{
		unsigned int tmpv;
		OP_CHECK32(OP_EORA32); tmpv = REG_READ(op & 0x0f); OP_EORA(tmpv);
	}
	while(0);
}

// OP a, (ri)
static void op11(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr1_read(op); OP_SUBA(tmpv);
}

static void op31(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr1_read(op); OP_CMPA(tmpv);
}

static void op41(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr1_read(op); OP_ADDA(tmpv);
}

static void op51(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr1_read(op); OP_ANDA(tmpv);
}

static void op61(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr1_read(op); OP_ORA (tmpv);
}

static void op71(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr1_read(op); OP_EORA(tmpv);
}

// OP a, adr
static void op03(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ssp->RAM[op & 0x1ff]; OP_LDA (tmpv);
}

static void op13(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ssp->RAM[op & 0x1ff]; OP_SUBA(tmpv);
}

static void op33(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ssp->RAM[op & 0x1ff]; OP_CMPA(tmpv);
}

static void op43(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ssp->RAM[op & 0x1ff]; OP_ADDA(tmpv);
}

static void op53(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ssp->RAM[op & 0x1ff]; OP_ANDA(tmpv);
}

static void op63(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ssp->RAM[op & 0x1ff]; OP_ORA (tmpv);
}

static void op73(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ssp->RAM[op & 0x1ff]; OP_EORA(tmpv);
}

// OP a, imm
static void op14(unsigned int op, unsigned int imm)
{
	OP_SUBA(imm);
}

static void op34(unsigned int op, unsigned int imm)
{
	OP_CMPA(imm);
}

static void op44(unsigned int op, unsigned int imm)
{
	OP_ADDA(imm);
}

static void op54(unsigned int op, unsigned int imm)
{
	OP_ANDA(imm);
}

static void op64(unsigned int op, unsigned int imm)
{
	OP_ORA (imm);
}

static void op74(unsigned int op, unsigned int imm)
{
	OP_EORA(imm);
}

// OP a, ((ri))
static void op15(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr2_read(op); OP_SUBA(tmpv);
}

static void op35(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr2_read(op); OP_CMPA(tmpv);
}

static void op45(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr2_read(op); OP_ADDA(tmpv);
}

static void op55(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr2_read(op); OP_ANDA(tmpv);
}

static void op65(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr2_read(op); OP_ORA (tmpv);
}

static void op75(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = ptr2_read(op); OP_EORA(tmpv);
}

// OP a, ri
static void op19(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = rIJ[IJind]; OP_SUBA(tmpv);
}

static void op39(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = rIJ[IJind]; OP_CMPA(tmpv);
}

static void op49(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = rIJ[IJind]; OP_ADDA(tmpv);
}

static void op59(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = rIJ[IJind]; OP_ANDA(tmpv);
}

static void op69(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = rIJ[IJind]; OP_ORA (tmpv);
}

static void op79(unsigned int op, unsigned int imm)
{
	unsigned int tmpv;
	tmpv = rIJ[IJind]; OP_EORA(tmpv);
}

// OP simm
static void op1c(unsigned int op, unsigned int imm)
{
	OP_SUBA(op & 0xff);
}

static void op3c(unsigned int op, unsigned int imm)
{
	OP_CMPA(op & 0xff);
}

static void op4c(unsigned int op, unsigned int imm)
{
	OP_ADDA(op & 0xff);
}

static void op5c(unsigned int op, unsigned int imm)
{
	OP_ANDA(op & 0xff);
}

static void op6c(unsigned int op, unsigned int imm)
{
	OP_ORA (op & 0xff);
}

static void op7c(unsigned int op, unsigned int imm)
{
	OP_EORA(op & 0xff);
}

typedef void (in_func)(unsigned int op, unsigned int imm);

static in_func *in_funcs[0x80] =
{
	op00, op01, op02, op03, op04, op05, op06, op07,
	NULL, op09, op0a, NULL, op0c, op0c, op0c, op0c,
	op10, op11, NULL, op13, op14, op15, NULL, NULL,
	NULL, op19, NULL, op1b, op1c, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, op24, op25, op26, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	op30, op31, NULL, op33, op34, op35, NULL, NULL,
	NULL, op39, NULL, NULL, op3c, NULL, NULL, NULL,
	op40, op41, NULL, op43, op44, op45, NULL, NULL,
	op48, op49, NULL, op4b, op4c, NULL, NULL, NULL,
	op50, op51, NULL, op53, op54, op55, NULL, NULL,
	NULL, op59, NULL, op5b, op5c, NULL, NULL, NULL,
	op60, op61, NULL, op63, op64, op65, NULL, NULL,
	NULL, op69, NULL, NULL, op6c, NULL, NULL, NULL,
	op70, op71, NULL, op73, op74, op75, NULL, NULL,
	NULL, op79, NULL, NULL, op7c, NULL, NULL, NULL,
};

// -----------------------------------------------------

static unsigned char iram_context_map[] =
{
	 0, 0, 0, 0, 1, 0, 0, 0, // 04
	 0, 0, 0, 0, 0, 0, 2, 0, // 0e
	 0, 0, 0, 0, 0, 3, 0, 4, // 15 17
	 5, 0, 0, 6, 0, 7, 0, 0, // 18 1b 1d
	 8, 9, 0, 0, 0,10, 0, 0, // 20 21 25
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0,11, 0, 0,12, 0, 0, // 32 35
	13,14, 0, 0, 0, 0, 0, 0  // 38 39
};

static int get_iram_context(void)
{
	unsigned char *ir = (unsigned char *)svp->iram_rom;
	int val1, val = ir[0x083^1] + ir[0x4FA^1] + ir[0x5F7^1] + ir[0x47B^1];
	val1 = iram_context_map[(val>>1)&0x3f];

	if (val1 == 0) {
		printf("val: %02x PC=%04x\n", (val>>1)&0x3f, rPC);
		//debug_dump2file(name, svp->iram_rom, 0x800);
		exit(1);
	}
//	elprintf(EL_ANOMALY, "iram_context: %02i", val1);
	return val1;
}

// -----------------------------------------------------
/*
enum {
	SSP_GR0, SSP_X,     SSP_Y,   SSP_A,
	SSP_ST,  SSP_STACK, SSP_PC,  SSP_P,
	SSP_PM0, SSP_PM1,   SSP_PM2, SSP_XST,
	SSP_PM4, SSP_gr13,  SSP_PMC, SSP_AL
};
*/
/* regs with known values */
static struct
{
	ssp_reg_t gr[8];
	unsigned char r[8];
} known_regs;

#define KRREG_X     (1 << SSP_X)
#define KRREG_Y     (1 << SSP_Y)
#define KRREG_A     (1 << SSP_A)	/* AH only */
#define KRREG_ST    (1 << SSP_ST)
#define KRREG_STACK (1 << SSP_STACK)
#define KRREG_PC    (1 << SSP_PC)
#define KRREG_P     (1 << SSP_P)
#define KRREG_PR0   (1 << 8)
#define KRREG_PR4   (1 << 12)
#define KRREG_AL    (1 << 16)

/* bitfield of known register values */
static u32 known_regb = 0;

/* known vals, which need to be flushed
 * (only ST, P, r0-r7)
 * ST means flags are being held in ARM PSR
 * P means that it needs to be recalculated
 */
static u32 dirty_regb = 0;

/* known values of host regs.
 * -1            - unknown
 * 000000-00ffff - 16bit value
 * 100000-10ffff - base reg (r7) + 16bit val
 * 0r0000        - means reg (low) eq gr[r].h, r != AL
 */
static int hostreg_r[4];

static void hostreg_clear(void)
{
	int i;
	for (i = 0; i < 4; i++)
		hostreg_r[i] = -1;
}

static void hostreg_sspreg_changed(int sspreg)
{
	int i;
	for (i = 0; i < 4; i++)
		if (hostreg_r[i] == (sspreg<<16)) hostreg_r[i] = -1;
}


#define PROGRAM(x) ((unsigned short *)svp->iram_rom)[x]

static void tr_unhandled(void)
{
	FILE *f = fopen("tcache.bin", "wb");
	fwrite(tcache, 1, (tcache_ptr - tcache)*4, f);
	fclose(f);
	printf("unhandled @ %04x\n", known_regs.gr[SSP_PC].h<<1);
	exit(1);
}

/* update P, if needed. Trashes r1 */
static void tr_flush_dirty_P(void)
{
	// TODO: const regs
	if (!(dirty_regb & KRREG_P)) return;
	EOP_MOV_REG_ASR(10, 4, 16);		// mov  r10, r4, asr #16
	EOP_MOV_REG_LSL( 1, 4, 16);		// mov  r1,  r4, lsl #16
	EOP_MOV_REG_ASR( 1, 1, 15);		// mov  r1,  r1, asr #15
	EOP_MUL(10, 1, 10);			// mul  r10, r1, r10
	dirty_regb &= ~KRREG_P;
}

/* write dirty pr to host reg. Nothing is trashed */
static void tr_flush_dirty_pr(int r)
{
	int ror = 0, reg;

	if (!(dirty_regb & (1 << (r+8)))) return;

	switch (r&3) {
		case 0: ror =    0; break;
		case 1: ror = 24/2; break;
		case 2: ror = 16/2; break;
	}
	reg = (r < 4) ? 8 : 9;
	EOP_BIC_IMM(reg,reg,ror,0xff);
	if (known_regs.r[r] != 0)
		EOP_ORR_IMM(reg,reg,ror,known_regs.r[r]);
	dirty_regb &= ~(1 << (r+8));
}

/* write all dirty pr0-pr7 to host regs. Nothing is trashed */
static void tr_flush_dirty_prs(void)
{
	int i, ror = 0, reg;
	int dirty = dirty_regb >> 8;
	/* r0-r7 */
	for (i = 0; dirty && i < 8; i++, dirty >>= 1)
	{
		if (!(dirty&1)) continue;
		switch (i&3) {
			case 0: ror =    0; break;
			case 1: ror = 24/2; break;
			case 2: ror = 16/2; break;
		}
		reg = (i < 4) ? 8 : 9;
		EOP_BIC_IMM(reg,reg,ror,0xff);
		if (known_regs.r[i] != 0)
			EOP_ORR_IMM(reg,reg,ror,known_regs.r[i]);
	}
	dirty_regb &= ~0xff00;
}

/* write dirty pr and "forget" it. Nothing is trashed. */
static void tr_release_pr(int r)
{
	tr_flush_dirty_pr(r);
	known_regb &= ~(1 << (r+8));
}

/* fush ARM PSR to r6. Trashes r1 */
static void tr_flush_dirty_ST(void)
{
	if (!(dirty_regb & KRREG_ST)) return;
	EOP_BIC_IMM(6,6,0,0x0f);
	EOP_MRS(1);
	EOP_ORR_REG_LSR(6,6,1,28);
	dirty_regb &= ~KRREG_ST;
	hostreg_r[1] = -1;
}

/* inverse of above. Trashes r1 */
static void tr_make_dirty_ST(void)
{
	if (dirty_regb & KRREG_ST) return;
	if (known_regb & KRREG_ST) {
		int flags = 0;
		if (known_regs.gr[SSP_ST].h & SSP_FLAG_N) flags |= 8;
		if (known_regs.gr[SSP_ST].h & SSP_FLAG_Z) flags |= 4;
		EOP_MSR_IMM(4/2, flags);
	} else {
		EOP_MOV_REG_LSL(1, 6, 28);
		EOP_MSR_REG(1);
		hostreg_r[1] = -1;
	}
	dirty_regb |= KRREG_ST;
}

/* load 16bit val into host reg r0-r3. Nothing is trashed */
static void tr_mov16(int r, int val)
{
	if (hostreg_r[r] != val) {
		emit_mov_const(A_COND_AL, r, val);
		hostreg_r[r] = val;
	}
}

static void tr_mov16_cond(int cond, int r, int val)
{
	emit_mov_const(cond, r, val);
	hostreg_r[r] = -1;
}

/* read bank word to r0. Thrashes r1. */
static void tr_bank_read(int addr) /* word addr 0-0x1ff */
{
	int breg = 7;
	if (addr > 0x7f) {
		if (hostreg_r[1] != (0x100000|((addr&0x180)<<1))) {
			EOP_ADD_IMM(1,7,30/2,(addr&0x180)>>1);	// add  r1, r7, ((op&0x180)<<1)
			hostreg_r[1] = 0x100000|((addr&0x180)<<1);
		}
		breg = 1;
	}
	EOP_LDRH_IMM(0,breg,(addr&0x7f)<<1);	// ldrh r0, [r1, (op&0x7f)<<1]
	hostreg_r[0] = -1;
}

/* write r0 to bank. Trashes r1. */
static void tr_bank_write(int addr)
{
	int breg = 7;
	if (addr > 0x7f) {
		if (hostreg_r[1] != (0x100000|((addr&0x180)<<1))) {
			EOP_ADD_IMM(1,7,30/2,(addr&0x180)>>1);	// add  r1, r7, ((op&0x180)<<1)
			hostreg_r[1] = 0x100000|((addr&0x180)<<1);
		}
		breg = 1;
	}
	EOP_STRH_IMM(0,breg,(addr&0x7f)<<1);		// strh r0, [r1, (op&0x7f)<<1]
}

/* handle RAM bank pointer modifiers. if need_modulo, trash r1-r3, else nothing */
static void tr_ptrr_mod(int r, int mod, int need_modulo, int count)
{
	int modulo_shift = -1;	/* unknown */

	if (mod == 0) return;

	if (!need_modulo || mod == 1) // +!
		modulo_shift = 8;
	else if (need_modulo && (known_regb & KRREG_ST)) {
		modulo_shift = known_regs.gr[SSP_ST].h & 7;
		if (modulo_shift == 0) modulo_shift = 8;
	}

	if (modulo_shift == -1)
	{
		int reg = (r < 4) ? 8 : 9;
		tr_release_pr(r);
		tr_flush_dirty_ST();
		EOP_C_DOP_IMM(A_COND_AL,A_OP_AND,1,6,1,0,0x70);	// ands  r1, r6, #0x70
		EOP_C_DOP_IMM(A_COND_EQ,A_OP_MOV,0,0,1,0,0x80); // moveq r1, #0x80
		EOP_MOV_REG_LSR(1, 1, 4);		// mov r1, r1, lsr #4
		EOP_RSB_IMM(2, 1, 0, 8);		// rsb r1, r1, #8
		EOP_MOV_IMM(3, 8/2, count);		// mov r3, #0x01000000
		if (r&3)
			EOP_ADD_IMM(1, 1, 0, (r&3)*8);	// add r1, r1, #(r&3)*8
		EOP_MOV_REG2_ROR(reg,reg,1);		// mov reg, reg, ror r1
		if (mod == 2)
		     EOP_SUB_REG2_LSL(reg,reg,3,2);	// sub reg, reg, #0x01000000 << r2
		else EOP_ADD_REG2_LSL(reg,reg,3,2);
		EOP_RSB_IMM(1, 1, 0, 32);		// rsb r1, r1, #32
		EOP_MOV_REG2_ROR(reg,reg,1);		// mov reg, reg, ror r1
		hostreg_r[1] = hostreg_r[2] = hostreg_r[3] = -1;
	}
	else if (known_regb & (1 << (r + 8)))
	{
		int modulo = (1 << modulo_shift) - 1;
		if (mod == 2)
		     known_regs.r[r] = (known_regs.r[r] & ~modulo) | ((known_regs.r[r] - count) & modulo);
		else known_regs.r[r] = (known_regs.r[r] & ~modulo) | ((known_regs.r[r] + count) & modulo);
	}
	else
	{
		int reg = (r < 4) ? 8 : 9;
		int ror = ((r&3) + 1)*8 - (8 - modulo_shift);
		EOP_MOV_REG_ROR(reg,reg,ror);
		// {add|sub} reg, reg, #1<<shift
		EOP_C_DOP_IMM(A_COND_AL,(mod==2)?A_OP_SUB:A_OP_ADD,0,reg,reg, 8/2, count << (8 - modulo_shift));
		EOP_MOV_REG_ROR(reg,reg,32-ror);
	}
}

/* handle writes r0 to (rX). Trashes r1.
 * fortunately we can ignore modulo increment modes for writes. */
static void tr_rX_write1(int op)
{
	if ((op&3) == 3)
	{
		int mod = (op>>2) & 3; // direct addressing
		tr_bank_write((op & 0x100) + mod);
	}
	else
	{
		int r = (op&3) | ((op>>6)&4);
		if (known_regb & (1 << (r + 8))) {
			tr_bank_write((op&0x100) | known_regs.r[r]);
		} else {
			int reg = (r < 4) ? 8 : 9;
			int ror = ((4 - (r&3))*8) & 0x1f;
			EOP_AND_IMM(1,reg,ror/2,0xff);			// and r1, r{7,8}, <mask>
			if (r >= 4)
				EOP_ORR_IMM(1,1,((ror-8)&0x1f)/2,1);		// orr r1, r1, 1<<shift
			if (r&3) EOP_ADD_REG_LSR(1,7,1, (r&3)*8-1);	// add r1, r7, r1, lsr #lsr
			else     EOP_ADD_REG_LSL(1,7,1,1);
			EOP_STRH_SIMPLE(0,1);				// strh r0, [r1]
			hostreg_r[1] = -1;
		}
		tr_ptrr_mod(r, (op>>2) & 3, 0, 1);
	}
}

/* read (rX) to r0. Trashes r1-r3. */
static void tr_rX_read(int r, int mod)
{
	if ((r&3) == 3)
	{
		tr_bank_read(((r << 6) & 0x100) + mod); // direct addressing
	}
	else
	{
		if (known_regb & (1 << (r + 8))) {
			tr_bank_read(((r << 6) & 0x100) | known_regs.r[r]);
		} else {
			int reg = (r < 4) ? 8 : 9;
			int ror = ((4 - (r&3))*8) & 0x1f;
			EOP_AND_IMM(1,reg,ror/2,0xff);			// and r1, r{7,8}, <mask>
			if (r >= 4)
				EOP_ORR_IMM(1,1,((ror-8)&0x1f)/2,1);		// orr r1, r1, 1<<shift
			if (r&3) EOP_ADD_REG_LSR(1,7,1, (r&3)*8-1);	// add r1, r7, r1, lsr #lsr
			else     EOP_ADD_REG_LSL(1,7,1,1);
			EOP_LDRH_SIMPLE(0,1);				// ldrh r0, [r1]
			hostreg_r[1] = -1;
		}
		tr_ptrr_mod(r, mod, 1, 1);
	}
}


/* get ARM cond which would mean that SSP cond is satisfied. No trash. */
static int tr_cond_check(int op)
{
	int f = (op & 0x100) >> 8;
	switch (op&0xf0) {
		case 0x00: return A_COND_AL;	/* always true */
		case 0x50:			/* Z matches f(?) bit */
			if (dirty_regb & KRREG_ST) return f ? A_COND_EQ : A_COND_NE;
			EOP_TST_IMM(6, 0, 4);
			return f ? A_COND_NE : A_COND_EQ;
		case 0x70:			/* N matches f(?) bit */
			if (dirty_regb & KRREG_ST) return f ? A_COND_MI : A_COND_PL;
			EOP_TST_IMM(6, 0, 8);
			return f ? A_COND_NE : A_COND_EQ;
		default:
			printf("unimplemented cond?\n");
			tr_unhandled();
			return 0;
	}
}

static int tr_neg_cond(int cond)
{
	switch (cond) {
		case A_COND_AL: printf("neg for AL?\n"); exit(1);
		case A_COND_EQ: return A_COND_NE;
		case A_COND_NE: return A_COND_EQ;
		case A_COND_MI: return A_COND_PL;
		case A_COND_PL: return A_COND_MI;
		default:        printf("bad cond for neg\n"); exit(1);
	}
	return 0;
}

//	SSP_GR0, SSP_X,     SSP_Y,   SSP_A,
//	SSP_ST,  SSP_STACK, SSP_PC,  SSP_P,
//@ r4:  XXYY
//@ r5:  A
//@ r6:  STACK and emu flags
//@ r7:  SSP context
//@ r10: P

// read general reg to r0. Trashes r1
static void tr_GR0_to_r0(void)
{
	tr_mov16(0, 0xffff);
}

static void tr_X_to_r0(void)
{
	if (hostreg_r[0] != (SSP_X<<16)) {
		EOP_MOV_REG_LSR(0, 4, 16);	// mov  r0, r4, lsr #16
		hostreg_r[0] = SSP_X<<16;
	}
}

static void tr_Y_to_r0(void)
{
	// TODO..
	if (hostreg_r[0] != (SSP_Y<<16)) {
		EOP_MOV_REG_SIMPLE(0, 4);	// mov  r0, r4
		hostreg_r[0] = SSP_Y<<16;
	}
}

static void tr_A_to_r0(void)
{
	if (hostreg_r[0] != (SSP_A<<16)) {
		EOP_MOV_REG_LSR(0, 5, 16);	// mov  r0, r5, lsr #16  @ AH
		hostreg_r[0] = SSP_A<<16;
	}
}

static void tr_ST_to_r0(void)
{
	// VR doesn't need much accuracy here..
	EOP_MOV_REG_LSR(0, 6, 4);		// mov  r0, r6, lsr #4
	EOP_AND_IMM(0, 0, 0, 0x67);		// and  r0, r0, #0x67
	hostreg_r[0] = -1;
}

static void tr_STACK_to_r0(void)
{
	// 448
	EOP_SUB_IMM(6, 6,  8/2, 0x20);		// sub  r6, r6, #1<<29
	EOP_ADD_IMM(1, 7, 24/2, 0x04);		// add  r1, r7, 0x400
	EOP_ADD_IMM(1, 1, 0, 0x48);		// add  r1, r1, 0x048
	EOP_ADD_REG_LSR(1, 1, 6, 28);		// add  r1, r1, r6, lsr #28
	EOP_LDRH_SIMPLE(0, 1);			// ldrh r0, [r1]
	hostreg_r[0] = hostreg_r[1] = -1;
}

static void tr_PC_to_r0(void)
{
	tr_mov16(0, known_regs.gr[SSP_PC].h);
}

static void tr_P_to_r0(void)
{
	tr_flush_dirty_P();
	EOP_MOV_REG_LSR(0, 10, 16);		// mov  r0, r10, lsr #16
	hostreg_r[0] = -1;
}

typedef void (tr_read_func)(void);

static tr_read_func *tr_read_funcs[8] =
{
	tr_GR0_to_r0,
	tr_X_to_r0,
	tr_Y_to_r0,
	tr_A_to_r0,
	tr_ST_to_r0,
	tr_STACK_to_r0,
	tr_PC_to_r0,
	tr_P_to_r0
};


// write r0 to general reg handlers. Trashes r1
#define TR_WRITE_R0_TO_REG(reg) \
{ \
	hostreg_sspreg_changed(reg); \
	hostreg_r[0] = (reg)<<16; \
	if (const_val != -1) { \
		known_regs.gr[reg].h = const_val; \
		known_regb |= 1 << (reg); \
	} else { \
		known_regb &= ~(1 << (reg)); \
	} \
}

static void tr_r0_to_GR0(int const_val)
{
	// do nothing
}

static void tr_r0_to_X(int const_val)
{
	EOP_MOV_REG_LSL(4, 4, 16);		// mov  r4, r4, lsl #16
	EOP_MOV_REG_LSR(4, 4, 16);		// mov  r4, r4, lsr #16
	EOP_ORR_REG_LSL(4, 4, 0, 16);		// orr  r4, r4, r0, lsl #16
	dirty_regb |= KRREG_P;			// touching X or Y makes P dirty.
	TR_WRITE_R0_TO_REG(SSP_X);
}

static void tr_r0_to_Y(int const_val)
{
	EOP_MOV_REG_LSR(4, 4, 16);		// mov  r4, r4, lsr #16
	EOP_ORR_REG_LSL(4, 4, 0, 16);		// orr  r4, r4, r0, lsl #16
	EOP_MOV_REG_ROR(4, 4, 16);		// mov  r4, r4, ror #16
	dirty_regb |= KRREG_P;
	TR_WRITE_R0_TO_REG(SSP_Y);
}

static void tr_r0_to_A(int const_val)
{
	EOP_MOV_REG_LSL(5, 5, 16);		// mov  r5, r5, lsl #16
	EOP_MOV_REG_LSR(5, 5, 16);		// mov  r5, r5, lsr #16  @ AL
	EOP_ORR_REG_LSL(5, 5, 0, 16);		// orr  r5, r5, r0, lsl #16
	TR_WRITE_R0_TO_REG(SSP_A);
}

static void tr_r0_to_ST(int const_val)
{
	// VR doesn't need much accuracy here..
	EOP_AND_IMM(1, 0,   0, 0x67);		// and   r1, r0, #0x67
	EOP_AND_IMM(6, 6, 8/2, 0xe0);		// and   r6, r6, #7<<29     @ preserve STACK
	EOP_ORR_REG_LSL(6, 6, 1, 4);		// orr   r6, r6, r1, lsl #4
	TR_WRITE_R0_TO_REG(SSP_ST);
	hostreg_r[1] = -1;
	dirty_regb &= ~KRREG_ST;
}

static void tr_r0_to_STACK(int const_val)
{
	// 448
	EOP_ADD_IMM(1, 7, 24/2, 0x04);		// add  r1, r7, 0x400
	EOP_ADD_IMM(1, 1, 0, 0x48);		// add  r1, r1, 0x048
	EOP_ADD_REG_LSR(1, 1, 6, 28);		// add  r1, r1, r6, lsr #28
	EOP_STRH_SIMPLE(0, 1);			// strh r0, [r1]
	EOP_ADD_IMM(6, 6,  8/2, 0x20);		// add  r6, r6, #1<<29
	hostreg_r[1] = -1;
}

static void tr_r0_to_PC(int const_val)
{
	EOP_MOV_REG_LSL(1, 0, 16);		// mov  r1, r0, lsl #16
	EOP_STR_IMM(1,7,0x400+6*4);		// str  r1, [r7, #(0x400+6*8)]
	hostreg_r[1] = -1;
}

typedef void (tr_write_func)(int const_val);

static tr_write_func *tr_write_funcs[8] =
{
	tr_r0_to_GR0,
	tr_r0_to_X,
	tr_r0_to_Y,
	tr_r0_to_A,
	tr_r0_to_ST,
	tr_r0_to_STACK,
	tr_r0_to_PC,
	(tr_write_func *)tr_unhandled
};


static int translate_op(unsigned int op, int *pc, int imm)
{
	u32 tmpv, tmpv2;
	int ret = 0;
	known_regs.gr[SSP_PC].h = *pc;

	switch (op >> 9)
	{
		// ld d, s
		case 0x00:
			if (op == 0) { ret++; break; } // nop
			tmpv  = op & 0xf; // src
			tmpv2 = (op >> 4) & 0xf; // dst
			if (tmpv >= 8 || tmpv2 >= 8) return -1; // TODO
			if (tmpv2 == SSP_A && tmpv == SSP_P) { // ld A, P
				tr_flush_dirty_P();
				EOP_MOV_REG_SIMPLE(5, 10);
				hostreg_sspreg_changed(SSP_A); \
				known_regb &= ~(KRREG_A|KRREG_AL);
				ret++; break;
			}
			tr_read_funcs[tmpv]();
			tr_write_funcs[tmpv2]((known_regb & (1 << tmpv)) ? known_regs.gr[tmpv].h : -1);
			ret++; break;

		// ld d, (ri)
		case 0x01: {
			// tmpv = ptr1_read(op); REG_WRITE((op & 0xf0) >> 4, tmpv); break;
			int r = (op&3) | ((op>>6)&4);
			int mod = (op>>2)&3;
			tmpv = (op >> 4) & 0xf; // dst
			if (tmpv >= 8) return -1; // TODO
			if (tmpv != 0)
			     tr_rX_read(r, mod);
			else tr_ptrr_mod(r, mod, 1, 1);
			tr_write_funcs[tmpv](-1);
			ret++; break;
		}

		// ld (ri), s
		case 0x02:
			tmpv = (op >> 4) & 0xf; // src
			if (tmpv >= 8) return -1; // TODO
			tr_read_funcs[tmpv]();
			tr_rX_write1(op);
			ret++; break;

		// ld a, adr
		case 0x03:
			tr_bank_read(op&0x1ff);
			tr_r0_to_A(-1);
			ret++; break;

		// ldi d, imm
		case 0x04:
			tmpv = (op & 0xf0) >> 4;
			if (tmpv < 8)
			{
				tr_mov16(0, imm);
				tr_write_funcs[tmpv](imm);
				ret += 2; break;
			}
			else if (tmpv == 0xe && (PROGRAM(*pc) >> 9) == 4)
			{
				// programming PMC..
				(*pc)++;
				tmpv = imm | (PROGRAM((*pc)++) << 16);
				ret += 2;
				emit_mov_const(A_COND_AL, 0, tmpv);
				EOP_LDR_IMM(1,7,0x484);		// ldr r1, [r7, #0x484] // emu_status
				EOP_STR_IMM(0,7,0x400+14*4);	// PMC
				// reads on fe06, fe08; next op is ld -,
				if ((tmpv == 0x187f03 || tmpv == 0x187f04) && (PROGRAM(*pc) & 0xfff0) == 0)
				{
					int flag = (tmpv == 0x187f03) ? SSP_WAIT_30FE06 : SSP_WAIT_30FE08;
					tr_flush_dirty_ST();
					EOP_LDR_IMM(0,7,0x490); // dram_ptr
					EOP_ADD_IMM(0,0,24/2,0xfe);				// add  r0, r0, #0xfe00
					EOP_LDRH_IMM(0,0,(tmpv == 0x187f03) ? 6 : 8);		// ldrh r0, [r0, #8]
					EOP_TST_REG_SIMPLE(0,0);
					EOP_C_DOP_IMM(A_COND_EQ,A_OP_ADD,0,11,11,22/2,1);	// add r11, r11, #1024
					EOP_C_DOP_IMM(A_COND_EQ,A_OP_ORR,0, 1, 1,24/2,flag>>8);	// orr r1, r1, #SSP_WAIT_30FE08
				}
				EOP_ORR_IMM(1,1,0,SSP_PMC_SET);		// orr r1, r1, #SSP_PMC_SET
				EOP_STR_IMM(1,7,0x484);			// str r1, [r7, #0x484] // emu_status
				hostreg_r[0] = hostreg_r[1] = -1;
				ret += 2; break;
			}
			else
				return -1;	/* TODO.. */

		// ld d, ((ri))
		case 0x05: {
			int r;
			r = (op&3) | ((op>>6)&4); // src
			tmpv2 = (op >> 4) & 0xf;  // dst
			if (tmpv2 >= 8) return -1; // TODO

			if ((r&3) == 3) {
				tr_bank_read((op&0x100) | ((op>>2)&3));
			} else if (known_regb & (1 << (r+8))) {
				tr_bank_read((op&0x100) | known_regs.r[r]);
			} else {
				int reg = (r < 4) ? 8 : 9;
				int ror = ((4 - (r&3))*8) & 0x1f;
				EOP_AND_IMM(1,reg,ror/2,0xff);			// and r1, r{7,8}, <mask>
				if (r >= 4)
					EOP_ORR_IMM(1,1,((ror-8)&0x1f)/2,1);		// orr r1, r1, 1<<shift
				if (r&3) EOP_ADD_REG_LSR(1,7,1, (r&3)*8-1);	// add r1, r7, r1, lsr #lsr
				else     EOP_ADD_REG_LSL(1,7,1,1);
				EOP_LDRH_SIMPLE(0,1);				// ldrh r0, [r1]
			}
			EOP_LDR_IMM(2,7,0x48c);					// ptr_iram_rom
			EOP_ADD_REG_LSL(2,2,0,1);				// add  r2, r2, r0, lsl #1
			EOP_ADD_IMM(0,0,0,1);					// add  r0, r0, #1
			if ((r&3) == 3) {
				tr_bank_write((op&0x100) | ((op>>2)&3));
			} else if (known_regb & (1 << (r+8))) {
				tr_bank_write((op&0x100) | known_regs.r[r]);
			} else {
				EOP_STRH_SIMPLE(0,1);				// strh r0, [r1]
				hostreg_r[1] = -1;
			}
			EOP_LDRH_SIMPLE(0,2);					// ldrh r0, [r2]
			hostreg_r[0] = hostreg_r[2] = -1;
			tr_write_funcs[tmpv2](-1);
			ret += 3; break; /* should certainly take > 1 */
		}

		// ldi (ri), imm
		case 0x06:
			tr_mov16(0, imm);
			tr_rX_write1(op);
			ret += 2; break;

		// ld adr, a
		case 0x07:
			tr_A_to_r0();
			tr_bank_write(op&0x1ff);
			ret++; break;

		// ld d, ri
		case 0x09: {
			int r;
			r = (op&3) | ((op>>6)&4); // src
			tmpv2 = (op >> 4) & 0xf;  // dst
			if (tmpv2 >= 8) tr_unhandled();
			if ((r&3) == 3) tr_unhandled();

			if (known_regb & (1 << (r+8))) {
				tr_mov16(0, known_regs.r[r]);
				tr_write_funcs[tmpv2](known_regs.r[r]);
			} else {
				int reg = (r < 4) ? 8 : 9;
				if (r&3) EOP_MOV_REG_LSR(0, reg, (r&3)*8);	// mov r0, r{7,8}, lsr #lsr
				EOP_AND_IMM(0, (r&3)?0:reg, 0, 0xff);		// and r0, r{7,8}, <mask>
				hostreg_r[0] = -1;
				tr_write_funcs[tmpv2](-1);
			}
			ret++; break;
		}

		// ld ri, s
		case 0x0a: {
			int r;
			r = (op&3) | ((op>>6)&4); // dst
			tmpv = (op >> 4) & 0xf;   // src
			if (tmpv >= 8)  tr_unhandled();
			if ((r&3) == 3) tr_unhandled();

			if (known_regb & (1 << tmpv)) {
				known_regs.r[r] = known_regs.gr[tmpv].h;
				known_regb |= 1 << (r + 8);
				dirty_regb |= 1 << (r + 8);
			} else {
				int reg = (r < 4) ? 8 : 9;
				int ror = ((4 - (r&3))*8) & 0x1f;
				tr_read_funcs[tmpv]();
				EOP_BIC_IMM(reg, reg, ror/2, 0xff);		// bic r{7,8}, r{7,8}, <mask>
				EOP_AND_IMM(0, 0, 0, 0xff);			// and r0, r0, 0xff
				EOP_ORR_REG_LSL(reg, reg, 0, (r&3)*8);		// orr r{7,8}, r{7,8}, r0, lsl #lsl
				hostreg_r[0] = -1;
				known_regb &= ~(1 << (r+8));
				dirty_regb &= ~(1 << (r+8));
			}
			ret++; break;
		}

		// ldi ri, simm
		case 0x0c ... 0x0f:
			tmpv = (op>>8)&7;
			known_regs.r[tmpv] = op;
			known_regb |= 1 << (tmpv + 8);
			dirty_regb |= 1 << (tmpv + 8);
			ret++; break;

		// call cond, addr
		case 0x24: {
			u32 *jump_op = NULL;
			tmpv = tr_cond_check(op);
			if (tmpv != A_COND_AL) {
				jump_op = tcache_ptr;
				EOP_MOV_IMM(0, 0, 0); // placeholder for branch
			}
			tr_mov16(0, *pc);
			tr_r0_to_STACK(*pc);
			if (tmpv != A_COND_AL) {
				u32 *real_ptr = tcache_ptr;
				tcache_ptr = jump_op;
				EOP_C_B(tr_neg_cond(tmpv),0,real_ptr - jump_op - 2);
				tcache_ptr = real_ptr;
			}
			tr_mov16_cond(tmpv, 0, imm);
			if (tmpv != A_COND_AL) {
				tr_mov16_cond(tr_neg_cond(tmpv), 0, *pc);
			}
			tr_r0_to_PC(tmpv == A_COND_AL ? imm : -1);
			ret += 2; break;
		}

		// ld d, (a)
		case 0x25:
			tmpv2 = (op >> 4) & 0xf;  // dst
			if (tmpv2 >= 8) return -1; // TODO

			tr_A_to_r0();
			EOP_LDR_IMM(1,7,0x48c);					// ptr_iram_rom
			EOP_ADD_REG_LSL(0,1,0,1);				// add  r0, r1, r0, lsl #1
			EOP_LDRH_SIMPLE(0,0);					// ldrh r0, [r0]
			hostreg_r[0] = hostreg_r[1] = -1;
			tr_write_funcs[tmpv2](-1);
			ret += 3; break;

		// bra cond, addr
		case 0x26:
			tmpv = tr_cond_check(op);
			tr_mov16_cond(tmpv, 0, imm);
			if (tmpv != A_COND_AL) {
				tr_mov16_cond(tr_neg_cond(tmpv), 0, *pc);
			}
			tr_r0_to_PC(tmpv == A_COND_AL ? imm : -1);
			ret += 2; break;

		// mod cond, op
		case 0x48: {
			// check for repeats of this op
			tmpv = 1; // count
			while (PROGRAM(*pc) == op && (op & 7) != 6) {
				(*pc)++; tmpv++;
			}
			if ((op&0xf0) != 0) // !always
				tr_make_dirty_ST();

			tmpv2 = tr_cond_check(op);
			switch (op & 7) {
				case 2: EOP_C_DOP_REG_XIMM(tmpv2,A_OP_MOV,1,0,5,tmpv,A_AM1_ASR,5); break; // shr (arithmetic)
				case 3: EOP_C_DOP_REG_XIMM(tmpv2,A_OP_MOV,1,0,5,tmpv,A_AM1_LSL,5); break; // shl
				case 6: EOP_C_DOP_IMM(tmpv2,A_OP_RSB,1,5,5,0,0); break; // neg
				case 7: EOP_C_DOP_REG_XIMM(tmpv2,A_OP_EOR,0,5,1,31,A_AM1_ASR,5); // eor  r1, r5, r5, asr #31
					EOP_C_DOP_REG_XIMM(tmpv2,A_OP_ADD,1,1,5,31,A_AM1_LSR,5); // adds r5, r1, r5, lsr #31
					hostreg_r[1] = -1; break; // abs
				default: tr_unhandled();
			}

			hostreg_sspreg_changed(SSP_A);
			dirty_regb |=  KRREG_ST;
			known_regb &= ~KRREG_ST;
			known_regb &= ~(KRREG_A|KRREG_AL);
			ret += tmpv; break;
		}
/*
		// mpys?
		case 0x1b:
			read_P(); // update P
			rA32 -= rP.v;			// maybe only upper word?
			UPD_ACC_ZN			// there checking flags after this
			rX = ptr1_read_(op&3, 0, (op<<1)&0x18); // ri (maybe rj?)
			rY = ptr1_read_((op>>4)&3, 4, (op>>3)&0x18); // rj
			break;

		// mpya (rj), (ri), b
		case 0x4b:
			read_P(); // update P
			rA32 += rP.v; // confirmed to be 32bit
			UPD_ACC_ZN // ?
			rX = ptr1_read_(op&3, 0, (op<<1)&0x18); // ri (maybe rj?)
			rY = ptr1_read_((op>>4)&3, 4, (op>>3)&0x18); // rj
			break;

		// mld (rj), (ri), b
		case 0x5b:
			EOP_MOV_IMM(5, 0, 0);			// mov r5, #0
			known_regs.r[SSP_A].v = 0;
			known_regb |= (KRREG_A|KRREG_AL);
			EOP_BIC_IMM(6, 6, 0, 0x0f);		// bic r6, r6, 0xf // flags
			EOP_BIC_IMM(6, 6, 0, 0x04);		// bic r6, r6, 4 // set Z
			// TODO
			ret++; break;
*/
	}

	return ret;
}

static void *translate_block(int pc)
{
	unsigned int op, op1, imm, ccount = 0;
	unsigned int *block_start;
	int ret, ret_prev = -1;

	// create .pool
	//*tcache_ptr++ = (u32) in_funcs;			// -1 func pool

	printf("translate %04x -> %04x\n", pc<<1, (tcache_ptr-tcache)<<2);
	block_start = tcache_ptr;
	known_regb = 0;
	dirty_regb = KRREG_P;
	hostreg_clear();

	emit_block_prologue();

	for (; ccount < 100;)
	{
		//printf("  insn #%i\n", icount);
		op = PROGRAM(pc++);
		op1 = op >> 9;
		imm = (u32)-1;

		if ((op1 & 0xf) == 4 || (op1 & 0xf) == 6)
			imm = PROGRAM(pc++); // immediate

		ret = translate_op(op, &pc, imm);
		if (ret <= 0)
		{
			tr_flush_dirty_prs();
			tr_flush_dirty_ST();

			emit_mov_const(A_COND_AL, 0, op);

			// need immediate?
			if (imm != (u32)-1)
				emit_mov_const(A_COND_AL, 1, imm);

			// dump PC
			emit_pc_dump(pc);

			if (ret_prev > 0) emit_call(regfile_store);
			emit_call(in_funcs[op1]);
			emit_call(regfile_load);

			if (in_funcs[op1] == NULL) {
				printf("NULL func! op=%08x (%02x)\n", op, op1);
				exit(1);
			}
			ccount++;
			hostreg_clear();
			dirty_regb |= KRREG_P;
			known_regb = 0;
		}
		else
			ccount += ret;

		if (op1 == 0x24 || op1 == 0x26 || // call, bra
			((op1 == 0 || op1 == 1 || op1 == 4 || op1 == 5 || op1 == 9 || op1 == 0x25) &&
				(op & 0xf0) == 0x60)) { // ld PC
			break;
		}
		ret_prev = ret;
	}

	tr_flush_dirty_prs();
	tr_flush_dirty_ST();
	emit_block_epilogue(ccount + 1);
	*tcache_ptr++ = 0xffffffff; // end of block
	//printf("  %i inst\n", icount);

	if (tcache_ptr - tcache > TCACHE_SIZE/4) {
		printf("tcache overflow!\n");
		fflush(stdout);
		exit(1);
	}

	// stats
	nblocks++;
	//if (pc >= 0x400)
	printf("%i blocks, %i bytes\n", nblocks, (tcache_ptr - tcache)*4);
	//printf("%p %p\n", tcache_ptr, emit_block_epilogue);

#ifdef DUMP_BLOCK
	{
		FILE *f = fopen("tcache.bin", "wb");
		fwrite(tcache, 1, (tcache_ptr - tcache)*4, f);
		fclose(f);
	}
	exit(0);
#endif

	handle_caches();

	return block_start;
}



// -----------------------------------------------------

int ssp1601_dyn_startup(void)
{
	memset(tcache, 0, TCACHE_SIZE);
	memset(block_table, 0, sizeof(block_table));
	memset(block_table_iram, 0, sizeof(block_table_iram));
	tcache_ptr = tcache;
	*tcache_ptr++ = 0xffffffff;

// TODO: rm
{
static unsigned short dummy = 0;
PC = &dummy;
}
	return 0;
}


void ssp1601_dyn_reset(ssp1601_t *ssp)
{
	ssp1601_reset_local(ssp);
	ssp->ptr_rom = (unsigned int) Pico.rom;
	ssp->ptr_iram_rom = (unsigned int) svp->iram_rom;
	ssp->ptr_dram = (unsigned int) svp->dram;
}

void ssp1601_dyn_run(int cycles)
{
	if (ssp->emu_status & SSP_WAIT_MASK) return;
	//{ printf("%i wait\n", Pico.m.frame_count); return; }
	//printf("%i  %04x\n", Pico.m.frame_count, rPC<<1);

#ifdef DUMP_BLOCK
	rPC = DUMP_BLOCK >> 1;
#endif
	while (cycles > 0)
	{
		int (*trans_entry)(void);
		if (rPC < 0x800/2)
		{
			if (iram_dirty) {
				iram_context = get_iram_context();
				iram_dirty--;
			}
			if (block_table_iram[iram_context][rPC] == NULL)
				block_table_iram[iram_context][rPC] = translate_block(rPC);
			trans_entry = (void *) block_table_iram[iram_context][rPC];
		}
		else
		{
			if (block_table[rPC] == NULL)
				block_table[rPC] = translate_block(rPC);
			trans_entry = (void *) block_table[rPC];
		}

		//printf("enter %04x\n", rPC<<1);
		cycles -= trans_entry();
		//printf("leave %04x\n", rPC<<1);
	}
//	debug_dump2file("tcache.bin", tcache, (tcache_ptr - tcache) << 1);
//	exit(1);
}

