/*
 * note about silly things like emith_eor_r_r_r_lsl:
 * these are here because the compiler was designed
 * for ARM as it's primary target.
 */
#include <stdarg.h>

enum { xAX = 0, xCX, xDX, xBX, xSP, xBP, xSI, xDI };

#define CONTEXT_REG xBP

#define IOP_JO  0x70
#define IOP_JNO 0x71
#define IOP_JB  0x72
#define IOP_JAE 0x73
#define IOP_JE  0x74
#define IOP_JNE 0x75
#define IOP_JBE 0x76
#define IOP_JA  0x77
#define IOP_JS  0x78
#define IOP_JNS 0x79
#define IOP_JL  0x7c
#define IOP_JGE 0x7d
#define IOP_JLE 0x7e
#define IOP_JG  0x7f

// unified conditions (we just use rel8 jump instructions for x86)
#define DCOND_EQ IOP_JE
#define DCOND_NE IOP_JNE
#define DCOND_MI IOP_JS      // MInus
#define DCOND_PL IOP_JNS     // PLus or zero
#define DCOND_HI IOP_JA      // higher (unsigned)
#define DCOND_HS IOP_JAE     // higher || same (unsigned)
#define DCOND_LO IOP_JB      // lower (unsigned)
#define DCOND_LS IOP_JBE     // lower || same (unsigned)
#define DCOND_GE IOP_JGE     // greater || equal (signed)
#define DCOND_GT IOP_JG      // greater (signed)
#define DCOND_LE IOP_JLE     // less || equal (signed)
#define DCOND_LT IOP_JL      // less (signed)
#define DCOND_VS IOP_JO      // oVerflow Set
#define DCOND_VC IOP_JNO     // oVerflow Clear

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

// _r_r
#define emith_move_r_r(dst, src) \
	EMIT_OP_MODRM(0x8b, 3, dst, src)

#define emith_add_r_r(d, s) \
	EMIT_OP_MODRM(0x01, 3, s, d)

#define emith_sub_r_r(d, s) \
	EMIT_OP_MODRM(0x29, 3, s, d)

#define emith_adc_r_r(d, s) \
	EMIT_OP_MODRM(0x11, 3, s, d)

#define emith_sbc_r_r(d, s) \
	EMIT_OP_MODRM(0x19, 3, s, d) /* SBB */

#define emith_or_r_r(d, s) \
	EMIT_OP_MODRM(0x09, 3, s, d)

#define emith_and_r_r(d, s) \
	EMIT_OP_MODRM(0x21, 3, s, d)

#define emith_eor_r_r(d, s) \
	EMIT_OP_MODRM(0x31, 3, s, d) /* XOR */

#define emith_tst_r_r(d, s) \
	EMIT_OP_MODRM(0x85, 3, s, d) /* TEST */

#define emith_cmp_r_r(d, s) \
	EMIT_OP_MODRM(0x39, 3, s, d)

// fake teq - test equivalence - get_flags(d ^ s)
#define emith_teq_r_r(d, s) { \
	emith_push(d); \
	emith_eor_r_r(d, s); \
	emith_pop(d); \
}

// _r_r_r
#define emith_eor_r_r_r(d, s1, s2) { \
	if (d != s1) \
		emith_move_r_r(d, s1); \
	emith_eor_r_r(d, s2); \
}

#define emith_or_r_r_r_lsl(d, s1, s2, lslimm) { \
	if (d != s2 && d != s1) { \
		emith_lsl(d, s2, lslimm); \
		emith_or_r_r(d, s1); \
	} else { \
		if (d != s1) \
			emith_move_r_r(d, s1); \
		emith_push(s2); \
		emith_lsl(s2, s2, lslimm); \
		emith_or_r_r(d, s2); \
		emith_pop(s2); \
	} \
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

// 2 - adc, 3 - sbb, 6 - xor
#define emith_add_r_imm(r, imm) \
	emith_arith_r_imm(0, r, imm)

#define emith_or_r_imm(r, imm) \
	emith_arith_r_imm(1, r, imm)

#define emith_and_r_imm(r, imm) \
	emith_arith_r_imm(4, r, imm)

#define emith_sub_r_imm(r, imm) \
	emith_arith_r_imm(5, r, imm)

#define emith_cmp_r_imm(r, imm) \
	emith_arith_r_imm(7, r, imm)

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
	emith_add_r_imm(r, imm); \
}

#define emith_or_r_imm_c(cond, r, imm) { \
	(void)(cond); \
	emith_or_r_imm(r, imm); \
}

#define emith_sub_r_imm_c(cond, r, imm) { \
	(void)(cond); \
	emith_sub_r_imm(r, imm); \
}

#define emith_bic_r_imm_c(cond, r, imm) { \
	(void)(cond); \
	emith_bic_r_imm(r, imm); \
}

// shift
#define emith_shift(op, d, s, cnt) { \
	if (d != s) \
		emith_move_r_r(d, s); \
	EMIT_OP_MODRM(0xc1, 3, op, d); \
	EMIT(cnt, u8); \
}

#define emith_lsl(d, s, cnt) \
	emith_shift(4, d, s, cnt)

#define emith_lsr(d, s, cnt) \
	emith_shift(5, d, s, cnt)

#define emith_asr(d, s, cnt) \
	emith_shift(7, d, s, cnt)

#define emith_rol(d, s, cnt) \
	emith_shift(0, d, s, cnt)

#define emith_ror(d, s, cnt) \
	emith_shift(1, d, s, cnt)

#define emith_rolc(r) \
	EMIT_OP_MODRM(0xd1, 3, 2, r)

#define emith_rorc(r) \
	EMIT_OP_MODRM(0xd1, 3, 3, r)

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

// put bit0 of r0 to carry
#define emith_set_carry(r0) { \
	emith_tst_r_imm(r0, 1); /* clears C */ \
	EMITH_SJMP_START(DCOND_EQ); \
	EMIT_OP(0xf9); /* STC */ \
	EMITH_SJMP_END(DCOND_EQ); \
}

// put bit0 of r0 to carry (for subtraction)
#define emith_set_carry_sub emith_set_carry

// XXX: stupid mess
#define emith_mul_(op, dlo, dhi, s1, s2) { \
	int rmr; \
	if (dlo != xAX && dhi != xAX) \
		emith_push(xAX); \
	if (dlo != xDX && dhi != xDX) \
		emith_push(xDX); \
	if ((s1) == xAX) \
		rmr = s2; \
	else if ((s2) == xAX) \
		rmr = s1; \
	else { \
		emith_move_r_r(xAX, s1); \
		rmr = s2; \
	} \
	EMIT_OP_MODRM(0xf7, 3, op, rmr); /* xMUL rmr */ \
	/* XXX: using push/pop for the case of edx->eax; eax->edx */ \
	if (dhi != xDX && dhi != -1) \
		emith_push(xDX); \
	if (dlo != xAX) \
		emith_move_r_r(dlo, xAX); \
	if (dhi != xDX && dhi != -1) \
		emith_pop(dhi); \
	if (dlo != xDX && dhi != xDX) \
		emith_pop(xDX); \
	if (dlo != xAX && dhi != xAX) \
		emith_pop(xAX); \
}

#define emith_mul_u64(dlo, dhi, s1, s2) \
	emith_mul_(4, dlo, dhi, s1, s2) /* MUL */

#define emith_mul_s64(dlo, dhi, s1, s2) \
	emith_mul_(5, dlo, dhi, s1, s2) /* IMUL */

#define emith_mul(d, s1, s2) \
	emith_mul_(4, d, -1, s1, s2)

// "flag" instructions are the same
#define emith_subf_r_imm emith_sub_r_imm
#define emith_addf_r_r   emith_add_r_r
#define emith_subf_r_r   emith_sub_r_r
#define emith_adcf_r_r   emith_adc_r_r
#define emith_sbcf_r_r   emith_sbc_r_r

#define emith_lslf  emith_lsl
#define emith_lsrf  emith_lsr
#define emith_asrf  emith_asr
#define emith_rolf  emith_rol
#define emith_rorf  emith_ror
#define emith_rolcf emith_rolc
#define emith_rorcf emith_rorc

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

#define emith_write_sr(srcr) { \
	int tmp = rcache_get_tmp(); \
	int srr = rcache_get_reg(SHR_SR, RC_GR_RMW); \
	emith_clear_msb(tmp, srcr, 20); \
	emith_bic_r_imm(srr, 0xfff); \
	emith_or_r_r(srr, tmp); \
	rcache_free_tmp(tmp); \
}

#define emith_carry_to_t(srr, is_sub) { \
	int tmp = rcache_get_tmp(); \
	EMIT_OP(0x0f); \
	EMIT(0x92, u8); \
	EMIT_MODRM(3, 0, tmp); /* SETC */ \
	emith_bic_r_imm(srr, 1); \
	EMIT_OP_MODRM(0x08, 3, tmp, srr); /* OR srrl, tmpl */ \
	rcache_free_tmp(tmp); \
}

