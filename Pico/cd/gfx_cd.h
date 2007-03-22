#ifndef _GFX_CD_H
#define _GFX_CD_H


typedef struct
{
	unsigned int Reg_58;		// Stamp_Size
	unsigned int Reg_5A;
	unsigned int Reg_5C;
	unsigned int Reg_5E;
	unsigned int Reg_60;
	unsigned int Reg_62;
	unsigned int Reg_64;		// V_Dot
	unsigned int Reg_66;

	unsigned int Stamp_Map_Adr;
	unsigned int Vector_Adr;
	unsigned int Function;		// Jmp_Adr;
	unsigned int Float_Part;
	unsigned int Draw_Speed;
	unsigned int YD;

	int pad[10];
} Rot_Comp;


void gfx_cd_update(void);

unsigned int gfx_cd_read(unsigned int a);
void gfx_cd_write16(unsigned int a, unsigned int d);

void gfx_cd_reset(void);

void DmaSlowCell(unsigned int source, unsigned int a, int len, unsigned char inc);

#endif // _GFX_CD_H

