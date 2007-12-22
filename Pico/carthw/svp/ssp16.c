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
 */

#include "../../PicoInt.h"

#define rX     ssp->gr[SSP_X].l
#define rY     ssp->gr[SSP_Y].l
#define rA     ssp->gr[SSP_A]		// 4
#define rST    ssp->gr[SSP_ST].l
#define rSTACK ssp->gr[SSP_STACK].l
#define rPC    ssp->gr[SSP_PC].l
#define rP     ssp->gr[SSP_P]		// 8
#define rPM0   ssp->gr[SSP_PM0].l
#define rPM1   ssp->gr[SSP_PM1].l
#define rPM2   ssp->gr[SSP_PM2].l
#define rXST   ssp->gr[SSP_XST].l	// 12
#define rPM4   ssp->gr[SSP_PM4].l	// 14
#define rPMC   ssp->gr[SSP_PMC].l
#define rAL    ssp->gr[SSP_A].l

#define GET_PC() (PC - (unsigned short *)Pico.rom)
#define SET_PC() PC = (unsigned short *)Pico.rom + rPC

void ssp1601_reset(ssp1601_t *ssp)
{
	ssp->emu_status = 0;
	ssp->gr[SSP_GR0].v = 0xffff;
	rPC = 0x400;
	rSTACK = 5; // ?
}


void ssp1601_run(ssp1601_t *ssp, int cycles)
{
	unsigned short *PC;
	int op;

	SET_PC();

	while (cycles > 0)
	{
		op = *PC;
		switch (op >> 9)
		{
			// ld d, s
			case 0:
			{
				int s, d, opdata = 0;
				if (op == 0) break; // nop
				s =  op & 0x0f;
				d = (op & 0xf0) >> 4;
				if (s == SSP_A || s == SSP_P) opdata |= 1; // src is 32bit
				if (d == SSP_A || d == SSP_P) opdata |= 2; // dst is 32bit
				if (s == SSP_STACK) opdata |= 4; // src is stack
				if (d == SSP_STACK) opdata |= 8; // dst is stack
				switch (opdata)
				{
					case 0x0: ssp->gr[d].l = ssp->gr[s].l; break; // 16 <- 16
					case 0x1: ssp->gr[d].l = ssp->gr[s].h; break; // 16 <- 32
					case 0x2: ssp->gr[d].h = ssp->gr[s].l; break; // 32 <- 16
						  // TODO: MAME claims that only hi word is transfered. Go figure.
					case 0x3: ssp->gr[d].v = ssp->gr[s].v; break; // 32 <- 32
					case 0x4: ; // TODO
				}
				if (d == SSP_PC)
				{
					SET_PC();
					cycles--;
				}
				break;
			}

			default:
			elprintf(0xffff, "ssp: unhandled op %04x @ %04x", op, GET_PC()<<1);
			break;
		}
		cycles--;
		PC++;
	}

	rPC = GET_PC();
}

