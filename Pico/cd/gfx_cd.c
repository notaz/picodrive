// TODO...

// #include <string.h>
#include "../PicoInt.h"

#define rot_comp Pico_mcd->rot_comp

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


#if 1*0
typedef struct
{
	unsigned int Reg_58;	// Stamp_Size
	unsigned int Reg_5A;
	unsigned int Reg_5C;
	unsigned int Reg_5E;
	unsigned int Reg_60;
	unsigned int Reg_62;
	unsigned int Reg_64;	// V_Dot
	unsigned int Reg_66;

	unsigned int Stamp_Map_Adr;
	unsigned int Buffer_Adr;
	unsigned int Vector_Adr;
	unsigned int Jmp_Adr;
	unsigned int Float_Part;
	unsigned int Draw_Speed;

	unsigned int XS;
	unsigned int YS;
	unsigned int DXS;
	unsigned int DYS;
	unsigned int XD;
	unsigned int YD;
	unsigned int XD_Mul;
	unsigned int H_Dot;
} Rot_Comp;
#endif

static void gfx_cd_start(void)
{
	int upd_len;

	dprintf("gfx_cd_start()");

	upd_len = (rot_comp.Reg_62 >> 3) & 0x3f;
	upd_len = Table_Rot_Time[upd_len];

	rot_comp.Draw_Speed = rot_comp.Float_Part = upd_len;

	rot_comp.Reg_58 |= 0x8000;	// Stamp_Size,  we start a new GFX operation

	gfx_cd_update();
}


static void gfx_completed(void)
{
	rot_comp.Reg_58 &= 0x7fff;	// Stamp_Size
	rot_comp.Reg_64  = 0;
	if (Pico_mcd->s68k_regs[0x33] & (1<<1))
	{
		dprintf("gfx_cd irq 1");
		SekInterruptS68k(1);
	}
}


//static void gfx_do(void)
//{
//}


void gfx_cd_update(void)
{
	unsigned char *V_Dot = (unsigned char *) &rot_comp.Reg_64;
	int jobs;

	dprintf("gfx_cd_update, Reg_64 = %04x", rot_comp.Reg_64);

	if (!*V_Dot)
	{
		// ...
		gfx_completed();
		return;
	}

	jobs = rot_comp.Float_Part >> 16;

	if (!jobs)
	{
		rot_comp.Float_Part += rot_comp.Draw_Speed;
		return;
	}

	rot_comp.Float_Part &= 0xffff;
	rot_comp.Float_Part += rot_comp.Draw_Speed;

	while (jobs--)
	{
		// jmp [Jmp_Adr]:
		(*V_Dot)--;	// dec byte [V_Dot]

		if (!*V_Dot)
		{
			// GFX_Completed:
			gfx_completed();
			return;
		}
	}
}


unsigned int gfx_cd_read(unsigned int a)
{
	unsigned int d = 0;

	switch (a) {
		case 0x58: d = rot_comp.Reg_58; break;
		case 0x5A: d = rot_comp.Reg_5A; break;
		case 0x5C: d = rot_comp.Reg_5C; break;
		case 0x5E: d = rot_comp.Reg_5E; break;
		case 0x60: d = rot_comp.Reg_60; break;
		case 0x62: d = rot_comp.Reg_62; break;
		case 0x64: d = rot_comp.Reg_64; break;
		case 0x66: break;
		default: dprintf("gfx_cd_read: unexpected address: %02x", a); break;
	}

	dprintf("gfx_cd_read(%02x) = %04x", a, d);

	return 0;
}

void gfx_cd_write(unsigned int a, unsigned int d)
{
	dprintf("gfx_cd_write(%x, %04x)", a, d);

	switch (a) {
		case 0x58: // .Reg_Stamp_Size
			rot_comp.Reg_58 = d & 7;
			return;

		case 0x5A: // .Reg_Stamp_Adr
			rot_comp.Reg_5A = d & 0xffe0;
			return;

		case 0x5C: // .Reg_IM_VCell_Size
			rot_comp.Reg_5C = d & 0x1f;
			return;

		case 0x5E: // .Reg_IM_Adr
			rot_comp.Reg_5E = d & 0xFFF8;
			return;

		case 0x60: // .Reg_IM_Offset
			rot_comp.Reg_60 = d & 0x3f;
			return;

		case 0x62: // .Reg_IM_HDot_Size
			rot_comp.Reg_62 = d & 0x1ff;
			return;

		case 0x64: // .Reg_IM_VDot_Size
			rot_comp.Reg_64 = d & 0xff;	// V_Dot, must be 32bit?
			return;

		case 0x66: // .Reg_Vector_Adr
			rot_comp.Reg_66 = d & 0xfffe;
			if (Pico_mcd->s68k_regs[3]&4) return; // can't do tanformations in 1M mode
			gfx_cd_start();
			return;

		default: dprintf("gfx_cd_write: unexpected address: %02x", a); return;
	}
}


void gfx_cd_reset(void)
{
	memset(&rot_comp.Reg_58, 0, 0/*sizeof(Pico_mcd->rot_comp)*/);
}

