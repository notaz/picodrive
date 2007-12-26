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
 *       210 - RPL (?)       (e: "loop size", fir16_32.sc)
 *       43  - RB (?)
 *       5   - GP0_0 (ST5?)  Changed before acessing AL (affects banking?).
 *       6   - GP0_1 (ST6?)  Cleared before acessing AL (affects banking?). Set after.
 *       7   - IE (?)        Not used by SVP code (never set, but preserved)?
 *       8   - OP (?)        Not used by SVP code (only cleared)?
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
 * They can be accessed directly, or 2 indirection levels can be used [ (r0), ((r0)) ],
 * which work similar to * and ** operators in C.
 *
 * r0,r1,r2,r4,r5,r6 can be modified [ex: ldi r0, 5].
 * 3 modifiers can be applied (optional):
 *  + : post-increment [ex: ld a, (r0+) ]
 *  - : post-decrement
 *  +!: same as '+' ???
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

#define GET_PC() (PC - (unsigned short *)Pico.rom)
#define GET_PC_OFFS() ((unsigned int)PC - (unsigned int)Pico.rom)
#define SET_PC(d) PC = (unsigned short *)Pico.rom + d

#define REG_READ(r) (((r) <= 4) ? ssp->gr[r].h : read_handlers[r]())
#define REG_WRITE(r,d) { \
	int r1 = r; \
	if (r1 > 4) write_handlers[r1](d); \
	else if (r1 > 0) ssp->gr[r1].h = d; \
}

static ssp1601_t *ssp = NULL;
static unsigned short *PC;
static int g_cycles;

// -----------------------------------------------------
// register i/o handlers

// 0-4, 13
static u32 read_unknown(void)
{
	elprintf(EL_ANOMALY|EL_SVP, "ssp16: unknown read @ %04x", GET_PC_OFFS());
	return 0;
}

static void write_unknown(u32 d)
{
	elprintf(EL_ANOMALY|EL_SVP, "ssp16: unknown write @ %04x", GET_PC_OFFS());
}

// 5
static u32 read_STACK(void)
{
	u32 d = 0;
	if (rSTACK < 6) {
		d = ssp->stack[rSTACK];
		rSTACK++;
	} else
		elprintf(EL_ANOMALY|EL_SVP, "ssp16: stack underflow! (%i) @ %04x", rSTACK, GET_PC_OFFS());
	return d;
}

static void write_STACK(u32 d)
{
	if (rSTACK > 0) {
		rSTACK--;
		ssp->stack[rSTACK] = d;
	} else
		elprintf(EL_ANOMALY|EL_SVP, "ssp16: stack overflow! (%i) @ %04x", rSTACK, GET_PC_OFFS());
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

static u32 pm_io(int reg, int write, u32 d)
{
	if (ssp->emu_status & SSP_PMC_SET) {
		elprintf(EL_SVP, "PM%i (%c) set to %08x @ %04x", reg, write ? 'w' : 'r', rPMC.v, GET_PC_OFFS());
		ssp->pmac_read[write ? reg + 6 : reg] = rPMC.v;
		ssp->emu_status &= ~SSP_PMC_SET;
		return 0;
	}

	if (ssp->pmac_read[reg] != 0) {
		elprintf(EL_SVP, "PM%i %c @ %04x", reg, write ? 'w' : 'r', GET_PC_OFFS());
		// do something depending on mode
		return 0;
	}

	return (u32)-1;
}

// 8
static u32 read_PM0(void)
{
	u32 d = pm_io(0, 0, 0);
	if (d != (u32)-1) return d;
	elprintf(EL_SVP, "PM0 raw r %04x @ %04x", rPM0, GET_PC_OFFS());
	return rPM0;
}

static void write_PM0(u32 d)
{
	u32 r = pm_io(0, 1, d);
	if (r != (u32)-1) return;
	elprintf(EL_SVP, "PM0 raw w %04x @ %04x", d, GET_PC_OFFS());
	rPM0 = d;
}

// 9
static u32 read_PM1(void)
{
	u32 d = pm_io(1, 0, 0);
	if (d != (u32)-1) return d;
	// can be removed?
	elprintf(EL_SVP, "PM1 raw r %04x @ %04x", rPM1, GET_PC_OFFS());
	return rPM0;
}

static void write_PM1(u32 d)
{
	u32 r = pm_io(1, 1, d);
	if (r != (u32)-1) return;
	// can be removed?
	elprintf(EL_SVP, "PM1 raw w %04x @ %04x", d, GET_PC_OFFS());
	rPM0 = d;
}

// 10
static u32 read_PM2(void)
{
	u32 d = pm_io(2, 0, 0);
	if (d != (u32)-1) return d;
	// can be removed?
	elprintf(EL_SVP, "PM2 raw r %04x @ %04x", rPM2, GET_PC_OFFS());
	return rPM0;
}

static void write_PM2(u32 d)
{
	u32 r = pm_io(2, 1, d);
	if (r != (u32)-1) return;
	// can be removed?
	elprintf(EL_SVP, "PM2 raw w %04x @ %04x", d, GET_PC_OFFS());
	rPM0 = d;
}

// 11
static u32 read_XST(void)
{
	// can be removed?
	u32 d = pm_io(3, 0, 0);
	if (d != (u32)-1) return d;

	elprintf(EL_SVP, "XST raw r %04x @ %04x", rXST, GET_PC_OFFS());
	return rPM0;
}

static void write_XST(u32 d)
{
	// can be removed?
	u32 r = pm_io(3, 1, d);
	if (r != (u32)-1) return;

	elprintf(EL_SVP, "XST raw w %04x @ %04x", d, GET_PC_OFFS());
	rPM0 = d;
}

// 12
static u32 read_PM4(void)
{
	u32 d = pm_io(4, 0, 0);
	if (d != (u32)-1) return d;
	// can be removed?
	elprintf(EL_SVP, "PM4 raw r %04x @ %04x", rPM4, GET_PC_OFFS());
	return rPM0;
}

static void write_PM4(u32 d)
{
	u32 r = pm_io(4, 1, d);
	if (r != (u32)-1) return;
	// can be removed?
	elprintf(EL_SVP, "PM4 raw w %04x @ %04x", d, GET_PC_OFFS());
	rPM0 = d;
}

// 14
static u32 read_PMC(void)
{
	if (ssp->emu_status & SSP_PMC_HAVE_ADDR) {
		if (ssp->emu_status & SSP_PMC_SET)
			elprintf(EL_ANOMALY|EL_SVP, "prev PMC not used @ %04x", GET_PC_OFFS());
		ssp->emu_status |= SSP_PMC_SET;
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
			elprintf(EL_ANOMALY|EL_SVP, "prev PMC not used @ %04x", GET_PC_OFFS());
		ssp->emu_status |= SSP_PMC_SET;
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
	write_unknown,	// 4 ST
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

void ssp1601_reset(ssp1601_t *l_ssp)
{
	ssp = l_ssp;
	ssp->emu_status = 0;
	ssp->gr[SSP_GR0].v = 0xffff0000;
	rPC = 0x400;
	rSTACK = 6; // ? using descending stack
}


void ssp1601_run(int cycles)
{
	int op;

	SET_PC(rPC);
	g_cycles = cycles;

	while (g_cycles > 0)
	{
		op = *PC;
		switch (op >> 9)
		{
			// ld d, s
			case 0:
				if (op == 0) break; // nop
				if (op == ((SSP_A<<4)|SSP_P)) { // A <- P
					// not sure. MAME claims that only hi word is transfered.
					read_P(); // update P
					ssp->gr[SSP_A].v = ssp->gr[SSP_P].v;
					break;
				}
				{
					u32 d = REG_READ(op & 0x0f);
					REG_WRITE((op & 0xf0) >> 4, d);
				}
				// flags?
				break;

			default:
				elprintf(EL_ANOMALY|EL_SVP, "ssp16: unhandled op %04x @ %04x", op, GET_PC_OFFS());
				break;
		}
		g_cycles--;
		PC++;
	}

	read_P(); // update P
	rPC = GET_PC();

	if (ssp->gr[SSP_GR0].v != 0xffff0000)
		elprintf(EL_ANOMALY|EL_SVP, "ssp16: REG 0 corruption! %08x", ssp->gr[SSP_GR0].v);
}

