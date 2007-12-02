// This is a direct rewrite of gfx_cd.asm (x86 asm to C).
// You can even find some x86 register names :)
// Original code (c) 2002 by Stéphane Dallongeville

// (c) Copyright 2007, Grazvydas "notaz" Ignotas


#include "../PicoInt.h"

#define _rot_comp Pico_mcd->rot_comp

static const int Table_Rot_Time[] =
{
	0x00054000, 0x00048000, 0x00040000, 0x00036000,          //; 008-032               ; briefing - sprite
	0x0002E000, 0x00028000, 0x00024000, 0x00022000,          //; 036-064               ; arbre souvent
	0x00021000, 0x00020000, 0x0001E000, 0x0001B800,          //; 068-096               ; map thunderstrike
	0x00019800, 0x00017A00, 0x00015C00, 0x00013E00,          //; 100-128               ; logo défoncé

	0x00012000, 0x00011800, 0x00011000, 0x00010800,          //; 132-160               ; briefing - map
	0x00010000, 0x0000F800, 0x0000F000, 0x0000E800,          //; 164-192
	0x0000E000, 0x0000D800, 0x0000D000, 0x0000C800,          //; 196-224
	0x0000C000, 0x0000B800, 0x0000B000, 0x0000A800,          //; 228-256               ; batman visage

	0x0000A000, 0x00009F00, 0x00009E00, 0x00009D00,          //; 260-288
	0x00009C00, 0x00009B00, 0x00009A00, 0x00009900,          //; 292-320
	0x00009800, 0x00009700, 0x00009600, 0x00009500,          //; 324-352
	0x00009400, 0x00009300, 0x00009200, 0x00009100,          //; 356-384

	0x00009000, 0x00008F00, 0x00008E00, 0x00008D00,          //; 388-416
	0x00008C00, 0x00008B00, 0x00008A00, 0x00008900,          //; 420-448
	0x00008800, 0x00008700, 0x00008600, 0x00008500,          //; 452-476
	0x00008400, 0x00008300, 0x00008200, 0x00008100,          //; 480-512
};


static void gfx_cd_start(void)
{
	int upd_len;

	// _rot_comp.XD_Mul = ((_rot_comp.Reg_5C & 0x1f) + 1) * 4; // unused
	_rot_comp.Function = (_rot_comp.Reg_58 & 7) | (Pico_mcd->s68k_regs[3] & 0x18);	// Jmp_Adr
	// _rot_comp.Buffer_Adr = (_rot_comp.Reg_5E & 0xfff8) << 2; // unused?
	_rot_comp.YD = (_rot_comp.Reg_60 >> 3) & 7;
	_rot_comp.Vector_Adr = (_rot_comp.Reg_66 & 0xfffe) << 2;

	upd_len = (_rot_comp.Reg_62 >> 3) & 0x3f;
	upd_len = Table_Rot_Time[upd_len];
	_rot_comp.Draw_Speed = _rot_comp.Float_Part = upd_len;

	_rot_comp.Reg_58 |= 0x8000;	// Stamp_Size,  we start a new GFX operation

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

	dprintf("gfx_cd_start, stamp_map_addr=%06x", _rot_comp.Stamp_Map_Adr);

	gfx_cd_update();
}


static void gfx_completed(void)
{
	_rot_comp.Reg_58 &= 0x7fff;	// Stamp_Size
	_rot_comp.Reg_64  = 0;
	if (Pico_mcd->s68k_regs[0x33] & (1<<1))
	{
		elprintf(EL_INTS, "gfx_cd irq 1");
		SekInterruptS68k(1);
	}
}


static void gfx_do(unsigned int func, unsigned short *stamp_base, unsigned int H_Dot)
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


PICO_INTERNAL void gfx_cd_update(void)
{
	int V_Dot = _rot_comp.Reg_64 & 0xff;
	int jobs;

	dprintf("gfx_cd_update, Reg_64 = %04x", _rot_comp.Reg_64);

	if (!V_Dot)
	{
		gfx_completed();
		return;
	}

	jobs = _rot_comp.Float_Part >> 16;

	if (!jobs)
	{
		_rot_comp.Float_Part += _rot_comp.Draw_Speed;
		return;
	}

	_rot_comp.Float_Part &= 0xffff;
	_rot_comp.Float_Part += _rot_comp.Draw_Speed;

	if (PicoOpt & 0x1000)			// scale/rot enabled
	{
		unsigned int func = _rot_comp.Function;
		unsigned int H_Dot = _rot_comp.Reg_62 & 0x1ff;
		unsigned short *stamp_base = (unsigned short *) (Pico_mcd->word_ram2M + _rot_comp.Stamp_Map_Adr);

		while (jobs--)
		{
			gfx_do(func, stamp_base, H_Dot);	// jmp [Jmp_Adr]:

			V_Dot--;				// dec byte [V_Dot]
			if (V_Dot == 0)
			{
				// GFX_Completed:
				gfx_completed();
				return;
			}
		}
	}
	else
	{
		if (jobs >= V_Dot)
		{
			gfx_completed();
			return;
		}
		V_Dot -= jobs;
	}

	_rot_comp.Reg_64 = V_Dot;
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

PICO_INTERNAL_ASM void gfx_cd_write16(unsigned int a, unsigned int d)
{
	dprintf("gfx_cd_write16(%x, %04x)", a, d);

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
      rendstatus|=0x10;
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

