
#include "app.h"

static unsigned char OpData[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static unsigned short CPU_CALL OpRead16(unsigned int a)
{
  return (unsigned short)( (OpData[a&15]<<8) | OpData[(a+1)&15] );
}

// For opcode 'op' use handler 'use'
void OpUse(int op,int use)
{
  char text[64]="";
  CyJump[op]=use;

  if (op!=use) return;

  // Disassemble opcode
  DisaPc=0;
  DisaText=text;
  DisaWord=OpRead16;

  DisaGet();
  ot(";@ ---------- [%.4x] %s uses Op%.4x ----------\n",op,text,use);
}

void OpStart(int op)
{
  Cycles=0;
  OpUse(op,op); // This opcode obviously uses this handler
  ot("Op%.4x%s\n", op, ms?"":":");
}

void OpEnd()
{
  ot("  ldrh r8,[r4],#2 ;@ Fetch next opcode\n");
  ot("  subs r5,r5,#%d ;@ Subtract cycles\n",Cycles);
  ot("  ldrge pc,[r6,r8,asl #2] ;@ Jump to opcode handler\n");
  ot("  b CycloneEnd\n");
  ot("\n");
}

int OpBase(int op,int sepa)
{
  int ea=op&0x3f; // Get Effective Address
  if (ea<0x10) return sepa?(op&~0x7):(op&~0xf); // Use 1 handler for d0-d7 and a0-a7
  if (ea>=0x18 && ea<0x28 && (ea&7)==7) return op; // Specific handler for (a7)+ and -(a7)
  if (ea<0x38) return op&~7;   // Use 1 handler for (a0)-(a7), etc...
  return op;
}

// Get flags, trashes r2
int OpGetFlags(int subtract,int xbit,int specialz)
{
  if (specialz) ot("  orr r2,r9,#0xb0000000 ;@ for old Z\n");

  ot("  mrs r9,cpsr ;@ r9=flags\n");

  if (specialz) ot("  andeq r9,r9,r2 ;@ fix Z\n");

  if (subtract) ot("  eor r9,r9,#0x20000000 ;@ Invert carry\n");

  if (xbit)
  {
    ot("  mov r2,r9,lsr #28\n");
    ot("  strb r2,[r7,#0x45] ;@ Save X bit\n");
  }
  return 0;
}

// -----------------------------------------------------------------

void OpAny(int op)
{
  memset(OpData,0x33,sizeof(OpData));
  OpData[0]=(unsigned char)(op>>8);
  OpData[1]=(unsigned char)op;

  if ((op&0xf100)==0x0000) OpArith(op);
  if ((op&0xc000)==0x0000) OpMove(op);
  if ((op&0xf5bf)==0x003c) OpArithSr(op); // Ori/Andi/Eori $nnnn,sr
  if ((op&0xf100)==0x0100) OpBtstReg(op);
  if ((op&0xf138)==0x0108) OpMovep(op);
  if ((op&0xff00)==0x0800) OpBtstImm(op);
  if ((op&0xf900)==0x4000) OpNeg(op);
  if ((op&0xf140)==0x4100) OpChk(op);
  if ((op&0xf1c0)==0x41c0) OpLea(op);
  if ((op&0xf9c0)==0x40c0) OpMoveSr(op);
  if ((op&0xffc0)==0x4800) OpNbcd(op);
  if ((op&0xfff8)==0x4840) OpSwap(op);
  if ((op&0xffc0)==0x4840) OpPea(op);
  if ((op&0xffb8)==0x4880) OpExt(op);
  if ((op&0xfb80)==0x4880) OpMovem(op);
  if ((op&0xff00)==0x4a00) OpTst(op);
  if ((op&0xffc0)==0x4ac0) OpTas(op);
  if ((op&0xfff0)==0x4e40) OpTrap(op);
  if ((op&0xfff8)==0x4e50) OpLink(op);
  if ((op&0xfff8)==0x4e58) OpUnlk(op);
  if ((op&0xfff0)==0x4e60) OpMoveUsp(op);
  if ((op&0xfff8)==0x4e70) Op4E70(op); // Reset/Rts etc
  if ((op&0xfffd)==0x4e70) OpStopReset(op);
  if ((op&0xff80)==0x4e80) OpJsr(op);
  if ((op&0xf000)==0x5000) OpAddq(op);
  if ((op&0xf0c0)==0x50c0) OpSet(op);
  if ((op&0xf0f8)==0x50c8) OpDbra(op);
  if ((op&0xf000)==0x6000) OpBranch(op);
  if ((op&0xf100)==0x7000) OpMoveq(op);
  if ((op&0xa000)==0x8000) OpArithReg(op); // Or/Sub/And/Add
  if ((op&0xb1f0)==0x8100) OpAbcd(op);
  if ((op&0xb0c0)==0x80c0) OpMul(op);
  if ((op&0x90c0)==0x90c0) OpAritha(op);
  if ((op&0xb130)==0x9100) OpAddx(op);
  if ((op&0xf000)==0xb000) OpCmpEor(op);
  if ((op&0xf138)==0xb108) OpCmpm(op);
  if ((op&0xf130)==0xc100) OpExg(op);
  if ((op&0xf000)==0xe000) OpAsr(op); // Asr/l/Ror/l etc
  if ((op&0xf8c0)==0xe0c0) OpAsrEa(op);
}
