// memory map related stuff

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define M68K_MEM_SHIFT 16

extern unsigned long m68k_read8_map  [0x1000000 >> M68K_MEM_SHIFT];
extern unsigned long m68k_read16_map [0x1000000 >> M68K_MEM_SHIFT];
extern unsigned long m68k_write8_map [0x1000000 >> M68K_MEM_SHIFT];
extern unsigned long m68k_write16_map[0x1000000 >> M68K_MEM_SHIFT];

extern unsigned long s68k_read8_map  [0x1000000 >> M68K_MEM_SHIFT];
extern unsigned long s68k_read16_map [0x1000000 >> M68K_MEM_SHIFT];
extern unsigned long s68k_write8_map [0x1000000 >> M68K_MEM_SHIFT];
extern unsigned long s68k_write16_map[0x1000000 >> M68K_MEM_SHIFT];

void z80_map_set(unsigned long *map, int start_addr, int end_addr,
    void *func_or_mh, int is_func);
void cpu68k_map_set(unsigned long *map, int start_addr, int end_addr,
    void *func_or_mh, int is_func);
void cpu68k_map_all_ram(int start_addr, int end_addr, void *ptr, int is_sub);
void m68k_map_unmap(int start_addr, int end_addr);

// top-level handlers that cores can use
// (or alternatively build them into themselves)
// XXX: unhandled: *16 and *32 might cross the bank boundaries
typedef u32  (cpu68k_read_f)(u32 a);
typedef void (cpu68k_write_f)(u32 a, u32 d);

#define MAKE_68K_READ8(name, map)               \
u32 name(u32 a)                                 \
{                                               \
  unsigned long v;                              \
  a &= 0x00ffffff;                              \
  v = map[a >> M68K_MEM_SHIFT];                 \
  if (v & 0x80000000)                           \
    return ((cpu68k_read_f *)(v << 1))(a);      \
  else                                          \
    return *(u8 *)((v << 1) + (a ^ 1));         \
}

#define MAKE_68K_READ16(name, map)              \
u32 name(u32 a)                                 \
{                                               \
  unsigned long v;                              \
  a &= 0x00fffffe;                              \
  v = map[a >> M68K_MEM_SHIFT];                 \
  if (v & 0x80000000)                           \
    return ((cpu68k_read_f *)(v << 1))(a);      \
  else                                          \
    return *(u16 *)((v << 1) + a);              \
}

#define MAKE_68K_READ32(name, map)              \
u32 name(u32 a)                                 \
{                                               \
  unsigned long v, vs;                          \
  u32 d;                                        \
  a &= 0x00fffffe;                              \
  v = map[a >> M68K_MEM_SHIFT];                 \
  vs = v << 1;                                  \
  if (v & 0x80000000) {                         \
    d  = ((cpu68k_read_f *)vs)(a) << 16;        \
    d |= ((cpu68k_read_f *)vs)(a + 2);          \
  }                                             \
  else {                                        \
    u16 *m = (u16 *)(vs + a);                   \
    d = (m[0] << 16) | m[1];                    \
  }                                             \
  return d;                                     \
}

#define MAKE_68K_WRITE8(name, map)              \
void name(u32 a, u8 d)                          \
{                                               \
  unsigned long v;                              \
  a &= 0x00ffffff;                              \
  v = map[a >> M68K_MEM_SHIFT];                 \
  if (v & 0x80000000)                           \
    ((cpu68k_write_f *)(v << 1))(a, d);         \
  else                                          \
    *(u8 *)((v << 1) + (a ^ 1)) = d;            \
}

#define MAKE_68K_WRITE16(name, map)             \
void name(u32 a, u16 d)                         \
{                                               \
  unsigned long v;                              \
  a &= 0x00fffffe;                              \
  v = map[a >> M68K_MEM_SHIFT];                 \
  if (v & 0x80000000)                           \
    ((cpu68k_write_f *)(v << 1))(a, d);         \
  else                                          \
    *(u16 *)((v << 1) + a) = d;                 \
}

#define MAKE_68K_WRITE32(name, map)             \
void name(u32 a, u32 d)                         \
{                                               \
  unsigned long v, vs;                          \
  a &= 0x00fffffe;                              \
  v = map[a >> M68K_MEM_SHIFT];                 \
  vs = v << 1;                                  \
  if (v & 0x80000000) {                         \
    ((cpu68k_write_f *)vs)(a, d >> 16);         \
    ((cpu68k_write_f *)vs)(a + 2, d);           \
  }                                             \
  else {                                        \
    u16 *m = (u16 *)(vs + a);                   \
    m[0] = d >> 16;                             \
    m[1] = d;                                   \
  }                                             \
}

