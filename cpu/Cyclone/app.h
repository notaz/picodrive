
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

// Disa.c
#include "Disa/Disa.h"

// Ea.cpp
extern int g_jmp_cycle_table[];
extern int g_jsr_cycle_table[];
extern int g_lea_cycle_table[];
extern int g_pea_cycle_table[];
extern int g_movem_cycle_table[];
int Ea_add_ns(int *tab, int ea); // add nonstandard EA cycles
int EaCalc(int a,int mask,int ea,int size,int top=0,int sign_extend=1); // 6
int EaRead(int a,int v,int ea,int size,int mask,int top=0,int sign_extend=1); // 7
int EaCalcRead(int r_ea,int r,int ea,int size,int mask,int sign_extend=1); // 6
int EaCalcReadNoSE(int r_ea,int r,int ea,int size,int mask);
int EaCanRead(int ea,int size);
int EaWrite(int a,int v,int ea,int size,int mask,int top=0,int sign_extend_ea=1);
int EaCanWrite(int ea);
int EaAn(int ea);

// Main.cpp
extern int *CyJump;   // Jump table
extern int  ms;       // If non-zero, output in Microsoft ARMASM format
extern char *Narm[4]; // Normal ARM Extensions for operand sizes 0,1,2
extern char *Sarm[4]; // Sign-extend ARM Extensions for operand sizes 0,1,2
extern int  Cycles;   // Current cycles for opcode
extern int  pc_dirty; // something changed PC during processing
void ot(const char *format, ...);
void ltorg();
void CheckInterrupt(int op);
int MemHandler(int type,int size,int addrreg=0);
void FlushPC(void);

// OpAny.cpp
extern int g_op;
int OpGetFlags(int subtract,int xbit,int sprecialz=0);
void OpUse(int op,int use);
void OpStart(int op,int sea=0,int tea=0);
void OpEnd(int sea=0,int tea=0);
int OpBase(int op,int size,int sepa=0);
void OpAny(int op);

//----------------------
// OpArith.cpp
int OpArith(int op);
int OpLea(int op);
int OpAddq(int op);
int OpArithReg(int op);
int OpMul(int op);
int OpAbcd(int op);
int OpNbcd(int op);
int OpAritha(int op);
int OpAddx(int op);
int OpCmpEor(int op);
int OpCmpm(int op);
int OpChk(int op);
int GetXBit(int subtract);

// OpBranch.cpp
void OpPush32();
void OpPushSr(int high);
int OpTrap(int op);
int OpLink(int op);
int OpUnlk(int op);
int Op4E70(int op);
int OpJsr(int op);
int OpBranch(int op);
int OpDbra(int op);

// OpLogic.cpp
int OpBtstReg(int op);
int OpBtstImm(int op);
int OpNeg(int op);
int OpSwap(int op);
int OpTst(int op);
int OpExt(int op);
int OpSet(int op);
int OpAsr(int op);
int OpAsrEa(int op);
int OpTas(int op, int gen_special=0);

// OpMove.cpp
int OpMove(int op);
int OpLea(int op);
void OpFlagsToReg(int high);
void OpRegToFlags(int high);
int OpMoveSr(int op);
int OpArithSr(int op);
int OpPea(int op);
int OpMovem(int op);
int OpMoveq(int op);
int OpMoveUsp(int op);
int OpExg(int op);
int OpMovep(int op);
int OpStopReset(int op);
void SuperCheck(int op);
void SuperEnd(void);
void SuperChange(int op,int load_srh=1);

