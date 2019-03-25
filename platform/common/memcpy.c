/*
 * (C) 2018 Kai-Uwe Bloem <derkub@gmail.com>
 *
 * 32bit ARM/MIPS optimized C implementation of memcpy and memove, designed for
 * good performance with gcc.
 * - if src and dest have the same alignment, 4-word copy is used.
 * - if src and dest are unaligned to each other, still loads word data and
 *   stores correctly shifted word data (for all but the first and last bytes
 *   to avoid under/overstepping the src region).
 *
 * ATTN does dirty aliasing tricks with undefined behaviour by standard.
 *	(however, this was needed to improve the generated code).
 * ATTN uses struct assignment, which only works if the compiler is inlining
 *	this (else it would probably call memcpy :-)).
 */
#include <stdlib.h>
#include <stdint.h>

#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define	_L_ >>
#define	_U_ <<
#else
#define	_L_ <<
#define	_U_ >>
#endif

void *memcpy(void *dest, const void *src, size_t n)
{
	struct _16 { uint32_t a[4]; };
	union { const void *v; uint8_t *c; uint32_t *i; uint64_t *l; struct _16 *s; }
		ss = { src }, ds = { dest };
	const int lm = sizeof(uint32_t)-1;

	/* align src to word */
	while (((unsigned)ss.c & lm) && n > 0)
		*ds.c++ = *ss.c++, n--;
	if (((unsigned)ds.c & lm) == 0) {
		/* fast copy if pointers have the same aligment */
		while (n >= sizeof(struct _16))	/* copy 16 bytes blocks */
			*ds.s++ = *ss.s++, n -= sizeof(struct _16);
		if (n >= sizeof(uint64_t))	/* copy leftover 8 byte block */
			*ds.l++ = *ss.l++, n -= sizeof(uint64_t);
	} else if (n >= 2*sizeof(uint32_t)) {
		/* unaligned data big enough to avoid overstepping src */
		uint32_t v1, v2, b, s;
		/* align dest to word */
		while (((unsigned)ds.c & lm) && n > 0)
			*ds.c++ = *ss.c++, n--;
		/* copy loop: load aligned words and store shifted words */
		b = (unsigned)ss.c & lm, s = b*8; ss.c -= b;
		v1 = *ss.i++, v2 = *ss.i++;
		while (n >= 3*sizeof(uint32_t)) {
			*ds.i++ = (v1 _L_ s) | (v2 _U_ (32-s)); v1 = *ss.i++;
			*ds.i++ = (v2 _L_ s) | (v1 _U_ (32-s)); v2 = *ss.i++;
			n -= 2*sizeof(uint32_t);
		}
		/* data for one more store is already loaded */
		if (n >= sizeof(uint32_t)) {
			*ds.i++ = (v1 _L_ s) | (v2 _U_ (32-s));
			n -= sizeof(uint32_t);
			ss.c += sizeof(uint32_t);
		}
		ss.c += b - 2*sizeof(uint32_t);
	}
	/* copy 0-7 leftover bytes */
	while (n >= 4) {
		*ds.c++ = *ss.c++, n--; *ds.c++ = *ss.c++, n--;
		*ds.c++ = *ss.c++, n--; *ds.c++ = *ss.c++, n--;
	}
	while (n > 0)
		*ds.c++ = *ss.c++, n--;
	return dest;
}

void *memmove (void *dest, const void *src, size_t n)
{
	struct _16 { uint32_t a[4]; };
	union { const void *v; uint8_t *c; uint32_t *i; uint64_t *l; struct _16 *s; }
		ss = { src+n }, ds = { dest+n };
	const int lm = sizeof(uint32_t)-1;

	if (dest <= src || dest >= src+n)
		return memcpy(dest, src, n);

	/* align src to word */
	while (((unsigned)ss.c & lm) && n > 0)
		*--ds.c = *--ss.c, n--;
	if (((unsigned)ds.c & lm) == 0) {
		/* fast copy if pointers have the same aligment */
		while (n >= sizeof(struct _16))	/* copy 16 byte blocks */
			*--ds.s = *--ss.s, n -= sizeof(struct _16);
		if (n >= sizeof(uint64_t))	/* copy leftover 8 byte block */
			*--ds.l = *--ss.l, n -= sizeof(uint64_t);
	} else if (n >= 2*sizeof(uint32_t)) {
		/* unaligned data big enough to avoid understepping src */
		uint32_t v1, v2, b, s;
		/* align dest to word */
		while (((unsigned)ds.c & lm) && n > 0)
			*--ds.c = *--ss.c, n--;
		/* copy loop: load aligned words and store shifted words */
		b = (unsigned)ss.c & lm, s = b*8; ss.c += b;
		v1 = *--ss.i, v2 = *--ss.i;
		while (n >= 3*sizeof(uint32_t)) {
			*--ds.i = (v1 _U_ s) | (v2 _L_ (32-s)); v1 = *--ss.i;
			*--ds.i = (v2 _U_ s) | (v1 _L_ (32-s)); v2 = *--ss.i;
			n -= 2*sizeof(uint32_t);
		}
		/* data for one more store is already loaded */
		if (n >= sizeof(uint32_t)) {
			*--ds.i = (v1 _U_ s) | (v2 _L_ (32-s));
			n -= sizeof(uint32_t);
			ss.c -= sizeof(uint32_t);
		}
		ss.c -= b - 2*sizeof(uint32_t);
	}
	/* copy 0-7 leftover bytes */
	while (n >= 4) {
		*--ds.c = *--ss.c, n--; *--ds.c = *--ss.c, n--;
		*--ds.c = *--ss.c, n--; *--ds.c = *--ss.c, n--;
	}
	while (n > 0)
		*--ds.c = *--ss.c, n--;
	return dest;
}
