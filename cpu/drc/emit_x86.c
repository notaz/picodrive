#include <stdarg.h>

// TODO: move
static int reg_map_g2h[] = {
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
};

enum { xAX = 0, xCX, xDX, xBX, xSP, xBP, xSI, xDI };

#define EMIT_PTR(ptr, val, type) \
	*(type *)(ptr) = val

#define EMIT(val, type) { \
	EMIT_PTR(tcache_ptr, val, type); \
	tcache_ptr = (char *)tcache_ptr + sizeof(type); \
}

#define EMIT_MODRM(mod,r,rm) \
	EMIT(((mod)<<6) | ((r)<<3) | (rm), u8)

#define EMIT_OP_MODRM(op,mod,r,rm) { \
	EMIT(op, u8); \
	EMIT_MODRM(mod, r, rm); \
}

#define emith_move_r_r(dst, src) \
	EMIT_OP_MODRM(0x8b, 3, dst, src)

#define emith_move_r_imm(r, imm) { \
	EMIT(0xb8 + (r), u8); \
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
	EMIT_OP_MODRM(0x8b, 1, r, 5); \
	EMIT(offs, u8); 	/* mov tmp, [ebp+#offs] */ \
}

#define emith_ctx_write(r, offs) { \
	EMIT_OP_MODRM(0x89, 1, r, 5); \
	EMIT(offs, u8); 	/* mov [ebp+#offs], tmp */ \
}

#define emith_ctx_sub(val, offs) { \
	EMIT_OP_MODRM(0x81, 1, 5, 5); \
	EMIT(offs, u8); \
	EMIT(val, u32); 	/* sub [ebp+#offs], dword val */ \
}

#define emith_test_t() { \
	if (reg_map_g2h[SHR_SR] == -1) { \
		EMIT(0xf6, u8); \
		EMIT_MODRM(1, 0, 5); \
		EMIT(SHR_SR * 4, u8); \
		EMIT(0x01, u8); /* test [ebp+SHR_SR], byte 1 */ \
	} else { \
		EMIT(0xf7, u8); \
		EMIT_MODRM(3, 0, reg_map_g2h[SHR_SR]); \
		EMIT(0x01, u16); /* test <reg>, word 1 */ \
	} \
}

#define emith_jump(ptr) { \
	u32 disp = (u32)ptr - ((u32)tcache_ptr + 5); \
	EMIT(0xe9, u8); \
	EMIT(disp, u32); \
}

#define emith_call(ptr) { \
	u32 disp = (u32)ptr - ((u32)tcache_ptr + 5); \
	EMIT(0xe8, u8); \
	EMIT(disp, u32); \
}

#define EMIT_CONDITIONAL(code, is_nonzero) { \
	char *ptr = tcache_ptr; \
	tcache_ptr = (char *)tcache_ptr + 2; \
	code; \
	EMIT_PTR(ptr, ((is_nonzero) ? 0x75 : 0x74), u8); \
	EMIT_PTR(ptr + 1, ((char *)tcache_ptr - (ptr + 2)), u8); \
}

static void emith_pass_arg(int count, ...)
{
	va_list vl;
	int i;

	va_start(vl, count);

	for (i = 0; i < count; i++) {
		long av = va_arg(vl, long);
		int r = 7;

		switch (i) {
		case 0: r = xAX; break;
		case 1: r = xDX; break;
		case 2: r = xCX; break;
		}
		emith_move_r_imm(r, av);
	}

	va_end(vl);
}

