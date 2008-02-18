// 187 blocks, 12072 bytes
// 14 IRAM blocks

#include "../../PicoInt.h"

#define TCACHE_SIZE (1024*1024)
static unsigned int *block_table[0x5090/2];
static unsigned int *block_table_iram[15][0x800/2];
static unsigned int *tcache = NULL;
static unsigned int *tcache_ptr = NULL;

static int had_jump = 0;
static int nblocks = 0;
static int iram_context = 0;

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


#define PROGRAM(x) ((unsigned short *)svp->iram_rom)[x]

static void *translate_block(int pc)
{
	unsigned int op, op1, icount = 0;
	unsigned int *block_start;

	// create .pool
	*tcache_ptr++ = (u32) &g_cycles;		// -3 g_cycles
	*tcache_ptr++ = (u32) &ssp->gr[SSP_PC].v;	// -2 ptr to rPC
	*tcache_ptr++ = (u32) in_funcs;			// -1 func pool

	printf("translate %04x -> %04x\n", pc<<1, (tcache_ptr-tcache)<<2);
	block_start = tcache_ptr;

	emit_block_prologue();

	for (; icount < 100;)
	{
		icount++;
		//printf("  insn #%i\n", icount);
		op = PROGRAM(pc++);
		op1 = op >> 9;

		emit_mov_const(0, op);

		// need immediate?
		if ((op1 & 0xf) == 4 || (op1 & 0xf) == 6) {
			emit_mov_const(1, PROGRAM(pc++)); // immediate
		}

		// dump PC
		emit_pc_inc(block_start, pc);

		emit_call(block_start, op1);

		if (in_funcs[op1] == NULL) {
			printf("NULL func! op=%08x (%02x)\n", op, op1);
			exit(1);
		}
		if (op1 == 0x24 || op1 == 0x26 || // call, bra
			((op1 == 0 || op1 == 1 || op1 == 4 || op1 == 5 || op1 == 9 || op1 == 0x25) &&
				(op & 0xf0) == 0x60)) { // ld PC
			break;
		}
	}

	emit_block_epilogue(block_start, icount + 1);
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

#if 0
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

int ssp1601_dyn_init(void)
{
	tcache = tcache_ptr = malloc(TCACHE_SIZE);
	if (tcache == NULL) {
		printf("oom\n");
		exit(1);
	}
	memset(tcache, 0, sizeof(TCACHE_SIZE));
	memset(block_table, 0, sizeof(block_table));
	memset(block_table_iram, 0, sizeof(block_table_iram));
	*tcache_ptr++ = 0xffffffff;

	return 0;
}


void ssp1601_dyn_reset(ssp1601_t *ssp)
{
	ssp1601_reset_local(ssp);
}

void ssp1601_dyn_run(int cycles)
{
	while (cycles > 0)
	{
		void (*trans_entry)(void);
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

		//printf("enter @ %04x, PC=%04x\n", (PC - tcache)<<1, rPC<<1);
		g_cycles = 0;
		//printf("enter %04x\n", rPC);
		trans_entry();
		//printf("leave %04x\n", rPC);
		cycles -= g_cycles;
/*
		if (!had_jump) {
			// no jumps
			if (pc_old < 0x800/2)
				rPC += (PC - block_table_iram[iram_context][pc_old]) - 1;
			else
				rPC += (PC - block_table[pc_old]) - 1;
		}
*/
		//printf("end   @ %04x, PC=%04x\n", (PC - tcache)<<1, rPC<<1);
/*
		if (pc_old < 0x400) {
			// flush IRAM cache
			tcache_ptr = block_table[pc_old];
			block_table[pc_old] = NULL;
			nblocks--;
		}
		if (pc_old >= 0x400 && rPC < 0x400)
		{
			int i, crc = chksum_crc32(svp->iram_rom, 0x800);
			for (i = 0; i < 32; i++)
				if (iram_crcs[i] == crc) break;
			if (i == 32) {
				char name[32];
				for (i = 0; i < 32 && iram_crcs[i]; i++);
				iram_crcs[i] = crc;
				printf("%i IRAMs\n", i+1);
				sprintf(name, "ir%08x.bin", crc);
				debug_dump2file(name, svp->iram_rom, 0x800);
			}
			printf("CRC %08x %08x\n", crc, iram_id);
		}
*/
	}
//	debug_dump2file("tcache.bin", tcache, (tcache_ptr - tcache) << 1);
//	exit(1);
}

