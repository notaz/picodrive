/*
 * Basic macros to emit MIPS II/MIPS32 Release 1 instructions and some utils
 * Copyright (C) 2019 kub
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */
#define HOST_REGS	32
#define CONTEXT_REG	23 // s7
#define RET_REG		2  // v0

// NB: the ubiquitous JZ74[46]0 uses MIPS32 Release 1, a slight MIPS II superset

// registers usable for user code: r1-r25, others reserved or special
#define Z0		0  // zero register
#define	GP		28 // global pointer
#define	SP		29 // stack pointer
#define	FP		30 // frame pointer
#define	LR		31 // link register
// internally used by code emitter:
#define AT		1  // used to hold intermediate results
#define FNZ		15 // emulated processor flags: N (bit 31) ,Z (all bits)
#define FC		24 // emulated processor flags: C (bit 0), others 0
#define FV		25 // emulated processor flags: Nt^Ns (bit 31). others x


// unified conditions; virtual, not corresponding to anything real on MIPS
#define DCOND_EQ 0x0
#define DCOND_NE 0x1
#define DCOND_HS 0x2
#define DCOND_LO 0x3
#define DCOND_MI 0x4
#define DCOND_PL 0x5
#define DCOND_VS 0x6
#define DCOND_VC 0x7
#define DCOND_HI 0x8
#define DCOND_LS 0x9
#define DCOND_GE 0xa
#define DCOND_LT 0xb
#define DCOND_GT 0xc
#define DCOND_LE 0xd

#define DCOND_CS DCOND_LO
#define DCOND_CC DCOND_HS

// unified insn
#define MIPS_INSN(op, rs, rt, rd, sa, fn) \
	(((op)<<26)|((rs)<<21)|((rt)<<16)|((rd)<<11)|((sa)<<6)|((fn)<<0))

#define _	0 // marker for "field unused"
#define __(n)	o##n // enum marker for "undefined"

// opcode field (encoded in op)
enum { OP__FN=000, OP__RT, OP_J, OP_JAL, OP_BEQ, OP_BNE, OP_BLEZ, OP_BGTZ };
enum { OP_ADDI=010, OP_ADDIU, OP_SLTI, OP_SLTIU, OP_ANDI, OP_ORI, OP_XORI, OP_LUI };
enum { OP_LB=040, OP_LH, OP_LWL, OP_LW, OP_LBU, OP_LHU, OP_LWR };
enum { OP_SB=050, OP_SH, OP_SWL, OP_SW, __(54), __(55), OP_SWR };
// function field (encoded in fn if opcode = OP__FN)
enum { FN_SLL=000, __(01), FN_SRL, FN_SRA, FN_SLLV, __(05), FN_SRLV, FN_SRAV };
enum { FN_MFHI=020, FN_MTHI, FN_MFLO, FN_MTLO };
enum { FN_MULT=030, FN_MULTU, FN_DIV, FN_DIVU };
enum { FN_ADD=040, FN_ADDU, FN_SUB, FN_SUBU, FN_AND, FN_OR, FN_XOR, FN_NOR };
enum { FN_JR=010, FN_JALR, FN_MOVZ, FN_MOVN, FN_SYNC=017, FN_SLT=052, FN_SLTU };
// rt field (encoded in rt if opcode = OP__RT)
enum { RT_BLTZ=000, RT_BGEZ, RT_BLTZAL=020, RT_BGEZAL, RT_SYNCI=037 };

#define	MIPS_NOP 000	// null operation: SLL r0, r0, #0

// arithmetic/logical

#define MIPS_OP_REG(op, rd, rs, rt) \
	MIPS_INSN(OP__FN, rs, rt, rd, _, op)	// R-type, SPECIAL
#define MIPS_OP_IMM(op, rt, rs, imm) \
	MIPS_INSN(op, rs, rt, _, _, (u16)(imm))	// I-type

// rd = rt OP rs
#define MIPS_ADD_REG(rd, rs, rt) \
	MIPS_OP_REG(FN_ADDU, rd, rs, rt)
#define MIPS_SUB_REG(rd, rs, rt) \
	MIPS_OP_REG(FN_SUBU, rd, rs, rt)

#define MIPS_NEG_REG(rd, rt) \
	MIPS_SUB_REG(rd, Z0, rt)

#define MIPS_XOR_REG(rd, rs, rt) \
	MIPS_OP_REG(FN_XOR, rd, rs, rt)
#define MIPS_OR_REG(rd, rs, rt) \
	MIPS_OP_REG(FN_OR, rd, rs, rt)
#define MIPS_AND_REG(rd, rs, rt) \
	MIPS_OP_REG(FN_AND, rd, rs, rt)
#define MIPS_NOR_REG(rd, rs, rt) \
	MIPS_OP_REG(FN_NOR, rd, rs, rt)

#define MIPS_MOVE_REG(rd, rs) \
	MIPS_OR_REG(rd, rs, Z0)
#define MIPS_MVN_REG(rd, rs) \
	MIPS_NOR_REG(rd, rs, Z0)

// rd = rt SHIFT rs
#define MIPS_LSL_REG(rd, rt, rs) \
	MIPS_OP_REG(FN_SLLV, rd, rs, rt)
#define MIPS_LSR_REG(rd, rt, rs) \
	MIPS_OP_REG(FN_SRLV, rd, rs, rt)
#define MIPS_ASR_REG(rd, rt, rs) \
	MIPS_OP_REG(FN_SRAV, rd, rs, rt)

// rd = (rs < rt)
#define MIPS_SLT_REG(rd, rs, rt) \
	MIPS_OP_REG(FN_SLT, rd, rs, rt)
#define MIPS_SLTU_REG(rd, rs, rt) \
	MIPS_OP_REG(FN_SLTU, rd, rs, rt)

// rt = rs OP imm16
#define MIPS_ADD_IMM(rt, rs, imm16) \
	MIPS_OP_IMM(OP_ADDIU, rt, rs, imm16)

#define MIPS_XOR_IMM(rt, rs, imm16) \
	MIPS_OP_IMM(OP_XORI, rt, rs, imm16)
#define MIPS_OR_IMM(rt, rs, imm16) \
	MIPS_OP_IMM(OP_ORI, rt, rs, imm16)
#define MIPS_AND_IMM(rt, rs, imm16) \
	MIPS_OP_IMM(OP_ANDI, rt, rs, imm16)

// rt = (imm16 << (0|16))
#define MIPS_MOV_IMM(rt, imm16) \
	MIPS_OP_IMM(OP_ORI, rt, Z0, imm16)
#define MIPS_MOVT_IMM(rt, imm16) \
	MIPS_OP_IMM(OP_LUI, rt, _, imm16)

// rd = rt SHIFT imm5
#define MIPS_LSL_IMM(rd, rt, bits) \
	MIPS_INSN(OP__FN, _, rt, rd, bits, FN_SLL)
#define MIPS_LSR_IMM(rd, rt, bits) \
	MIPS_INSN(OP__FN, _, rt, rd, bits, FN_SRL)
#define MIPS_ASR_IMM(rd, rt, bits) \
	MIPS_INSN(OP__FN, _, rt, rd, bits, FN_SRA)

// rt = (rs < imm16)
#define MIPS_SLT_IMM(rt, rs, imm16) \
	MIPS_OP_IMM(OP_SLTI, rt, rs, imm16)
#define MIPS_SLTU_IMM(rt, rs, imm16) \
	MIPS_OP_IMM(OP_SLTIU, rt, rs, imm16)

// multiplication

#define MIPS_MULT(rt, rs) \
	MIPS_OP_REG(FN_MULT, _, rs, rt)
#define MIPS_MULTU(rt, rs) \
	MIPS_OP_REG(FN_MULTU, _, rs, rt)
#define MIPS_MFLO(rd) \
	MIPS_OP_REG(FN_MFLO, rd, _, _)
#define MIPS_MFHI(rd) \
	MIPS_OP_REG(FN_MFHI, rd, _, _)

// branching

#define MIPS_J(abs26) \
	MIPS_INSN(OP_J, _,_,_,_, (abs26) >> 2)	// J-type
#define MIPS_JAL(abs26) \
	MIPS_INSN(OP_JAL, _,_,_,_, (abs26) >> 2)
#define MIPS_JR(rs) \
	MIPS_OP_REG(FN_JR,_,rs,_)
#define MIPS_JALR(rd, rs) \
	MIPS_OP_REG(FN_JALR,rd,rs,_)

// conditional branches; no condition code, these compare rs against rt or Z0
#define MIPS_BEQ (OP_BEQ  << 5)
#define MIPS_BNE (OP_BNE  << 5)
#define MIPS_BLE (OP_BLEZ << 5)
#define MIPS_BGT (OP_BGTZ << 5)
#define MIPS_BLT ((OP__RT << 5)|RT_BLTZ)
#define MIPS_BGE ((OP__RT << 5)|RT_BGEZ)
#define MIPS_BGTL ((OP__RT << 5)|RT_BLTZAL)
#define MIPS_BGEL ((OP__RT << 5)|RT_BGEZAL)

#define MIPS_BCONDZ(cond, rs, offs16) \
	MIPS_OP_IMM((cond >> 5), (cond & 0x1f), rs, (offs16) >> 2)
#define MIPS_B(offs16) \
	MIPS_BCONDZ(MIPS_BEQ, Z0, offs16)
#define MIPS_BL(offs16) \
	MIPS_BCONDZ(MIPS_BGEL, Z0, offs16)

// load/store indexed base

#define MIPS_LW(rt, rs, offs16) \
	MIPS_INSN(OP_LW, rs, rt, _,_, (u16)(offs16))
#define MIPS_LH(rt, rs, offs16) \
	MIPS_INSN(OP_LH, rs, rt, _,_, (u16)(offs16))
#define MIPS_LB(rt, rs, offs16) \
	MIPS_INSN(OP_LB, rs, rt, _,_, (u16)(offs16))
#define MIPS_LHU(rt, rs, offs16) \
	MIPS_INSN(OP_LHU, rs, rt, _,_, (u16)(offs16))
#define MIPS_LBU(rt, rs, offs16) \
	MIPS_INSN(OP_LBU, rs, rt, _,_, (u16)(offs16))

#define MIPS_SW(rt, rs, offs16) \
	MIPS_INSN(OP_SW, rs, rt, _,_, (u16)(offs16))
#define MIPS_SH(rt, rs, offs16) \
	MIPS_INSN(OP_SH, rs, rt, _,_, (u16)(offs16))
#define MIPS_SB(rt, rs, offs16) \
	MIPS_INSN(OP_SB, rs, rt, _,_, (u16)(offs16))

// XXX: tcache_ptr type for SVP and SH2 compilers differs..
#define EMIT_PTR(ptr, x) \
	do { \
		*(u32 *)(ptr) = x; \
		ptr = (void *)((u8 *)(ptr) + sizeof(u32)); \
	} while (0)

// FIFO for 2 instructions, for delay slot handling
static u32 emith_last_insns[2] = { -1,-1 };
static int emith_last_idx, emith_last_cnt;

#define EMIT_PUSHOP() \
	do { \
		emith_last_idx ^= 1; \
		if (emith_last_insns[emith_last_idx] != -1) { \
			u32 *p = (u32 *)tcache_ptr - emith_last_cnt; \
			EMIT_PTR(p, emith_last_insns[emith_last_idx]);\
			emith_last_cnt --; \
		} \
		emith_last_insns[emith_last_idx] = -1; \
	} while (0)

#define EMIT(op) \
	do { \
		EMIT_PUSHOP(); \
		tcache_ptr = (void *)((u32 *)tcache_ptr + 1); \
		emith_last_insns[emith_last_idx] = op; \
		emith_last_cnt ++; \
		COUNT_OP; \
	} while (0)

#define emith_flush() \
	do { \
		int i; for (i = 0; i < 2; i++) EMIT_PUSHOP(); \
	} while (0)

#define emith_insn_ptr()	(u8 *)((u32 *)tcache_ptr - emith_last_cnt)

// delay slot stuff
static int emith_is_j(u32 op)	// J, JAL
		{ return ((op>>26) & 076) == OP_J; }
static int emith_is_jr(u32 op)	// JR, JALR
		{ return  (op>>26) == OP__FN && (op & 076) == FN_JR; }
static int emith_is_b(u32 op)	// B
		{ return ((op>>26) & 074) == OP_BEQ ||
			 ((op>>26) == OP__RT && ((op>>16) & 036) == RT_BLTZ); }
// register usage for dependency evaluation XXX better do this as in emit_arm?
static uint64_t emith_has_rs[3] = // OP__FN, OP__RT, others
	{  0x00fffffffffa0ff0ULL, 0x000fff0fUL, 0xffffffff0f007ff0ULL };
static uint64_t emith_has_rt[3] = // OP__FN, OP__RT, others
	{  0xff00fffffff00cffULL, 0x00000000UL, 0x8000ff0000000030ULL };
static uint64_t emith_has_rd[3] = // OP__FN, OP__RT, others (rt instead of rd)
	{  0xff00fffffff50fffULL, 0x00000000UL, 0x119100ff0f00ff00ULL };
#define emith_has_(rx,ix,op,sa,m) \
	(emith_has_##rx[ix] & (1ULL << (((op)>>(sa)) & (m))))
static int emith_rs(u32 op)
		{ if ((op>>26) == OP__FN)
			return	emith_has_(rs,0,op, 0,0x3f) ? (op>>21)&0x1f : 0;
		  if ((op>>26) == OP__RT)
			return	emith_has_(rs,1,op,16,0x1f) ? (op>>21)&0x1f : 0;
		  return	emith_has_(rs,2,op,26,0x3f) ? (op>>21)&0x1f : 0;
		}
static int emith_rt(u32 op)
		{ if ((op>>26) == OP__FN)
			return	emith_has_(rt,0,op, 0,0x3f) ? (op>>16)&0x1f : 0;
		  if ((op>>26) == OP__RT)
		  	return 0;
		  return	emith_has_(rt,2,op,26,0x3f) ? (op>>16)&0x1f : 0;
		}
static int emith_rd(u32 op)
		{ if ((op>>26) == OP__FN)
			return	emith_has_(rd,0,op, 0,0x3f) ? (op>>11)&0x1f :-1;
		  if ((op>>26) == OP__RT)
		  	return -1;
		  return	emith_has_(rd,2,op,26,0x3f) ? (op>>16)&0x1f :-1;
		}

static int emith_b_isswap(u32 bop, u32 lop)
{
	if (emith_is_j(bop))
		return bop;
	else if (emith_is_jr(bop) && emith_rd(lop) != emith_rs(bop))
		return bop;
	else if (emith_is_b(bop) &&  emith_rd(lop) != emith_rs(bop))
		if ((bop & 0xffff) != 0x7fff)	// displacement overflow?
			return (bop & 0xffff0000) | ((bop+1) & 0x0000ffff);
	return 0;
}

// emit branch, trying to fill the delay slot with one of the last insns
static void *emith_branch(u32 op)
{
	int idx = emith_last_idx;
	u32 op1 = emith_last_insns[idx], op2 = emith_last_insns[idx^1];
	u32 bop = 0;
	void *bp;

	// check last insn (op1)
	if (op1 != -1 && op1)
		bop = emith_b_isswap(op, op1);
	// if not, check older insn (op2); mustn't interact with op1 to overtake
	if (!bop && op2 != -1 && op2 && emith_rd(op1) != emith_rd(op2) &&
	    emith_rs(op1) != emith_rd(op2) && emith_rt(op1) != emith_rd(op2) &&
	    emith_rs(op2) != emith_rd(op1) && emith_rt(op2) != emith_rd(op1)) {
		idx ^= 1;
		bop = emith_b_isswap(op, op2);
	}

	// flush FIFO and branch
	tcache_ptr = (void *)((u32 *)tcache_ptr - emith_last_cnt);
	if (emith_last_insns[idx^1] != -1)
		EMIT_PTR(tcache_ptr, emith_last_insns[idx^1]);
	if (bop) { // can swap
		bp = tcache_ptr;
		EMIT_PTR(tcache_ptr, bop); COUNT_OP;
		EMIT_PTR(tcache_ptr, emith_last_insns[idx]);
	} else { // can't swap
		if (emith_last_insns[idx] != -1)
			EMIT_PTR(tcache_ptr, emith_last_insns[idx]);
		bp = tcache_ptr;
		EMIT_PTR(tcache_ptr, op); COUNT_OP;
		EMIT_PTR(tcache_ptr, MIPS_NOP); COUNT_OP;
	}
	emith_last_insns[0] = emith_last_insns[1] = -1;
	emith_last_cnt = 0;
	return bp;
}

// if-then-else conditional execution helpers
#define JMP_POS(ptr) \
	ptr = emith_branch(MIPS_BCONDZ(cond_m, cond_r, 0));

#define JMP_EMIT(cond, ptr) { \
	u32 val_ = (u8 *)tcache_ptr - (u8 *)(ptr) - 4; \
	emith_flush(); /* NO delay slot handling across jump targets */ \
	EMIT_PTR(ptr, MIPS_BCONDZ(cond_m, cond_r, val_ & 0x0003ffff)); \
}

#define JMP_EMIT_NC(ptr) { \
	u32 val_ = (u8 *)tcache_ptr - (u8 *)(ptr) - 4; \
	emith_flush(); \
	EMIT_PTR(ptr, MIPS_B(val_ & 0x0003ffff)); \
}

#define EMITH_JMP_START(cond) { \
	int cond_r, cond_m = emith_cond_check(cond, &cond_r); \
	u8 *cond_ptr; \
	JMP_POS(cond_ptr)

#define EMITH_JMP_END(cond) \
	JMP_EMIT(cond, cond_ptr); \
}

#define EMITH_JMP3_START(cond) { \
	int cond_r, cond_m = emith_cond_check(cond, &cond_r); \
	u8 *cond_ptr, *else_ptr; \
	JMP_POS(cond_ptr)

#define EMITH_JMP3_MID(cond) \
	JMP_POS(else_ptr); \
	JMP_EMIT(cond, cond_ptr);

#define EMITH_JMP3_END() \
	JMP_EMIT_NC(else_ptr); \
}

// "simple" jump (no more then a few insns)
// ARM32 will use conditional instructions here
#define EMITH_SJMP_START EMITH_JMP_START
#define EMITH_SJMP_END EMITH_JMP_END

#define EMITH_SJMP3_START EMITH_JMP3_START
#define EMITH_SJMP3_MID EMITH_JMP3_MID
#define EMITH_SJMP3_END EMITH_JMP3_END

#define EMITH_SJMP2_START(cond) \
	EMITH_SJMP3_START(cond)
#define EMITH_SJMP2_MID(cond) \
	EMITH_SJMP3_MID(cond)
#define EMITH_SJMP2_END(cond) \
	EMITH_SJMP3_END()


// flag register emulation. this is modelled after arm/x86.
// the FNZ register stores the result of the last flag setting operation for
// N and Z flag, used for EQ,NE,MI,PL branches.
// the FC register stores the C flag (used for HI,HS,LO,LS,CC,CS).
// the FV register stores information for V flag calculation (used for
// GT,GE,LT,LE,VC,VS). V flag is costly and only fully calculated when needed.
// the core registers may be temp registers, since the condition after calls
// is undefined anyway. 

// flag emulation creates 2 (ie cmp #0/beq) up to 9 (ie adcf/ble) extra insns.
// flag handling shortcuts may reduce this by 1-4 insns, see emith_cond_check()
static int emith_flg_rs, emith_flg_rt;	// registers used in FNZ=rs-rt (cmp_r_r)
static int emith_flg_noV;		// V flag known not to be set

// store minimal cc information: rd, rt^rs, carry
// NB: the result *must* first go to FNZ, in case rd == rs or rd == rt.
// NB: for adcf and sbcf, carry-in must be dealt with separately (see there)
static void emith_set_arith_flags(int rd, int rt, int rs, s32 imm, int sub)
{
	if (sub && rd == FNZ && rt > AT && rs > AT)	// is this cmp_r_r?
		emith_flg_rs = rs, emith_flg_rt = rt;
	else	emith_flg_rs = emith_flg_rt = 0;

        if (sub)				// C = sub:rt<rd, add:rd<rt
		EMIT(MIPS_SLTU_REG(FC, rt, FNZ));
	else	EMIT(MIPS_SLTU_REG(FC, FNZ, rt));// C in FC, bit 0 

	emith_flg_noV = 0;
	if (rs > 0)				// Nt^Ns
		EMIT(MIPS_XOR_REG(FV, rt, rs));
	else if (imm < 0)
		EMIT(MIPS_NOR_REG(FV, rt, Z0));
	else if (imm > 0)
		EMIT(MIPS_OR_REG(FV, rt, Z0));	// Nt^Ns in FV, bit 31
	else	emith_flg_noV = 1;		// imm #0, never overflows
	// full V = Nd^Nt^Ns^C calculation is deferred until really needed

	if (rd != FNZ)
		EMIT(MIPS_MOVE_REG(rd, FNZ));	// N,Z via result value in FNZ
}

// data processing, register
#define emith_move_r_r_ptr(d, s) \
	EMIT(MIPS_MOVE_REG(d, s))
#define emith_move_r_r_ptr_c(cond, d, s) \
	emith_move_r_r_ptr(d, s)

#define emith_move_r_r(d, s) \
	emith_move_r_r_ptr(d, s)
#define emith_move_r_r_c(cond, d, s) \
	emith_move_r_r(d, s)

#define emith_mvn_r_r(d, s) \
	EMIT(MIPS_MVN_REG(d, s))

#define emith_add_r_r_r_lsl_ptr(d, s1, s2, simm) do { \
	if (simm) { \
		EMIT(MIPS_LSL_IMM(AT, s2, simm)); \
		EMIT(MIPS_ADD_REG(d, s1, AT)); \
	} else	EMIT(MIPS_ADD_REG(d, s1, s2)); \
} while (0)
#define emith_add_r_r_r_lsl(d, s1, s2, simm) \
	emith_add_r_r_r_lsl_ptr(d, s1, s2, simm)

#define emith_add_r_r_r_lsr(d, s1, s2, simm) do { \
	if (simm) { \
		EMIT(MIPS_LSR_IMM(AT, s2, simm)); \
		EMIT(MIPS_ADD_REG(d, s1, AT)); \
	} else	EMIT(MIPS_ADD_REG(d, s1, s2)); \
} while (0)

#define emith_addf_r_r_r_lsl(d, s1, s2, simm) do { \
	if (simm) { \
		EMIT(MIPS_LSL_IMM(AT, s2, simm)); \
		EMIT(MIPS_ADD_REG(FNZ, s1, AT)); \
		emith_set_arith_flags(d, s1, AT, 0, 0); \
	} else { \
		EMIT(MIPS_ADD_REG(FNZ, s1, s2)); \
		emith_set_arith_flags(d, s1, s2, 0, 0); \
	} \
} while (0)

#define emith_addf_r_r_r_lsr(d, s1, s2, simm) do { \
	if (simm) { \
		EMIT(MIPS_LSR_IMM(AT, s2, simm)); \
		EMIT(MIPS_ADD_REG(FNZ, s1, AT)); \
		emith_set_arith_flags(d, s1, AT, 0, 0); \
	} else { \
		EMIT(MIPS_ADD_REG(FNZ, s1, s2)); \
		emith_set_arith_flags(d, s1, s2, 0, 0); \
	} \
} while (0)

#define emith_sub_r_r_r_lsl(d, s1, s2, simm) do { \
	if (simm) { \
		EMIT(MIPS_LSL_IMM(AT, s2, simm)); \
		EMIT(MIPS_SUB_REG(d, s1, AT)); \
	} else	EMIT(MIPS_SUB_REG(d, s1, s2)); \
} while (0)

#define emith_subf_r_r_r_lsl(d, s1, s2, simm) do { \
	if (simm) { \
		EMIT(MIPS_LSL_IMM(AT, s2, simm)); \
		EMIT(MIPS_SUB_REG(FNZ, s1, AT)); \
		emith_set_arith_flags(d, s1, AT, 0, 1); \
	} else { \
		EMIT(MIPS_SUB_REG(FNZ, s1, s2)); \
		emith_set_arith_flags(d, s1, s2, 0, 1); \
	} \
} while (0)

#define emith_or_r_r_r_lsl(d, s1, s2, simm) do { \
	if (simm) { \
		EMIT(MIPS_LSL_IMM(AT, s2, simm)); \
		EMIT(MIPS_OR_REG(d, s1, AT)); \
	} else	EMIT(MIPS_OR_REG(d, s1, s2)); \
} while (0)

#define emith_eor_r_r_r_lsl(d, s1, s2, simm) do { \
	if (simm) { \
		EMIT(MIPS_LSL_IMM(AT, s2, simm)); \
		EMIT(MIPS_XOR_REG(d, s1, AT)); \
	} else	EMIT(MIPS_XOR_REG(d, s1, s2)); \
} while (0)

#define emith_eor_r_r_r_lsr(d, s1, s2, simm) do { \
	if (simm) { \
		EMIT(MIPS_LSR_IMM(AT, s2, simm)); \
		EMIT(MIPS_XOR_REG(d, s1, AT)); \
	} else	EMIT(MIPS_XOR_REG(d, s1, s2)); \
} while (0)

#define emith_and_r_r_r_lsl(d, s1, s2, simm) do { \
	if (simm) { \
		EMIT(MIPS_LSL_IMM(AT, s2, simm)); \
		EMIT(MIPS_AND_REG(d, s1, AT)); \
	} else	EMIT(MIPS_AND_REG(d, s1, s2)); \
} while (0)

#define emith_or_r_r_lsl(d, s, lslimm) \
	emith_or_r_r_r_lsl(d, d, s, lslimm)

#define emith_eor_r_r_lsr(d, s, lsrimm) \
	emith_eor_r_r_r_lsr(d, d, s, lsrimm)

#define emith_add_r_r_r(d, s1, s2) \
	emith_add_r_r_r_lsl(d, s1, s2, 0)

#define emith_addf_r_r_r(d, s1, s2) \
	emith_addf_r_r_r_lsl(d, s1, s2, 0)

#define emith_sub_r_r_r(d, s1, s2) \
	emith_sub_r_r_r_lsl(d, s1, s2, 0)

#define emith_subf_r_r_r(d, s1, s2) \
	emith_subf_r_r_r_lsl(d, s1, s2, 0)

#define emith_or_r_r_r(d, s1, s2) \
	emith_or_r_r_r_lsl(d, s1, s2, 0)

#define emith_eor_r_r_r(d, s1, s2) \
	emith_eor_r_r_r_lsl(d, s1, s2, 0)

#define emith_and_r_r_r(d, s1, s2) \
	emith_and_r_r_r_lsl(d, s1, s2, 0)

#define emith_add_r_r_ptr(d, s) \
	emith_add_r_r_r_lsl_ptr(d, d, s, 0)
#define emith_add_r_r(d, s) \
	emith_add_r_r_r(d, d, s)

#define emith_sub_r_r(d, s) \
	emith_sub_r_r_r(d, d, s)

#define emith_neg_r_r(d, s) \
	EMIT(MIPS_NEG_REG(d, s))

#define emith_adc_r_r_r(d, s1, s2) do { \
	emith_add_r_r_r(AT, s1, FC); \
	emith_add_r_r_r(d, AT, s2); \
} while (0)

#define emith_adc_r_r(d, s) \
	emith_adc_r_r_r(d, d, s)

// NB: the incoming carry Cin can cause Cout if s2+Cin=0 (or s1+Cin=0 FWIW)
// moreover, if s2+Cin=0 caused Cout, s1+s2+Cin=s1+0 can't cause another Cout
#define emith_adcf_r_r_r(d, s1, s2) do { \
	emith_add_r_r_r(FNZ, s2, FC); \
	EMIT(MIPS_SLTU_REG(AT, FNZ, FC)); \
	emith_add_r_r_r(FNZ, s1, FNZ); \
	emith_set_arith_flags(d, s1, s2, 0, 0); \
	emith_or_r_r(FC, AT); \
} while (0)

#define emith_sbcf_r_r_r(d, s1, s2) do { \
	emith_add_r_r_r(FNZ, s2, FC); \
	EMIT(MIPS_SLTU_REG(AT, FNZ, FC)); \
	emith_sub_r_r_r(FNZ, s1, FNZ); \
	emith_set_arith_flags(d, s1, s2, 0, 1); \
	emith_or_r_r(FC, AT); \
} while (0)

#define emith_and_r_r(d, s) \
	emith_and_r_r_r(d, d, s)
#define emith_and_r_r_c(cond, d, s) \
	emith_and_r_r(d, s)

#define emith_or_r_r(d, s) \
	emith_or_r_r_r(d, d, s)

#define emith_eor_r_r(d, s) \
	emith_eor_r_r_r(d, d, s)

#define emith_tst_r_r_ptr(d, s) \
	emith_and_r_r_r(FNZ, d, s)
#define emith_tst_r_r(d, s) \
	emith_tst_r_r_ptr(d, s)

#define emith_teq_r_r(d, s) \
	emith_eor_r_r_r(FNZ, d, s)

#define emith_cmp_r_r(d, s) \
	emith_subf_r_r_r(FNZ, d, s)

#define emith_addf_r_r(d, s) \
	emith_addf_r_r_r(d, d, s)

#define emith_subf_r_r(d, s) \
	emith_subf_r_r_r(d, d, s)

#define emith_adcf_r_r(d, s) \
	emith_adcf_r_r_r(d, d, s)

#define emith_sbcf_r_r(d, s) \
	emith_sbcf_r_r_r(d, d, s)

#define emith_negcf_r_r(d, s) \
	emith_sbcf_r_r_r(d, Z0, s)


// move immediate
static void emith_move_imm(int r, uintptr_t imm)
{
	if ((s16)imm == imm) {
		EMIT(MIPS_ADD_IMM(r, Z0, imm));
	} else if (!(imm >> 16)) {
		EMIT(MIPS_OR_IMM(r, Z0, imm));
	} else {
		int s = Z0;
		if (imm >> 16) {
			EMIT(MIPS_MOVT_IMM(r, imm >> 16));
			s = r;
		}
		if ((u16)imm)
			EMIT(MIPS_OR_IMM(r, s, (u16)imm));
	}
}

#define emith_move_r_ptr_imm(r, imm) \
	emith_move_imm(r, (uintptr_t)(imm))

#define emith_move_r_imm(r, imm) \
	emith_move_imm(r, (u32)(imm))
#define emith_move_r_imm_c(cond, r, imm) \
	emith_move_r_imm(r, imm)

#define emith_move_r_imm_s8_patchable(r, imm) \
	EMIT(MIPS_ADD_IMM(r, Z0, (s8)(imm)))
#define emith_move_r_imm_s8_patch(ptr, imm) do { \
	u32 *ptr_ = (u32 *)ptr; \
	while (*ptr_ >> 26 != OP_ADDIU) ptr_++; \
	EMIT_PTR(ptr_, (*ptr_ & 0xffff0000) | (u16)(s8)(imm)); \
} while (0)

// arithmetic, immediate
static void emith_arith_imm(int op, int rd, int rs, u32 imm)
{
	if ((s16)imm != imm) {
		emith_move_r_imm(AT, imm);
		EMIT(MIPS_OP_REG(FN_ADD + (op-OP_ADDI), rd, rs, AT));
	} else if (imm || rd != rs)
		EMIT(MIPS_OP_IMM(op, rd, rs, imm));
}

#define emith_add_r_imm(r, imm) \
	emith_add_r_r_imm(r, r, imm)
#define emith_add_r_imm_c(cond, r, imm) \
	emith_add_r_imm(r, imm)

#define emith_addf_r_imm(r, imm) \
	emith_addf_r_r_imm(r, imm)

#define emith_sub_r_imm(r, imm) \
	emith_sub_r_r_imm(r, r, imm)
#define emith_sub_r_imm_c(cond, r, imm) \
	emith_sub_r_imm(r, imm)

#define emith_subf_r_imm(r, imm) \
	emith_subf_r_r_imm(r, r, imm)

#define emith_adc_r_imm(r, imm) \
	emith_adc_r_r_imm(r, r, imm);

#define emith_adcf_r_imm(r, imm) \
	emith_adcf_r_r_imm(r, r, imm)

#define emith_cmp_r_imm(r, imm) \
	emith_subf_r_r_imm(FNZ, r, (s16)imm)


#define emith_add_r_r_ptr_imm(d, s, imm) \
	emith_arith_imm(OP_ADDIU, d, s, imm)

#define emith_add_r_r_imm(d, s, imm) \
	emith_add_r_r_ptr_imm(d, s, imm)

#define emith_addf_r_r_imm(d, s, imm) do { \
	emith_add_r_r_imm(FNZ, s, imm); \
	emith_set_arith_flags(d, s, 0, imm, 0); \
} while (0)

#define emith_adc_r_r_imm(d, s, imm) do { \
	emith_add_r_r_r(AT, s, FC); \
	emith_add_r_r_imm(d, AT, imm); \
} while (0)

#define emith_adcf_r_r_imm(d, s, imm) do { \
	emith_add_r_r_r(FNZ, s, FC); \
	EMIT(MIPS_SLTU_REG(AT, FNZ, FC)); \
	emith_add_r_r_imm(FNZ, FNZ, imm); \
	emith_set_arith_flags(d, s, 0, imm, 0); \
	emith_or_r_r(FC, AT); \
} while (0)

// NB: no SUBI in MIPS II, since ADDI takes a signed imm
#define emith_sub_r_r_imm(d, s, imm) \
	emith_add_r_r_imm(d, s, -(imm))
#define emith_sub_r_r_imm_c(cond, d, s, imm) \
	emith_sub_r_r_imm(d, s, imm)

#define emith_subf_r_r_imm(d, s, imm) do { \
	emith_sub_r_r_imm(FNZ, s, imm); \
	emith_set_arith_flags(d, s, 0, imm, 1); \
} while (0)

// logical, immediate
static void emith_log_imm(int op, int rd, int rs, u32 imm)
{
	if (imm >> 16) {
		emith_move_r_imm(AT, imm);
		EMIT(MIPS_OP_REG(FN_AND + (op-OP_ANDI), rd, rs, AT));
	} else if (op == OP_ANDI || imm || rd != rs)
		EMIT(MIPS_OP_IMM(op, rd, rs, imm));
}

#define emith_and_r_imm(r, imm) \
	emith_log_imm(OP_ANDI, r, r, imm)

#define emith_or_r_imm(r, imm) \
	emith_log_imm(OP_ORI, r, r, imm)
#define emith_or_r_imm_c(cond, r, imm) \
	emith_or_r_imm(r, imm)

#define emith_eor_r_imm_ptr(r, imm) \
	emith_log_imm(OP_XORI, r, r, imm)
#define emith_eor_r_imm_ptr_c(cond, r, imm) \
	emith_eor_r_imm_ptr(r, imm)

#define emith_eor_r_imm(r, imm) \
	emith_eor_r_imm_ptr(r, imm)
#define emith_eor_r_imm_c(cond, r, imm) \
	emith_eor_r_imm(r, imm)

/* NB: BIC #imm not available in MIPS; use AND #~imm instead */
#define emith_bic_r_imm(r, imm) \
	emith_log_imm(OP_ANDI, r, r, ~(imm))
#define emith_bic_r_imm_c(cond, r, imm) \
	emith_bic_r_imm(r, imm)

#define emith_tst_r_imm(r, imm) \
	emith_log_imm(OP_ANDI, FNZ, r, imm)
#define emith_tst_r_imm_c(cond, r, imm) \
	emith_tst_r_imm(r, imm)

#define emith_and_r_r_imm(d, s, imm) \
	emith_log_imm(OP_ANDI, d, s, imm)

#define emith_or_r_r_imm(d, s, imm) \
	emith_log_imm(OP_ORI, d, s, imm)

#define emith_eor_r_r_imm(d, s, imm) \
	emith_log_imm(OP_XORI, d, s, imm)

// shift
#define emith_lsl(d, s, cnt) \
	EMIT(MIPS_LSL_IMM(d, s, cnt))

#define emith_lsr(d, s, cnt) \
	EMIT(MIPS_LSR_IMM(d, s, cnt))

#define emith_asr(d, s, cnt) \
	EMIT(MIPS_ASR_IMM(d, s, cnt))

// NB: mips32r2 has ROT (SLR with R bit set)
#define emith_ror(d, s, cnt) do { \
	EMIT(MIPS_LSL_IMM(AT, s, 32-(cnt))); \
	EMIT(MIPS_LSR_IMM(d, s, cnt)); \
	EMIT(MIPS_OR_REG(d, d, AT)); \
} while (0)
#define emith_ror_c(cond, d, s, cnt) \
	emith_ror(d, s, cnt)

#define emith_rol(d, s, cnt) do { \
	EMIT(MIPS_LSR_IMM(AT, s, 32-(cnt))); \
	EMIT(MIPS_LSL_IMM(d, s, cnt)); \
	EMIT(MIPS_OR_REG(d, d, AT)); \
} while (0)

// NB: all flag setting shifts make V undefined
// NB: mips32r2 has EXT (useful for extracting C)
#define emith_lslf(d, s, cnt) do { \
	int _s = s; \
	if ((cnt) > 1) { \
		emith_lsl(d, s, cnt-1); \
		_s = d; \
	} \
	if ((cnt) > 0) { \
		emith_lsr(FC, _s, 31); \
		emith_lsl(d, _s, 1); \
	} \
	emith_move_r_r(FNZ, d); \
} while (0)

#define emith_lsrf(d, s, cnt) do { \
	int _s = s; \
	if ((cnt) > 1) { \
		emith_lsr(d, s, cnt-1); \
		_s = d; \
	} \
	if ((cnt) > 0) { \
		emith_and_r_r_imm(FC, _s, 1); \
		emith_lsr(d, _s, 1); \
	} \
	emith_move_r_r(FNZ, d); \
} while (0)

#define emith_asrf(d, s, cnt) do { \
	int _s = s; \
	if ((cnt) > 1) { \
		emith_asr(d, s, cnt-1); \
		_s = d; \
	} \
	if ((cnt) > 0) { \
		emith_and_r_r_imm(FC, _s, 1); \
		emith_asr(d, _s, 1); \
	} \
	emith_move_r_r(FNZ, d); \
} while (0)

#define emith_rolf(d, s, cnt) do { \
	emith_rol(d, s, cnt); \
	emith_and_r_r_imm(FC, d, 1); \
	emith_move_r_r(FNZ, d); \
} while (0)

#define emith_rorf(d, s, cnt) do { \
	emith_ror(d, s, cnt); \
	emith_lsr(FC, d, 31); \
	emith_move_r_r(FNZ, d); \
} while (0)

#define emith_rolcf(d) do { \
	emith_lsr(AT, d, 31); \
	emith_lsl(d, d, 1); \
	emith_or_r_r(d, FC); \
	emith_move_r_r(FC, AT); \
	emith_move_r_r(FNZ, d); \
} while (0)

#define emith_rorcf(d) do { \
	emith_and_r_r_imm(AT, d, 1); \
	emith_lsr(d, d, 1); \
	emith_lsl(FC, FC, 31); \
	emith_or_r_r(d, FC); \
	emith_move_r_r(FC, AT); \
	emith_move_r_r(FNZ, d); \
} while (0)

// signed/unsigned extend
// NB: mips32r2 has EXT and INS
#define emith_clear_msb(d, s, count) /* bits to clear */ do { \
	u32 t; \
	if ((count) >= 16) { \
		t = (count) - 16; \
		t = 0xffff >> t; \
		emith_and_r_r_imm(d, s, t); \
	} else { \
		emith_lsl(d, s, count); \
		emith_lsr(d, d, count); \
	} \
} while (0)
#define emith_clear_msb_c(cond, d, s, count) \
	emith_clear_msb(d, s, count)

// NB: mips32r2 has SE[BH]H
#define emith_sext(d, s, count) /* bits to keep */ do { \
	emith_lsl(d, s, 32-(count)); \
	emith_asr(d, d, 32-(count)); \
} while (0)

// multiply Rd = Rn*Rm (+ Ra); NB: next 2 insns after MFLO/MFHI mustn't be MULT
static u8 *last_lohi;
static void emith_lohi_nops(void)
{
	u32 d;
	while ((d = (u8 *)tcache_ptr - last_lohi) < 8 && d >= 0) EMIT(MIPS_NOP);
}

#define emith_mul(d, s1, s2) do { \
	emith_lohi_nops(); \
	EMIT(MIPS_MULTU(s1, s2)); \
	EMIT(MIPS_MFLO(d)); \
	last_lohi = (u8 *)tcache_ptr; \
} while (0)

#define emith_mul_u64(dlo, dhi, s1, s2) do { \
	emith_lohi_nops(); \
	EMIT(MIPS_MULTU(s1, s2)); \
	EMIT(MIPS_MFLO(dlo)); \
	EMIT(MIPS_MFHI(dhi)); \
	last_lohi = (u8 *)tcache_ptr; \
} while (0)

#define emith_mul_s64(dlo, dhi, s1, s2) do { \
	emith_lohi_nops(); \
	EMIT(MIPS_MULT(s1, s2)); \
	EMIT(MIPS_MFLO(dlo)); \
	EMIT(MIPS_MFHI(dhi)); \
	last_lohi = (u8 *)tcache_ptr; \
} while (0)

#define emith_mula_s64(dlo, dhi, s1, s2) do { \
	int t_ = rcache_get_tmp(); \
	emith_lohi_nops(); \
	EMIT(MIPS_MULT(s1, s2)); \
	EMIT(MIPS_MFLO(AT)); \
	emith_add_r_r(dlo, AT); \
	EMIT(MIPS_SLTU_REG(t_, dlo, AT)); \
	EMIT(MIPS_MFHI(AT)); \
	last_lohi = (u8 *)tcache_ptr; \
	emith_add_r_r(dhi, AT); \
	emith_add_r_r(dhi, t_); \
	rcache_free_tmp(t_); \
} while (0)
#define emith_mula_s64_c(cond, dlo, dhi, s1, s2) \
	emith_mula_s64(dlo, dhi, s1, s2)

// load/store. offs has 16 bits signed, which is currently sufficient
#define emith_read_r_r_offs_ptr(r, rs, offs) \
	EMIT(MIPS_LW(r, rs, offs))
#define emith_read_r_r_offs_ptr_c(cond, r, rs, offs) \
	emith_read_r_r_offs_ptr(r, rs, offs)

#define emith_read_r_r_offs(r, rs, offs) \
	emith_read_r_r_offs_ptr(r, rs, offs)
#define emith_read_r_r_offs_c(cond, r, rs, offs) \
	emith_read_r_r_offs(r, rs, offs)
 
#define emith_read_r_r_r_ptr(r, rs, rm) do { \
	emith_add_r_r_r(AT, rs, rm); \
	EMIT(MIPS_LW(r, AT, 0)); \
} while (0)

#define emith_read_r_r_r(r, rs, rm) \
	emith_read_r_r_r_ptr(r, rs, rm)
#define emith_read_r_r_r_c(cond, r, rs, rm) \
	emith_read_r_r_r(r, rs, rm)

#define emith_read8_r_r_offs(r, rs, offs) \
	EMIT(MIPS_LBU(r, rs, offs))
#define emith_read8_r_r_offs_c(cond, r, rs, offs) \
	emith_read8_r_r_offs(r, rs, offs)

#define emith_read8_r_r_r(r, rs, rm) do { \
	emith_add_r_r_r(AT, rs, rm); \
	EMIT(MIPS_LBU(r, AT, 0)); \
} while (0)
#define emith_read8_r_r_r_c(cond, r, rs, rm) \
	emith_read8_r_r_r(r, rs, rm)

#define emith_read16_r_r_offs(r, rs, offs) \
	EMIT(MIPS_LHU(r, rs, offs))
#define emith_read16_r_r_offs_c(cond, r, rs, offs) \
	emith_read16_r_r_offs(r, rs, offs)

#define emith_read16_r_r_r(r, rs, rm) do { \
	emith_add_r_r_r(AT, rs, rm); \
	EMIT(MIPS_LHU(r, AT, 0)); \
} while (0)
#define emith_read16_r_r_r_c(cond, r, rs, rm) \
	emith_read16_r_r_r(r, rs, rm)

#define emith_read8s_r_r_offs(r, rs, offs) \
	EMIT(MIPS_LB(r, rs, offs))
#define emith_read8s_r_r_offs_c(cond, r, rs, offs) \
	emith_read8s_r_r_offs(r, rs, offs)

#define emith_read8s_r_r_r(r, rs, rm) do { \
	emith_add_r_r_r(AT, rs, rm); \
	EMIT(MIPS_LB(r, AT, 0)); \
} while (0)
#define emith_read8s_r_r_r_c(cond, r, rs, rm) \
	emith_read8s_r_r_r(r, rs, rm)

#define emith_read16s_r_r_offs(r, rs, offs) \
	EMIT(MIPS_LH(r, rs, offs))
#define emith_read16s_r_r_offs_c(cond, r, rs, offs) \
	emith_read16s_r_r_offs(r, rs, offs)

#define emith_read16s_r_r_r(r, rs, rm) do { \
	emith_add_r_r_r(AT, rs, rm); \
	EMIT(MIPS_LH(r, AT, 0)); \
} while (0)
#define emith_read16s_r_r_r_c(cond, r, rs, rm) \
	emith_read16s_r_r_r(r, rs, rm)


#define emith_write_r_r_offs_ptr(r, rs, offs) \
	EMIT(MIPS_SW(r, rs, offs))
#define emith_write_r_r_offs_ptr_c(cond, r, rs, offs) \
	emith_write_r_r_offs_ptr(r, rs, offs)

#define emith_write_r_r_r_ptr(r, rs, rm) do { \
	emith_add_r_r_r(AT, rs, rm); \
	EMIT(MIPS_SW(r, AT, 0)); \
} while (0)
#define emith_write_r_r_r_ptr_c(cond, r, rs, rm) \
	emith_write_r_r_r_ptr(r, rs, rm)

#define emith_write_r_r_offs(r, rs, offs) \
	emith_write_r_r_offs_ptr(r, rs, offs)
#define emith_write_r_r_offs_c(cond, r, rs, offs) \
	emith_write_r_r_offs(r, rs, offs)

#define emith_write_r_r_r(r, rs, rm) \
	emith_write_r_r_r_ptr(r, rs, rm)
#define emith_write_r_r_r_c(cond, r, rs, rm) \
	emith_write_r_r_r(r, rs, rm)

#define emith_ctx_read_ptr(r, offs) \
	emith_read_r_r_offs_ptr(r, CONTEXT_REG, offs)

#define emith_ctx_read(r, offs) \
	emith_read_r_r_offs(r, CONTEXT_REG, offs)
#define emith_ctx_read_c(cond, r, offs) \
	emith_ctx_read(r, offs)

#define emith_ctx_write_ptr(r, offs) \
	emith_write_r_r_offs_ptr(r, CONTEXT_REG, offs)

#define emith_ctx_write(r, offs) \
	emith_write_r_r_offs(r, CONTEXT_REG, offs)

#define emith_ctx_read_multiple(r, offs, cnt, tmpr) do { \
	int r_ = r, offs_ = offs, cnt_ = cnt;     \
	for (; cnt_ > 0; r_++, offs_ += 4, cnt_--) \
		emith_ctx_read(r_, offs_);        \
} while (0)

#define emith_ctx_write_multiple(r, offs, cnt, tmpr) do { \
	int r_ = r, offs_ = offs, cnt_ = cnt;     \
	for (; cnt_ > 0; r_++, offs_ += 4, cnt_--) \
		emith_ctx_write(r_, offs_);       \
} while (0)

// function call handling
#define emith_save_caller_regs(mask) do { \
	int _c; u32 _m = mask & 0x300fffc; /* r2-r15,r24-r25 */ \
	if (__builtin_parity(_m) == 1) _m |= 0x1; /* ABI align */ \
	int _s = count_bits(_m) * 4, _o = _s; \
	if (_s) emith_sub_r_imm(SP, _s); \
	for (_c = HOST_REGS-1; _m && _c >= 0; _m &= ~(1 << _c), _c--) \
		if (_m & (1 << _c)) \
			{ _o -= 4; if (_c) emith_write_r_r_offs(_c, SP, _o); } \
} while (0)

#define emith_restore_caller_regs(mask) do { \
	int _c; u32 _m = mask & 0x300fffc; \
	if (__builtin_parity(_m) == 1) _m |= 0x1; \
	int _s = count_bits(_m) * 4, _o = 0; \
	for (_c = 0; _m && _c < HOST_REGS; _m &= ~(1 << _c), _c++) \
		if (_m & (1 << _c)) \
			{ if (_c) emith_read_r_r_offs(_c, SP, _o); _o += 4; } \
	if (_s) emith_add_r_imm(SP, _s); \
} while (0)

#define host_arg2reg(rd, arg) \
	rd = (arg+4)

#define emith_pass_arg_r(arg, reg) \
	emith_move_r_r(arg, reg)

#define emith_pass_arg_imm(arg, imm) \
	emith_move_r_imm(arg, imm)

// branching
#define emith_invert_branch(cond) /* inverted conditional branch */ \
	(((cond) >> 5) == OP__RT ? (cond) ^ 0x01 : (cond) ^ 0x20)

// evaluate the emulated condition, returns a register/branch type pair
static int emith_cond_check(int cond, int *r)
{
	int b = 0;

	// shortcut for comparing 2 registers
	if (emith_flg_rs || emith_flg_rt) switch (cond) {
	case DCOND_LS:	EMIT(MIPS_SLTU_REG(AT, emith_flg_rs, emith_flg_rt));
			*r = AT, b = MIPS_BEQ; break;	// s <= t unsigned
	case DCOND_HI:	EMIT(MIPS_SLTU_REG(AT, emith_flg_rs, emith_flg_rt));
			*r = AT, b = MIPS_BNE; break;	// s >  t unsigned
	case DCOND_LT:	EMIT(MIPS_SLT_REG(AT, emith_flg_rt, emith_flg_rs));
			*r = AT, b = MIPS_BNE; break;	// s <  t
	case DCOND_GE:	EMIT(MIPS_SLT_REG(AT, emith_flg_rt, emith_flg_rs));
			*r = AT, b = MIPS_BEQ; break;	// s >= t
	case DCOND_LE:	EMIT(MIPS_SLT_REG(AT, emith_flg_rs, emith_flg_rt));
			*r = AT, b = MIPS_BEQ; break;	// s <= t
	case DCOND_GT:	EMIT(MIPS_SLT_REG(AT, emith_flg_rs, emith_flg_rt));
			*r = AT, b = MIPS_BNE; break;	// s >  t
	}

	// shortcut for V known to be 0
	if (!b && emith_flg_noV) switch (cond) {
	case DCOND_VS:	*r = Z0; b = MIPS_BNE; break;		// never
	case DCOND_VC:	*r = Z0; b = MIPS_BEQ; break;		// always
	case DCOND_LT:	*r = FNZ, b = MIPS_BLT;	break;		// N
	case DCOND_GE:	*r = FNZ, b = MIPS_BGE;	break;		// !N
	case DCOND_LE:	*r = FNZ, b = MIPS_BLE;	break;		// N || Z
	case DCOND_GT:	*r = FNZ, b = MIPS_BGT;	break;		// !N && !Z
	}

	// the full monty if no shortcut
	if (!b) switch (cond) {
	// conditions using NZ
	case DCOND_EQ:	*r = FNZ; b = MIPS_BEQ; break;		// Z
	case DCOND_NE:	*r = FNZ; b = MIPS_BNE; break;		// !Z
	case DCOND_MI:	*r = FNZ; b = MIPS_BLT; break;		// N
	case DCOND_PL:	*r = FNZ; b = MIPS_BGE; break;		// !N
	// conditions using C
	case DCOND_LO:	*r = FC; b = MIPS_BNE; break;		// C
	case DCOND_HS:	*r = FC; b = MIPS_BEQ; break;		// !C
	// conditions using CZ
	case DCOND_LS:						// C || Z
	case DCOND_HI:						// !C && !Z
		EMIT(MIPS_ADD_IMM(AT, FC, (u16)-1)); // !C && !Z
		EMIT(MIPS_AND_REG(AT, FNZ, AT));
		*r = AT, b = (cond == DCOND_HI ? MIPS_BNE : MIPS_BEQ);
		break;

	// conditions using V
	case DCOND_VS:						// V
	case DCOND_VC:						// !V
		EMIT(MIPS_XOR_REG(AT, FV, FNZ)); // V = Nt^Ns^Nd^C
		EMIT(MIPS_LSR_IMM(AT, AT, 31));
		EMIT(MIPS_XOR_REG(AT, AT, FC));
		*r = AT, b = (cond == DCOND_VS ? MIPS_BNE : MIPS_BEQ);
		break;
	// conditions using VNZ
	case DCOND_LT:						// N^V
	case DCOND_GE:						// !(N^V)
		EMIT(MIPS_LSR_IMM(AT, FV, 31)); // Nd^V = Nt^Ns^C
		EMIT(MIPS_XOR_REG(AT, FC, AT));
		*r = AT, b = (cond == DCOND_LT ? MIPS_BNE : MIPS_BEQ);
		break;
	case DCOND_LE:						// (N^V) || Z
	case DCOND_GT:						// !(N^V) && !Z
		EMIT(MIPS_LSR_IMM(AT, FV, 31)); // Nd^V = Nt^Ns^C
		EMIT(MIPS_XOR_REG(AT, FC, AT));
		EMIT(MIPS_ADD_IMM(AT, AT, (u16)-1)); // !(Nd^V) && !Z
		EMIT(MIPS_AND_REG(AT, FNZ, AT));
		*r = AT, b = (cond == DCOND_GT ? MIPS_BNE : MIPS_BEQ);
		break;
	}
	return b;
}

// NB: assumes all targets are in the same 256MB segment
#define emith_jump(target) \
	emith_branch(MIPS_J((uintptr_t)target & 0x0fffffff))
#define emith_jump_patchable(target) \
	emith_jump(target)

// NB: MIPS conditional branches have only +/- 128KB range
#define emith_jump_cond(cond, target) do { \
	int r_, mcond_ = emith_cond_check(cond, &r_); \
	u32 disp_ = (u8 *)target - (u8 *)tcache_ptr - 4; \
	emith_branch(MIPS_BCONDZ(mcond_,r_,disp_ & 0x0003ffff)); \
} while (0)
#define emith_jump_cond_patchable(cond, target) \
	emith_jump_cond(cond, target)

#define emith_jump_cond_inrange(target) \
	((u8 *)target - (u8 *)tcache_ptr - 4 <  0x00020000U || \
	 (u8 *)target - (u8 *)tcache_ptr - 4 >= 0xfffe0010U) // mind cond_check

// NB: returns position of patch for cache maintenance
#define emith_jump_patch(ptr, target, pos) do { \
	u32 *ptr_ = (u32 *)ptr-1; /* must skip condition check code */ \
	u32 disp_, mask_; \
	while (!emith_is_j(*ptr_) && !emith_is_b(*ptr_)) ptr_ ++; \
	if (emith_is_b(*ptr_)) \
		mask_ = 0xffff0000, disp_ = (u8 *)target - (u8 *)ptr_ - 4; \
	else	mask_ = 0xfc000000, disp_ = (uintptr_t)target; \
	EMIT_PTR(ptr_, (*ptr_ & mask_) | ((disp_ >> 2) & ~mask_)); \
	if ((void *)(pos) != NULL) *(u8 **)(pos) = (u8 *)(ptr_-1); \
} while (0)

#define emith_jump_patch_inrange(ptr, target) \
	((u8 *)target - (u8 *)ptr - 4 <  0x00020000U || \
	 (u8 *)target - (u8 *)ptr - 4 >= 0xfffe0010U) // mind cond_check
#define emith_jump_patch_size() 4

#define emith_jump_at(ptr, target) do { \
	u32 *ptr_ = (u32 *)ptr; \
	EMIT_PTR(ptr_, MIPS_J((uintptr_t)target & 0x0fffffff)); \
	EMIT_PTR(ptr_, MIPS_NOP); \
} while (0)
#define emith_jump_at_size() 8

#define emith_jump_reg(r) \
	emith_branch(MIPS_JR(r))
#define emith_jump_reg_c(cond, r) \
	emith_jump_reg(r)

#define emith_jump_ctx(offs) do { \
	emith_ctx_read_ptr(AT, offs); \
	emith_jump_reg(AT); \
} while (0)
#define emith_jump_ctx_c(cond, offs) \
	emith_jump_ctx(offs)

#define emith_call(target) \
	emith_branch(MIPS_JAL((uintptr_t)target & 0x0fffffff))
#define emith_call_cond(cond, target) \
	emith_call(target)

#define emith_call_reg(r) \
	emith_branch(MIPS_JALR(LR, r))

#define emith_call_ctx(offs) do { \
	emith_ctx_read_ptr(AT, offs); \
	emith_call_reg(AT); \
} while (0)

#define emith_call_cleanup()	/**/

#define emith_ret() \
	emith_branch(MIPS_JR(LR))
#define emith_ret_c(cond) \
	emith_ret()

#define emith_ret_to_ctx(offs) \
	emith_ctx_write_ptr(LR, offs)

#define emith_add_r_ret(r) \
	emith_add_r_r_ptr(r, LR)

// NB: ABI SP alignment is 8 for compatibility with MIPS IV
#define emith_push_ret(r) do { \
	emith_sub_r_imm(SP, 8+16); /* reserve new arg save area (16) */ \
	emith_write_r_r_offs(LR, SP, 4+16); \
	if ((r) > 0) emith_write_r_r_offs(r, SP, 0+16); \
} while (0)

#define emith_pop_and_ret(r) do { \
	if ((r) > 0) emith_read_r_r_offs(r, SP, 0+16); \
	emith_read_r_r_offs(LR, SP, 4+16); \
	emith_add_r_imm(SP, 8+16); \
	emith_ret(); \
} while (0)


// emitter ABI stuff
#define emith_pool_check()	/**/
#define emith_pool_commit(j)	/**/
// NB: mips32r2 has SYNCI
#define host_instructions_updated(base, end) __builtin___clear_cache(base, end)
#define	emith_update_cache()	/**/
#define emith_rw_offs_max()	0x7fff

// SH2 drc specific
#define emith_sh2_drc_entry() do { \
	int _c; u32 _m = 0xd0ff0000; \
	if (__builtin_parity(_m) == 1) _m |= 0x1; /* ABI align for SP is 8 */ \
	int _s = count_bits(_m) * 4 + 16, _o = _s; /* 16 byte arg save area */ \
	if (_s) emith_sub_r_imm(SP, _s); \
	for (_c = HOST_REGS-1; _m && _c >= 0; _m &= ~(1 << _c), _c--) \
		if (_m & (1 << _c)) \
			{ _o -= 4; if (_c) emith_write_r_r_offs(_c, SP, _o); } \
} while (0)
#define emith_sh2_drc_exit() do { \
	int _c; u32 _m = 0xd0ff0000; \
	if (__builtin_parity(_m) == 1) _m |= 0x1; \
	int _s = count_bits(_m) * 4 + 16, _o = 16; \
	for (_c = 0; _m && _c < HOST_REGS; _m &= ~(1 << _c), _c++) \
		if (_m & (1 << _c)) \
			{ if (_c) emith_read_r_r_offs(_c, SP, _o); _o += 4; } \
	if (_s) emith_add_r_imm(SP, _s); \
	emith_ret(); \
} while (0)

// NB: assumes a is in arg0, tab, func and mask are temp
#define emith_sh2_rcall(a, tab, func, mask) do { \
	emith_lsr(mask, a, SH2_READ_SHIFT); \
	emith_add_r_r_r_lsl_ptr(tab, tab, mask, 3); \
	emith_read_r_r_offs_ptr(func, tab, 0); \
	emith_read_r_r_offs(mask, tab, 4); \
	emith_addf_r_r_r/*_ptr*/(func, func, func); \
} while (0)

// NB: assumes a, val are in arg0 and arg1, tab and func are temp
#define emith_sh2_wcall(a, val, tab, func) do { \
	emith_lsr(func, a, SH2_WRITE_SHIFT); \
	emith_lsl(func, func, 2); \
	emith_read_r_r_r_ptr(func, tab, func); \
	emith_move_r_r_ptr(6, CONTEXT_REG); /* arg2 */ \
	emith_jump_reg(func); \
} while (0)

#define emith_sh2_delay_loop(cycles, reg) do {			\
	int sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);	\
	int t1 = rcache_get_tmp();				\
	int t2 = rcache_get_tmp();				\
	int t3 = rcache_get_tmp();				\
	/* if (sr < 0) return */				\
	emith_cmp_r_imm(sr, 0);					\
	EMITH_JMP_START(DCOND_LE);				\
	/* turns = sr.cycles / cycles */			\
	emith_asr(t2, sr, 12);					\
	emith_move_r_imm(t3, (u32)((1ULL<<32) / (cycles)) + 1);	\
	emith_mul_u64(t1, t2, t2, t3); /* multiply by 1/x */	\
	rcache_free_tmp(t3);					\
	if (reg >= 0) {						\
		/* if (reg <= turns) turns = reg-1 */		\
		t3 = rcache_get_reg(reg, RC_GR_RMW, NULL);	\
		emith_cmp_r_r(t3, t2);				\
		EMITH_SJMP_START(DCOND_HI);			\
		emith_sub_r_r_imm_c(DCOND_LS, t2, t3, 1);	\
		EMITH_SJMP_END(DCOND_HI);			\
		/* if (reg <= 1) turns = 0 */			\
		emith_cmp_r_imm(t3, 1);				\
		EMITH_SJMP_START(DCOND_HI);			\
		emith_move_r_imm_c(DCOND_LS, t2, 0);		\
		EMITH_SJMP_END(DCOND_HI);			\
		/* reg -= turns */				\
		emith_sub_r_r(t3, t2);				\
	}							\
	/* sr.cycles -= turns * cycles; */			\
	emith_move_r_imm(t1, cycles);				\
	emith_mul(t1, t2, t1);					\
	emith_sub_r_r_r_lsl(sr, sr, t1, 12);			\
	EMITH_JMP_END(DCOND_LE);				\
	rcache_free_tmp(t1);					\
	rcache_free_tmp(t2);					\
} while (0)

/*
 * if Q
 *   t = carry(Rn += Rm)
 * else
 *   t = carry(Rn -= Rm)
 * T ^= t
 */
#define emith_sh2_div1_step(rn, rm, sr) do {      \
	emith_tst_r_imm(sr, Q);  /* if (Q ^ M) */ \
	EMITH_JMP3_START(DCOND_EQ);               \
	emith_addf_r_r(rn, rm);                   \
	EMITH_JMP3_MID(DCOND_EQ);                 \
	emith_subf_r_r(rn, rm);                   \
	EMITH_JMP3_END();                         \
	emith_eor_r_r(sr, FC);                    \
} while (0)

/* mh:ml += rn*rm, does saturation if required by S bit. rn, rm must be TEMP */
#define emith_sh2_macl(ml, mh, rn, rm, sr) do {   \
	emith_tst_r_imm(sr, S);                   \
	EMITH_SJMP_START(DCOND_EQ);               \
	/* MACH top 16 bits unused if saturated. sign ext for overfl detect */ \
	emith_sext(mh, mh, 16);                   \
	EMITH_SJMP_END(DCOND_EQ);                 \
	emith_mula_s64(ml, mh, rn, rm);           \
	emith_tst_r_imm(sr, S);                   \
	EMITH_SJMP_START(DCOND_EQ);               \
	/* overflow if top 17 bits of MACH aren't all 1 or 0 */ \
	/* to check: add MACH >> 31 to MACH >> 15. this is 0 if no overflow */ \
	emith_asr(rn, mh, 15);                    \
	emith_add_r_r_r_lsr(rn, rn, mh, 31); /* sum = (MACH>>31)+(MACH>>15) */ \
	emith_teq_r_r(rn, Z0); /* (need only N and Z flags) */ \
	EMITH_SJMP_START(DCOND_EQ); /* sum != 0 -> ov */ \
	emith_move_r_imm_c(DCOND_NE, ml, 0x0000); /* -overflow */ \
	emith_move_r_imm_c(DCOND_NE, mh, 0x8000); \
	EMITH_SJMP_START(DCOND_PL); /* sum > 0 -> +ovl */ \
	emith_sub_r_imm_c(DCOND_MI, ml, 1); /* 0xffffffff */ \
	emith_sub_r_imm_c(DCOND_MI, mh, 1); /* 0x00007fff */ \
	EMITH_SJMP_END(DCOND_PL);                 \
	EMITH_SJMP_END(DCOND_EQ);                 \
	EMITH_SJMP_END(DCOND_EQ);                 \
} while (0)

/* mh:ml += rn*rm, does saturation if required by S bit. rn, rm must be TEMP */
#define emith_sh2_macw(ml, mh, rn, rm, sr) do {   \
	emith_tst_r_imm(sr, S);                   \
	EMITH_SJMP_START(DCOND_EQ);               \
	/* XXX: MACH should be untouched when S is set? */ \
	emith_asr(mh, ml, 31); /* sign ext MACL to MACH for ovrfl check */ \
	EMITH_SJMP_END(DCOND_EQ);                 \
	emith_mula_s64(ml, mh, rn, rm);           \
	emith_tst_r_imm(sr, S);                   \
	EMITH_SJMP_START(DCOND_EQ);               \
	/* overflow if top 33 bits of MACH:MACL aren't all 1 or 0 */ \
	/* to check: add MACL[31] to MACH. this is 0 if no overflow */ \
	emith_lsr(rn, ml, 31);                    \
	emith_add_r_r(rn, mh); /* sum = MACH + ((MACL>>31)&1) */ \
	emith_teq_r_r(rn, Z0); /* (need only N and Z flags) */ \
	EMITH_SJMP_START(DCOND_EQ); /* sum != 0 -> overflow */ \
	/* XXX: LSB signalling only in SH1, or in SH2 too? */ \
	emith_move_r_imm_c(DCOND_NE, mh, 0x00000001); /* LSB of MACH */ \
	emith_move_r_imm_c(DCOND_NE, ml, 0x80000000); /* negative ovrfl */ \
	EMITH_SJMP_START(DCOND_PL); /* sum > 0 -> positive ovrfl */ \
	emith_sub_r_imm_c(DCOND_MI, ml, 1); /* 0x7fffffff */ \
	EMITH_SJMP_END(DCOND_PL);                 \
	EMITH_SJMP_END(DCOND_EQ);                 \
	EMITH_SJMP_END(DCOND_EQ);                 \
} while (0)

#define emith_write_sr(sr, srcr) do { \
	emith_lsr(sr, sr, 10); \
	emith_or_r_r_r_lsl(sr, sr, srcr, 22); \
	emith_ror(sr, sr, 22); \
} while (0)

#define emith_carry_to_t(srr, is_sub) do { \
	emith_lsr(sr, sr, 1); \
	emith_adc_r_r(sr, sr); \
} while (0)

#define emith_tpop_carry(sr, is_sub) do { \
	emith_and_r_r_imm(FC, sr, 1); \
	emith_lsr(sr, sr, 1); \
} while (0)

#define emith_tpush_carry(sr, is_sub) \
	emith_adc_r_r(sr, sr)

#ifdef T
// T bit handling
#define emith_invert_cond(cond) \
	((cond) ^ 1)

static void emith_clr_t_cond(int sr)
{
  emith_bic_r_imm(sr, T);
}

static void emith_set_t_cond(int sr, int cond)
{
  EMITH_SJMP_START(emith_invert_cond(cond));
  emith_or_r_imm_c(cond, sr, T);
  EMITH_SJMP_END(emith_invert_cond(cond));
}

#define emith_get_t_cond()      -1

#define emith_sync_t(sr)	((void)sr)

#define emith_invalidate_t()

static void emith_set_t(int sr, int val)
{
  if (val) 
    emith_or_r_imm(sr, T);
  else
    emith_bic_r_imm(sr, T);
}

static int emith_tst_t(int sr, int tf)
{
  emith_tst_r_imm(sr, T);
  return tf ? DCOND_NE: DCOND_EQ;
}
#endif
