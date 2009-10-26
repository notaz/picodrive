#include <stdarg.h>

enum { xAX = 0, xCX, xDX, xBX, xSP, xBP, xSI, xDI };

#define CONTEXT_REG xBP

#define IOP_JE  0x74
#define IOP_JNE 0x75
#define IOP_JBE 0x76
#define IOP_JA  0x77
#define IOP_JS  0x78
#define IOP_JNS 0x79
#define IOP_JLE 0x7e

// unified conditions (we just use rel8 jump instructions for x86)
#define DCOND_EQ IOP_JE
#define DCOND_NE IOP_JNE
#define DCOND_MI IOP_JS      // MInus
#define DCOND_PL IOP_JNS     // PLus or zero

#define EMIT_PTR(ptr, val, type) \
	*(type *)(ptr) = val

#define EMIT(val, type) { \
	EMIT_PTR(tcache_ptr, val, type); \
	tcache_ptr += sizeof(type); \
}

#define EMIT_OP(op) { \
	COUNT_OP; \
	EMIT(op, u8); \
}

#define EMIT_MODRM(mod,r,rm) \
	EMIT(((mod)<<6) | ((r)<<3) | (rm), u8)

#define EMIT_OP_MODRM(op,mod,r,rm) { \
	EMIT_OP(op); \
	EMIT_MODRM(mod, r, rm); \
}

#define JMP8_POS(ptr) \
	ptr = tcache_ptr; \
	tcache_ptr += 2

#define JMP8_EMIT(op, ptr) \
	EMIT_PTR(ptr, op, u8); \
	EMIT_PTR(ptr + 1, (tcache_ptr - (ptr+2)), u8)

#define emith_move_r_r(dst, src) \
	EMIT_OP_MODRM(0x8b, 3, dst, src)

#define emith_add_r_r(d, s) \
	EMIT_OP_MODRM(0x01, 3, s, d)

#define emith_sub_r_r(d, s) \
	EMIT_OP_MODRM(0x29, 3, s, d)

#define emith_or_r_r(d, s) \
	EMIT_OP_MODRM(0x09, 3, s, d)

#define emith_eor_r_r(d, s) \
	EMIT_OP_MODRM(0x31, 3, s, d)

// fake teq - test equivalence - get_flags(d ^ s)
#define emith_teq_r_r(d, s) { \
	emith_push(d); \
	emith_eor_r_r(d, s); \
	emith_pop(d); \
}

// _r_imm
#define emith_move_r_imm(r, imm) { \
	EMIT_OP(0xb8 + (r)); \
	EMIT(imm, u32); \
}

#define emith_arith_r_imm(op, r, imm) { \
	EMIT_OP_MODRM(0x81, 3, op, r); \
	EMIT(imm, u32); \
}

// 2 - adc, 3 - sbb, 6 - xor, 7 - cmp
#define emith_add_r_imm(r, imm) \
	emith_arith_r_imm(0, r, imm)

#define emith_or_r_imm(r, imm) \
	emith_arith_r_imm(1, r, imm)

#define emith_and_r_imm(r, imm) \
	emith_arith_r_imm(4, r, imm)

#define emith_sub_r_imm(r, imm) \
	emith_arith_r_imm(5, r, imm)

#define emith_tst_r_imm(r, imm) { \
	EMIT_OP_MODRM(0xf7, 3, 0, r); \
	EMIT(imm, u32); \
}

// fake
#define emith_bic_r_imm(r, imm) \
	emith_arith_r_imm(4, r, ~(imm))

// fake conditionals (using SJMP instead)
#define emith_add_r_imm_c(cond, r, imm) { \
	(void)(cond); \
	emith_arith_r_imm(0, r, imm); \
}

#define emith_or_r_imm_c(cond, r, imm) { \
	(void)(cond); \
	emith_arith_r_imm(1, r, imm); \
}

#define emith_sub_r_imm_c(cond, r, imm) { \
	(void)(cond); \
	emith_arith_r_imm(5, r, imm); \
}

// shift
#define emith_shift(op, d, s, cnt) { \
	if (d != s) \
		emith_move_r_r(d, s); \
	EMIT_OP_MODRM(0xc1, 3, op, d); \
	EMIT(cnt, u8); \
}

#define emith_asr(d, s, cnt) \
	emith_shift(7, d, s, cnt)

#define emith_lsl(d, s, cnt) \
	emith_shift(4, d, s, cnt)

// misc
#define emith_push(r) \
	EMIT_OP(0x50 + (r))

#define emith_pop(r) \
	EMIT_OP(0x58 + (r))

#define emith_neg_r(r) \
	EMIT_OP_MODRM(0xf7, 3, 3, r)

#define emith_clear_msb(d, s, count) { \
	u32 t = (u32)-1; \
	t >>= count; \
	if (d != s) \
		emith_move_r_r(d, s); \
	emith_and_r_imm(d, t); \
}

#define emith_sext(d, s, bits) { \
	emith_lsl(d, s, 32 - (bits)); \
	emith_asr(d, d, 32 - (bits)); \
}

// XXX: stupid mess
#define emith_mul(d, s1, s2) { \
	int rmr; \
	if (d != xAX) \
		emith_push(xAX); \
	if ((s1) == xAX) \
		rmr = s2; \
	else if ((s2) == xAX) \
		rmr = s1; \
	else { \
		emith_move_r_r(xAX, s1); \
		rmr = s2; \
	} \
	emith_push(xDX); \
	EMIT_OP_MODRM(0xf7, 3, 4, rmr); /* MUL rmr */ \
	emith_pop(xDX); \
	if (d != xAX) { \
		emith_move_r_r(d, xAX); \
		emith_pop(xAX); \
	} \
}

// "flag" instructions are the same
#define emith_subf_r_imm emith_sub_r_imm
#define emith_subf_r_r   emith_sub_r_r

// XXX: offs is 8bit only
#define emith_ctx_read(r, offs) { \
	EMIT_OP_MODRM(0x8b, 1, r, xBP); \
	EMIT(offs, u8); 	/* mov tmp, [ebp+#offs] */ \
}

#define emith_ctx_write(r, offs) { \
	EMIT_OP_MODRM(0x89, 1, r, xBP); \
	EMIT(offs, u8); 	/* mov [ebp+#offs], tmp */ \
}

#define emith_jump(ptr) { \
	u32 disp = (u32)ptr - ((u32)tcache_ptr + 5); \
	EMIT_OP(0xe9); \
	EMIT(disp, u32); \
}

#define emith_call(ptr) { \
	u32 disp = (u32)ptr - ((u32)tcache_ptr + 5); \
	EMIT_OP(0xe8); \
	EMIT(disp, u32); \
}

// "simple" or "short" jump
#define EMITH_SJMP_START(cond) { \
	u8 *cond_ptr; \
	JMP8_POS(cond_ptr)

#define EMITH_SJMP_END(cond) \
	JMP8_EMIT(cond, cond_ptr); \
}

#define host_arg2reg(rd, arg) \
	switch (arg) { \
	case 0: rd = xAX; break; \
	case 1: rd = xDX; break; \
	case 2: rd = xCX; break; \
	}

#define emith_pass_arg_r(arg, reg) { \
	int rd = 7; \
	host_arg2reg(rd, arg); \
	emith_move_r_r(rd, reg); \
}

#define emith_pass_arg_imm(arg, imm) { \
	int rd = 7; \
	host_arg2reg(rd, arg); \
	emith_move_r_imm(rd, imm); \
}

/* SH2 drc specific */
#define emith_sh2_test_t() { \
	int t = rcache_get_reg(SHR_SR, RC_GR_READ); \
	EMIT_OP_MODRM(0xf6, 3, 0, t); \
	EMIT(0x01, u8); /* test <reg>, byte 1 */ \
}

#define emith_sh2_dtbf_loop() { \
	u8 *jmp0; /* negative cycles check */            \
	u8 *jmp1; /* unsinged overflow check */          \
	int cr, rn;                                      \
	tmp = rcache_get_tmp();                          \
	cr = rcache_get_reg(SHR_SR, RC_GR_RMW);          \
	rn = rcache_get_reg((op >> 8) & 0x0f, RC_GR_RMW);\
	emith_sub_r_imm(rn, 1);                          \
	emith_sub_r_imm(cr, (cycles+1) << 12);           \
	cycles = 0;                                      \
	emith_asr(tmp, cr, 2+12);                        \
	JMP8_POS(jmp0); /* no negative cycles */         \
	emith_move_r_imm(tmp, 0);                        \
	JMP8_EMIT(IOP_JNS, jmp0);                        \
	emith_and_r_imm(cr, 0xffe);                      \
	emith_subf_r_r(rn, tmp);                         \
	JMP8_POS(jmp1); /* no overflow */                \
	emith_neg_r(rn); /* count left */                \
	emith_lsl(rn, rn, 2+12);                         \
	emith_or_r_r(cr, rn);                            \
	emith_or_r_imm(cr, 1);                           \
	emith_move_r_imm(rn, 0);                         \
	JMP8_EMIT(IOP_JA, jmp1);                         \
	rcache_free_tmp(tmp);                            \
}

