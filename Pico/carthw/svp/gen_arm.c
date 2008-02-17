#define EMIT(x) *tcache_ptr++ = x

#define A_R14M (1 << 14)

#define A_COND_AL 0xe

/* addressing mode 1 */
#define A_AM1_LSL 0
#define A_AM1_LSR 1
#define A_AM1_ASR 2
#define A_AM1_ROR 3

#define A_AM1_IMM(ror2,imm8)                  (((ror2)<<8) | (imm8) | 0x02000000)
#define A_AM1_REG_XIMM(shift_imm,shift_op,rm) (((shift_imm)<<7) | ((shift_op)<<5) | (rm))

/* data processing op */
#define A_OP_ORR 0xc
#define A_OP_MOV 0xd

#define EOP_C_DOP_X(cond,op,s,rn,rd,shifter_op) \
	EMIT(((cond)<<28) | ((op)<< 21) | ((s)<<20) | ((rn)<<16) | ((rd)<<12) | (shifter_op))

#define EOP_C_DOP_IMM(cond,op,s,rn,rd,ror2,imm8)             EOP_C_DOP_X(cond,op,s,rn,rd,A_AM1_IMM(ror2,imm8))
#define EOP_C_DOP_REG(cond,op,s,rn,rd,shift_imm,shift_op,rm) EOP_C_DOP_X(cond,op,s,rn,rd,A_AM1_REG_XIMM(shift_imm,shift_op,rm))

#define EOP_MOV_IMM(s,   rd,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_MOV,s, 0,rd,ror2,imm8)
#define EOP_ORR_IMM(s,rn,rd,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_ORR,s,rn,rd,ror2,imm8)

#define EOP_MOV_REG(s,   rd,shift_imm,shift_op,rm) EOP_C_DOP_REG(A_COND_AL,A_OP_MOV,s, 0,rd,shift_imm,shift_op,rm)

#define EOP_MOV_REG_SIMPLE(rd,rm) EOP_MOV_REG(0,rd,0,A_AM1_LSL,rm)

/* ldr and str */
#define EOP_C_XXR_IMM(cond,u,b,l,rn,rd,offset_12) \
	EMIT(((cond)<<28) | 0x05000000 | ((u)<<23) | ((b)<<22) | ((l)<<20) | ((rn)<<16) | ((rd)<<12) | (offset_12))

#define EOP_LDR_IMM(   rd,rn,offset_12) EOP_C_XXR_IMM(A_COND_AL,1,0,1,rn,rd,offset_12)
#define EOP_LDR_NEGIMM(rd,rn,offset_12) EOP_C_XXR_IMM(A_COND_AL,0,0,1,rn,rd,offset_12)
#define EOP_LDR_SIMPLE(rd,rn)           EOP_C_XXR_IMM(A_COND_AL,1,0,1,rn,rd,0)
#define EOP_STR_SIMPLE(rd,rn)           EOP_C_XXR_IMM(A_COND_AL,1,0,0,rn,rd,0)

/* ldm and stm */
#define EOP_XXM(cond,p,u,s,w,l,rn,list) \
	EMIT(((cond)<<28) | (1<<27) | ((p)<<24) | ((u)<<23) | ((s)<<22) | ((w)<<21) | ((l)<<20) | ((rn)<<16) | (list))

#define EOP_STMFD_ST(list) EOP_XXM(A_COND_AL,1,0,0,1,0,13,list)
#define EOP_LDMFD_ST(list) EOP_XXM(A_COND_AL,0,1,0,1,1,13,list)

/* branches */
#define EOP_C_BX(cond,rm) \
	EMIT(((cond)<<28) | 0x012fff10 | (rm))

#define EOP_BX(rm) EOP_C_BX(A_COND_AL,rm)


static void emit_mov_const16(int d, unsigned int val)
{
	int need_or = 0;
	if (val & 0xff00) {
		EOP_MOV_IMM(0, d, 24/2, (val>>8)&0xff);
		need_or = 1;
	}
	if ((val & 0xff) || !need_or)
		EOP_C_DOP_IMM(A_COND_AL,need_or ? A_OP_ORR : A_OP_MOV, 0, d, d, 0, val&0xff);
}

static void emit_block_prologue(void)
{
	// stack LR
	EOP_STMFD_ST(A_R14M);			// stmfd r13!, {r14}
}

static void emit_block_epilogue(unsigned int *block_start, int icount)
{
	int back = (tcache_ptr - block_start) + 2;
	back += 3; // g_cycles
	EOP_LDR_NEGIMM(2,15,back<<2);		// ldr r2,[pc,#back]
	emit_mov_const16(3, icount);
	EOP_STR_SIMPLE(3,2);			// str r3,[r2]

	EOP_LDMFD_ST(A_R14M);			// ldmfd r13!, {r14}
	EOP_BX(14);				// bx r14
}

static void emit_pc_inc(unsigned int *block_start, int pc)
{
	int back = (tcache_ptr - block_start) + 2;
	back += 2; // rPC ptr
	EOP_LDR_NEGIMM(2,15,back<<2);		// ldr r2,[pc,#back]
	emit_mov_const16(3, pc);
	EOP_STR_SIMPLE(3,2);			// str r3,[r2]
}

static void emit_call(unsigned int *block_start, unsigned int op1)
{
	int back = (tcache_ptr - block_start) + 2;
	back += 1; // func table
	EOP_LDR_NEGIMM(2,15,back<<2);		// ldr r2,[pc,#back]
	EOP_MOV_REG_SIMPLE(14,15);		// mov lr,pc
	EOP_LDR_IMM(15,2,op1<<2);		// ldr pc,[r2,#op1]
}


