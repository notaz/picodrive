// Basic macros to emit ARM instructions and some utils

// (c) Copyright 2008-2009, Grazvydas "notaz" Ignotas
// Free for non-commercial use.

#define CONTEXT_REG 7

// XXX: tcache_ptr type for SVP and SH2 compilers differs..
#define EMIT_PTR(ptr, x) \
	do { \
		*(u32 *)ptr = x; \
		ptr = (void *)((u8 *)ptr + sizeof(u32)); \
		COUNT_OP; \
	} while (0)

#define EMIT(x) EMIT_PTR(tcache_ptr, x)

#define A_R4M  (1 << 4)
#define A_R5M  (1 << 5)
#define A_R6M  (1 << 6)
#define A_R7M  (1 << 7)
#define A_R8M  (1 << 8)
#define A_R9M  (1 << 9)
#define A_R10M (1 << 10)
#define A_R11M (1 << 11)
#define A_R14M (1 << 14)

#define A_COND_AL 0xe
#define A_COND_EQ 0x0
#define A_COND_NE 0x1
#define A_COND_HS 0x2
#define A_COND_LO 0x3
#define A_COND_MI 0x4
#define A_COND_PL 0x5
#define A_COND_VS 0x6
#define A_COND_VC 0x7
#define A_COND_HI 0x8
#define A_COND_LS 0x9
#define A_COND_GE 0xa
#define A_COND_LT 0xb
#define A_COND_GT 0xc
#define A_COND_LE 0xd

/* unified conditions */
#define DCOND_EQ A_COND_EQ
#define DCOND_NE A_COND_NE
#define DCOND_MI A_COND_MI
#define DCOND_PL A_COND_PL
#define DCOND_HI A_COND_HI
#define DCOND_HS A_COND_HS
#define DCOND_LO A_COND_LO
#define DCOND_GE A_COND_GE
#define DCOND_GT A_COND_GT
#define DCOND_LT A_COND_LT
#define DCOND_LS A_COND_LS
#define DCOND_LE A_COND_LE
#define DCOND_VS A_COND_VS
#define DCOND_VC A_COND_VC
#define DCOND_CS A_COND_HS
#define DCOND_CC A_COND_LO

/* addressing mode 1 */
#define A_AM1_LSL 0
#define A_AM1_LSR 1
#define A_AM1_ASR 2
#define A_AM1_ROR 3

#define A_AM1_IMM(ror2,imm8)                  (((ror2)<<8) | (imm8) | 0x02000000)
#define A_AM1_REG_XIMM(shift_imm,shift_op,rm) (((shift_imm)<<7) | ((shift_op)<<5) | (rm))
#define A_AM1_REG_XREG(rs,shift_op,rm)        (((rs)<<8) | ((shift_op)<<5) | 0x10 | (rm))

/* data processing op */
#define A_OP_AND 0x0
#define A_OP_EOR 0x1
#define A_OP_SUB 0x2
#define A_OP_RSB 0x3
#define A_OP_ADD 0x4
#define A_OP_ADC 0x5
#define A_OP_SBC 0x6
#define A_OP_TST 0x8
#define A_OP_TEQ 0x9
#define A_OP_CMP 0xa
#define A_OP_ORR 0xc
#define A_OP_MOV 0xd
#define A_OP_BIC 0xe
#define A_OP_MVN 0xf

#define EOP_C_DOP_X(cond,op,s,rn,rd,shifter_op) \
	EMIT(((cond)<<28) | ((op)<< 21) | ((s)<<20) | ((rn)<<16) | ((rd)<<12) | (shifter_op))

#define EOP_C_DOP_IMM(     cond,op,s,rn,rd,ror2,imm8)             EOP_C_DOP_X(cond,op,s,rn,rd,A_AM1_IMM(ror2,imm8))
#define EOP_C_DOP_REG_XIMM(cond,op,s,rn,rd,shift_imm,shift_op,rm) EOP_C_DOP_X(cond,op,s,rn,rd,A_AM1_REG_XIMM(shift_imm,shift_op,rm))
#define EOP_C_DOP_REG_XREG(cond,op,s,rn,rd,rs,       shift_op,rm) EOP_C_DOP_X(cond,op,s,rn,rd,A_AM1_REG_XREG(rs,       shift_op,rm))

#define EOP_MOV_IMM(rd,   ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_MOV,0, 0,rd,ror2,imm8)
#define EOP_ORR_IMM(rd,rn,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_ORR,0,rn,rd,ror2,imm8)
#define EOP_EOR_IMM(rd,rn,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_EOR,0,rn,rd,ror2,imm8)
#define EOP_ADD_IMM(rd,rn,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_ADD,0,rn,rd,ror2,imm8)
#define EOP_BIC_IMM(rd,rn,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_BIC,0,rn,rd,ror2,imm8)
#define EOP_AND_IMM(rd,rn,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_AND,0,rn,rd,ror2,imm8)
#define EOP_SUB_IMM(rd,rn,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_SUB,0,rn,rd,ror2,imm8)
#define EOP_TST_IMM(   rn,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_TST,1,rn, 0,ror2,imm8)
#define EOP_CMP_IMM(   rn,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_CMP,1,rn, 0,ror2,imm8)
#define EOP_RSB_IMM(rd,rn,ror2,imm8) EOP_C_DOP_IMM(A_COND_AL,A_OP_RSB,0,rn,rd,ror2,imm8)

#define EOP_MOV_IMM_C(cond,rd,   ror2,imm8) EOP_C_DOP_IMM(cond,A_OP_MOV,0, 0,rd,ror2,imm8)
#define EOP_ORR_IMM_C(cond,rd,rn,ror2,imm8) EOP_C_DOP_IMM(cond,A_OP_ORR,0,rn,rd,ror2,imm8)
#define EOP_RSB_IMM_C(cond,rd,rn,ror2,imm8) EOP_C_DOP_IMM(cond,A_OP_RSB,0,rn,rd,ror2,imm8)

#define EOP_MOV_REG(cond,s,rd,   rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_MOV,s, 0,rd,shift_imm,shift_op,rm)
#define EOP_ORR_REG(cond,s,rd,rn,rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_ORR,s,rn,rd,shift_imm,shift_op,rm)
#define EOP_ADD_REG(cond,s,rd,rn,rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_ADD,s,rn,rd,shift_imm,shift_op,rm)
#define EOP_ADC_REG(cond,s,rd,rn,rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_ADC,s,rn,rd,shift_imm,shift_op,rm)
#define EOP_SUB_REG(cond,s,rd,rn,rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_SUB,s,rn,rd,shift_imm,shift_op,rm)
#define EOP_SBC_REG(cond,s,rd,rn,rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_SBC,s,rn,rd,shift_imm,shift_op,rm)
#define EOP_AND_REG(cond,s,rd,rn,rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_AND,s,rn,rd,shift_imm,shift_op,rm)
#define EOP_EOR_REG(cond,s,rd,rn,rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_EOR,s,rn,rd,shift_imm,shift_op,rm)
#define EOP_CMP_REG(cond,     rn,rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_CMP,1,rn, 0,shift_imm,shift_op,rm)
#define EOP_TST_REG(cond,     rn,rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_TST,1,rn, 0,shift_imm,shift_op,rm)
#define EOP_TEQ_REG(cond,     rn,rm,shift_op,shift_imm) EOP_C_DOP_REG_XIMM(cond,A_OP_TEQ,1,rn, 0,shift_imm,shift_op,rm)

#define EOP_MOV_REG2(s,rd,   rm,shift_op,rs) EOP_C_DOP_REG_XREG(A_COND_AL,A_OP_MOV,s, 0,rd,rs,shift_op,rm)
#define EOP_ADD_REG2(s,rd,rn,rm,shift_op,rs) EOP_C_DOP_REG_XREG(A_COND_AL,A_OP_ADD,s,rn,rd,rs,shift_op,rm)
#define EOP_SUB_REG2(s,rd,rn,rm,shift_op,rs) EOP_C_DOP_REG_XREG(A_COND_AL,A_OP_SUB,s,rn,rd,rs,shift_op,rm)

#define EOP_MOV_REG_SIMPLE(rd,rm)           EOP_MOV_REG(A_COND_AL,0,rd,rm,A_AM1_LSL,0)
#define EOP_MOV_REG_LSL(rd,   rm,shift_imm) EOP_MOV_REG(A_COND_AL,0,rd,rm,A_AM1_LSL,shift_imm)
#define EOP_MOV_REG_LSR(rd,   rm,shift_imm) EOP_MOV_REG(A_COND_AL,0,rd,rm,A_AM1_LSR,shift_imm)
#define EOP_MOV_REG_ASR(rd,   rm,shift_imm) EOP_MOV_REG(A_COND_AL,0,rd,rm,A_AM1_ASR,shift_imm)
#define EOP_MOV_REG_ROR(rd,   rm,shift_imm) EOP_MOV_REG(A_COND_AL,0,rd,rm,A_AM1_ROR,shift_imm)

#define EOP_ORR_REG_SIMPLE(rd,rm)           EOP_ORR_REG(A_COND_AL,0,rd,rd,rm,A_AM1_LSL,0)
#define EOP_ORR_REG_LSL(rd,rn,rm,shift_imm) EOP_ORR_REG(A_COND_AL,0,rd,rn,rm,A_AM1_LSL,shift_imm)
#define EOP_ORR_REG_LSR(rd,rn,rm,shift_imm) EOP_ORR_REG(A_COND_AL,0,rd,rn,rm,A_AM1_LSR,shift_imm)
#define EOP_ORR_REG_ASR(rd,rn,rm,shift_imm) EOP_ORR_REG(A_COND_AL,0,rd,rn,rm,A_AM1_ASR,shift_imm)
#define EOP_ORR_REG_ROR(rd,rn,rm,shift_imm) EOP_ORR_REG(A_COND_AL,0,rd,rn,rm,A_AM1_ROR,shift_imm)

#define EOP_ADD_REG_SIMPLE(rd,rm)           EOP_ADD_REG(A_COND_AL,0,rd,rd,rm,A_AM1_LSL,0)
#define EOP_ADD_REG_LSL(rd,rn,rm,shift_imm) EOP_ADD_REG(A_COND_AL,0,rd,rn,rm,A_AM1_LSL,shift_imm)
#define EOP_ADD_REG_LSR(rd,rn,rm,shift_imm) EOP_ADD_REG(A_COND_AL,0,rd,rn,rm,A_AM1_LSR,shift_imm)

#define EOP_TST_REG_SIMPLE(rn,rm)           EOP_TST_REG(A_COND_AL,  rn,   0,A_AM1_LSL,rm)

#define EOP_MOV_REG2_LSL(rd,   rm,rs)       EOP_MOV_REG2(0,rd,   rm,A_AM1_LSL,rs)
#define EOP_MOV_REG2_ROR(rd,   rm,rs)       EOP_MOV_REG2(0,rd,   rm,A_AM1_ROR,rs)
#define EOP_ADD_REG2_LSL(rd,rn,rm,rs)       EOP_ADD_REG2(0,rd,rn,rm,A_AM1_LSL,rs)
#define EOP_SUB_REG2_LSL(rd,rn,rm,rs)       EOP_SUB_REG2(0,rd,rn,rm,A_AM1_LSL,rs)

/* addressing mode 2 */
#define EOP_C_AM2_IMM(cond,u,b,l,rn,rd,offset_12) \
	EMIT(((cond)<<28) | 0x05000000 | ((u)<<23) | ((b)<<22) | ((l)<<20) | ((rn)<<16) | ((rd)<<12) | (offset_12))

/* addressing mode 3 */
#define EOP_C_AM3(cond,u,r,l,rn,rd,s,h,immed_reg) \
	EMIT(((cond)<<28) | 0x01000090 | ((u)<<23) | ((r)<<22) | ((l)<<20) | ((rn)<<16) | ((rd)<<12) | \
			((s)<<6) | ((h)<<5) | (immed_reg))

#define EOP_C_AM3_IMM(cond,u,l,rn,rd,s,h,offset_8) EOP_C_AM3(cond,u,1,l,rn,rd,s,h,(((offset_8)&0xf0)<<4)|((offset_8)&0xf))

#define EOP_C_AM3_REG(cond,u,l,rn,rd,s,h,rm)       EOP_C_AM3(cond,u,0,l,rn,rd,s,h,rm)

/* ldr and str */
#define EOP_LDR_IMM(   rd,rn,offset_12) EOP_C_AM2_IMM(A_COND_AL,1,0,1,rn,rd,offset_12)
#define EOP_LDR_NEGIMM(rd,rn,offset_12) EOP_C_AM2_IMM(A_COND_AL,0,0,1,rn,rd,offset_12)
#define EOP_LDR_SIMPLE(rd,rn)           EOP_C_AM2_IMM(A_COND_AL,1,0,1,rn,rd,0)
#define EOP_STR_IMM(   rd,rn,offset_12) EOP_C_AM2_IMM(A_COND_AL,1,0,0,rn,rd,offset_12)
#define EOP_STR_SIMPLE(rd,rn)           EOP_C_AM2_IMM(A_COND_AL,1,0,0,rn,rd,0)

#define EOP_LDRH_IMM(   rd,rn,offset_8)  EOP_C_AM3_IMM(A_COND_AL,1,1,rn,rd,0,1,offset_8)
#define EOP_LDRH_SIMPLE(rd,rn)           EOP_C_AM3_IMM(A_COND_AL,1,1,rn,rd,0,1,0)
#define EOP_LDRH_REG(   rd,rn,rm)        EOP_C_AM3_REG(A_COND_AL,1,1,rn,rd,0,1,rm)
#define EOP_STRH_IMM(   rd,rn,offset_8)  EOP_C_AM3_IMM(A_COND_AL,1,0,rn,rd,0,1,offset_8)
#define EOP_STRH_SIMPLE(rd,rn)           EOP_C_AM3_IMM(A_COND_AL,1,0,rn,rd,0,1,0)
#define EOP_STRH_REG(   rd,rn,rm)        EOP_C_AM3_REG(A_COND_AL,1,0,rn,rd,0,1,rm)

/* ldm and stm */
#define EOP_XXM(cond,p,u,s,w,l,rn,list) \
	EMIT(((cond)<<28) | (1<<27) | ((p)<<24) | ((u)<<23) | ((s)<<22) | ((w)<<21) | ((l)<<20) | ((rn)<<16) | (list))

#define EOP_STMFD_ST(list) EOP_XXM(A_COND_AL,1,0,0,1,0,13,list)
#define EOP_LDMFD_ST(list) EOP_XXM(A_COND_AL,0,1,0,1,1,13,list)

/* branches */
#define EOP_C_BX(cond,rm) \
	EMIT(((cond)<<28) | 0x012fff10 | (rm))

#define EOP_BX(rm) EOP_C_BX(A_COND_AL,rm)

#define EOP_C_B(cond,l,signed_immed_24) \
	EMIT(((cond)<<28) | 0x0a000000 | ((l)<<24) | (signed_immed_24))

#define EOP_B( signed_immed_24) EOP_C_B(A_COND_AL,0,signed_immed_24)
#define EOP_BL(signed_immed_24) EOP_C_B(A_COND_AL,1,signed_immed_24)

/* misc */
#define EOP_C_MUL(cond,s,rd,rs,rm) \
	EMIT(((cond)<<28) | ((s)<<20) | ((rd)<<16) | ((rs)<<8) | 0x90 | (rm))

#define EOP_C_UMULL(cond,s,rdhi,rdlo,rs,rm) \
	EMIT(((cond)<<28) | 0x00800000 | ((s)<<20) | ((rdhi)<<16) | ((rdlo)<<12) | ((rs)<<8) | 0x90 | (rm))

#define EOP_C_SMULL(cond,s,rdhi,rdlo,rs,rm) \
	EMIT(((cond)<<28) | 0x00c00000 | ((s)<<20) | ((rdhi)<<16) | ((rdlo)<<12) | ((rs)<<8) | 0x90 | (rm))

#define EOP_MUL(rd,rm,rs) EOP_C_MUL(A_COND_AL,0,rd,rs,rm) // note: rd != rm

#define EOP_C_MRS(cond,rd) \
	EMIT(((cond)<<28) | 0x010f0000 | ((rd)<<12))

#define EOP_C_MSR_IMM(cond,ror2,imm) \
	EMIT(((cond)<<28) | 0x0328f000 | ((ror2)<<8) | (imm)) // cpsr_f

#define EOP_C_MSR_REG(cond,rm) \
	EMIT(((cond)<<28) | 0x0128f000 | (rm)) // cpsr_f

#define EOP_MRS(rd)           EOP_C_MRS(A_COND_AL,rd)
#define EOP_MSR_IMM(ror2,imm) EOP_C_MSR_IMM(A_COND_AL,ror2,imm)
#define EOP_MSR_REG(rm)       EOP_C_MSR_REG(A_COND_AL,rm)


static void emith_op_imm(int cond, int s, int op, int r, unsigned int imm)
{
	int ror2, rd = r, rn = r;
	u32 v;

	if (op == A_OP_MOV)
		rn = 0;
	else if (op == A_OP_TST || op == A_OP_TEQ)
		rd = 0;
	else if (imm == 0)
		return;

	for (v = imm, ror2 = 0; v != 0 || op == A_OP_MOV; v >>= 8, ror2 -= 8/2) {
		/* shift down to get 'best' rot2 */
		for (; v && !(v & 3); v >>= 2)
			ror2--;

		EOP_C_DOP_IMM(cond, op, s, rn, rd, ror2 & 0x0f, v & 0xff);

		if (op == A_OP_MOV) {
			op = A_OP_ORR;
			rn = r;
		}
	}
}

#define is_offset_24(val) \
	((val) >= (int)0xff000000 && (val) <= 0x00ffffff)

static int emith_xbranch(int cond, void *target, int is_call)
{
	int val = (u32 *)target - (u32 *)tcache_ptr - 2;
	int direct = is_offset_24(val);
	u32 *start_ptr = (u32 *)tcache_ptr;

	if (direct)
	{
		EOP_C_B(cond,is_call,val & 0xffffff);		// b, bl target
	}
	else
	{
#ifdef __EPOC32__
//		elprintf(EL_SVP, "emitting indirect jmp %08x->%08x", tcache_ptr, target);
		if (is_call)
			EOP_ADD_IMM(14,15,0,8);			// add lr,pc,#8
		EOP_C_AM2_IMM(cond,1,0,1,15,15,0);		// ldrcc pc,[pc]
		EOP_MOV_REG_SIMPLE(15,15);			// mov pc, pc
		EMIT((u32)target);
#else
		// should never happen
		elprintf(EL_STATUS|EL_SVP|EL_ANOMALY, "indirect jmp %08x->%08x", target, tcache_ptr);
		exit(1);
#endif
	}

	return (u32 *)tcache_ptr - start_ptr;
}


// fake "simple" or "short" jump - using cond insns instead
#define EMITH_SJMP_START(cond) \
	(void)(cond)

#define EMITH_SJMP_END(cond) \
	(void)(cond)

#define emith_move_r_r(d, s) \
	EOP_MOV_REG_SIMPLE(d, s)

#define emith_or_r_r_r_lsl(d, s1, s2, lslimm) \
	EOP_ORR_REG(A_COND_AL,0,d,s1,s2,A_AM1_LSL,lslimm)

#define emith_eor_r_r_r_lsl(d, s1, s2, lslimm) \
	EOP_EOR_REG(A_COND_AL,0,d,s1,s2,A_AM1_LSL,lslimm)

#define emith_or_r_r_r(d, s1, s2) \
	emith_or_r_r_r_lsl(d, s1, s2, 0)

#define emith_eor_r_r_r(d, s1, s2) \
	emith_eor_r_r_r_lsl(d, s1, s2, 0)

#define emith_add_r_r(d, s) \
	EOP_ADD_REG(A_COND_AL,0,d,d,s,A_AM1_LSL,0)

#define emith_sub_r_r(d, s) \
	EOP_SUB_REG(A_COND_AL,0,d,d,s,A_AM1_LSL,0)

#define emith_and_r_r(d, s) \
	EOP_AND_REG(A_COND_AL,0,d,d,s,A_AM1_LSL,0)

#define emith_or_r_r(d, s) \
	emith_or_r_r_r(d, d, s)

#define emith_eor_r_r(d, s) \
	emith_eor_r_r_r(d, d, s)

#define emith_tst_r_r(d, s) \
	EOP_TST_REG(A_COND_AL,d,s,A_AM1_LSL,0)

#define emith_teq_r_r(d, s) \
	EOP_TEQ_REG(A_COND_AL,d,s,A_AM1_LSL,0)

#define emith_cmp_r_r(d, s) \
	EOP_CMP_REG(A_COND_AL,d,s,A_AM1_LSL,0)

#define emith_addf_r_r(d, s) \
	EOP_ADD_REG(A_COND_AL,1,d,d,s,A_AM1_LSL,0)

#define emith_subf_r_r(d, s) \
	EOP_SUB_REG(A_COND_AL,1,d,d,s,A_AM1_LSL,0)

#define emith_adcf_r_r(d, s) \
	EOP_ADC_REG(A_COND_AL,1,d,d,s,A_AM1_LSL,0)

#define emith_sbcf_r_r(d, s) \
	EOP_SBC_REG(A_COND_AL,1,d,d,s,A_AM1_LSL,0)

#define emith_move_r_imm(r, imm) \
	emith_op_imm(A_COND_AL, 0, A_OP_MOV, r, imm)

#define emith_add_r_imm(r, imm) \
	emith_op_imm(A_COND_AL, 0, A_OP_ADD, r, imm)

#define emith_sub_r_imm(r, imm) \
	emith_op_imm(A_COND_AL, 0, A_OP_SUB, r, imm)

#define emith_bic_r_imm(r, imm) \
	emith_op_imm(A_COND_AL, 0, A_OP_BIC, r, imm)

#define emith_or_r_imm(r, imm) \
	emith_op_imm(A_COND_AL, 0, A_OP_ORR, r, imm)

// note: use 8bit imm only
#define emith_tst_r_imm(r, imm) \
	emith_op_imm(A_COND_AL, 1, A_OP_TST, r, imm)

#define emith_subf_r_imm(r, imm) \
	emith_op_imm(A_COND_AL, 1, A_OP_SUB, r, imm)

#define emith_add_r_imm_c(cond, r, imm) \
	emith_op_imm(cond, 0, A_OP_ADD, r, imm)

#define emith_sub_r_imm_c(cond, r, imm) \
	emith_op_imm(cond, 0, A_OP_SUB, r, imm)

#define emith_or_r_imm_c(cond, r, imm) \
	emith_op_imm(cond, 0, A_OP_ORR, r, imm)

#define emith_bic_r_imm_c(cond, r, imm) \
	emith_op_imm(cond, 0, A_OP_BIC, r, imm)

#define emith_lsl(d, s, cnt) \
	EOP_MOV_REG(A_COND_AL,0,d,s,A_AM1_LSL,cnt)

#define emith_lsr(d, s, cnt) \
	EOP_MOV_REG(A_COND_AL,0,d,s,A_AM1_LSR,cnt)

#define emith_lslf(d, s, cnt) \
	EOP_MOV_REG(A_COND_AL,1,d,s,A_AM1_LSL,cnt)

#define emith_asrf(d, s, cnt) \
	EOP_MOV_REG(A_COND_AL,1,d,s,A_AM1_ASR,cnt)

#define emith_mul(d, s1, s2) { \
	if ((d) != (s1)) /* rd != rm limitation */ \
		EOP_MUL(d, s1, s2); \
	else \
		EOP_MUL(d, s2, s1); \
}

#define emith_mul_u64(dlo, dhi, s1, s2) \
	EOP_C_UMULL(A_COND_AL,0,dhi,dlo,s1,s2)

#define emith_mul_s64(dlo, dhi, s1, s2) \
	EOP_C_SMULL(A_COND_AL,0,dhi,dlo,s1,s2)

// misc
#define emith_ctx_read(r, offs) \
	EOP_LDR_IMM(r, CONTEXT_REG, offs)

#define emith_ctx_write(r, offs) \
	EOP_STR_IMM(r, CONTEXT_REG, offs)

#define emith_clear_msb(d, s, count) { \
	u32 t; \
	if ((count) <= 8) { \
		t = (count) - 8; \
		t = (0xff << t) & 0xff; \
		EOP_BIC_IMM(d,s,8/2,t); \
	} else if ((count) >= 24) { \
		t = (count) - 24; \
		t = 0xff >> t; \
		EOP_AND_IMM(d,s,0,t); \
	} else { \
		EOP_MOV_REG_LSL(d,s,count); \
		EOP_MOV_REG_LSR(d,d,count); \
	} \
}

#define emith_sext(d, s, bits) { \
	EOP_MOV_REG_LSL(d,s,32 - (bits)); \
	EOP_MOV_REG_ASR(d,d,32 - (bits)); \
}

// put bit0 of r0 to carry
#define emith_set_carry(r0) \
	EOP_TST_REG(A_COND_AL,r0,r0,A_AM1_LSR,1) /* shift out to carry */ \

// put bit0 of r0 to carry (for subtraction, inverted on ARM)
#define emith_set_carry_sub(r0) { \
	int t = rcache_get_tmp(); \
	EOP_EOR_IMM(t,r0,0,1); /* invert */ \
	EOP_MOV_REG(A_COND_AL,1,t,t,A_AM1_LSR,1); /* shift out to carry */ \
	rcache_free_tmp(t); \
}

#define host_arg2reg(rd, arg) \
	rd = arg

// upto 4 args
#define emith_pass_arg_r(arg, reg) \
	EOP_MOV_REG_SIMPLE(arg, reg)

#define emith_pass_arg_imm(arg, imm) \
	emith_move_r_imm(arg, imm)

#define emith_call_cond(cond, target) \
	emith_xbranch(cond, target, 1)

#define emith_jump_cond(cond, target) \
	emith_xbranch(cond, target, 0)

#define emith_call(target) \
	emith_call_cond(A_COND_AL, target)

#define emith_jump(target) \
	emith_jump_cond(A_COND_AL, target)

/* SH2 drc specific */
#define emith_sh2_test_t() { \
	int r = rcache_get_reg(SHR_SR, RC_GR_READ); \
	EOP_TST_IMM(r, 0, 1); \
}

#define emith_sh2_dtbf_loop() { \
	int cr, rn;                                                          \
	tmp = rcache_get_tmp();                                              \
	cr = rcache_get_reg(SHR_SR, RC_GR_RMW);                              \
	rn = rcache_get_reg((op >> 8) & 0x0f, RC_GR_RMW);                    \
	emith_sub_r_imm(rn, 1);                /* sub rn, #1 */              \
	emith_bic_r_imm(cr, 1);                /* bic cr, #1 */              \
	emith_sub_r_imm(cr, (cycles+1) << 12); /* sub cr, #(cycles+1)<<12 */ \
	cycles = 0;                                                          \
	emith_asrf(tmp, cr, 2+12);             /* movs tmp, cr, asr #2+12 */ \
	EOP_MOV_IMM_C(A_COND_MI,tmp,0,0);      /* movmi tmp, #0 */           \
	emith_lsl(cr, cr, 20);                 /* mov cr, cr, lsl #20 */     \
	emith_lsr(cr, cr, 20);                 /* mov cr, cr, lsr #20 */     \
	emith_subf_r_r(rn, tmp);               /* subs rn, tmp */            \
	EOP_RSB_IMM_C(A_COND_LS,tmp,rn,0,0);   /* rsbls tmp, rn, #0 */       \
	EOP_ORR_REG(A_COND_LS,0,cr,cr,tmp,A_AM1_LSL,12+2); /* orrls cr,tmp,lsl #12+2 */\
	EOP_ORR_IMM_C(A_COND_LS,cr,cr,0,1);    /* orrls cr, #1 */            \
	EOP_MOV_IMM_C(A_COND_LS,rn,0,0);       /* movls rn, #0 */            \
	rcache_free_tmp(tmp);                                                \
}

