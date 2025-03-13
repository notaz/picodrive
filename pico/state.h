#include <stdlib.h>
#include <assert.h>
#include "pico_types.h"

typedef size_t (arearw)(void *p, size_t _size, size_t _n, void *file);
typedef size_t (areaeof)(void *file);
typedef int    (areaseek)(void *file, long offset, int whence);
typedef int    (areaclose)(void *file);

int PicoStateFP(void *afile, int is_save,
  arearw *read, arearw *write, areaeof *eof, areaseek *seek);

static inline void save_u8_(u8 *buf, size_t *b, u32 u)
{
	assert(!(u & ~0xff));
	buf[(*b)++] = u;
}

static inline void save_s8_(u8 *buf, size_t *b, s32 s)
{
	s32 s_sext = (s32)((u32)s << 24) >> 24;
	assert(s == s_sext); (void)s_sext;
	buf[(*b)++] = s;
}

static inline void save_u16(u8 *buf, size_t *b, u32 u)
{
	assert(!(u & ~0xffff));
	buf[(*b)++] = u;
	buf[(*b)++] = u >> 8;
}

static inline void save_s16(u8 *buf, size_t *b, s32 s)
{
	s32 s_sext = (s32)((u32)s << 16) >> 16;
	assert(s == s_sext); (void)s_sext;
	buf[(*b)++] = s;
	buf[(*b)++] = s >> 8;
}

static inline void save_u32(u8 *buf, size_t *b, u32 u)
{
	buf[(*b)++] = u;
	buf[(*b)++] = u >> 8;
	buf[(*b)++] = u >> 16;
	buf[(*b)++] = u >> 24;
}

static inline void save_s32(u8 *buf, size_t *b, s32 s)
{
	buf[(*b)++] = s;
	buf[(*b)++] = s >> 8;
	buf[(*b)++] = s >> 16;
	buf[(*b)++] = s >> 24;
}

static inline u8 load_u8_(const u8 *buf, size_t *b)
{
	return buf[(*b)++];
}

static inline s8 load_s8_(const u8 *buf, size_t *b)
{
	return buf[(*b)++];
}

static inline u16 load_u16(const u8 *buf, size_t *b)
{
	u16 r = (buf[*b + 1] << 8) | buf[*b];
	(*b) += 2;
	return r;
}

static inline s16 load_s16(const u8 *buf, size_t *b)
{
	s16 r = (buf[*b + 1] << 8) | buf[*b];
	(*b) += 2;
	return r;
}

static inline u32 load_u32(const u8 *buf, size_t *b)
{
	u32 r = (buf[*b + 3] << 24) | (buf[*b + 2] << 16) | (buf[*b + 1] << 8) | buf[*b];
	(*b) += 4;
	return r;
}

static inline s32 load_s32(const u8 *buf, size_t *b)
{
	s32 r = (buf[*b + 3] << 24) | (buf[*b + 2] << 16) | (buf[*b + 1] << 8) | buf[*b];
	(*b) += 4;
	return r;
}
