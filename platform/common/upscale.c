/*
 * upscale.c		image upscaling
 *
 * This file contains upscalers for picodrive.
 *
 * scaler types:
 * nn:	nearest neighbour
 * snn:	"smoothed" nearest neighbour (see below)
 * bln:	bilinear (using only 0.25, 0.5, 0.75 as weight for better performance)
 *
 * "smoothed" nearest neighbour: uses the average of the source pixels if no
 *	source pixel covers more than 65% of the result pixel. It definitely
 *	looks better than nearest neighbour and is still quite fast. It creates
 *	a sharper look than a bilinear filter, at the price of some visible jags
 *	on diagonal edges.
 *	[NB this has been brought to my attn, which is probably very similar:
 *	https://www.drdobbs.com/image-scaling-with-bresenham/184405045?pgno=1]
 * 
 * scaling modes:
 * 256x___ -> 320x___	only horizontal scaling. Produces an aspect error of
 *			~7% for NTSC 224 line modes, but is correct for PAL
 * 256/320x224/240
 *	-> 320x240	always produces 320x240 at DAR 4:3
 * 160x144 -> 320x240	game gear (currently unused)
 * 
 * (C) 2021 kub <derkub@gmail.com>
 */

#include "upscale.h"

/* 256x___ -> 320x___, H32/mode 4, PAR 5:4, for PAL DAR 4:3 (wrong for NTSC) */
void upscale_clut_nn_256_320x___(u8 *__restrict di, int ds, u8 *__restrict si, int ss, int height)
{
	int y;

	for (y = 0; y < height; y++) {
		h_upscale_nn_4_5(di, ds, si, ss, 256, f_nop);
	}
}

void upscale_rgb_nn_256_320x___(u16 *__restrict di, int ds, u8 *__restrict si, int ss, int height, u16 *pal)
{
	int y;

	for (y = 0; y < height; y++) {
		h_upscale_nn_4_5(di, ds, si, ss, 256, f_pal);
	}
}

void upscale_rgb_snn_256_320x___(u16 *__restrict di, int ds, u8 *__restrict si, int ss, int height, u16 *pal)
{
	int y;

	for (y = 0; y < height; y++) {
		h_upscale_snn_4_5(di, ds, si, ss, 256, f_pal);
	}
}

void upscale_rgb_bln_256_320x___(u16 *__restrict di, int ds, u8 *__restrict si, int ss, int height, u16 *pal)
{
	int y;

	for (y = 0; y < height; y++) {
		h_upscale_bln_4_5(di, ds, si, ss, 256, f_pal);
	}
}

/* 256x224 -> 320x240, H32/mode 4, PAR 5:4, for NTSC DAR 4:3 (wrong for PAL) */
void upscale_clut_nn_256_320x224_240(u8 *__restrict di, int ds, u8 *__restrict si, int ss)
{
	int y, j;

	/* 14:15, 0 1 2 3 4 5 6 6 7 8 9 10 11 12 13 */
	for (y = 0; y < 224; y += 14) {
		/* lines 0-6 */
		for (j = 0; j < 7; j++) {
			h_upscale_nn_4_5(di, ds, si, ss, 256, f_nop);
		}
		/* lines 8-14 */
		di += ds;
		for (j = 0; j < 7; j++) {
			h_upscale_nn_4_5(di, ds, si, ss, 256, f_nop);
		}
		/* line 7 */
		di -= 8*ds;
		v_copy(&di[0], &di[-ds], 320, f_nop);
		di += 8*ds;
	}
}

void upscale_rgb_nn_256_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal)
{
	int y, j;

	for (y = 0; y < 224; y += 14) {
		for (j = 0; j < 7; j++) {
			h_upscale_nn_4_5(di, ds, si, ss, 256, f_pal);
		}
		di +=  ds;
		for (j = 0; j < 7; j++) {
			h_upscale_nn_4_5(di, ds, si, ss, 256, f_pal);
		}

		di -= 8*ds;
		v_copy(&di[0], &di[-ds], 320, f_nop);
		di += 8*ds;
	}
}

void upscale_rgb_snn_256_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal)
{
	int y, j;

	/* 14:15, 0 1 2 3 4 5 5+6 6+7 7+8 8 9 10 11 12 13 */
	for (y = 0; y < 224; y += 14) {
		for (j = 0; j < 7; j++) {
			h_upscale_snn_4_5(di, ds, si, ss, 256, f_pal);
		}
		di +=  ds;
		for (j = 0; j < 7; j++) {
			h_upscale_snn_4_5(di, ds, si, ss, 256, f_pal);
		}

		/* mix lines 6-8 */
		di -= 8*ds;
		v_mix(&di[0], &di[-ds], &di[ds], 320, p_05, f_nop);
		v_mix(&di[-ds], &di[-2*ds], &di[-ds], 320, p_05, f_nop);
		v_mix(&di[ ds], &di[ ds], &di[ 2*ds], 320, p_05, f_nop);
		di += 8*ds;
	}
}

void upscale_rgb_bln_256_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal)
{
	int y, j;

	/* 14:15, 0 1 2 2+3 3+4 4+5 5+6 6+7 7+8 8+9 9+10 10+11 11 12 13 */
	for (y = 0; y < 224; y += 14) {
		/* lines 0-2 */
		for (j = 0; j < 3; j++) {
			h_upscale_bln_4_5(di, ds, si, ss, 256, f_pal);
		}
		/* lines 3-14 mixing prep */
		di += ds;
		for (j = 0; j < 11; j++) {
			h_upscale_bln_4_5(di, ds, si, ss, 256, f_pal);
		}
		di -= 12*ds;
		/* mixing line 3: line 2 = -ds, line 3 = +ds */
			v_mix(&di[0], &di[-ds], &di[ds], 320, p_025, f_nop);
			di += ds;
		/* mixing lines 4-5: line n-1 = 0, line n = +ds */
		for (j = 0; j < 2; j++) {
			v_mix(&di[0], &di[0], &di[ds], 320, p_025, f_nop);
			di += ds;
			}
		/* mixing lines 6-8 */
		for (j = 0; j < 3; j++) {
			v_mix(&di[0], &di[0], &di[ds], 320, p_05, f_nop);
			di += ds;
		}
		/* mixing lines 9-11 */
		for (j = 0; j < 3; j++) {
			v_mix(&di[0], &di[0], &di[ds], 320, p_075, f_nop);
			di += ds;
		}
		/* lines 12-14, already in place */
		di += 3*ds;
	}
}

/* 320x224 -> 320x240, PAR 1:1, for NTSC, DAR 4:3 (wrong for PAL) */
void upscale_clut_nn_320x224_240(u8 *__restrict di, int ds, u8 *__restrict si, int ss)
{
	int y, j;

	for (y = 0; y < 224; y += 14) {
		for (j = 0; j < 7; j++) {
			h_copy(di, ds, si, ss, 320, f_nop);
		}
		di += ds;
		for (j = 0; j < 7; j++) {
			h_copy(di, ds, si, ss, 320, f_nop);
		}

		di -= 8*ds;
		v_copy(&di[0], &di[-ds], 320, f_nop);
		di += 8*ds;

	}
}

void upscale_rgb_nn_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal)
{
	int y, j;

	for (y = 0; y < 224; y += 14) {
		for (j = 0; j < 7; j++) {
			h_copy(di, ds, si, ss, 320, f_pal);
		}
		di +=  ds;
		for (j = 0; j < 7; j++) {
			h_copy(di, ds, si, ss, 320, f_pal);
		}

		di -= 8*ds;
		v_copy(&di[0], &di[-ds], 320, f_nop);
		di += 8*ds;
	}
}

void upscale_rgb_snn_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal)
{
	int y, j;

	for (y = 0; y < 224; y += 14) {
		for (j = 0; j < 7; j++) {
			h_copy(di, ds, si, ss, 320, f_pal);
		}
		di +=  ds;
		for (j = 0; j < 7; j++) {
			h_copy(di, ds, si, ss, 320, f_pal);
		}

		di -= 8*ds;
		v_mix(&di[  0], &di[-ds], &di[ds], 320, p_05, f_nop);
		v_mix(&di[-ds], &di[-2*ds], &di[-ds], 320, p_05, f_nop);
		v_mix(&di[ ds], &di[ ds], &di[ 2*ds], 320, p_05, f_nop);
		di += 8*ds;
	}
}

void upscale_rgb_bln_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal)
{
	int y, j;

	for (y = 0; y < 224; y += 14) {
		for (j = 0; j < 3; j++) {
			h_copy(di, ds, si, ss, 320, f_pal);
		}
		for (j = 0; j < 3; j++) {
			v_mix(&di[0], &si[-ss], &si[0], 320, p_025, f_pal);
			di += ds;
			si += ss;
		}
		for (j = 0; j < 3; j++) {
			v_mix(&di[0], &si[-ss], &si[0], 320, p_05, f_pal);
			di += ds;
			si += ss;
		}
		for (j = 0; j < 3; j++) {
			v_mix(&di[0], &si[-ss], &si[0], 320, p_075, f_pal);
			di += ds;
			si += ss;
		}
		si -= ss;
		for (j = 0; j < 3; j++) {
			h_copy(di, ds, si, ss, 320, f_pal);
		}
	}
}

/* 160x144 -> 320x240: GG, PAR 6:5, scaling to 320x240 for DAR 4:3 */
/* NB for smoother image could scale to 288x216, x*9/5, y*3/2 ?
 *	 h: 11111 11112 22222 22233 33333 33444 44444 45555 55555
 *            1     1     2    2+3    3    3+4    4     5     5
 *       v: 11  12  22
 *          1   1+2 2
 */
void upscale_clut_nn_160_320x144_240(u8 *__restrict di, int ds, u8 *__restrict si, int ss)
{
	int y, j;

	/* 3:5, 0 0 1 1 2 */
	for (y = 0; y < 144; y += 3) {
		/* lines 0,2,4 */
		for (j = 0; j < 3; j++) {
			h_upscale_nn_1_2(di, ds, si, ss, 160, f_nop);
			di += ds;
		}
		/* lines 1,3 */
		di -= 5*ds;
		for (j = 0; j < 2; j++) {
			v_copy(&di[0], &di[-ds], 320, f_nop);
			di += 2*ds;
		}
	}
}

void upscale_rgb_nn_160_320x144_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal)
{
	int y, j;

	for (y = 0; y < 144; y += 3) {
		for (j = 0; j < 3; j++) {
			h_upscale_nn_1_2(di, ds, si, ss, 160, f_pal);
			di += ds;
		}
		di -= 5*ds;
		for (j = 0; j < 2; j++) {
			v_copy(&di[0], &di[-ds], 320, f_nop);
			di += 2*ds;
		}
	}
}

void upscale_rgb_snn_160_320x144_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal)
{
	int y, j;

	/* 3:5, 0 0+1 1 1+2 2 */
	for (y = 0; y < 144; y += 3) {
		for (j = 0; j < 3; j++) {
			h_upscale_nn_1_2(di, ds, si, ss, 160, f_pal);
			di += ds;
		}
		di -= 5*ds;
		for (j = 0; j < 2; j++) {
			v_mix(&di[0], &di[-ds], &di[ds], 320, p_05, f_nop);
			di += 2*ds;
		}
	}
}

void upscale_rgb_bln_160_320x144_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal)
{
	int y, j;

	for (y = 0; y < 144; y += 3) {
		for (j = 0; j < 3; j++) {
			h_upscale_nn_1_2(di, ds, si, ss, 160, f_pal);
			di += ds;
		}
		di -= 5*ds;
		v_mix(&di[0], &di[-ds], &di[ds], 320, p_075, f_nop);
		di += 2*ds;
		v_mix(&di[0], &di[-ds], &di[ds], 320, p_025, f_nop);
		di += 2*ds;
	}
}

