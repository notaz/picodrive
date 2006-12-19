
// Dave's Disa 68000 Disassembler

#ifdef __cplusplus
extern "C" {
#endif

#if defined(ARM) || defined(GP32) || !defined (__WINS__)
#define CPU_CALL
#else
#define CPU_CALL __fastcall
#endif

extern unsigned int DisaPc;
extern char *DisaText; // Text buffer to write in

extern unsigned short (CPU_CALL *DisaWord)(unsigned int a);
int DisaGetEa(char *t,int ea,int size);

int DisaGet();

#ifdef __cplusplus
} // End of extern "C"
#endif
