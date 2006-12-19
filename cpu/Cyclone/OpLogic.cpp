#include "app.h"

// --------------------- Opcodes 0x0100+ ---------------------
// Emit a Btst (Register) opcode 0000nnn1 ttaaaaaa
int OpBtstReg(int op)
{
  int use=0;
  int type=0,sea=0,tea=0;
  int size=0;

  type=(op>>6)&3; // Btst/Bchg/Bclr/Bset
  // Get source and target EA
  sea=(op>>9)&7;
  tea=op&0x003f;
  if (tea<0x10) size=2; // For registers, 32-bits

  if ((tea&0x38)==0x08) return 1; // movep

  // See if we can do this opcode:
  if (EaCanRead(tea,0)==0) return 1;
  if (type>0)
  {
    if (EaCanWrite(tea)==0) return 1;
  }

  use=OpBase(op);
  use&=~0x0e00; // Use same handler for all registers
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);

  if(type==1||type==3) {
    Cycles=8;
  } else {
    Cycles=type?8:4;
    if(size>=2) Cycles+=2;
  }

  EaCalc (0,0x0e00,sea,0);
  EaRead (0,     0,sea,0,0x0e00);
  if (tea>=0x10)
       ot("  and r10,r0,#7  ;@ mem - do mod 8\n");
  else ot("  and r10,r0,#31 ;@ reg - do mod 32\n");
  ot("\n");

  EaCalc(11,0x003f,tea,size);
  EaRead(11,     0,tea,size,0x003f);
  ot("  mov r1,#1\n");
  ot("  tst r0,r1,lsl r10 ;@ Do arithmetic\n");
  ot("  bicne r9,r9,#0x40000000\n");
  ot("  orreq r9,r9,#0x40000000 ;@ Get Z flag\n");
  ot("\n");

  if (type>0)
  {
    if (type==1) ot("  eor r1,r0,r1,lsl r10 ;@ Toggle bit\n");
    if (type==2) ot("  bic r1,r0,r1,lsl r10 ;@ Clear bit\n");
    if (type==3) ot("  orr r1,r0,r1,lsl r10 ;@ Set bit\n");
    ot("\n");
    EaWrite(11,   1,tea,size,0x003f);
  }
  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x0800+ ---------------------
// Emit a Btst/Bchg/Bclr/Bset (Immediate) opcode 00001000 ttaaaaaa nn
int OpBtstImm(int op)
{
  int type=0,sea=0,tea=0;
  int use=0;
  int size=0;

  type=(op>>6)&3;
  // Get source and target EA
  sea=   0x003c;
  tea=op&0x003f;
  if (tea<0x10) size=2; // For registers, 32-bits

  // See if we can do this opcode:
  if (EaCanRead(tea,0)==0||EaAn(tea)||tea==0x3c) return 1;
  if (type>0)
  {
    if (EaCanWrite(tea)==0) return 1;
  }

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);

  ot("  mov r10,#1\n");
  ot("\n");
  EaCalc ( 0,0x0000,sea,0);
  EaRead ( 0,     0,sea,0,0);
  ot("  bic r9,r9,#0x40000000 ;@ Blank Z flag\n");
  if (tea>=0x10)
       ot("  and r0,r0,#7 ;@ mem - do mod 8\n");
  else ot("  and r0,r0,#0x1F ;@ reg - do mod 32\n");
  ot("  mov r10,r10,lsl r0 ;@ Make bit mask\n");
  ot("\n");

  if(type==1||type==3) {
    Cycles=12;
  } else {
    Cycles=type?12:8;
    if(size>=2) Cycles+=2;
  }

  EaCalc (11,0x003f,tea,size);
  EaRead (11,     0,tea,size,0x003f);
  ot("  tst r0,r10 ;@ Do arithmetic\n");
  ot("  orreq r9,r9,#0x40000000 ;@ Get Z flag\n");
  ot("\n");

  if (type>0)
  {
    if (type==1) ot("  eor r1,r0,r10 ;@ Toggle bit\n");
    if (type==2) ot("  bic r1,r0,r10 ;@ Clear bit\n");
    if (type==3) ot("  orr r1,r0,r10 ;@ Set bit\n");
    ot("\n");
    EaWrite(11,   1,tea,size,0x003f);
  }

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4000+ ---------------------
int OpNeg(int op)
{
  // 01000tt0 xxeeeeee (tt=negx/clr/neg/not, xx=size, eeeeee=EA)
  int type=0,size=0,ea=0,use=0;

  type=(op>>9)&3;
  ea  =op&0x003f;
  size=(op>>6)&3; if (size>=3) return 1;

  // See if we can do this opcode:
  if (EaCanRead (ea,size)==0||EaAn(ea)) return 1;
  if (EaCanWrite(ea     )==0) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);   Cycles=size<2?4:6;
  if(ea >= 0x10) {
    Cycles*=2;
#ifdef CYCLONE_FOR_GENESIS
    // This is same as in Starscream core, CLR uses only 6 cycles for memory EAs.
    // May be this is similar case as with TAS opcode, but this time the dummy
    // read is ignored somehow? Without this hack Fatal Rewind hangs even in Gens.
    if(type==1&&size<2) Cycles-=2;
#endif
  }

  EaCalc (10,0x003f,ea,size);

  if (type!=1) EaRead (10,0,ea,size,0x003f); // Don't need to read for 'clr'
  if (type==1) ot("\n");

  if (type==0)
  {
    ot(";@ Negx:\n");
    GetXBit(1);
    if(size!=2) ot("  mov r0,r0,lsl #%i\n",size?16:24);
    ot("  rscs r1,r0,#0 ;@ do arithmetic\n");
    ot("  orr r3,r9,#0xb0000000 ;@ for old Z\n");
    OpGetFlags(1,1,0);
    if(size!=2) {
	  ot("  movs r1,r1,asr #%i\n",size?16:24);
      ot("  orreq r9,r9,#0x40000000 ;@ possily missed Z\n");
	}
	ot("  andeq r9,r9,r3 ;@ fix Z\n");
    ot("\n");
  }

  if (type==1)
  {
    ot(";@ Clear:\n");
    ot("  mov r1,#0\n");
    ot("  mov r9,#0x40000000 ;@ NZCV=0100\n");
    ot("\n");
  }

  if (type==2)
  {
    ot(";@ Neg:\n");
    if(size!=2) ot("  mov r0,r0,lsl #%i\n",size?16:24);
    ot("  rsbs r1,r0,#0\n");
    OpGetFlags(1,1);
    if(size!=2) ot("  mov r1,r1,asr #%i\n",size?16:24);
    ot("\n");
  }

  if (type==3)
  {
    ot(";@ Not:\n");
    ot("  mvn r1,r0\n");
    ot("  adds r1,r1,#0 ;@ Defines NZ, clears CV\n");
    OpGetFlags(0,0);
    ot("\n");
  }

  EaWrite(10,     1,ea,size,0x003f);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4840+ ---------------------
// Swap, 01001000 01000nnn swap Dn
int OpSwap(int op)
{
  int ea=0,use=0;

  ea=op&7;
  use=op&~0x0007; // Use same opcode for all An

  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalc (10,0x0007,ea,2,1);
  EaRead (10,     0,ea,2,0x0007,1);

  ot("  mov r1,r0,ror #16\n");
  ot("  adds r1,r1,#0 ;@ Defines NZ, clears CV\n");
  OpGetFlags(0,0);

  EaWrite(10,     1,8,2,0x0007,1);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4a00+ ---------------------
// Emit a Tst opcode, 01001010 xxeeeeee
int OpTst(int op)
{
  int sea=0;
  int size=0,use=0;

  sea=op&0x003f;
  size=(op>>6)&3; if (size>=3) return 1;

  // See if we can do this opcode:
  if (EaCanWrite(sea)==0||EaAn(sea)) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalc ( 0,0x003f,sea,size,1);
  EaRead ( 0,     0,sea,size,0x003f,1);

  ot("  adds r0,r0,#0 ;@ Defines NZ, clears CV\n");
  ot("  mrs r9,cpsr ;@ r9=flags\n");
  ot("\n");

  OpEnd();
  return 0;
}

// --------------------- Opcodes 0x4880+ ---------------------
// Emit an Ext opcode, 01001000 1x000nnn
int OpExt(int op)
{
  int ea=0;
  int size=0,use=0;
  int shift=0;

  ea=op&0x0007;
  size=(op>>6)&1;
  shift=32-(8<<size);

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalc (10,0x0007,ea,size+1);
  EaRead (10,     0,ea,size+1,0x0007);

  ot("  mov r0,r0,asl #%d\n",shift);
  ot("  adds r0,r0,#0 ;@ Defines NZ, clears CV\n");
  ot("  mrs r9,cpsr ;@ r9=flags\n");
  ot("  mov r1,r0,asr #%d\n",shift);
  ot("\n");

  EaWrite(10,     1,ea,size+1,0x0007);

  OpEnd();
  return 0;
}

// --------------------- Opcodes 0x50c0+ ---------------------
// Emit a Set cc opcode, 0101cccc 11eeeeee
int OpSet(int op)
{
  int cc=0,ea=0;
  int size=0,use=0;
  char *cond[16]=
  {
    "al","", "hi","ls","cc","cs","ne","eq",
    "vc","vs","pl","mi","ge","lt","gt","le"
  };

  cc=(op>>8)&15;
  ea=op&0x003f;

  if ((ea&0x38)==0x08) return 1; // dbra, not scc
  
  // See if we can do this opcode:
  if (EaCanWrite(ea)==0) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=8;
  if (ea<8) Cycles=4;

  ot("  mov r1,#0\n");

  if (cc!=1)
  {
    ot(";@ Is the condition true?\n");
    if ((cc&~1)==2) ot("  eor r9,r9,#0x20000000 ;@ Invert carry for hi/ls\n");
    ot("  msr cpsr_flg,r9 ;@ ARM flags = 68000 flags\n");
    if ((cc&~1)==2) ot("  eor r9,r9,#0x20000000 ;@ Invert carry for hi/ls\n");
    ot("  mvn%s r1,r1\n",cond[cc]);
  }

  if (cc!=1 && ea<8) ot("  sub%s r5,r5,#2 ;@ Extra cycles\n",cond[cc]);
  ot("\n");

  EaCalc (0,0x003f, ea,size);
  EaWrite(0,     1, ea,size,0x003f);

  OpEnd();
  return 0;
}

// Emit a Asr/Lsr/Roxr/Ror opcode
static int EmitAsr(int op,int type,int dir,int count,int size,int usereg)
{
  char pct[8]=""; // count
  int shift=32-(8<<size);

  if (count>=1) sprintf(pct,"#%d",count); // Fixed count

  if (usereg)
  {
    ot(";@ Use Dn for count:\n");
    ot("  and r2,r8,#7<<9\n");
    ot("  ldr r2,[r7,r2,lsr #7]\n");
    ot("  and r2,r2,#63\n");
    ot("\n");
    strcpy(pct,"r2");
  }
  else if (count<0)
  {
    ot("  mov r2,r8,lsr #9 ;@ Get 'n'\n");
    ot("  and r2,r2,#7\n\n"); strcpy(pct,"r2");
  }

  // Take 2*n cycles:
  if (count<0) ot("  sub r5,r5,r2,asl #1 ;@ Take 2*n cycles\n\n");
  else Cycles+=count<<1;

  if (type<2)
  {
    // Asr/Lsr
    if (dir==0 && size<2)
    {
      ot(";@ For shift right, use loworder bits for the operation:\n");
      ot("  mov r0,r0,%s #%d\n",type?"lsr":"asr",32-(8<<size));
      ot("\n");
    }

    if (type==0 && dir) ot("  mov r3,r0 ;@ save old value for V flag calculation\n");

    ot(";@ Shift register:\n");
    if (type==0) ot("  movs r0,r0,%s %s\n",dir?"asl":"asr",pct);
    if (type==1) ot("  movs r0,r0,%s %s\n",dir?"lsl":"lsr",pct);

    if (dir==0 && size<2)
    {
      ot(";@ restore after right shift:\n");
      ot("  mov r0,r0,lsl #%d\n",32-(8<<size));
      ot("\n");
    }

    OpGetFlags(0,0);
    if (usereg) { // store X only if count is not 0
      ot("  cmp %s,#0 ;@ shifting by 0?\n",pct);
      ot("  biceq r9,r9,#0x20000000 ;@ if so, clear carry\n");
      ot("  movne r1,r9,lsr #28\n");
      ot("  strneb r1,[r7,#0x45] ;@ else Save X bit\n");
    } else {
      // count will never be 0 if we use immediate
      ot("  mov r1,r9,lsr #28\n");
      ot("  strb r1,[r7,#0x45] ;@ Save X bit\n");
    }

    if (type==0 && dir) {
      ot(";@ calculate V flag (set if sign bit changes at anytime):\n");
      ot("  mov r1,#0x80000000\n");
      ot("  ands r3,r3,r1,asr %s\n", pct);
      ot("  cmpne r3,r1,asr %s\n", pct);
      ot("  biceq r9,r9,#0x10000000\n");
      ot("  orrne r9,r9,#0x10000000\n");
    }

    ot("\n");
  }

  // --------------------------------------
  if (type==2)
  {
    int wide=8<<size;

    // Roxr
    if(count == 1) {
      if(dir==0) {
        if(size!=2) {
		  ot("  orr r0,r0,r0,lsr #%i\n", size?16:24);
		  ot("  bic r0,r0,#0x%x\n", 1<<(32-wide));
		}
        GetXBit(0);
        ot("  movs r0,r0,rrx\n");
        OpGetFlags(0,1);
      } else {
        ot("  ldrb r3,[r7,#0x45]\n");
        ot("  movs r0,r0,lsl #1\n");
        OpGetFlags(0,1);
        ot("  tst r3,#2\n");
        ot("  orrne r0,r0,#0x%x\n", 1<<(32-wide));
        ot("  bicne r9,r9,#0x40000000 ;@ clear Z in case it got there\n");
      }
      ot("  bic r9,r9,#0x10000000 ;@ make suve V is clear\n");
      return 0;
    }

    if (usereg)
    {
      ot(";@ Reduce r2 until <0:\n");
      ot("Reduce_%.4x%s\n",op,ms?"":":");
      ot("  subs r2,r2,#%d\n",wide+1);
      ot("  bpl Reduce_%.4x\n",op);
      ot("  adds r2,r2,#%d ;@ Now r2=0-%d\n",wide+1,wide);
      ot("  beq norotx%.4x\n",op);
      ot("\n");
    }

    if (usereg||count < 0)
    {
      if (dir) ot("  rsb r2,r2,#%d ;@ Reverse direction\n",wide+1);
    }
    else
    {
      if (dir) ot("  mov r2,#%d ;@ Reversed\n",wide+1-count);
      else     ot("  mov r2,#%d\n",count);
    }

    if (shift) ot("  mov r0,r0,lsr #%d ;@ Shift down\n",shift);

    ot(";@ Rotate bits:\n");
    ot("  mov r3,r0,lsr r2 ;@ Get right part\n");
    ot("  rsbs r2,r2,#%d ;@ should also clear ARM V\n",wide+1);
    ot("  movs r0,r0,lsl r2 ;@ Get left part\n");
    ot("  orr r0,r3,r0 ;@ r0=Rotated value\n");

    ot(";@ Insert X bit into r2-1:\n");
    ot("  ldrb r3,[r7,#0x45]\n");
    ot("  sub r2,r2,#1\n");
    ot("  and r3,r3,#2\n");
    ot("  mov r3,r3,lsr #1\n");
    ot("  orr r0,r0,r3,lsl r2\n");
    ot("\n");

    if (shift) ot("  movs r0,r0,lsl #%d ;@ Shift up and get correct NC flags\n",shift);
    OpGetFlags(0,!usereg);
    if (!shift) {
      ot("  tst r0,r0\n");
      ot("  bicne r9,r9,#0x40000000 ;@ make sure we didn't mess Z\n");
    }
    if (usereg) { // store X only if count is not 0
      ot("  mov r2,r9,lsr #28\n");
      ot("  strb r2,[r7,#0x45] ;@ if not 0, Save X bit\n");
      ot("  b nozerox%.4x\n",op);
      ot("norotx%.4x%s\n",op,ms?"":":");
      ot("  ldrb r2,[r7,#0x45]\n");
      ot("  adds r0,r0,#0 ;@ Defines NZ, clears CV\n");
      OpGetFlags(0,0);
      ot("  and r2,r2,#2\n");
      ot("  orr r9,r9,r2,lsl #28 ;@ C = old_X\n");
      ot("nozerox%.4x%s\n",op,ms?"":":");
    }

    ot("\n");
  }

  // --------------------------------------
  if (type==3)
  {
    // Ror
    if (size<2)
    {
      ot(";@ Mirror value in whole 32 bits:\n");
      if (size<=0) ot("  orr r0,r0,r0,lsr #8\n");
      if (size<=1) ot("  orr r0,r0,r0,lsr #16\n");
      ot("\n");
    }

    ot(";@ Rotate register:\n");
    if (count<0)
    {
      if (dir) ot("  rsbs %s,%s,#32\n",pct,pct);
      ot("  movs r0,r0,ror %s\n",pct);
    }
    else
    {
      int ror=count;
      if (dir) ror=32-ror;
      if (ror&31) ot("  movs r0,r0,ror #%d\n",ror);
    }

    OpGetFlags(0,0);
    if (!dir) ot("  bic r9,r9,#0x10000000 ;@ make suve V is clear\n");
    if (dir)
    {
      ot(";@ Get carry bit from bit 0:\n");
      if (usereg)
      {
        ot("  cmp %s,#32 ;@ rotating by 0?\n",pct);
        ot("  tstne r0,#1 ;@ no, check bit 0\n");
      }
      else
        ot("  tst r0,#1\n");
      ot("  orrne r9,r9,#0x20000000\n");
      ot("  biceq r9,r9,#0x20000000\n");
    }
    else if (usereg)
    {
      // if we rotate something by 0, ARM doesn't clear C
      // so we need to detect that
      ot("  cmp %s,#0\n",pct);
      ot("  biceq r9,r9,#0x20000000\n");
    }
    ot("\n");

  }
  // --------------------------------------
  
  return 0;
}

// Emit a Asr/Lsr/Roxr/Ror opcode - 1110cccd xxuttnnn
// (ccc=count, d=direction(r,l) xx=size extension, u=use reg for count, tt=type, nnn=register Dn)
int OpAsr(int op)
{
  int ea=0,use=0;
  int count=0,dir=0;
  int size=0,usereg=0,type=0;

  ea=0;
  count =(op>>9)&7;
  dir   =(op>>8)&1;
  size  =(op>>6)&3;
  if (size>=3) return 1; // use OpAsrEa()
  usereg=(op>>5)&1;
  type  =(op>>3)&3;

  if (usereg==0) count=((count-1)&7)+1; // because ccc=000 means 8

  // Use the same opcode for target registers:
  use=op&~0x0007;

  // As long as count is not 8, use the same opcode for all shift counts::
  if (usereg==0 && count!=8 && !(count==1&&type==2)) { use|=0x0e00; count=-1; }
  if (usereg) { use&=~0x0e00; count=-1; } // Use same opcode for all Dn

  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=size<2?6:8;

  EaCalc(10,0x0007, ea,size,1);
  EaRead(10,     0, ea,size,0x0007,1);

  EmitAsr(op,type,dir,count, size,usereg);

  EaWrite(10,    0, ea,size,0x0007,1);

  OpEnd();

  return 0;
}

// Asr/Lsr/Roxr/Ror etc EA - 11100ttd 11eeeeee 
int OpAsrEa(int op)
{
  int use=0,type=0,dir=0,ea=0,size=1;

  type=(op>>9)&3;
  dir =(op>>8)&1;
  ea  = op&0x3f;

  if (ea<0x10) return 1;
  // See if we can do this opcode:
  if (EaCanRead(ea,0)==0) return 1;
  if (EaCanWrite(ea)==0) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=6; // EmitAsr() will add 2

  EaCalc (10,0x003f,ea,size,1);
  EaRead (10,     0,ea,size,0x003f,1);

  EmitAsr(op,type,dir,1,size,0);

  EaWrite(10,     0,ea,size,0x003f,1);

  OpEnd();
  return 0;
}

int OpTas(int op)
{
  int ea=0;
  int use=0;

  ea=op&0x003f;

  // See if we can do this opcode:
  if (EaCanWrite(ea)==0 || EaAn(ea)) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;
  if(ea>=8) Cycles+=10;

  EaCalc (10,0x003f,ea,0,1);
  EaRead (10,     1,ea,0,0x003f,1);

  ot("  adds r1,r1,#0 ;@ Defines NZ, clears CV\n");
  OpGetFlags(0,0);
  ot("\n");

#if CYCLONE_FOR_GENESIS
  // the original Sega hardware ignores write-back phase (to memory only)
  if (ea < 0x10) {
#endif
    ot("  orr r1,r1,#0x80000000 ;@ set bit7\n");

    EaWrite(10,     1,ea,0,0x003f,1);
#if CYCLONE_FOR_GENESIS
  }
#endif

  OpEnd();
  return 0;
}

