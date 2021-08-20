/*
 * upscale.h		image upscaling
 *
 * This file contains upscalers for picodrive.
 *
 * scaler types:
 * nn:	nearest neighbour
 * snn:	"smoothed" nearest neighbour (see below)
 * bln:	n-level-bilinear with n quantized weights
 *	quantization: 0: a<1/2*n, 1/n: 1/2*n<=a<3/2*n, etc
 *	currently n=2, n=4 are implemented (there's n=8 mixing, but no filters)
 *	[NB this has been brought to my attn, which is probably the same as bl2:
 *	https://www.drdobbs.com/image-scaling-with-bresenham/184405045?pgno=1]
 *
 * "smoothed" nearest neighbour: uses the average of the source pixels if no
 *	source pixel covers more than 65% of the result pixel. It definitely
 *	looks better than nearest neighbour and is still quite fast. It creates
 *	a sharper look than a bilinear filter, at the price of some visible jags
 *	on diagonal edges.
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
#include <pico/pico_types.h>

/* RGB565 pixel mixing, see https://www.compuphase.com/graphic/scale3.htm and
  			    http://blargg.8bitalley.com/info/rgb_mixing.html */
/* 2-level mixing */
//#define p_05(p1,p2)	(((p1)+(p2)  + ( ((p1)^(p2))&0x0821))>>1) // round up
//#define p_05(p1,p2)	(((p1)+(p2)  - ( ((p1)^(p2))&0x0821))>>1) // round down
#define p_05(p1,p2)	(((p1)&(p2)) + ((((p1)^(p2))&~0x0821)>>1))
/* 4-level mixing, 2 times slower */
// 1/4*p1 + 3/4*p2 = 1/2*(1/2*(p1+p2) + p2)
#define p_025(p1,p2)	(t=p_05(p1, p2),   p_05( t, p2))
#define p_075(p1,p2)	p_025(p2,p1)
/* 8-level mixing, 3 times slower */
// 1/8*p1 + 7/8*p2 = 1/2*(1/2*(1/2*(p1+p2) + p2) + p2)
#define p_0125(p1,p2)	(t=p_05(p1, p2), u=p_05( t, p2), p_05( u, p2))
// 3/8*p1 + 5/8*p2 = 1/2*(1/2*(1/2*(p1+p2) + p2) + 1/2*(p1+p2))
#define p_0375(p1,p2)	(t=p_05(p1, p2), u=p_05( t, p2), p_05( u,  t))
#define p_0625(p1,p2)	p_0375(p2,p1)
#define p_0875(p1,p2)	p_0125(p2,p1)

/* pixel transforms */
#define	f_pal(v)	pal[v]	// convert CLUT index -> RGB565
#define f_nop(v)	(v)	// source already in dest format (CLUT/RGB)
#define f_or(v)		(v|pal)	// CLUT, add palette selection

/*
scalers h:
256->320:       - (4:5)         (256x224/240 -> 320x224/240)
256->299:	- (6:7)		(256x224 -> 299x224, DAR 4:3, 10.5 px border )
160->320:       - (1:2) 2x      (160x144 -> 320x240, GG)
160->288:	- (5:9)		(160x144 -> 288x216, GG alt?)
*/

/* scale 4:5 */
#define h_upscale_nn_4_5(di,ds,si,ss,w,f) do {		\
	int i;						\
	for (i = w/4; i > 0; i--, si += 4, di += 5) {	\
		di[0] = f(si[0]);			\
		di[1] = f(si[1]);			\
		di[2] = f(si[1]);			\
		di[3] = f(si[2]);			\
		di[4] = f(si[3]);			\
	}						\
	di += ds - w/4*5;				\
	si += ss - w;					\
} while (0)

// reverse version for overlapping buffers
#define rh_upscale_nn_4_5(di,ds,si,ss,w,f) do {	\
	int i;						\
	di += w/4*5;					\
	si += w;					\
	for (i = w/4; i > 0; i--, si -= 4, di -= 5) {	\
		di[-1] = f(si[-1]);			\
		di[-2] = f(si[-2]);			\
		di[-3] = f(si[-3]);			\
		di[-4] = f(si[-3]);			\
		di[-5] = f(si[-4]);			\
	}						\
	di += ds;					\
	si += ss;					\
} while (0)

#define h_upscale_snn_4_5(di,ds,si,ss,w,f) do {		\
	int i;						\
	for (i = w/4; i > 0; i--, si += 4, di += 5) {	\
		di[0] = f(si[0]);			\
		di[1] = f(si[1]);			\
		di[2] = p_05(f(si[1]),f(si[2]));	\
		di[3] = f(si[2]);			\
		di[4] = f(si[3]);			\
	}						\
	di += ds - w/4*5;				\
	si += ss - w;					\
} while (0)

#define h_upscale_bln_4_5(di,ds,si,ss,w,f) do {		\
	int i, t; 					\
	for (i = w/4; i > 0; i--, si += 4, di += 5) {	\
		di[0] = f(si[0]);			\
		di[1] = p_025(f(si[0]),f(si[1]));	\
		di[2] = p_05 (f(si[1]),f(si[2]));	\
		di[3] = p_075(f(si[2]),f(si[3]));	\
		di[4] = f(si[3]);			\
	}						\
	di += ds - w/4*5;				\
	si += ss - w;					\
} while (0)

#define h_upscale_bl2_4_5(di,ds,si,ss,w,f) do {		\
	int i;						\
	for (i = w/4; i > 0; i--, si += 4, di += 5) {	\
		di[0] = f(si[0]);			\
		di[1] = p_05(f(si[0]),f(si[1]));	\
		di[2] = p_05(f(si[1]),f(si[2]));	\
		di[3] = f(si[2]);			\
		di[4] = f(si[3]);			\
	}						\
	di += ds - w/4*5;				\
	si += ss - w;					\
} while (0)

#define h_upscale_bl4_4_5(di,ds,si,ss,w,f) do {		\
	int i, t; uint p = f(si[0]);			\
	for (i = w/4; i > 0; i--, si += 4, di += 5) {	\
		di[0] = p_025(p, f(si[0]));		\
		di[1] = p_05 (f(si[0]),f(si[1]));	\
		di[2] = p_05 (f(si[1]),f(si[2]));	\
		di[3] = p_075(f(si[2]),f(si[3]));	\
		di[4] = p = f(si[3]);			\
	}						\
	di += ds - w/4*5;				\
	si += ss - w;					\
} while (0)

#define h_upscale_bl8_4_5(di,ds,si,ss,w,f) do {		\
	int i, t, u; uint p = f(si[0]);			\
	for (i = w/4; i > 0; i--, si += 4, di += 5) {	\
		di[0] = p_025(p, f(si[0]));		\
		di[1] = p_0375(f(si[0]),f(si[1]));	\
		di[2] = p_0625(f(si[1]),f(si[2]));	\
		di[3] = p_075(f(si[2]),f(si[3]));	\
		di[4] = p = f(si[3]);			\
	}						\
	di += ds - w/4*5;				\
	si += ss - w;					\
} while (0)

/* scale 6:7 */
#define h_upscale_nn_6_7(di,ds,si,ss,w,f) do {		\
	int i;						\
	for (i = w/6; i > 0; i--, si += 6, di += 7) {	\
		di[0] = f(si[0]);			\
		di[1] = f(si[1]);			\
		di[2] = f(si[2]);			\
		di[3] = f(si[2]);			\
		di[4] = f(si[3]);			\
		di[5] = f(si[4]);			\
		di[6] = f(si[5]);			\
	}						\
	di += ds - w/6*7;				\
	si += ss - w;					\
} while (0)

// reverse version for overlapping buffers
#define rh_upscale_nn_6_7(di,ds,si,ss,w,f) do {	\
	int i;						\
	di += w/6*7;					\
	si += w;					\
	for (i = w/6; i > 0; i--, si -= 6, di -= 7) {	\
		di[-1] = f(si[-1]);			\
		di[-2] = f(si[-2]);			\
		di[-3] = f(si[-3]);			\
		di[-4] = f(si[-4]);			\
		di[-5] = f(si[-4]);			\
		di[-6] = f(si[-5]);			\
		di[-7] = f(si[-6]);			\
	}						\
	di += ds;					\
	si += ss;					\
} while (0)

#define h_upscale_snn_6_7(di,ds,si,ss,w,f) do {		\
	int i;						\
	for (i = w/6; i > 0; i--, si += 6, di += 7) {	\
		di[0] = f(si[0]);			\
		di[1] = f(si[1]);			\
		di[2] = f(si[2]);			\
		di[3] = p_05(f(si[2]),f(si[3]));	\
		di[4] = f(si[3]);			\
		di[5] = f(si[4]);			\
		di[6] = f(si[5]);			\
	}						\
	di += ds - w/6*7;				\
	si += ss - w;					\
} while (0)

#define h_upscale_bl2_6_7(di,ds,si,ss,w,f) do {		\
	int i;						\
	for (i = w/6; i > 0; i--, si += 6, di += 7) {	\
		di[0] = f(si[0]);			\
		di[1] = p_05(f(si[0]),f(si[1]));	\
		di[2] = p_05(f(si[1]),f(si[2]));	\
		di[3] = p_05(f(si[2]),f(si[3]));	\
		di[4] = p_05(f(si[3]),f(si[4]));	\
		di[5] = f(si[4]);			\
		di[6] = f(si[5]);			\
	}						\
	di += ds - w/6*7;				\
	si += ss - w;					\
} while (0)

#define h_upscale_bl4_6_7(di,ds,si,ss,w,f) do {		\
	int i, t; uint p = f(si[0]);			\
	for (i = w/6; i > 0; i--, si += 6, di += 7) {	\
		di[0] = p_025(p,f(si[0]));		\
		di[1] = p_025(f(si[0]),f(si[1]));	\
		di[2] = p_05 (f(si[1]),f(si[2]));	\
		di[3] = p_05 (f(si[2]),f(si[3]));	\
		di[4] = p_075(f(si[3]),f(si[4]));	\
		di[5] = p_075(f(si[4]),f(si[5]));	\
		di[6] = p = f(si[5]);			\
	}						\
	di += ds - w/6*7;				\
	si += ss - w;					\
} while (0)

/* scale 5:9 */
#define h_upscale_nn_5_9(di,ds,si,ss,w,f) do {		\
	int i;						\
	for (i = w/5; i > 0; i--, si += 5, di += 9) {	\
		di[0] = f(si[0]);			\
		di[1] = f(si[0]);			\
		di[2] = f(si[1]);			\
		di[3] = f(si[1]);			\
		di[4] = f(si[2]);			\
		di[5] = f(si[3]);			\
		di[6] = f(si[3]);			\
		di[7] = f(si[4]);			\
		di[8] = f(si[4]);			\
	}						\
	di += ds - w/5*9;				\
	si += ss - w;					\
} while (0)

#define h_upscale_snn_5_9(di,ds,si,ss,w,f) do {		\
	int i;						\
	for (i = w/5; i > 0; i--, si += 5, di += 9) {	\
		di[0] = f(si[0]);			\
		di[1] = f(si[0]);			\
		di[2] = f(si[1]);			\
		di[3] = p_05(f(si[1]),f(si[2]));	\
		di[4] = f(si[2]);			\
		di[5] = p_05(f(si[2]),f(si[3]));	\
		di[6] = f(si[3]);			\
		di[7] = f(si[4]);			\
		di[8] = f(si[4]);			\
	}						\
	di += ds - w/5*9;				\
	si += ss - w;					\
} while (0)

#define h_upscale_bl2_5_9(di,ds,si,ss,w,f) do {		\
	int i;						\
	for (i = w/5; i > 0; i--, si += 5, di += 9) {	\
		di[0] = f(si[0]);			\
		di[1] = p_05(f(si[0]),f(si[1]));	\
		di[2] = f(si[1]);			\
		di[3] = p_05(f(si[1]),f(si[2]));	\
		di[4] = f(si[2]);			\
		di[5] = p_05(f(si[2]),f(si[3]));	\
		di[6] = f(si[3]);			\
		di[7] = p_05(f(si[3]),f(si[4]));	\
		di[8] = f(si[4]);			\
	}						\
	di += ds - w/5*9;				\
	si += ss - w;					\
} while (0)

#define h_upscale_bl4_5_9(di,ds,si,ss,w,f) do {		\
	int i, t; uint p = f(si[0]);			\
	for (i = w/5; i > 0; i--, si += 5, di += 9) {	\
		di[0] = p_05 (p,f(si[0]));		\
		di[1] = f(si[0]);			\
		di[2] = p_025(f(si[0]),f(si[1]));	\
		di[3] = p_075(f(si[1]),f(si[2]));	\
		di[4] = p_025(f(si[1]),f(si[2]));	\
		di[5] = p_075(f(si[2]),f(si[3]));	\
		di[6] = f(si[3]);			\
		di[7] = p_05 (f(si[3]),f(si[4]));	\
		di[8] = p = f(si[4]);			\
	}						\
	di += ds - w/5*9;				\
	si += ss - w;					\
} while (0)

/* scale 1:2 integer scale */
#define h_upscale_nn_1_2(di,ds,si,ss,w,f) do {		\
	int i;						\
	for (i = w/2; i > 0; i--, si += 2, di += 4) {	\
		di[0] = f(si[0]);			\
		di[1] = f(si[0]);			\
		di[2] = f(si[1]);			\
		di[3] = f(si[1]);			\
	}						\
	di += ds - w*2;					\
	si += ss - w;					\
} while (0)

#define h_upscale_bl2_1_2(di,ds,si,ss,w,f) do {		\
	int i; uint p = f(si[0]);			\
	for (i = w/2; i > 0; i--, si += 2, di += 4) {	\
		di[0] = p_05 (p, f(si[0]));		\
		di[1] = f(si[0]);			\
		di[2] = p_05 (f(si[0]), f(si[1]));	\
		di[3] = p = f(si[1]);			\
	}						\
	di += ds - w*2;					\
	si += ss - w;					\
} while (0)

/* scale 1:1, copy */
#define h_copy(di,ds,si,ss,w,f) do {			\
	int i;						\
	for (i = w/4; i > 0; i--, si += 4, di += 4) {	\
		di[0] = f(si[0]);			\
		di[1] = f(si[1]);			\
		di[2] = f(si[2]);			\
		di[3] = f(si[3]);			\
	}						\
	di += ds - w;					\
	si += ss - w;					\
} while (0)

/*
scalers v:
224->240:       - (14:15)       (256/320x224 -> 320x240)
224->238:       - (16:17)       (256/320x224 -> 320x238 alt?)
144->240:       - (3:5)         (160x144 -> 320x240, GG)
144->216:	- (2:3)		(160x144 -> 288x216, GG alt?)
*/

#define v_mix(di,li,ri,w,p_mix,f) do {			\
	int i, t, u; (void)t, (void)u;			\
	for (i = 0; i < w; i += 4) {			\
		(di)[i  ] = p_mix(f((li)[i  ]), f((ri)[i  ])); \
		(di)[i+1] = p_mix(f((li)[i+1]), f((ri)[i+1])); \
		(di)[i+2] = p_mix(f((li)[i+2]), f((ri)[i+2])); \
		(di)[i+3] = p_mix(f((li)[i+3]), f((ri)[i+3])); \
	}						\
} while (0)

#define v_copy(di,ri,w,f) do {				\
	int i;						\
	for (i = 0; i < w; i += 4) {			\
		(di)[i  ] = f((ri)[i  ]);		\
		(di)[i+1] = f((ri)[i+1]);		\
		(di)[i+2] = f((ri)[i+2]);		\
		(di)[i+3] = f((ri)[i+3]);		\
	}						\
} while (0)



/* 256x___ -> 320x___, H32/mode 4, PAR 5:4, for PAL DAR 4:3 (wrong for NTSC) */
void upscale_clut_nn_256_320x___(u8 *__restrict di, int ds, u8 *__restrict si, int ss, int height);
void upscale_rgb_nn_256_320x___(u16 *__restrict di, int ds, u8 *__restrict si, int ss, int height, u16 *pal);
void upscale_rgb_snn_256_320x___(u16 *__restrict di, int ds, u8 *__restrict si, int ss, int height, u16 *pal);
void upscale_rgb_bl2_256_320x___(u16 *__restrict di, int ds, u8 *__restrict si, int ss, int height, u16 *pal);
void upscale_rgb_bl4_256_320x___(u16 *__restrict di, int ds, u8 *__restrict si, int ss, int height, u16 *pal);

/* 256x224 -> 320x240, H32/mode 4, PAR 5:4, for NTSC DAR 4:3 (wrong for PAL) */
void upscale_clut_nn_256_320x224_240(u8 *__restrict di, int ds, u8 *__restrict si, int ss);
void upscale_rgb_nn_256_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);
void upscale_rgb_snn_256_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);
void upscale_rgb_bl2_256_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);
void upscale_rgb_bl4_256_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);

/* 320x224 -> 320x240, PAR 1:1, for NTSC, DAR 4:3 (wrong for PAL) */
void upscale_clut_nn_320x224_240(u8 *__restrict di, int ds, u8 *__restrict si, int ss);
void upscale_rgb_nn_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);
void upscale_rgb_snn_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);
void upscale_rgb_bl2_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);
void upscale_rgb_bl4_320x224_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);

/* 160x144 -> 320x240: GG, PAR 6:5, scaling to 320x240 for DAR 4:3 */
void upscale_clut_nn_160_320x144_240(u8 *__restrict di, int ds, u8 *__restrict si, int ss);
void upscale_rgb_nn_160_320x144_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);
void upscale_rgb_snn_160_320x144_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);
void upscale_rgb_bl2_160_320x144_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);
void upscale_rgb_bl4_160_320x144_240(u16 *__restrict di, int ds, u8 *__restrict si, int ss, u16 *pal);
