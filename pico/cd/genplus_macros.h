#undef uint8
#undef uint16
#undef uint32
#undef int8
#undef int16
#undef int32

#define uint8  unsigned char
#define uint16 unsigned short
#define uint32 unsigned int
#define int8  signed char
#define int16 signed short
#define int32 signed int

typedef union
{
    uint16 w;
    struct
    {
#if 1
        uint8 l;
        uint8 h;
#else
        uint8 h;
        uint8 l;
#endif
    } byte;

} reg16_t;

#define READ_BYTE(BASE, ADDR) (BASE)[(ADDR)^1]
#define WRITE_BYTE(BASE, ADDR, VAL) (BASE)[(ADDR)^1] = (VAL)

#define load_param(param, size) \
  memcpy(param, &state[bufferptr], size); \
  bufferptr += size;
  
#define save_param(param, size) \
  memcpy(&state[bufferptr], param, size); \
  bufferptr += size;
