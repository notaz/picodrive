// 187 blocks, 12072 bytes
// 14 IRAM blocks

#include "../../PicoInt.h"
#include "compiler.h"

static unsigned int *block_table[0x5090/2];
static unsigned int *block_table_iram[15][0x800/2];
static unsigned int *tcache_ptr = NULL;

static int had_jump = 0;
static int nblocks = 0;
static int iram_context = 0;

#ifndef ARM
#define DUMP_BLOCK 0x40b0
unsigned int tcache[512*1024];
void regfile_load(void){}
void regfile_store(void){}
#endif

#define EMBED_INTERPRETER
#define ssp1601_reset ssp1601_reset_local
#define ssp1601_run ssp1601_run_local

#define GET_PC() rPC
#define GET_PPC_OFFS() (GET_PC()*2 - 2)
#define SET_PC(d) { had_jump = 1; rPC = d; }		/* must return to dispatcher after this */
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
} const_regs;

#define CRREG_X     (1 << SSP_X)
#define CRREG_Y     (1 << SSP_Y)
#define CRREG_A     (1 << SSP_A)	/* AH only */
#define CRREG_ST    (1 << SSP_ST)
#define CRREG_STACK (1 << SSP_STACK)
#define CRREG_PC    (1 << SSP_PC)
#define CRREG_P     (1 << SSP_P)
#define CRREG_PR0   (1 << 8)
#define CRREG_PR4   (1 << 12)
#define CRREG_AL    (1 << 16)

static u32 const_regb = 0;		/* bitfield of known register values */
static u32 dirty_regb = 0;		/* known vals, which need to be flushed (only r0-r7) */

/* known values of host regs.
 * -1          - unknown
 * 00000-0ffff - 16bit value
 * 10000-1ffff - base reg (r7) + 16bit val
 * 20000       - means reg (low) eq AH
 */
static int hostreg_r[4];

static void hostreg_clear(void)
{
	int i;
	for (i = 0; i < 4; i++)
		hostreg_r[i] = -1;
}

/*static*/ void hostreg_ah_changed(void)
{
	int i;
	for (i = 0; i < 4; i++)
		if (hostreg_r[i] == 0x20000) hostreg_r[i] = -1;
}


#define PROGRAM(x) ((unsigned short *)svp->iram_rom)[x]

/* load 16bit val into host reg r0-r3. Nothing is trashed */
static void tr_mov16(int r, int val)
{
	if (hostreg_r[r] != val) {
		emit_mov_const(r, val);
		hostreg_r[r] = val;
	}
}

/* write dirty r0-r7 to host regs. Nothing is trashed */
static void tr_flush_dirty(void)
{
	int i, ror = 0, reg;
	dirty_regb >>= 8;
	/* r0-r7 */
	for (i = 0; dirty_regb && i < 8; i++, dirty_regb >>= 1)
	{
		if (!(dirty_regb&1)) continue;
		switch (i&3) {
			case 0: ror =    0; break;
			case 1: ror = 24/2; break;
			case 2: ror = 16/2; break;
		}
		reg = (i < 4) ? 8 : 9;
		EOP_BIC_IMM(reg,reg,ror,0xff);
		if (const_regs.r[i] != 0)
			EOP_ORR_IMM(reg,reg,ror,const_regs.r[i]);
	}
}

/* read bank word to r0 (MSW may contain trash). Thrashes r1. */
static void tr_bank_read(int addr) /* word addr 0-0x1ff */
{
	if (addr&1) {
		int breg = 7;
		if (addr > 0x7f) {
			if (hostreg_r[1] != (0x10000|((addr&0x180)<<1))) {
				EOP_ADD_IMM(1,7,30/2,(addr&0x180)>>1);	// add  r1, r7, ((op&0x180)<<1)
				hostreg_r[1] = 0x10000|((addr&0x180)<<1);
			}
			breg = 1;
		}
		EOP_LDRH_IMM(0,breg,(addr&0x7f)<<1);	// ldrh r0, [r1, (op&0x7f)<<1]
	} else {
		EOP_LDR_IMM(0,7,(addr&0x1ff)<<1);	// ldr  r0, [r1, (op&0x1ff)<<1]
	}
	hostreg_r[0] = -1;
}

/* write r0 to bank. Trashes r1. */
static void tr_bank_write(int addr)
{
	int breg = 7;
	if (addr > 0x7f) {
		if (hostreg_r[1] != (0x10000|((addr&0x180)<<1))) {
			EOP_ADD_IMM(1,7,30/2,(addr&0x180)>>1);	// add  r1, r7, ((op&0x180)<<1)
			hostreg_r[1] = 0x10000|((addr&0x180)<<1);
		}
		breg = 1;
	}
	EOP_STRH_IMM(0,breg,(addr&0x7f)<<1);		// str  r0, [r1, (op&0x7f)<<1]
}

/* handle RAM bank pointer modifiers. Nothing is trashed. */
static void tr_ptrr_mod(int r, int mod, int need_modulo)
{
	int modulo = -1, modulo_shift = -1;	/* unknown */

	if (mod == 0) return;

	if (!need_modulo || mod == 1) // +!
		modulo_shift = 8;
	else if (need_modulo && (const_regb & CRREG_ST)) {
		modulo_shift = const_regs.gr[SSP_ST].h & 7;
		if (modulo_shift == 0) modulo_shift = 8;
	}

	if (mod > 1 && modulo_shift == -1) { printf("need var modulo\n"); exit(1); }
	modulo = (1 << modulo_shift) - 1;

	if (const_regb & (1 << (r + 8))) {
		if (mod == 2)
		     const_regs.r[r] = (const_regs.r[r] & ~modulo) | ((const_regs.r[r] - 1) & modulo);
		else const_regs.r[r] = (const_regs.r[r] & ~modulo) | ((const_regs.r[r] + 1) & modulo);
	} else {
		int reg = (r < 4) ? 8 : 9;
		int ror = ((r&3) + 1)*8 - (8 - modulo_shift);
		EOP_MOV_REG_ROR(reg,reg,ror);
		// {add|sub} reg, reg, #1<<shift
		EOP_C_DOP_IMM(A_COND_AL,(mod==2)?A_OP_SUB:A_OP_ADD,0,reg,reg, 8/2, 1<<(8 - modulo_shift));
		EOP_MOV_REG_ROR(reg,reg,32-ror);
	}
}


static int translate_op(unsigned int op, int *pc, int imm)
{
	u32 tmpv;
	int ret = 0;

	switch (op >> 9)
	{
		// ld d, s
		case 0x00:
			if (op == 0) { ret++; break; } // nop
			break;

		// ld a, adr
		case 0x03:
			tr_bank_read(op&0x1ff);
			EOP_MOV_REG_LSL(5, 5, 16);		// mov  r5, r5, lsl #16
			EOP_MOV_REG_LSR(5, 5, 16);		// mov  r5, r5, lsl #16  @ AL
			EOP_ORR_REG_LSL(5, 5, 0, 16);		// orr  r5, r5, r0, lsl #16
			const_regb &= ~CRREG_A;
			hostreg_r[0] = 0x20000;
			ret++; break;

		// ldi (ri), imm
		case 0x06:
			//tmpv = *PC++; ptr1_write(op, tmpv); break;
			// int t = (op&3) | ((op>>6)&4) | ((op<<1)&0x18);
			tr_mov16(0, imm);
			if ((op&3) == 3)
			{
				tmpv = (op>>2) & 3; // direct addressing
				if (op & 0x100) {
					if (hostreg_r[1] != 0x10200) {
						EOP_ADD_IMM(1,7,30/2,0x200>>2);	// add  r1, r7, 0x200
						hostreg_r[1] = 0x10200;
					}
					EOP_STRH_IMM(0,1,tmpv<<1);	// str  r0, [r1, {0,2,4,6}]
				} else {
					EOP_STRH_IMM(0,7,tmpv<<1);	// str  r0, [r7, {0,2,4,6}]
				}
			}
			else
			{
				int r = (op&3) | ((op>>6)&4);
				if (const_regb & (1 << (r + 8))) {
					tr_bank_write(const_regs.r[r] | ((r < 4) ? 0 : 0x100));
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
				tr_ptrr_mod(r, (op>>2) & 3, 0);
			}
			ret++; break;

		// ld adr, a
		case 0x07:
			if (hostreg_r[0] != 0x20000) {
				EOP_MOV_REG_LSR(0, 5, 16);		// mov  r0, r5, lsr #16  @ A
				hostreg_r[0] = 0x20000;
			}
			tr_bank_write(op&0x1ff);
			ret++; break;

		// ldi ri, simm
		case 0x0c ... 0x0f:
			tmpv = (op>>8)&7;
			const_regs.r[tmpv] = op;
			const_regb |= 1 << (tmpv + 8);
			dirty_regb |= 1 << (tmpv + 8);
			ret++; break;
	}

	return ret;
}

static void *translate_block(int pc)
{
	unsigned int op, op1, imm, ccount = 0;
	unsigned int *block_start;
	int ret;

	// create .pool
	//*tcache_ptr++ = (u32) in_funcs;			// -1 func pool

	printf("translate %04x -> %04x\n", pc<<1, (tcache_ptr-tcache)<<2);
	block_start = tcache_ptr;
	const_regb = dirty_regb = 0;
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
			tr_flush_dirty();

			emit_mov_const(0, op);

			// need immediate?
			if (imm != (u32)-1)
				emit_mov_const(1, imm);

			// dump PC
			emit_pc_dump(pc);

			emit_interpreter_call(in_funcs[op1]);

			if (in_funcs[op1] == NULL) {
				printf("NULL func! op=%08x (%02x)\n", op, op1);
				exit(1);
			}
			ccount++;
			hostreg_clear();
		}
		else
			ccount += ret;

		if (op1 == 0x24 || op1 == 0x26 || // call, bra
			((op1 == 0 || op1 == 1 || op1 == 4 || op1 == 5 || op1 == 9 || op1 == 0x25) &&
				(op & 0xf0) == 0x60)) { // ld PC
			break;
		}
	}

	tr_flush_dirty();
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

	return 0;
}


void ssp1601_dyn_reset(ssp1601_t *ssp)
{
	ssp1601_reset_local(ssp);
}

void ssp1601_dyn_run(int cycles)
{
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

		had_jump = 0;

		//printf("enter %04x\n", rPC<<1);
		cycles -= trans_entry();
		//printf("leave %04x\n", rPC<<1);
	}
//	debug_dump2file("tcache.bin", tcache, (tcache_ptr - tcache) << 1);
//	exit(1);
}

