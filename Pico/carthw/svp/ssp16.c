// basic, incomplete SSP160x (SSP1601?) interpreter

/*
 * Register info
 * most names taken from MAME code
 *
 * 0. "-"
 *   size: 16
 *   desc: Constant register with all bits set (0xffff).
 *
 * 1. "X"
 *   size: 16
 *   desc: Generic register. When set, updates P (P = X * Y * 2) ??
 *
 * 2. "Y"
 *   size: 16
 *   desc: Generic register. When set, updates P (P = X * Y * 2) ??
 *
 * 3. "A"
 *   size: 32
 *   desc: Accumulator.
 *
 * 4. "ST"
 *   size: 16
 *   desc: Status register. From MAME: bits 0-9 are CONTROL, other FLAG
 *     fedc ba98 7654 3210
 *       210 - RPL (?)       "Loop size". If non-zero, makes (rX+) and (rX-) respectively
 *                           modulo-increment and modulo-decrement. The value shows which
 *                           power of 2 to use, i.e. 4 means modulo by 16.
 *                           (e: fir16_32.sc, IIR_4B.SC, DECIM.SC)
 *       43  - RB (?)
 *       5   - GP0_0 (ST5?)  Changed before acessing PM0 (affects banking?).
 *       6   - GP0_1 (ST6?)  Cleared before acessing PM0 (affects banking?). Set after.
 *                           datasheet says these (5,6) bits correspond to hardware pins.
 *       7   - IE (?)        Not directly used by SVP code (never set, but preserved)?
 *       8   - OP (?)        Not used by SVP code (only cleared)? (MAME: saturated value
 *                           (probably means clamping? i.e. 0x7ffc + 9 -> 0x7fff))
 *       9   - MACS (?)      Not used by SVP code (only cleared)? (e: "mac shift")
 *       a   - GPI_0         Interrupt 0 enable/status?
 *       b   - GPI_1         Interrupt 1 enable/status?
 *       c   - L             L flag. Carry?
 *       d   - Z             Zero flag.
 *       e   - OV            Overflow flag.
 *       f   - N             Negative flag.
 *     seen directly changing code sequences:
 *       ldi ST, 0      ld  A, ST     ld  A, ST     ld  A, ST     ldi st, 20h
 *       ldi ST, 60h    ori A, 60h    and A, E8h    and A, E8h
 *                      ld  ST, A     ld  ST, A     ori 3
 *                                                  ld  ST, A
 *
 * 5. "STACK"
 *   size: 16
 *   desc: hw stack of 6 levels (according to datasheet)
 *
 * 6. "PC"
 *   size: 16
 *   desc: Program counter.
 *
 * 7. "P"
 *   size: 32
 *   desc: multiply result register. Updated after mp* instructions,
 *         or writes to X or Y (P = X * Y * 2) ??
 *         probably affected by MACS bit in ST.
 *
 * 8. "PM0" (PM from PMAR name from Tasco's docs)
 *   size: 16?
 *   desc: Programmable Memory access register.
 *         On reset, or when one (both?) GP0 bits are clear,
 *         acts as some additional status reg?
 *
 * 9. "PM1"
 *   size: 16?
 *   desc: Programmable Memory access register.
 *         This reg. is only used as PMAR.
 *
 * 10. "PM2"
 *   size: 16?
 *   desc: Programmable Memory access register.
 *         This reg. is only used as PMAR.
 *
 * 11. "XST"
 *   size: 16?
 *   desc: eXternal STate. Mapped to a15000 at 68k side.
 *         Can be programmed as PMAR? (only seen in test mode code)
 *
 * 12. "PM4"
 *   size: 16?
 *   desc: Programmable Memory access register.
 *         This reg. is only used as PMAR. The most used PMAR by VR.
 *
 * 13. (unused by VR)
 *
 * 14. "PMC" (PMC from PMAC name from Tasco's docs)
 *   size: 32?
 *   desc: Programmable Memory access Control. Set using 2 16bit writes,
 *         first address, then mode word. After setting PMAC, PMAR sould
 *         be accessed to program it.
 *
 * 15. "AL"
 *   size: 16
 *   desc: Accumulator Low. 16 least significant bits of accumulator (not 100% sure)
 *         (normally reading acc (ld X, A) you get 16 most significant bits).
 *
 *
 * There are 8 8-bit pointer registers rX. r0-r3 (ri) point to RAM0, r4-r7 (rj) point to RAM1.
 * They can be accessed directly, or 2 indirection levels can be used [ (rX), ((rX)) ],
 * which work similar to * and ** operators in C, only they use different memory banks and
 * ((rX)) also does post-increment. First indirection level (rX) accesses RAMx, second accesses
 * program memory at address read from (rX), and increments value in (rX).
 *
 * r0,r1,r2,r4,r5,r6 can be modified [ex: ldi r0, 5].
 * 3 modifiers can be applied (optional):
 *  + : post-increment [ex: ld a, (r0+) ]. Can be made modulo-increment by setting RPL bits in ST.
 *  - : post-decrement. Can be made modulo-decrement by setting RPL bits in ST (not sure).
 *  +!: post-increment, unaffected by RPL (probably).
 * These are only used on 1st indirection level, so things like [ld a, ((r0+))] and [ld X, r6-]
 * ar probably invalid.
 *
 * r3 and r7 are special and can not be changed (at least Samsung samples and SVP code never do).
 * They are fixed to the start of their RAM banks. (They are probably changeable for ssp1605+,
 * Samsung's old DSP page claims that).
 * 1 of these 4 modifiers must be used (short form direct addressing?):
 *  |00: RAMx[0] [ex: (r3|00), 0] (based on sample code)
 *  |01: RAMx[1]
 *  |10: RAMx[2] ? maybe 10h? accortding to Div_c_dp.sc, 2
 *  |11: RAMx[3]
 *
 *
 * Instruction notes
 *
 * mld (rj), (ri) [, b]
 *   operation: A = 0; P = (rj) * (ri)
 *   notes: based on IIR_4B.SC sample. flags? what is b???
 *   TODO: figure out if (rj) and (ri) get loaded in X and Y
 *
 * mpya (rj), (ri) [, b]
 *   name: multiply and add?
 *   operation: A += P; P = (rj) * (ri)
 *
 * mpys (rj), (ri), b
 *   name: multiply and subtract?
 *   notes: not used by VR code.
 *
 *
 * Assumptions in this code
 *   P is not directly writeable
 *   flags correspond to full 32bit accumulator
 *   only Z and N status flags are emulated (others unused by SVP)
 *   modifiers for 'OP a, ri' are ignored (invalid?/not used by SVP)
 *   modifiers '+' and '+!' act the same (this is most likely wrong)
 *   'ld d, (a)' loads from program ROM
 */

#include "../../PicoInt.h"

#define u32 unsigned int

// 0
#define rX     ssp->gr[SSP_X].h
#define rY     ssp->gr[SSP_Y].h
#define rA     ssp->gr[SSP_A].h
#define rST    ssp->gr[SSP_ST].h	// 4
#define rSTACK ssp->gr[SSP_STACK].h
#define rPC    ssp->gr[SSP_PC].h
#define rP     ssp->gr[SSP_P]
#define rPM0   ssp->gr[SSP_PM0].h	// 8
#define rPM1   ssp->gr[SSP_PM1].h
#define rPM2   ssp->gr[SSP_PM2].h
#define rXST   ssp->gr[SSP_XST].h
#define rPM4   ssp->gr[SSP_PM4].h	// 12
// 13
#define rPMC   ssp->gr[SSP_PMC]		// will keep addr in .h, mode in .l
#define rAL    ssp->gr[SSP_A].l

#define rA32   ssp->gr[SSP_A].v
#define rIJ    ssp->r

#define IJind  (((op>>6)&4)|(op&3))

#define GET_PC() (PC - (unsigned short *)svp->iram_rom)
#define GET_PPC_OFFS() ((unsigned int)PC - (unsigned int)svp->iram_rom - 2)
#define SET_PC(d) PC = (unsigned short *)svp->iram_rom + d

#define REG_READ(r) (((r) <= 4) ? ssp->gr[r].h : read_handlers[r]())
// if r is 'A', should we set flags?
#define REG_WRITE(r,d) { \
	int r1 = r; \
	if (r1 >= 4) write_handlers[r1](d); \
	else if (r1 > 0) ssp->gr[r1].h = d; \
}

// flags
#define FLAG_L (1<<0xc)
#define FLAG_Z (1<<0xd)
#define FLAG_V (1<<0xe)
#define FLAG_N (1<<0xf)

// update ZN according to 32bit ACC.
#define UPD_ACC_ZN \
	rST &= ~(FLAG_Z|FLAG_N); \
	if (!rA32) rST |= FLAG_Z; \
	else rST |= (rA32>>16)&FLAG_N;

// it seems SVP code never checks for L and OV, so we leave them out.
// rST |= (t>>4)&FLAG_L;
#define UPD_t_LZVN \
	rST &= ~(FLAG_L|FLAG_Z|FLAG_V|FLAG_N); \
	if (!t) rST |= FLAG_Z; \
	else    rST |= t&FLAG_N; \

// standard cond processing.
// again, only Z and N is checked, as SVP doesn't seem to use any other conds.
#define COND_CHECK \
	switch (op&0xf0) { \
		case 0x00: cond = 1; break; /* always true */ \
		case 0x50: cond = !((rST ^ (op<<5)) & FLAG_Z); break; /* Z matches f(?) bit */ \
		case 0x70: cond = !((rST ^ (op<<7)) & FLAG_N); break; /* N matches f(?) bit */ \
		default:elprintf(EL_SVP, "unimplemented cond @ %04x", GET_PPC_OFFS()); break; \
	}

// ops with accumulator.
// how is low word really affected by these?
// not sure if 'ld A' affects flags (assume it does..)
#define OP_LDA(x) \
	ssp->gr[SSP_A].h = x; \
	UPD_ACC_ZN

#define OP_SUBA(x) { \
	u32 t = (ssp->gr[SSP_A].v >> 16) - (x); \
	UPD_t_LZVN \
	ssp->gr[SSP_A].h = t; \
}

#define OP_CMPA(x) { \
	u32 t = (ssp->gr[SSP_A].v >> 16) - (x); \
	UPD_t_LZVN \
}

#define OP_ADDA(x) { \
	u32 t = (ssp->gr[SSP_A].v >> 16) + (x); \
	UPD_t_LZVN \
	ssp->gr[SSP_A].h = t; \
}

#define OP_ANDA(x) \
	ssp->gr[SSP_A].v &= (x) << 16; \
	UPD_ACC_ZN

#define OP_ORA(x) \
	ssp->gr[SSP_A].v |= (x) << 16; \
	UPD_ACC_ZN

#define OP_EORA(x) \
	ssp->gr[SSP_A].v ^= (x) << 16; \
	UPD_ACC_ZN


static ssp1601_t *ssp = NULL;
static unsigned short *PC;
static int g_cycles;
// debug
static int running = 0;

// -----------------------------------------------------
// register i/o handlers

// 0-4, 13
static u32 read_unknown(void)
{
	elprintf(EL_ANOMALY|EL_SVP, "ssp16: unknown read @ %04x", GET_PPC_OFFS());
	return 0;
}

static void write_unknown(u32 d)
{
	elprintf(EL_ANOMALY|EL_SVP, "ssp16: unknown write @ %04x", GET_PPC_OFFS());
}

// 4
static void write_ST(u32 d)
{
	if ((rST ^ d) & 7) {
		elprintf(EL_SVP, "ssp16: RPL %i -> %i @ %04x", rST&7, d&7, GET_PPC_OFFS());
		running = 0;
	}
	rST = d;
}

// 5
static u32 read_STACK(void)
{
	//elprintf(EL_SVP, "pop  %i @ %04x", rSTACK, GET_PPC_OFFS());
	--rSTACK;
	if ((short)rSTACK < 0) {
		rSTACK = 5;
		elprintf(EL_ANOMALY|EL_SVP, "ssp16: stack underflow! (%i) @ %04x", rSTACK, GET_PPC_OFFS());
	}
	return ssp->stack[rSTACK];
}

static void write_STACK(u32 d)
{
	if (rSTACK >= 6) {
		//running = 0;
		elprintf(EL_ANOMALY|EL_SVP, "ssp16: stack overflow! (%i) @ %04x", rSTACK, GET_PPC_OFFS());
		rSTACK = 0;
	}
	ssp->stack[rSTACK++] = d;
}

// 6
static u32 read_PC(void)
{
	return GET_PC();
}

static void write_PC(u32 d)
{
	SET_PC(d);
	g_cycles--;
}

// 7
static u32 read_P(void)
{
	rP.v = (u32)rX * rY * 2;
	return rP.h;
}

// -----------------------------------------------------

static void iram_write(int addr, u32 d, int reg, int inc)
{
	if ((addr&0xfc00) != 0x8000)
		elprintf(EL_SVP|EL_ANOMALY, "ssp invalid IRAM addr: %04x", addr<<1);
	elprintf(EL_SVP, "ssp IRAM w [%06x] %04x (inc %i)", (addr<<1)&0x7ff, d, inc);
	((unsigned short *)svp->iram_rom)[addr&0x3ff] = d;
	ssp->pmac_write[reg] += inc<<16;
}

static u32 pm_io(int reg, int write, u32 d)
{
	if (ssp->emu_status & SSP_PMC_SET) {
		elprintf(EL_SVP, "PM%i (%c) set to %08x @ %04x", reg, write ? 'w' : 'r', rPMC.v, GET_PPC_OFFS());
		ssp->pmac_read[write ? reg + 6 : reg] = rPMC.v;
		ssp->emu_status &= ~SSP_PMC_SET;
		return 0;
	}

	// just in case
	ssp->emu_status &= ~SSP_PMC_HAVE_ADDR;

//	if (ssp->pmac_read[reg] != 0)
	if (reg == 4 || (rST & 0x60))
	{
		if (write)
		{
			int mode = ssp->pmac_write[reg]&0xffff;
			int addr = ssp->pmac_write[reg]>>16;
			switch (mode) {
				case 0x0018: elprintf(EL_SVP, "ssp DRAM w [%06x] %04x", addr<<1, d);
				             ((unsigned short *)svp->dram)[addr] = d;
					     break;
				case 0x0818: elprintf(EL_SVP, "ssp DRAM w [%06x] %04x (inc 1)", addr<<1, d);
				             ((unsigned short *)svp->dram)[addr] = d;
					     ssp->pmac_write[reg] += 1<<16;
					     break;
				case 0x081c: iram_write(addr, d, reg, 1); break; // checked: used by code @ 0902
				case 0x101c: iram_write(addr, d, reg, 2); break; // checked: used by code @ 3b7c
				default:     elprintf(EL_SVP|EL_ANOMALY, "ssp PM%i unhandled write mode %04x, [%06x] %04x @ %04x",
							reg, mode, addr<<1, d, GET_PPC_OFFS()); break;
			}
		}
		else
		{
			int mode = ssp->pmac_read[reg]&0xffff;
			int addr = ssp->pmac_read[reg]>>16;
			switch (mode) {
				case 0x0809: elprintf(EL_SVP, "ssp ROM  r [%06x] %04x", (addr|((mode&0xf)<<16))<<1,
							((unsigned short *)Pico.rom)[addr|((mode&0xf)<<16)]);
				             // possibly correct, the first word read is some sort of counter, sane values in ROM
					     ssp->pmac_read[reg] += 1<<16;
				             return ((unsigned short *)Pico.rom)[addr|((mode&0xf)<<16)];
				case 0x0018: elprintf(EL_SVP, "ssp DRAM r [%06x] %04x", addr<<1, ((unsigned short *)svp->dram)[addr]);
				             return ((unsigned short *)svp->dram)[addr]; // checked
				case 0x0818: elprintf(EL_SVP, "ssp DRAM r [%06x] %04x (inc 1)", addr<<1, ((unsigned short *)svp->dram)[addr]);
					     ssp->pmac_read[reg] += 1<<16;
				             return ((unsigned short *)svp->dram)[addr];
				case 0x3018: elprintf(EL_SVP, "ssp DRAM r [%06x] %04x (inc 32)", addr<<1, ((unsigned short *)svp->dram)[addr]);
					     ssp->pmac_read[reg] += 32<<16;
				             return ((unsigned short *)svp->dram)[addr];
				default:     elprintf(EL_SVP|EL_ANOMALY, "ssp PM%i unhandled read  mode %04x, [%06x] @ %04x",
							reg, mode, addr<<1, GET_PPC_OFFS()); break;
			}
		}
		return 0;
	}

	return (u32)-1;
}

// 8
static u32 read_PM0(void)
{
	u32 d = pm_io(0, 0, 0);
	if (d != (u32)-1) return d;
	if (GET_PPC_OFFS() != 0x800 || rPM0 != 0) // debug
		elprintf(EL_SVP, "PM0 raw r %04x @ %04x", rPM0, GET_PPC_OFFS());
	return rPM0;
}

static void write_PM0(u32 d)
{
	u32 r = pm_io(0, 1, d);
	if (r != (u32)-1) return;
	elprintf(EL_SVP, "PM0 raw w %04x @ %04x", d, GET_PPC_OFFS());
	rPM0 = d;
}

// 9
static u32 read_PM1(void)
{
	u32 d = pm_io(1, 0, 0);
	if (d != (u32)-1) return d;
	// can be removed?
	elprintf(EL_SVP, "PM1 raw r %04x @ %04x", rPM1, GET_PPC_OFFS());
	return rPM1;
}

static void write_PM1(u32 d)
{
	u32 r = pm_io(1, 1, d);
	if (r != (u32)-1) return;
	// can be removed?
	elprintf(EL_SVP, "PM1 raw w %04x @ %04x", d, GET_PPC_OFFS());
	rPM1 = d;
}

// 10
static u32 read_PM2(void)
{
	u32 d = pm_io(2, 0, 0);
	if (d != (u32)-1) return d;
	// can be removed?
	elprintf(EL_SVP, "PM2 raw r %04x @ %04x", rPM2, GET_PPC_OFFS());
	return rPM2;
}

static void write_PM2(u32 d)
{
	u32 r = pm_io(2, 1, d);
	if (r != (u32)-1) return;
	// can be removed?
	elprintf(EL_SVP, "PM2 raw w %04x @ %04x", d, GET_PPC_OFFS());
	rPM2 = d;
}

// 11
static u32 read_XST(void)
{
	// can be removed?
	u32 d = pm_io(3, 0, 0);
	if (d != (u32)-1) return d;

	elprintf(EL_SVP, "XST raw r %04x @ %04x", rXST, GET_PPC_OFFS());
	return rXST;
}

static void write_XST(u32 d)
{
	// can be removed?
	u32 r = pm_io(3, 1, d);
	if (r != (u32)-1) return;

	elprintf(EL_SVP, "XST raw w %04x @ %04x", d, GET_PPC_OFFS());
	rXST = d;
}

// 12
static u32 read_PM4(void)
{
	u32 d = pm_io(4, 0, 0);
	if (d != (u32)-1) return d;
	// can be removed?
	elprintf(EL_SVP, "PM4 raw r %04x @ %04x", rPM4, GET_PPC_OFFS());
	return rPM4;
}

static void write_PM4(u32 d)
{
	u32 r = pm_io(4, 1, d);
	if (r != (u32)-1) return;
	// can be removed?
	elprintf(EL_SVP, "PM4 raw w %04x @ %04x", d, GET_PPC_OFFS());
	rPM4 = d;
}

// 14
static u32 read_PMC(void)
{
	if (ssp->emu_status & SSP_PMC_HAVE_ADDR) {
		if (ssp->emu_status & SSP_PMC_SET)
			elprintf(EL_ANOMALY|EL_SVP, "prev PMC not used @ %04x", GET_PPC_OFFS());
		ssp->emu_status |= SSP_PMC_SET;
		ssp->emu_status &= ~SSP_PMC_HAVE_ADDR;
		return rPMC.l;
	} else {
		ssp->emu_status |= SSP_PMC_HAVE_ADDR;
		return rPMC.h;
	}
}

static void write_PMC(u32 d)
{
	if (ssp->emu_status & SSP_PMC_HAVE_ADDR) {
		if (ssp->emu_status & SSP_PMC_SET)
			elprintf(EL_ANOMALY|EL_SVP, "prev PMC not used @ %04x", GET_PPC_OFFS());
		ssp->emu_status |= SSP_PMC_SET;
		ssp->emu_status &= ~SSP_PMC_HAVE_ADDR;
		rPMC.l = d;
	} else {
		ssp->emu_status |= SSP_PMC_HAVE_ADDR;
		rPMC.h = d;
	}
}

// 15
static u32 read_AL(void)
{
	// TODO: figure out what's up with those blind reads..
	return rAL;
}

static void write_AL(u32 d)
{
	rAL = d;
}


typedef u32 (*read_func_t)(void);
typedef void (*write_func_t)(u32 d);

static read_func_t read_handlers[16] =
{
	read_unknown, read_unknown, read_unknown, read_unknown, // -, X, Y, A
	read_unknown,	// 4 ST
	read_STACK,
	read_PC,
	read_P,
	read_PM0,	// 8
	read_PM1,
	read_PM2,
	read_XST,
	read_PM4,	// 12
	read_unknown,	// 13 gr13
	read_PMC,
	read_AL
};

static write_func_t write_handlers[16] =
{
	write_unknown, write_unknown, write_unknown, write_unknown, // -, X, Y, A
//	write_unknown,	// 4 ST
	write_ST,	// 4 ST (debug hook)
	write_STACK,
	write_PC,
	write_unknown,	// 7 P
	write_PM0,	// 8
	write_PM1,
	write_PM2,
	write_XST,
	write_PM4,	// 12
	write_unknown,	// 13 gr13
	write_PMC,
	write_AL
};

// -----------------------------------------------------
// pointer register handlers

//
#define ptr1_read(op) ptr1_read_(op&3,(op>>6)&4,(op<<1)&0x18)

static u32 ptr1_read_(int ri, int isj2, int modi3)
{
	//int t = (op&3) | ((op>>6)&4) | ((op<<1)&0x18);
	int t = ri | isj2 | modi3;
	switch (t)
	{
		// mod=0 (00)
		case 0x00:
		case 0x01:
		case 0x02: return ssp->RAM0[ssp->r0[t&3]];
		case 0x03: return ssp->RAM0[0];
		case 0x04:
		case 0x05:
		case 0x06: return ssp->RAM1[ssp->r1[t&3]];
		case 0x07: return ssp->RAM1[0];
		// mod=1 (01), "+!"
		// mod=3,      "+"
		case 0x08:
		case 0x18:
		case 0x09:
		case 0x19:
		case 0x0a:
		case 0x1a: return ssp->RAM0[ssp->r0[t&3]++];
		case 0x0b: return ssp->RAM0[1];
		case 0x0c:
		case 0x1c:
		case 0x0d:
		case 0x1d:
		case 0x0e:
		case 0x1e: return ssp->RAM1[ssp->r1[t&3]++];
		case 0x0f: return ssp->RAM1[1];
		// mod=2 (10), "-"
		case 0x10:
		case 0x11:
		case 0x12: return ssp->RAM0[ssp->r0[t&3]--];
		case 0x13: return ssp->RAM0[2];
		case 0x14:
		case 0x15:
		case 0x16: return ssp->RAM1[ssp->r1[t&3]--];
		case 0x17: return ssp->RAM1[2];
		// mod=3 (11)
		case 0x1b: return ssp->RAM0[3];
		case 0x1f: return ssp->RAM1[3];
	}

	return 0;
}

static void ptr1_write(int op, u32 d)
{
	int t = (op&3) | ((op>>6)&4) | ((op<<1)&0x18);
	switch (t)
	{
		// mod=0 (00)
		case 0x00:
		case 0x01:
		case 0x02: ssp->RAM0[ssp->r0[t&3]] = d; return;
		case 0x03: ssp->RAM0[0] = d; return;
		case 0x04:
		case 0x05:
		case 0x06: ssp->RAM1[ssp->r1[t&3]] = d; return;
		case 0x07: ssp->RAM1[0] = d; return;
		// mod=1 (01), "+!"
		// mod=3,      "+"
		case 0x08:
		case 0x18:
		case 0x09:
		case 0x19:
		case 0x0a:
		case 0x1a: ssp->RAM0[ssp->r0[t&3]++] = d; return;
		case 0x0b: ssp->RAM0[1] = d; return;
		case 0x0c:
		case 0x1c:
		case 0x0d:
		case 0x1d:
		case 0x0e:
		case 0x1e: ssp->RAM1[ssp->r1[t&3]++] = d; return;
		case 0x0f: ssp->RAM1[1] = d; return;
		// mod=2 (10), "-"
		case 0x10:
		case 0x11:
		case 0x12: ssp->RAM0[ssp->r0[t&3]--] = d; return;
		case 0x13: ssp->RAM0[2] = d; return;
		case 0x14:
		case 0x15:
		case 0x16: ssp->RAM1[ssp->r1[t&3]--] = d; return;
		case 0x17: ssp->RAM1[2] = d; return;
		// mod=3 (11)
		case 0x1b: ssp->RAM0[3] = d; return;
		case 0x1f: ssp->RAM1[3] = d; return;
	}
}

static u32 ptr2_read(int op)
{
	int mv = 0, t = (op&3) | ((op>>6)&4) | ((op<<1)&0x18);
	switch (t)
	{
		// mod=0 (00)
		case 0x00:
		case 0x01:
		case 0x02: mv = ssp->RAM0[ssp->r0[t&3]]++; break;
		case 0x03: mv = ssp->RAM0[0]++; break;
		case 0x04:
		case 0x05:
		case 0x06: mv = ssp->RAM1[ssp->r1[t&3]]++; break;
		case 0x07: mv = ssp->RAM1[0]++; break;
		// mod=1 (01)
		case 0x0b: mv = ssp->RAM0[1]++; break;
		case 0x0f: mv = ssp->RAM1[1]++; break;
		// mod=2 (10)
		case 0x13: mv = ssp->RAM0[2]++; break;
		case 0x17: mv = ssp->RAM1[2]++; break;
		// mod=3 (11)
		case 0x1b: mv = ssp->RAM0[3]++; break;
		case 0x1f: mv = ssp->RAM1[3]++; break;
		default:   elprintf(EL_SVP|EL_ANOMALY, "invalid mod in ((rX))? @ %04x", GET_PPC_OFFS());
		           return 0;
	}

	return ((unsigned short *)svp->iram_rom)[mv];
}


// -----------------------------------------------------

void ssp1601_reset(ssp1601_t *l_ssp)
{
	ssp = l_ssp;
	ssp->emu_status = 0;
	ssp->gr[SSP_GR0].v = 0xffff0000;
	rPC = 0x400;
	rSTACK = 0; // ? using ascending stack
}


static void debug_dump(void)
{
	printf("GR0:   %04x    X: %04x    Y: %04x  A: %08x\n", ssp->gr[SSP_GR0].h, rX, rY, ssp->gr[SSP_A].v);
	printf("PC:    %04x  (%04x)                P: %08x\n", GET_PC(), GET_PC() << 1, ssp->gr[SSP_P].v);
	printf("PM0:   %04x  PM1: %04x  PM2: %04x\n", rPM0, rPM1, rPM2);
	printf("XST:   %04x  PM4: %04x  PMC: %08x\n", rXST, rPM4, ssp->gr[SSP_PMC].v);
	printf(" ST:   %04x  %c%c%c%c,  GP0_0 %i,  GP0_1 %i\n", rST, rST&FLAG_N?'N':'n', rST&FLAG_V?'V':'v',
		rST&FLAG_Z?'Z':'z', rST&FLAG_L?'L':'l', (rST>>5)&1, (rST>>6)&1);
	printf("STACK: %i %04x %04x %04x %04x %04x %04x\n", rSTACK, ssp->stack[0], ssp->stack[1],
		ssp->stack[2], ssp->stack[3], ssp->stack[4], ssp->stack[5]);
	printf("r0-r2: %02x %02x %02x  r4-r6: %02x %02x %02x\n", rIJ[0], rIJ[1], rIJ[2], rIJ[4], rIJ[5], rIJ[6]);
	elprintf(EL_SVP, "cycles: %i, emu_status: %x", g_cycles, ssp->emu_status);
}

static void debug_dump_mem(void)
{
	int h, i;
	printf("RAM0\n");
	for (h = 0; h < 32; h++)
	{
		if (h == 16) printf("RAM1\n");
		printf("%03x:", h*16);
		for (i = 0; i < 16; i++)
			printf(" %04x", ssp->RAM[h*16+i]);
		printf("\n");
	}
}

static int bpts[10] = { 0, };

static void debug(unsigned int pc, unsigned int op)
{
	static char buffo[64] = {0,};
	char buff[64] = {0,};
	int i;

	if (running) {
		for (i = 0; i < 10; i++)
			if (pc != 0 && bpts[i] == pc) {
				printf("breakpoint %i\n", i);
				running = 0;
				break;
			}
	}
	if (running) return;

	printf("%04x (%02x) @ %04x\n", op, op >> 9, pc<<1);

	while (1)
	{
		printf("dbg> ");
		fflush(stdout);
		fgets(buff, sizeof(buff), stdin);
		if (buff[0] == '\n') strcpy(buff, buffo);
		else strcpy(buffo, buff);

		switch (buff[0]) {
			case   0: exit(0);
			case 'c':
			case 'r': running = 1; return;
			case 's':
			case 'n': return;
			case 'x': debug_dump(); break;
			case 'm': debug_dump_mem(); break;
			case 'b': {
				char *baddr = buff + 2;
				i = 0;
				if (buff[3] == ' ') { i = buff[2] - '0'; baddr = buff + 4; }
				bpts[i] = strtol(baddr, NULL, 16) >> 1;
				printf("breakpoint %i set @ %04x\n", i, bpts[i]<<1);
				break;
			}
			case 'd': {
				FILE *f = fopen("dump.bin", "wb");
				unsigned short *p = (unsigned short *)svp->iram_rom;
				int i;
				if (f) {
					for (i = 0; i < 0x10000; i++) p[i] = (p[i]<<8) | (p[i]>>8);
					fwrite(svp->iram_rom, 1, 0x20000, f);
					fclose(f);
					for (i = 0; i < 0x10000; i++) p[i] = (p[i]<<8) | (p[i]>>8);
					printf("dumped to dump.bin\n");
				}
				else
					printf("dump failed\n");
				break;
			}
			default:  printf("unknown command\n"); break;
		}
	}
}

void ssp1601_run(int cycles)
{
	SET_PC(rPC);
	g_cycles = cycles;
//running = 0;

	while (g_cycles > 0)
	{
		int op;
		u32 tmpv;

		op = *PC++;
		debug(GET_PC()-1, op);
		switch (op >> 9)
		{
			// ld d, s
			case 0x00:
				if (op == 0) break; // nop
				if (op == ((SSP_A<<4)|SSP_P)) { // A <- P
					// not sure. MAME claims that only hi word is transfered.
					read_P(); // update P
					ssp->gr[SSP_A].v = ssp->gr[SSP_P].v;
				}
				else
				{
					tmpv = REG_READ(op & 0x0f);
					REG_WRITE((op & 0xf0) >> 4, tmpv);
				}
				break;

			// ld d, (ri)
			case 0x01: tmpv = ptr1_read(op); REG_WRITE((op & 0xf0) >> 4, tmpv); break;

			// ld (ri), s
			case 0x02: tmpv = REG_READ((op & 0xf0) >> 4); ptr1_write(op, tmpv); break;

			// ldi d, imm
			case 0x04: tmpv = *PC++; REG_WRITE((op & 0xf0) >> 4, tmpv); break;

			// ld d, ((ri))
			case 0x05: tmpv = ptr2_read(op); REG_WRITE((op & 0xf0) >> 4, tmpv); break;

			// ldi (ri), imm
			case 0x06: tmpv = *PC++; ptr1_write(op, tmpv); break;

			// ld adr, a
			case 0x07: ssp->RAM[op & 0x1ff] = rA; break;

			// ld d, ri
			case 0x09: tmpv = rIJ[(op&3)|((op>>6)&4)]; REG_WRITE((op & 0xf0) >> 4, tmpv); break;

			// ld ri, s
			case 0x0a: rIJ[(op&3)|((op>>6)&4)] = REG_READ((op & 0xf0) >> 4); break;

			// ldi ri, simm
			case 0x0c:
			case 0x0d:
			case 0x0e:
			case 0x0f: rIJ[(op>>8)&7] = op; break;

			// call cond, addr
			case 0x24: {
				int cond = 0;
				COND_CHECK
				if (cond) { int new_PC = *PC++; write_STACK(GET_PC()); write_PC(new_PC); }
				else PC++;
				break;
			}

			// ld d, (a)
			case 0x25: tmpv = ((unsigned short *)svp->iram_rom)[rA]; REG_WRITE((op & 0xf0) >> 4, tmpv); break;

			// bra cond, addr
			case 0x26: {
				int cond = 0;
				COND_CHECK
				if (cond) { int new_PC = *PC++; write_PC(new_PC); }
				else PC++;
				break;
			}

			// mod cond, op
			case 0x48: {
				int cond = 0;
				COND_CHECK
				if (cond) {
					switch (op & 7) {
						case 2: rA32 >>= 1; break; // shr
						case 3: rA32 <<= 1; break; // shl
						case 6: rA32 = -(int)rA32; break; // neg
						case 7: if ((int)rA32 < 0) rA32 = -(int)rA32; break; // abs
						default: elprintf(EL_SVP, "ssp16: unhandled mod %i @ %04x", op&7, GET_PPC_OFFS());
					}
					UPD_ACC_ZN
				}
				break;
			}

			// mpya (rj), (ri), b
			case 0x4b:
				// dunno if this is correct. What about b?
				read_P(); // update P
				ssp->gr[SSP_A].v += ssp->gr[SSP_P].v; // maybe only upper word?
				rX = ptr1_read_(op&3, 0, (op<<1)&0x18); // ri (maybe rj?)
				rY = ptr1_read_((op>>4)&3, 4, (op>>3)&0x18); // rj
				break;

			// mld (rj), (ri), b
			case 0x5b:
				// dunno if this is correct. What about b?
				ssp->gr[SSP_A].v = 0; // maybe only upper word?
				rX = ptr1_read_(op&3, 0, (op<<1)&0x18); // ri (maybe rj?)
				rY = ptr1_read_((op>>4)&3, 4, (op>>3)&0x18); // rj
				break;

			// OP a, s
			case 0x10: tmpv = REG_READ(op & 0x0f); OP_SUBA(tmpv); break;
			case 0x30: tmpv = REG_READ(op & 0x0f); OP_CMPA(tmpv); break;
			case 0x40: tmpv = REG_READ(op & 0x0f); OP_ADDA(tmpv); break;
			case 0x50: tmpv = REG_READ(op & 0x0f); OP_ANDA(tmpv); break;
			case 0x60: tmpv = REG_READ(op & 0x0f); OP_ORA (tmpv); break;
			case 0x70: tmpv = REG_READ(op & 0x0f); OP_EORA(tmpv); break;

			// OP a, (ri)
			case 0x11: tmpv = ptr1_read(op); OP_SUBA(tmpv); break;
			case 0x31: tmpv = ptr1_read(op); OP_CMPA(tmpv); break;
			case 0x41: tmpv = ptr1_read(op); OP_ADDA(tmpv); break;
			case 0x51: tmpv = ptr1_read(op); OP_ANDA(tmpv); break;
			case 0x61: tmpv = ptr1_read(op); OP_ORA (tmpv); break;
			case 0x71: tmpv = ptr1_read(op); OP_EORA(tmpv); break;

			// OP a, adr
			case 0x03: tmpv = ssp->RAM[op & 0x1ff]; OP_LDA (tmpv); break;
			case 0x13: tmpv = ssp->RAM[op & 0x1ff]; OP_SUBA(tmpv); break;
			case 0x33: tmpv = ssp->RAM[op & 0x1ff]; OP_CMPA(tmpv); break;
			case 0x43: tmpv = ssp->RAM[op & 0x1ff]; OP_ADDA(tmpv); break;
			case 0x53: tmpv = ssp->RAM[op & 0x1ff]; OP_ANDA(tmpv); break;
			case 0x63: tmpv = ssp->RAM[op & 0x1ff]; OP_ORA (tmpv); break;
			case 0x73: tmpv = ssp->RAM[op & 0x1ff]; OP_EORA(tmpv); break;

			// OP a, imm
			case 0x14: tmpv = *PC++; OP_SUBA(tmpv); break;
			case 0x34: tmpv = *PC++; OP_CMPA(tmpv); break;
			case 0x44: tmpv = *PC++; OP_ADDA(tmpv); break;
			case 0x54: tmpv = *PC++; OP_ANDA(tmpv); break;
			case 0x64: tmpv = *PC++; OP_ORA (tmpv); break;
			case 0x74: tmpv = *PC++; OP_EORA(tmpv); break;

			// OP a, ((ri))
			case 0x15: tmpv = ptr2_read(op); OP_SUBA(tmpv); break;
			case 0x35: tmpv = ptr2_read(op); OP_CMPA(tmpv); break;
			case 0x45: tmpv = ptr2_read(op); OP_ADDA(tmpv); break;
			case 0x55: tmpv = ptr2_read(op); OP_ANDA(tmpv); break;
			case 0x65: tmpv = ptr2_read(op); OP_ORA (tmpv); break;
			case 0x75: tmpv = ptr2_read(op); OP_EORA(tmpv); break;

			// OP a, ri
			case 0x19: tmpv = rIJ[IJind]; OP_SUBA(tmpv); break;
			case 0x39: tmpv = rIJ[IJind]; OP_CMPA(tmpv); break;
			case 0x49: tmpv = rIJ[IJind]; OP_ADDA(tmpv); break;
			case 0x59: tmpv = rIJ[IJind]; OP_ANDA(tmpv); break;
			case 0x69: tmpv = rIJ[IJind]; OP_ORA (tmpv); break;
			case 0x79: tmpv = rIJ[IJind]; OP_EORA(tmpv); break;

			// OP simm
			case 0x1c: OP_SUBA(op & 0xff); break;
			case 0x3c: OP_CMPA(op & 0xff); break;
			case 0x4c: OP_ADDA(op & 0xff); break;
			// MAME code only does LSB of top word, but this looks wrong to me.
			case 0x5c: OP_ANDA(op & 0xff); break;
			case 0x6c: OP_ORA (op & 0xff); break;
			case 0x7c: OP_EORA(op & 0xff); break;

			default:
				elprintf(EL_ANOMALY|EL_SVP, "ssp16: unhandled op %04x @ %04x", op, GET_PPC_OFFS());
				break;
		}
		g_cycles--;
	}

	read_P(); // update P
	rPC = GET_PC();

	if (ssp->gr[SSP_GR0].v != 0xffff0000)
		elprintf(EL_ANOMALY|EL_SVP, "ssp16: REG 0 corruption! %08x", ssp->gr[SSP_GR0].v);
}

