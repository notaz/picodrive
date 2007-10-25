
// Dave's Disa 68000 Disassembler

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int DisaPc;
extern char *DisaText; // Text buffer to write in

extern unsigned short (*DisaWord)(unsigned int a);
int DisaGetEa(char *t,int ea,int size);

int DisaGet();

#ifdef __cplusplus
} // End of extern "C"
#endif
