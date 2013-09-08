// This is a direct rewrite of gfx_cd.asm (x86 asm to C).
// You can even find some x86 register names :)
// Original code (c) 2002 by Stéphane Dallongeville

// (c) Copyright 2007, Grazvydas "notaz" Ignotas


#include "../pico_int.h"

#undef dprintf
#define dprintf(...)

#define UPDATE_CYCLES 20000

#define _rot_comp Pico_mcd->rot_comp

static void gfx_do_line(unsigned int func, unsigned short *stamp_base,
	unsigned int H_Dot);

static void gfx_cd_start(void)
{
	int w, h, cycles;
	int y_step;

	w = _rot_comp.Reg_62;
	h = _rot_comp.Reg_64;
	if (w == 0 || h == 0) {
		elprintf(EL_CD|EL_ANOMALY, "gfx_cd_start with %ux%u", w, h);
		_rot_comp.Reg_64 = 0;
		// irq?
		return;
	}

	// _rot_comp.XD_Mul = ((_rot_comp.Reg_5C & 0x1f) + 1) * 4; // unused
	_rot_comp.Function = (_rot_comp.Reg_58 & 7) | (Pico_mcd->s68k_regs[3] & 0x18);	// Jmp_Adr
	// _rot_comp.Buffer_Adr = (_rot_comp.Reg_5E & 0xfff8) << 2; // unused?
	_rot_comp.YD = (_rot_comp.Reg_60 >> 3) & 7;
	_rot_comp.Vector_Adr = (_rot_comp.Reg_66 & 0xfffe) << 2;

	switch (_rot_comp.Reg_58 & 6)	// Scr_16?
	{
		case 0:	// ?
			_rot_comp.Stamp_Map_Adr = (_rot_comp.Reg_5A & 0xff80) << 2;
			break;
		case 2: // .Dot_32
			_rot_comp.Stamp_Map_Adr = (_rot_comp.Reg_5A & 0xffe0) << 2;
			break;
		case 4: // .Scr_16
			_rot_comp.Stamp_Map_Adr = 0x20000;
			break;
		case 6: // .Scr_16_Dot_32
			_rot_comp.Stamp_Map_Adr = (_rot_comp.Reg_5A & 0xe000) << 2;
			break;
	}

	_rot_comp.Reg_58 |= 0x8000;	// Stamp_Size,  we start a new GFX operation

	cycles = 5 * w * h;
	if (cycles > UPDATE_CYCLES)
		y_step = (UPDATE_CYCLES + 5 * w - 1) / (5 * w);
	else
		y_step = h;

	_rot_comp.y_step = y_step;
	pcd_event_schedule_s68k(PCD_EVENT_GFX, 5 * w * y_step);
}

void gfx_cd_update(unsigned int cycles)
{
	int w = _rot_comp.Reg_62;
	int h, next;

	if (!(Pico_mcd->rot_comp.Reg_58 & 0x8000))
		return;

	h = _rot_comp.Reg_64;
	_rot_comp.Reg_64 -= _rot_comp.y_step;

	if ((int)_rot_comp.Reg_64 <= 0) {
		Pico_mcd->rot_comp.Reg_58 &= 0x7fff;
		Pico_mcd->rot_comp.Reg_64  = 0;
		if (Pico_mcd->s68k_regs[0x33] & PCDS_IEN1) {
			elprintf(EL_INTS  |EL_CD, "s68k: gfx_cd irq 1");
			SekInterruptS68k(1);
		}
	}
	else {
		next = _rot_comp.Reg_64;
		if (next > _rot_comp.y_step)
			next = _rot_comp.y_step;

		pcd_event_schedule(cycles, PCD_EVENT_GFX, 5 * w * next);
		h = _rot_comp.y_step;
	}

	if (PicoOpt & POPT_EN_MCD_GFX)
	{
		unsigned int func = _rot_comp.Function;
		unsigned short *stamp_base = (unsigned short *)
			(Pico_mcd->word_ram2M + _rot_comp.Stamp_Map_Adr);

		while (h--)
			gfx_do_line(func, stamp_base, w);
	}
}

PICO_INTERNAL_ASM unsigned int gfx_cd_read(unsigned int a)
{
	unsigned int d = 0;

	switch (a) {
		case 0x58: d = _rot_comp.Reg_58; break;
		case 0x5A: d = _rot_comp.Reg_5A; break;
		case 0x5C: d = _rot_comp.Reg_5C; break;
		case 0x5E: d = _rot_comp.Reg_5E; break;
		case 0x60: d = _rot_comp.Reg_60; break;
		case 0x62: d = _rot_comp.Reg_62; break;
		case 0x64: d = _rot_comp.Reg_64; break;
		case 0x66: break;
		default: dprintf("gfx_cd_read FIXME: unexpected address: %02x", a); break;
	}

	dprintf("gfx_cd_read(%02x) = %04x", a, d);

	return d;

}

static void gfx_do_line(unsigned int func, unsigned short *stamp_base,
	unsigned int H_Dot)
{
	unsigned int eax, ebx, ecx, edx, esi, edi, pixel;
	unsigned int XD, Buffer_Adr;
	int DYXS;

	XD = _rot_comp.Reg_60 & 7;
	Buffer_Adr = ((_rot_comp.Reg_5E & 0xfff8) + _rot_comp.YD) << 2;
	ecx = *(unsigned int *)(Pico_mcd->word_ram2M + _rot_comp.Vector_Adr);
	edx = ecx >> 16;
	ecx = (ecx & 0xffff) << 8;
	edx <<= 8;
	DYXS = *(int *)(Pico_mcd->word_ram2M + _rot_comp.Vector_Adr + 4);
	_rot_comp.Vector_Adr += 8;

	// MAKE_IMAGE_LINE
	while (H_Dot)
	{
		// MAKE_IMAGE_PIXEL
		if (!(func & 1))	// NOT TILED
		{
			int mask = (func & 4) ? 0x00800000 : 0x00f80000;
			if ((ecx | edx) & mask)
			{
				if (func & 0x18) goto Next_Pixel;
				pixel = 0;
				goto Pixel_Out;
			}
		}

		if (func & 2)		// mode 32x32 dot
		{
			if (func & 4)	// 16x16 screen
			{
				ebx = ((ecx >> (11+5)) & 0x007f) |
				      ((edx >> (11-2)) & 0x3f80);
			}
			else		// 1x1 screen
			{
				ebx = ((ecx >> (11+5)) & 0x07) |
				      ((edx >> (11+2)) & 0x38);
			}
		}
		else			// mode 16x16 dot
		{
			if (func & 4)	// 16x16 screen
			{
				ebx = ((ecx >> (11+4)) & 0x00ff) |
				      ((edx >> (11-4)) & 0xff00);
			}
			else		// 1x1 screen
			{
				ebx = ((ecx >> (11+4)) & 0x0f) |
				      ((edx >> (11+0)) & 0xf0);
			}
		}

		edi = stamp_base[ebx];
		esi = (edi & 0x7ff) << 7;
		if (!esi) { pixel = 0; goto Pixel_Out; }
		edi >>= (11+1);
		edi &= (0x1c>>1);
		eax = ecx;
		ebx = edx;
		if (func & 2) edi |= 1;	// 32 dots?
		switch (edi)
		{
			case 0x00:	// No_Flip_0, 16x16 dots
				ebx = (ebx >> 9) & 0x3c;
				ebx += esi;
				edi = (eax & 0x3800) ^ 0x1000;		// bswap
				eax = ((eax >> 8) & 0x40) + ebx;
				break;
			case 0x01:	// No_Flip_0, 32x32 dots
				ebx = (ebx >> 9) & 0x7c;
				ebx += esi;
				edi = (eax & 0x3800) ^ 0x1000;		// bswap
				eax = ((eax >> 7) & 0x180) + ebx;
				break;
			case 0x02:	// No_Flip_90, 16x16 dots
				eax = (eax >> 9) & 0x3c;
				eax += esi;
				edi = (ebx & 0x3800) ^ 0x2800;		// bswap
				eax += ((ebx >> 8) & 0x40) ^ 0x40;
				break;
			case 0x03:	// No_Flip_90, 32x32 dots
				eax = (eax >> 9) & 0x7c;
				eax += esi;
				edi = (ebx & 0x3800) ^ 0x2800;		// bswap
				eax += ((ebx >> 7) & 0x180) ^ 0x180;
				break;
			case 0x04:	// No_Flip_180, 16x16 dots
				ebx = ((ebx >> 9) & 0x3c) ^ 0x3c;
				ebx += esi;
				edi = (eax & 0x3800) ^ 0x2800;		// bswap and flip
				eax = (((eax >> 8) & 0x40) ^ 0x40) + ebx;
				break;
			case 0x05:	// No_Flip_180, 32x32 dots
				ebx = ((ebx >> 9) & 0x7c) ^ 0x7c;
				ebx += esi;
				edi = (eax & 0x3800) ^ 0x2800;		// bswap and flip
				eax = (((eax >> 7) & 0x180) ^ 0x180) + ebx;
				break;
			case 0x06:	// No_Flip_270, 16x16 dots
				eax = ((eax >> 9) & 0x3c) ^ 0x3c;
				eax += esi;
				edi = (ebx & 0x3800) ^ 0x1000;		// bswap
				eax += (ebx >> 8) & 0x40;
				break;
			case 0x07:	// No_Flip_270, 32x32 dots
				eax = ((eax >> 9) & 0x7c) ^ 0x7c;
				eax += esi;
				edi = (ebx & 0x3800) ^ 0x1000;		// bswap
				eax += (ebx >> 7) & 0x180;
				break;
			case 0x08:	// Flip_0, 16x16 dots
				ebx = (ebx >> 9) & 0x3c;
				ebx += esi;
				edi = (eax & 0x3800) ^ 0x2800;		// bswap, flip
				eax = (((eax >> 8) & 0x40) ^ 0x40) + ebx;
				break;
			case 0x09:	// Flip_0, 32x32 dots
				ebx = (ebx >> 9) & 0x7c;
				ebx += esi;
				edi = (eax & 0x3800) ^ 0x2800;		// bswap, flip
				eax = (((eax >> 7) & 0x180) ^ 0x180) + ebx;
				break;
			case 0x0a:	// Flip_90, 16x16 dots
				eax = ((eax >> 9) & 0x3c) ^ 0x3c;
				eax += esi;
				edi = (ebx & 0x3800) ^ 0x2800;		// bswap, flip
				eax += ((ebx >> 8) & 0x40) ^ 0x40;
				break;
			case 0x0b:	// Flip_90, 32x32 dots
				eax = ((eax >> 9) & 0x7c) ^ 0x7c;
				eax += esi;
				edi = (ebx & 0x3800) ^ 0x2800;		// bswap, flip
				eax += ((ebx >> 7) & 0x180) ^ 0x180;
				break;
			case 0x0c:	// Flip_180, 16x16 dots
				ebx = ((ebx >> 9) & 0x3c) ^ 0x3c;
				ebx += esi;
				edi = (eax & 0x3800) ^ 0x1000;		// bswap
				eax = ((eax >> 8) & 0x40) + ebx;
				break;
			case 0x0d:	// Flip_180, 32x32 dots
				ebx = ((ebx >> 9) & 0x7c) ^ 0x7c;
				ebx += esi;
				edi = (eax & 0x3800) ^ 0x1000;		// bswap
				eax = ((eax >> 7) & 0x180) + ebx;
				break;
			case 0x0e:	// Flip_270, 16x16 dots
				eax = (eax >> 9) & 0x3c;
				eax += esi;
				edi = (ebx & 0x3800) ^ 0x1000;		// bswap, flip
				eax += (ebx >> 8) & 0x40;
				break;
			case 0x0f:	// Flip_270, 32x32 dots
				eax = (eax >> 9) & 0x7c;
				eax += esi;
				edi = (ebx & 0x3800) ^ 0x1000;		// bswap, flip
				eax += (ebx >> 7) & 0x180;
				break;
		}

		pixel = *(Pico_mcd->word_ram2M + (edi >> 12) + eax);
		if (!(edi & 0x800)) pixel >>= 4;
		else pixel &= 0x0f;

Pixel_Out:
		if (!pixel && (func & 0x18)) goto Next_Pixel;
		esi = Buffer_Adr + ((XD>>1)^1);				// pixel addr
		eax = *(Pico_mcd->word_ram2M + esi);			// old pixel
		if (XD & 1)
		{
			if ((eax & 0x0f) && (func & 0x18) == 0x08) goto Next_Pixel; // underwrite
			*(Pico_mcd->word_ram2M + esi) = pixel | (eax & 0xf0);
		}
		else
		{
			if ((eax & 0xf0) && (func & 0x18) == 0x08) goto Next_Pixel; // underwrite
			*(Pico_mcd->word_ram2M + esi) = (pixel << 4) | (eax & 0xf);
		}


Next_Pixel:
		ecx += (DYXS << 16) >> 16;	// _rot_comp.DXS;
		edx +=  DYXS >> 16;		// _rot_comp.DYS;
		XD++;
		if (XD >= 8)
		{
			Buffer_Adr += ((_rot_comp.Reg_5C & 0x1f) + 1) << 5;
			XD = 0;
		}
		H_Dot--;
	}
	// end while


// nothing_to_draw:
	_rot_comp.YD++;
	// _rot_comp.V_Dot--; // will be done by caller
}


PICO_INTERNAL_ASM void gfx_cd_write16(unsigned int a, unsigned int d)
{
	dprintf("gfx_cd_write16(%x, %04x)", a, d);

	if (_rot_comp.Reg_58 & 0x8000)
		elprintf(EL_CD|EL_ANOMALY, "cd: busy gfx reg write %02x %04x", a, d);

	switch (a) {
		case 0x58: // .Reg_Stamp_Size
			_rot_comp.Reg_58 = d & 7;
			return;

		case 0x5A: // .Reg_Stamp_Adr
			_rot_comp.Reg_5A = d & 0xffe0;
			return;

		case 0x5C: // .Reg_IM_VCell_Size
			_rot_comp.Reg_5C = d & 0x1f;
			return;

		case 0x5E: // .Reg_IM_Adr
			_rot_comp.Reg_5E = d & 0xFFF8;
			return;

		case 0x60: // .Reg_IM_Offset
			_rot_comp.Reg_60 = d & 0x3f;
			return;

		case 0x62: // .Reg_IM_HDot_Size
			_rot_comp.Reg_62 = d & 0x1ff;
			return;

		case 0x64: // .Reg_IM_VDot_Size
			_rot_comp.Reg_64 = d & 0xff;	// V_Dot, must be 32bit?
			return;

		case 0x66: // .Reg_Vector_Adr
			_rot_comp.Reg_66 = d & 0xfffe;
			if (Pico_mcd->s68k_regs[3]&4) return; // can't do tanformations in 1M mode
			gfx_cd_start();
			return;

		default: dprintf("gfx_cd_write16 FIXME: unexpected address: %02x", a); return;
	}
}


PICO_INTERNAL void gfx_cd_reset(void)
{
	memset(&_rot_comp.Reg_58, 0, sizeof(_rot_comp));
}


// --------------------------------

#include "cell_map.c"

#ifndef UTYPES_DEFINED
typedef unsigned short u16;
#endif

// check: Heart of the alien, jaguar xj 220
PICO_INTERNAL void DmaSlowCell(unsigned int source, unsigned int a, int len, unsigned char inc)
{
  unsigned char *base;
  unsigned int asrc, a2;
  u16 *r;

  base = Pico_mcd->word_ram1M[Pico_mcd->s68k_regs[3]&1];

  switch (Pico.video.type)
  {
    case 1: // vram
      r = Pico.vram;
      for(; len; len--)
      {
        asrc = cell_map(source >> 2) << 2;
        asrc |= source & 2;
        // if(a&1) d=(d<<8)|(d>>8); // ??
        r[a>>1] = *(u16 *)(base + asrc);
	source += 2;
        // AutoIncrement
        a=(u16)(a+inc);
      }
      rendstatus |= PDRAW_SPRITES_MOVED;
      break;

    case 3: // cram
      Pico.m.dirtyPal = 1;
      r = Pico.cram;
      for(a2=a&0x7f; len; len--)
      {
        asrc = cell_map(source >> 2) << 2;
        asrc |= source & 2;
        r[a2>>1] = *(u16 *)(base + asrc);
	source += 2;
        // AutoIncrement
        a2+=inc;
        // good dest?
        if(a2 >= 0x80) break;
      }
      a=(a&0xff00)|a2;
      break;

    case 5: // vsram[a&0x003f]=d;
      r = Pico.vsram;
      for(a2=a&0x7f; len; len--)
      {
        asrc = cell_map(source >> 2) << 2;
        asrc |= source & 2;
        r[a2>>1] = *(u16 *)(base + asrc);
	source += 2;
        // AutoIncrement
        a2+=inc;
        // good dest?
        if(a2 >= 0x80) break;
      }
      a=(a&0xff00)|a2;
      break;
  }
  // remember addr
  Pico.video.addr=(u16)a;
}

