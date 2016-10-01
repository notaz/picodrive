typedef unsigned char  u8;
typedef signed char    s8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define DRC_TCACHE_SIZE         (2*1024*1024)

#ifdef VITA
extern u8 *tcache;
#else
extern u8 tcache[DRC_TCACHE_SIZE];
#endif

void drc_cmn_init(void);
void drc_cmn_cleanup(void);

