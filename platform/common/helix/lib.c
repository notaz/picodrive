#include <stdlib.h>
#include <stdint.h>

// libgcc has this with gcc 4.x
void raise(int sig)
{
}

// very limited heap functions for helix decoder

static char heap[65000] __attribute__((aligned(16)));
static long heap_offs;

void __malloc_init(void)
{
	heap_offs = 0;
}

void *malloc(size_t size)
{
        void *chunk = heap + heap_offs;
        size = (size+15) & ~15;
        if (heap_offs + size > sizeof(heap))
                return NULL;
        else {
                heap_offs += size;
                return chunk;
        }
}

void free(void *chunk)
{
	if (chunk == heap)
		heap_offs = 0;
}

#if 0
void *memcpy (void *dest, const void *src, size_t n)
{
	char       *_dest = dest;
        const char *_src = src;
	while (n--) *_dest++ = *_src++;
	return dest;
}

void *memmove (void *dest, const void *src, size_t n)
{
	char       *_dest = dest+n;
        const char *_src = src+n;
	if (dest <= src || dest >= _src)
		return memcpy(dest, src, n);
	while (n--) *--_dest = *--_src;
	return dest;
}
#else
/* memcpy/memmove in C with some simple optimizations.
 * ATTN does dirty aliasing tricks with undefined behaviour by standard.
 * (this works fine with gcc, though...)
 */
void *memcpy(void *dest, const void *src, size_t n)
{
	struct _16 { uint32_t a[4]; };
	union { const void *v; char *c; uint64_t *l; struct _16 *s; }
		ss = { src }, ds = { dest };
	const int lm = sizeof(uint32_t)-1;

	if ((((unsigned)ss.c ^ (unsigned)ds.c) & lm) == 0) {
		/* fast copy if pointers have the same aligment */
		while (((unsigned)ss.c & lm) && n > 0)	/* align to word */
			*ds.c++ = *ss.c++, n--;
		while (n >= sizeof(struct _16))	/* copy 16 bytes blocks */
			*ds.s++ = *ss.s++, n -= sizeof(struct _16);
		if (n >= sizeof(uint64_t))	/* copy leftover 8 byte block */
			*ds.l++ = *ss.l++, n -= sizeof(uint64_t);
	} else {
		/* byte copy if pointers are unaligned */
		while (n >= 8) {		/* copy 8 byte blocks */
			*ds.c++ = *ss.c++, n--; *ds.c++ = *ss.c++, n--;
			*ds.c++ = *ss.c++, n--; *ds.c++ = *ss.c++, n--;
			*ds.c++ = *ss.c++, n--; *ds.c++ = *ss.c++, n--;
			*ds.c++ = *ss.c++, n--; *ds.c++ = *ss.c++, n--;
		}
	}
	/* copy max. 8 leftover bytes */
	while (n > 0)
		*ds.c++ = *ss.c++, n--;
	return dest;
}

void *memmove (void *dest, const void *src, size_t n)
{
	struct _16 { uint32_t a[4]; };
	union { const void *v; char *c; uint64_t *l; struct _16 *s; }
		ss = { src+n }, ds = { dest+n };
	const int lm = sizeof(uint32_t)-1;

	if (dest <= src || dest >= src+n)
		return memcpy(dest, src, n);

	if ((((unsigned)ss.c ^ (unsigned)ds.c) & lm) == 0) {
		/* fast copy if pointers have the same aligment */
		while (((unsigned)ss.c & lm) && n > 0)
			*--ds.c = *--ss.c, n--;
		while (n >= sizeof(struct _16))
			*--ds.s = *--ss.s, n -= sizeof(struct _16);
		if (n >= sizeof(uint64_t))
			*--ds.l = *--ss.l, n -= sizeof(uint64_t);
	} else {
		/* byte copy if pointers are unaligned */
		while (n >= 8) {
			*--ds.c = *--ss.c, n--; *--ds.c = *--ss.c, n--;
			*--ds.c = *--ss.c, n--; *--ds.c = *--ss.c, n--;
			*--ds.c = *--ss.c, n--; *--ds.c = *--ss.c, n--;
			*--ds.c = *--ss.c, n--; *--ds.c = *--ss.c, n--;
		}
	}
	/* copy max. 8 leftover bytes */
	while (n > 0)
		*--ds.c = *--ss.c, n--;
	return dest;
}
#endif
