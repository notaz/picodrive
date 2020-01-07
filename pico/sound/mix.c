/*
 * some code for sample mixing
 * (C) notaz, 2006,2007
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */

#include "string.h"

#define MAXOUT		(+32767)
#define MINOUT		(-32768)

/* limitter */
#define Limit16(val) \
	if ((short)val != val) val = (val < 0 ? MINOUT : MAXOUT)

int mix_32_to_16l_level;

static struct iir2 { // 2-pole IIR
	int	x[2];		// sample buffer
	int	y[2];		// filter intermediates
	int	i;
} lfi2, rfi2;

// NB ">>" rounds to -infinity, "/" to 0. To compensate the effect possibly use
// "-(-y>>n)" (round to +infinity) instead of "y>>n" in places.

// NB uses Q12 fixpoint; samples mustn't have more than 20 bits for this.
#define QB	12


// exponential moving average filter for DC filtering
// y[n] = (x[n]-y[n-1])*(1/8192) (corner approx. 20Hz, gain 1)
static inline int filter_exp(struct iir2 *fi2, int x)
{
	int xf = (x<<QB) - fi2->y[0];
	fi2->y[0] += xf >> 13;
	xf -= xf >> 2;	// level reduction to avoid clipping from overshoot
	return xf>>QB;
}

// unfiltered (for testing)
static inline int filter_null(struct iir2 *fi2, int x)
{
	return x;
}

#define mix_32_to_16l_stereo_core(dest, src, count, lv, fl) {	\
	int l, r;						\
								\
	for (; count > 0; count--)				\
	{							\
		l = r = *dest;					\
		l += *src++ >> lv;				\
		r += *src++ >> lv;				\
		l = fl(&lfi2, l);				\
		r = fl(&rfi2, r);				\
		Limit16(l);					\
		Limit16(r);					\
		*dest++ = l;					\
		*dest++ = r;					\
	}							\
}

void mix_32_to_16l_stereo_lvl(short *dest, int *src, int count)
{
	mix_32_to_16l_stereo_core(dest, src, count, mix_32_to_16l_level, filter_exp);
}

void mix_32_to_16l_stereo(short *dest, int *src, int count)
{
	mix_32_to_16l_stereo_core(dest, src, count, 0, filter_exp);
}

void mix_32_to_16_mono(short *dest, int *src, int count)
{
	int l;

	for (; count > 0; count--)
	{
		l = *dest;
		l += *src++;
		l = filter_exp(&lfi2, l);
		Limit16(l);
		*dest++ = l;
	}
}


void mix_16h_to_32(int *dest_buf, short *mp3_buf, int count)
{
	while (count--)
	{
		*dest_buf++ += *mp3_buf++ >> 1;
	}
}

void mix_16h_to_32_s1(int *dest_buf, short *mp3_buf, int count)
{
	count >>= 1;
	while (count--)
	{
		*dest_buf++ += *mp3_buf++ >> 1;
		*dest_buf++ += *mp3_buf++ >> 1;
		mp3_buf += 1*2;
	}
}

void mix_16h_to_32_s2(int *dest_buf, short *mp3_buf, int count)
{
	count >>= 1;
	while (count--)
	{
		*dest_buf++ += *mp3_buf++ >> 1;
		*dest_buf++ += *mp3_buf++ >> 1;
		mp3_buf += 3*2;
	}
}

void mix_reset(void)
{
	memset(&lfi2, 0, sizeof(lfi2));
	memset(&rfi2, 0, sizeof(rfi2));
}
