#define MAXOUT		(+32767)
#define MINOUT		(-32768)

/* limitter */
#define Limit(val, max,min) { \
	if ( val > max )      val = max; \
	else if ( val < min ) val = min; \
}




void memcpy32(int *dest, int *src, int count)
{
	while (count--)
		*dest++ = *src++;
}


void memset32(int *dest, int c, int count)
{
	while (count--)
		*dest++ = c;
}


void mix_32_to_16l_stereo(short *dest, int *src, int count)
{
	int l, r;

	for (; count > 0; count--)
	{
		l = r = *dest;
		l += *src++;
		r += *src++;
		Limit( l, MAXOUT, MINOUT );
		Limit( r, MAXOUT, MINOUT );
		*dest++ = l;
		*dest++ = r;
	}
}


void mix_32_to_16_mono(short *dest, int *src, int count)
{
	int l;

	for (; count > 0; count--)
	{
		l = *dest;
		l += *src++;
		Limit( l, MAXOUT, MINOUT );
		*dest++ = l;
	}
}


