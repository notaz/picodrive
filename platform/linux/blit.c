
// Convert 0000bbb0 ggg0rrr0 0000bbb0 ggg0rrr0
// to      00000000 rrr00000 ggg00000 bbb00000 ...

void vidConvCpyRGB32  (void *to, void *from, int pixels)
{
	unsigned short *ps = from;
	unsigned int   *pd = to;

	for (; pixels; pixels--, ps++, pd++)
	{
		*pd = ((*ps<<20)&0xe00000) | ((*ps<<8)&0xe000) | ((*ps>>4)&0xe0);
		*pd |= *pd >> 3;
	}
}

void vidConvCpyRGB32sh(void *to, void *from, int pixels)
{
	unsigned short *ps = from;
	unsigned int   *pd = to;

	for (; pixels; pixels--, ps++, pd++)
	{
		*pd = ((*ps<<20)&0xe00000) | ((*ps<<8)&0xe000) | ((*ps>>4)&0xe0);
		*pd >>= 1;
		*pd |= *pd >> 3;
	}
}

void vidConvCpyRGB32hi(void *to, void *from, int pixels)
{
	unsigned short *ps = from;
	unsigned int   *pd = to;

	for (; pixels; pixels--, ps++, pd++)
	{
		*pd = ((*ps<<20)&0xe00000) | ((*ps<<8)&0xe000) | ((*ps>>4)&0xe0);
		continue;
		*pd += 0x00404040;
		if (*pd & 0x01000000) *pd |= 0x00e00000;
		if (*pd & 0x00010000) *pd |= 0x0000e000;
		if (*pd & 0x00000100) *pd |= 0x000000e0;
		*pd &= 0x00e0e0e0;
		*pd |= *pd >> 3;
	}
}

void vidCpyM2_40col(void *dest, void *src)
{
	unsigned char *pd = dest, *ps = src;
	int i, u;

	for (i = 0; i < 224; i++)
	{
		ps += 8;
		for (u = 0; u < 320; u++)
			*pd++ = *ps++;
	}
}

void vidCpyM2_32col(void *dest, void *src)
{
	unsigned char *pd = dest, *ps = src;
	int i, u;

	for (i = 0; i < 224; i++)
	{
		ps += 8;
		pd += 32;
		for (u = 0; u < 256; u++)
			*pd++ = *ps++;
		ps += 64;
		pd += 32;
	}
}

void vidCpyM2_32col_nobord(void *dest, void *src)
{
	vidCpyM2_32col(dest, src);
}

