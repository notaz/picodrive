#include <stdarg.h>

#if (DRC_DEBUG & 1)
#define COUNT_OP \
	host_insn_count++
#else
#define COUNT_OP
#endif

enum { xAX = 0, xCX, xDX, xBX, xSP, xBP, xSI, xDI };

#define CONTEXT_REG xBP

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

#define emith_move_r_r(dst, src) \
	EMIT_OP_MODRM(0x8b, 3, dst, src)

#define emith_move_r_imm(r, imm) { \
	EMIT_OP(0xb8 + (r)); \
	EMIT(imm, u32); \
}

#define emith_add_r_imm(r, imm) { \
	EMIT_OP_MODRM(0x81, 3, 0, r); \
	EMIT(imm, u32); \
}

#define emith_sub_r_imm(r, imm) { \
	EMIT_OP_MODRM(0x81, 3, 5, r); \
	EMIT(imm, u32); \
}

// XXX: offs is 8bit only
#define emith_ctx_read(r, offs) { \
	EMIT_OP_MODRM(0x8b, 1, r, xBP); \
	EMIT(offs, u8); 	/* mov tmp, [ebp+#offs] */ \
}

#define emith_ctx_write(r, offs) { \
	EMIT_OP_MODRM(0x89, 1, r, xBP); \
	EMIT(offs, u8); 	/* mov [ebp+#offs], tmp */ \
}

#define emith_ctx_sub(val, offs) { \
	EMIT_OP_MODRM(0x81, 1, 5, xBP); \
	EMIT(offs, u8); \
	EMIT(val, u32); 	/* sub [ebp+#offs], dword val */ \
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

#define EMITH_CONDITIONAL(code, is_nonzero) { \
	u8 *ptr = tcache_ptr; \
	tcache_ptr = tcache_ptr + 2; \
	code; \
	EMIT_PTR(ptr, ((is_nonzero) ? 0x75 : 0x74), u8); \
	EMIT_PTR(ptr + 1, (tcache_ptr - (ptr + 2)), u8); \
}

#define arg2reg(rd, arg) \
	switch (arg) { \
	case 0: rd = xAX; break; \
	case 1: rd = xDX; break; \
	case 2: rd = xCX; break; \
	}

#define emith_pass_arg_r(arg, reg) { \
	int rd = 7; \
	arg2reg(rd, arg); \
	emith_move_r_r(rd, reg); \
}

#define emith_pass_arg_imm(arg, imm) { \
	int rd = 7; \
	arg2reg(rd, arg); \
	emith_move_r_imm(rd, imm); \
}

/* SH2 drc specific */
#define emith_test_t() { \
	if (reg_map_g2h[SHR_SR] == -1) { \
		EMIT_OP_MODRM(0xf6, 1, 0, 5); \
		EMIT(SHR_SR * 4, u8); \
		EMIT(0x01, u8); /* test [ebp+SHR_SR], byte 1 */ \
	} else { \
		EMIT_OP_MODRM(0xf7, 3, 0, reg_map_g2h[SHR_SR]); \
		EMIT(0x01, u16); /* test <reg>, word 1 */ \
	} \
}

