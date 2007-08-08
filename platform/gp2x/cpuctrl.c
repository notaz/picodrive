/*  cpuctrl for GP2X
    Copyright (C) 2005  Hermes/PS2Reality
	the gamma-routine was provided by theoddbot
	parts (c) Rlyehs Work & (C) 2006 god_at_hell

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/


#include <stdio.h>
#include <math.h>
#include "cpuctrl.h"


/* system registers */
static struct
{
	unsigned short SYSCLKENREG,SYSCSETREG,UPLLVSETREG,FPLLVSETREG,
		DUALINT920,DUALINT940,DUALCTRL940,MEMTIMEX0,MEMTIMEX1,DISPCSETREG,
		DPC_HS_WIDTH,DPC_HS_STR,DPC_HS_END,DPC_VS_END,DPC_DE;
}
system_reg;

static volatile unsigned short *MEM_REG;

#define SYS_CLK_FREQ 7372800

// Fout = (m * Fin) / (p * 2^s)
// m = MDIV+8, p = PDIV+2, s = SDIV

// m = (Fout * p * 2^s) / Fin

void cpuctrl_init(void)
{
	extern volatile unsigned short *gp2x_memregs; /* from minimal library rlyeh */
	MEM_REG=&gp2x_memregs[0];
	system_reg.DISPCSETREG=MEM_REG[0x924>>1];
	system_reg.UPLLVSETREG=MEM_REG[0x916>>1];
	system_reg.FPLLVSETREG=MEM_REG[0x912>>1];
	system_reg.SYSCSETREG=MEM_REG[0x91c>>1];
	system_reg.SYSCLKENREG=MEM_REG[0x904>>1];
	system_reg.DUALINT920=MEM_REG[0x3B40>>1];
	system_reg.DUALINT940=MEM_REG[0x3B42>>1];
	system_reg.DUALCTRL940=MEM_REG[0x3B48>>1];
	system_reg.MEMTIMEX0=MEM_REG[0x3802>>1];
	system_reg.MEMTIMEX1=MEM_REG[0x3804>>1];
	system_reg.DPC_HS_WIDTH=MEM_REG[0x281A>>1];
	system_reg.DPC_HS_STR=MEM_REG[0x281C>>1];
	system_reg.DPC_HS_END=MEM_REG[0x281E>>1];
	system_reg.DPC_VS_END=MEM_REG[0x2822>>1];
	system_reg.DPC_DE=MEM_REG[0x2826>>1];
}


void cpuctrl_deinit(void)
{
	MEM_REG[0x910>>1]=system_reg.FPLLVSETREG;
	MEM_REG[0x91c>>1]=system_reg.SYSCSETREG;
	MEM_REG[0x3B40>>1]=system_reg.DUALINT920;
	MEM_REG[0x3B42>>1]=system_reg.DUALINT940;
	MEM_REG[0x3B48>>1]=system_reg.DUALCTRL940;
	MEM_REG[0x904>>1]=system_reg.SYSCLKENREG;
	MEM_REG[0x3802>>1]=system_reg.MEMTIMEX0;
	MEM_REG[0x3804>>1]=system_reg.MEMTIMEX1 /*| 0x9000*/;
	unset_LCD_custom_rate();
}


void set_display_clock_div(unsigned div)
{
	div=((div & 63) | 64)<<8;
	MEM_REG[0x924>>1]=(MEM_REG[0x924>>1] & ~(255<<8)) | div;
}


void set_FCLK(unsigned MHZ)
{
	unsigned v;
	unsigned mdiv,pdiv=3,scale=0;
	MHZ*=1000000;
	mdiv=(MHZ*pdiv)/SYS_CLK_FREQ;
	mdiv=((mdiv-8)<<8) & 0xff00;
	pdiv=((pdiv-2)<<2) & 0xfc;
	scale&=3;
	v=mdiv | pdiv | scale;
	MEM_REG[0x910>>1]=v;
}


void set_920_Div(unsigned short div)
{
	unsigned short v;
	v = MEM_REG[0x91c>>1] & (~0x3);
	MEM_REG[0x91c>>1] = (div & 0x7) | v;
}


void set_DCLK_Div( unsigned short div )
{
	unsigned short v;
	v = (unsigned short)( MEM_REG[0x91c>>1] & (~(0x7 << 6)) );
	MEM_REG[0x91c>>1] = ((div & 0x7) << 6) | v;
}

/*
void Disable_940(void)
{
	MEM_REG[0x3B42>>1];
	MEM_REG[0x3B42>>1]=0;
	MEM_REG[0x3B46>>1]=0xffff;
	MEM_REG[0x3B48>>1]|= (1 << 7);
	MEM_REG[0x904>>1]&=0xfffe;
}
*/


typedef struct
{
	unsigned short reg, valmask, val;
}
reg_setting;

// ~59.998, couldn't figure closer values
static reg_setting rate_almost60[] =
{
	{ 0x0914, 0xffff, (212<<8)|(2<<2)|1 },	/* UPLLSETVREG */
	{ 0x0924, 0xff00, (2<<14)|(36<<8) },	/* DISPCSETREG */
	{ 0x281A, 0x00ff, 1 },			/* .HSWID(T2) */
	{ 0x281C, 0x00ff, 0 },			/* .HSSTR(T8) */
	{ 0x281E, 0x00ff, 2 },			/* .HSEND(T7) */
	{ 0x2822, 0x01ff, 12 },			/* .VSEND (T9) */
	{ 0x2826, 0x0ff0, 34<<4 },		/* .DESTR(T3) */
	{ 0, 0, 0 }
};

// perfect 50Hz?
static reg_setting rate_50[] =
{
	{ 0x0914, 0xffff, (39<<8)|(0<<2)|2 },	/* UPLLSETVREG */
	{ 0x0924, 0xff00, (2<<14)|(7<<8) },	/* DISPCSETREG */
	{ 0x281A, 0x00ff, 31 },			/* .HSWID(T2) */
	{ 0x281C, 0x00ff, 16 },			/* .HSSTR(T8) */
	{ 0x281E, 0x00ff, 15 },			/* .HSEND(T7) */
	{ 0x2822, 0x01ff, 15 },			/* .VSEND (T9) */
	{ 0x2826, 0x0ff0, 37<<4 },		/* .DESTR(T3) */
	{ 0, 0, 0 }
};

// 16639/2 ~120.20
static reg_setting rate_120_20[] =
{
	{ 0x0914, 0xffff, (96<<8)|(0<<2)|2 },	/* UPLLSETVREG */
	{ 0x0924, 0xff00, (2<<14)|(7<<8) },	/* DISPCSETREG */
	{ 0x281A, 0x00ff, 19 },			/* .HSWID(T2) */
	{ 0x281C, 0x00ff, 7 },			/* .HSSTR(T8) */
	{ 0x281E, 0x00ff, 7 },			/* .HSEND(T7) */
	{ 0x2822, 0x01ff, 12 },			/* .VSEND (T9) */
	{ 0x2826, 0x0ff0, 37<<4 },		/* .DESTR(T3) */
	{ 0, 0, 0 }
};

// 19997/2 ~100.02
static reg_setting rate_100_02[] =
{
	{ 0x0914, 0xffff, (98<<8)|(0<<2)|2 },	/* UPLLSETVREG */
	{ 0x0924, 0xff00, (2<<14)|(8<<8) },	/* DISPCSETREG */
	{ 0x281A, 0x00ff, 26 },			/* .HSWID(T2) */
	{ 0x281C, 0x00ff, 6 },			/* .HSSTR(T8) */
	{ 0x281E, 0x00ff, 6 },			/* .HSEND(T7) */
	{ 0x2822, 0x01ff, 31 },			/* .VSEND (T9) */
	{ 0x2826, 0x0ff0, 37<<4 },		/* .DESTR(T3) */
	{ 0, 0, 0 }
};

// 120.00 97/0/2/7|25/ 7/ 7/11/37
static reg_setting rate_120[] =
{
	{ 0x0914, 0xffff, (97<<8)|(0<<2)|2 },	/* UPLLSETVREG */
	{ 0x0924, 0xff00, (2<<14)|(7<<8) },	/* DISPCSETREG */
	{ 0x281A, 0x00ff, 25 },			/* .HSWID(T2) */
	{ 0x281C, 0x00ff, 7 },			/* .HSSTR(T8) */
	{ 0x281E, 0x00ff, 7 },			/* .HSEND(T7) */
	{ 0x2822, 0x01ff, 11 },			/* .VSEND (T9) */
	{ 0x2826, 0x0ff0, 37<<4 },		/* .DESTR(T3) */
	{ 0, 0, 0 }
};

// 100.00 96/0/2/7|29/25/53/15/37
static reg_setting rate_100[] =
{
	{ 0x0914, 0xffff, (96<<8)|(0<<2)|2 },	/* UPLLSETVREG */
	{ 0x0924, 0xff00, (2<<14)|(7<<8) },	/* DISPCSETREG */
	{ 0x281A, 0x00ff, 29 },			/* .HSWID(T2) */
	{ 0x281C, 0x00ff, 25 },			/* .HSSTR(T8) */
	{ 0x281E, 0x00ff, 53 },			/* .HSEND(T7) */
	{ 0x2822, 0x01ff, 15 },			/* .VSEND (T9) */
	{ 0x2826, 0x0ff0, 37<<4 },		/* .DESTR(T3) */
	{ 0, 0, 0 }
};



static reg_setting *possible_rates[] = { rate_almost60, rate_50, rate_120_20, rate_100_02, rate_120, rate_100 };

void set_LCD_custom_rate(lcd_rate_t rate)
{
	reg_setting *set;

	if (MEM_REG[0x2800>>1] & 0x100) // tv-out
	{
		return;
	}

	printf("setting custom LCD refresh, mode=%i... ", rate); fflush(stdout);
	for (set = possible_rates[rate]; set->reg; set++)
	{
		unsigned short val = MEM_REG[set->reg >> 1];
		val &= ~set->valmask;
		val |= set->val;
		MEM_REG[set->reg >> 1] = val;
	}
	printf("done.\n");
}

void unset_LCD_custom_rate(void)
{
	printf("reset to prev LCD refresh.\n");
	MEM_REG[0x914>>1]=system_reg.UPLLVSETREG;
	MEM_REG[0x924>>1]=system_reg.DISPCSETREG;
	MEM_REG[0x281A>>1]=system_reg.DPC_HS_WIDTH;
	MEM_REG[0x281C>>1]=system_reg.DPC_HS_STR;
	MEM_REG[0x281E>>1]=system_reg.DPC_HS_END;
	MEM_REG[0x2822>>1]=system_reg.DPC_VS_END;
	MEM_REG[0x2826>>1]=system_reg.DPC_DE;
}

void set_RAM_Timings(int tRC, int tRAS, int tWR, int tMRD, int tRFC, int tRP, int tRCD)
{
	tRC -= 1; tRAS -= 1; tWR -= 1; tMRD -= 1; tRFC -= 1; tRP -= 1; tRCD -= 1; // ???
	MEM_REG[0x3802>>1] = ((tMRD & 0xF) << 12) | ((tRFC & 0xF) << 8) | ((tRP & 0xF) << 4) | (tRCD & 0xF);
	MEM_REG[0x3804>>1] = /*0x9000 |*/ ((tRC & 0xF) << 8) | ((tRAS & 0xF) << 4) | (tWR & 0xF);
}


void set_gamma(int g100, int A_SNs_curve)
{
	float gamma = (float) g100 / 100;
	int i;
	gamma = 1/gamma;

	//enable gamma
	MEM_REG[0x2880>>1]&=~(1<<12);

	MEM_REG[0x295C>>1]=0;
	for(i=0; i<256; i++)
	{
		unsigned char g;
		unsigned short s;
		const unsigned short grey50=143, grey75=177, grey25=97;
		float blah;

		if (A_SNs_curve)
		{
			// The next formula is all about gaussian interpolation
			blah = ((  -128 * exp(-powf((float) i/64.0f + 2.0f , 2.0f))) +
				(   -64 * exp(-powf((float) i/64.0f + 1.0f , 2.0f))) +
				(grey25 * exp(-powf((float) i/64.0f - 1.0f , 2.0f))) +
				(grey50 * exp(-powf((float) i/64.0f - 2.0f , 2.0f))) +
				(grey75 * exp(-powf((float) i/64.0f - 3.0f , 2.0f))) +
				(   256 * exp(-powf((float) i/64.0f - 4.0f , 2.0f))) +
				(   320 * exp(-powf((float) i/64.0f - 5.0f , 2.0f))) +
				(   384 * exp(-powf((float) i/64.0f - 6.0f , 2.0f)))) / (1.772637f);
			blah += 0.5f;
		}
		else
		{
			blah = i;
		}

		g = (unsigned char)(255.0*pow(blah/255.0,gamma));
		//printf("%d : %d\n", i, g);
		s = (g<<8) | g;
		MEM_REG[0x295E>>1]= s;
		MEM_REG[0x295E>>1]= g;
	}
}

