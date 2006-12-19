
#include "app.h"

#if USE_CHECKPC_CALLBACK
static void CheckPc()
{
  ot(";@ Check Memory Base+pc (r4)\n");
  ot("  add lr,pc,#4\n");
  ot("  mov r0,r4\n");
  ot("  ldr pc,[r7,#0x64] ;@ Call checkpc()\n");
  ot("  mov r4,r0\n");
  ot("\n");
}
#endif

// Push 32-bit value in r1 - trashes r0-r3,r12,lr
void OpPush32()
{
  ot(";@ Push r1 onto stack\n");
  ot("  ldr r0,[r7,#0x3c]\n");
  ot("  sub r0,r0,#4 ;@ Predecrement A7\n");
  ot("  str r0,[r7,#0x3c] ;@ Save A7\n");
  MemHandler(1,2);
  ot("\n");
}

// Push SR - trashes r0-r3,r12,lr
void OpPushSr(int high)
{
  ot(";@ Push SR:\n");
  OpFlagsToReg(high);
  ot("  ldr r0,[r7,#0x3c]\n");
  ot("  sub r0,r0,#2 ;@ Predecrement A7\n");
  ot("  str r0,[r7,#0x3c] ;@ Save A7\n");
  MemHandler(1,1);
  ot("\n");
}

// Pop SR - trashes r0-r3
static void PopSr(int high)
{
  ot(";@ Pop SR:\n");
  ot("  ldr r0,[r7,#0x3c]\n");
  ot("  add r1,r0,#2 ;@ Postincrement A7\n");
  ot("  str r1,[r7,#0x3c] ;@ Save A7\n");
  MemHandler(0,1);
  ot("\n");
  OpRegToFlags(high);
}

// Pop PC - assumes r10=Memory Base - trashes r0-r3
static void PopPc()
{
  ot(";@ Pop PC:\n");
  ot("  ldr r0,[r7,#0x3c]\n");
  ot("  add r1,r0,#4 ;@ Postincrement A7\n");
  ot("  str r1,[r7,#0x3c] ;@ Save A7\n");
  MemHandler(0,2);
  ot("  add r4,r0,r10 ;@ r4=Memory Base+PC\n");
  ot("\n");
  CheckPc();
}

int OpTrap(int op)
{
  int use=0;

  use=op&~0xf;
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);
  ot("  and r0,r8,#0xf ;@ Get trap number\n");
  ot("  orr r0,r0,#0x20\n");
  ot("  mov r0,r0,asl #2\n");
  ot("  bl Exception\n");
  ot("\n");

  Cycles=38; OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4e50+ ---------------------
int OpLink(int op)
{
  int use=0,reg;

  use=op&~7;
  reg=op&7;
  if (reg==7) use=op;
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);

  if(reg!=7) {
    ot(";@ Get An\n");
    EaCalc(10, 7, 8, 2, 1);
    EaRead(10, 1, 8, 2, 7, 1);
  }

  ot("  ldr r0,[r7,#0x3c] ;@ Get A7\n");
  ot("  sub r0,r0,#4 ;@ A7-=4\n");
  ot("  mov r11,r0\n");
  if(reg==7) ot("  mov r1,r0\n");
  ot("\n");
  
  ot(";@ Write An to Stack\n");
  MemHandler(1,2);

  ot(";@ Save to An\n");
  if(reg!=7)
    EaWrite(10,11, 8, 2, 7, 1);

  ot(";@ Get offset:\n");
  EaCalc(0,0,0x3c,1);
  EaRead(0,0,0x3c,1,0);

  ot("  add r11,r11,r0 ;@ Add offset to A7\n");
  ot("  str r11,[r7,#0x3c]\n");
  ot("\n");

  Cycles=16;
  OpEnd();
  return 0;
}

// --------------------- Opcodes 0x4e58+ ---------------------
int OpUnlk(int op)
{
  int use=0;

  use=op&~7;
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);

  ot(";@ Get An\n");
  EaCalc(10, 7, 8, 2, 1);
  EaRead(10, 0, 8, 2, 7, 1);

  ot("  add r11,r0,#4 ;@ A7+=4\n");
  ot("\n");
  ot(";@ Pop An from stack:\n");
  MemHandler(0,2);
  ot("\n");
  ot("  str r11,[r7,#0x3c] ;@ Save A7\n");
  ot("\n");
  ot(";@ An = value from stack:\n");
  EaWrite(10, 0, 8, 2, 7, 1);
  
  Cycles=12;
  OpEnd();
  return 0;
}

// --------------------- Opcodes 0x4e70+ ---------------------
int Op4E70(int op)
{
  int type=0;

  type=op&7; // 01001110 01110ttt, reset/nop/stop/rte/rtd/rts/trapv/rtr

  switch (type)
  {
    case 1:  // nop
    OpStart(op);
    Cycles=4;
    OpEnd();
    return 0;

	case 3: // rte
    OpStart(op); Cycles=20;
	SuperCheck(op);
    PopSr(1);
    ot("  ldr r10,[r7,#0x60] ;@ Get Memory base\n");
    PopPc();
	SuperChange(op);
    CheckInterrupt(op);
    OpEnd();
	SuperEnd(op);
    return 0;

    case 5: // rts
    OpStart(op); Cycles=16;
    ot("  ldr r10,[r7,#0x60] ;@ Get Memory base\n");
    PopPc();
    OpEnd();
    return 0;

    case 6: // trapv
    OpStart(op); Cycles=4;
    ot("  tst r9,#0x10000000\n");
    ot("  subne r5,r5,#%i\n",30);
    ot("  movne r0,#0x1c ;@ TRAPV exception\n");
    ot("  blne Exception\n");
    OpEnd();
    return 0;

    case 7: // rtr
    OpStart(op); Cycles=20;
    PopSr(0);
    ot("  ldr r10,[r7,#0x60] ;@ Get Memory base\n");
    PopPc();
    OpEnd();
    return 0;

    default:
    return 1;
  }
}

// --------------------- Opcodes 0x4e80+ ---------------------
// Emit a Jsr/Jmp opcode, 01001110 1meeeeee
int OpJsr(int op)
{
  int use=0;
  int sea=0;

  sea=op&0x003f;

  // See if we can do this opcode:
  if (EaCanRead(sea,-1)==0) return 1;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);

  ot("  ldr r10,[r7,#0x60] ;@ Get Memory base\n");
  ot("\n");
  EaCalc(0,0x003f,sea,0);

  ot(";@ Jump - Get new PC from r0\n");
  if (op&0x40)
  {
    // Jmp - Get new PC from r0
    ot("  add r4,r0,r10 ;@ r4 = Memory Base + New PC\n");
    ot("\n");
  }
  else
  {
    ot(";@ Jsr - Push old PC first\n");
    ot("  sub r1,r4,r10 ;@ r1 = Old PC\n");
    ot("  add r4,r0,r10 ;@ r4 = Memory Base + New PC\n");
    ot("  mov r1,r1,lsl #8\n");
    ot("  ldr r0,[r7,#0x3c]\n");
    ot("  mov r1,r1,asr #8\n");
    ot(";@ Push r1 onto stack\n");
    ot("  sub r0,r0,#4 ;@ Predecrement A7\n");
    ot("  str r0,[r7,#0x3c] ;@ Save A7\n");
    MemHandler(1,2);
    ot("\n");
  }

#if USE_CHECKPC_CALLBACK
  CheckPc();
#endif

  Cycles=(op&0x40) ? 4 : 12;
  Cycles+=Ea_add_ns((op&0x40) ? g_jmp_cycle_table : g_jsr_cycle_table, sea);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x50c8+ ---------------------

// ARM version of 68000 condition codes:
static char *Cond[16]=
{
  "",  "",  "hi","ls","cc","cs","ne","eq",
  "vc","vs","pl","mi","ge","lt","gt","le"
};

// Emit a Dbra opcode, 0101cccc 11001nnn vv
int OpDbra(int op)
{
  int use=0;
  int cc=0;

  use=op&~7; // Use same handler
  cc=(op>>8)&15;
  
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler
  OpStart(op);

  if (cc>=2)
  {
    ot(";@ Is the condition true?\n");
    if ((cc&~1)==2) ot("  eor r9,r9,#0x20000000 ;@ Invert carry for hi/ls\n");
    ot("  msr cpsr_flg,r9 ;@ ARM flags = 68000 flags\n");
    if ((cc&~1)==2) ot("  eor r9,r9,#0x20000000\n");
    ot(";@ If so, don't dbra\n");
    ot("  b%s DbraTrue%.4x\n",Cond[cc],op);
    ot("\n");
  }

  ot(";@ Decrement Dn.w\n");
  ot("  and r1,r8,#0x0007\n");
  ot("  mov r1,r1,lsl #2\n");
  ot("  ldrsh r0,[r7,r1]\n");
  ot("  sub r0,r0,#1\n");
  ot("  strh r0,[r7,r1]\n");
  ot("\n");

  ot(";@ Check if Dn.w is -1\n");
  ot("  cmps r0,#-1\n");
  ot("  beq DbraMin1%.4x\n",op);
  ot("\n");

  ot(";@ Get Branch offset:\n");
  ot("  ldrsh r0,[r4]\n");
  ot("  add r4,r4,r0 ;@ r4 = New PC\n");
  ot("\n");
  Cycles=12-2;
  OpEnd();
  
  ot(";@ Dn.w is -1:\n");
  ot("DbraMin1%.4x%s\n", op, ms?"":":");
  ot("  add r4,r4,#2 ;@ Skip branch offset\n");
  ot("\n");
  Cycles=12+2;
  OpEnd();

  ot(";@ condition true:\n");
  ot("DbraTrue%.4x%s\n", op, ms?"":":");
  ot("  add r4,r4,#2 ;@ Skip branch offset\n");
  ot("\n");
  Cycles=12;
  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x6000+ ---------------------
// Emit a Branch opcode 0110cccc nn  (cccc=condition)
int OpBranch(int op)
{
  int size=0,use=0;
  int offset=0;
  int cc=0;

  offset=(char)(op&0xff);
  cc=(op>>8)&15;

  // Special offsets:
  if (offset==0)  size=1;
  if (offset==-1) size=2;

  if (size) use=op; // 16-bit or 32-bit
  else use=(op&0xff00)+1; // Use same opcode for all 8-bit branches

  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler
  OpStart(op);

  ot(";@ Get Branch offset:\n");
  if (size) 
  {
    EaCalc(0,0,0x3c,size);
    EaRead(0,0,0x3c,size,0);
  }

  // above code messes cycles
  Cycles=10; // Assume branch taken

  if (size==0) ot("  mov r0,r8,asl #24 ;@ Shift 8-bit signed offset up...\n\n");

  if (cc==1) ot("  ldr r10,[r7,#0x60] ;@ Get Memory base\n");

  if (cc>=2)
  {
    ot(";@ Is the condition true?\n");
    if ((cc&~1)==2) ot("  eor r9,r9,#0x20000000 ;@ Invert carry for hi/ls\n");
    ot("  msr cpsr_flg,r9 ;@ ARM flags = 68000 flags\n");
    if ((cc&~1)==2) ot("  eor r9,r9,#0x20000000\n");

    if (size==0) ot("  mov r0,r0,asr #24 ;@ ...shift down\n\n");

    ot("  b%s DontBranch%.4x\n",Cond[cc^1],op);

    ot("\n");
  }
  else
  {
    if (size==0) ot("  mov r0,r0,asr #24 ;@ ...shift down\n\n");
  }

  ot(";@ Branch taken - Add on r0 to PC\n");

  if (cc==1)
  {
    ot(";@ Bsr - remember old PC\n");
    ot("  sub r1,r4,r10 ;@ r1 = Old PC\n");
    ot("  mov r1,r1, lsl #8\n");
    ot("  mov r1,r1, asr #8\n");
    ot("\n");
    if (size) ot("  sub r4,r4,#%d ;@ (Branch is relative to Opcode+2)\n",1<<size);
    ot("  ldr r2,[r7,#0x3c]\n");
    ot("  add r4,r4,r0 ;@ r4 = New PC\n");
    ot(";@ Push r1 onto stack\n");
    ot("  sub r0,r2,#4 ;@ Predecrement A7\n");
    ot("  str r0,[r7,#0x3c] ;@ Save A7\n");
    MemHandler(1,2);
    ot("\n");
    Cycles=18; // always 18
  }
  else
  {
    if (size) ot("  sub r4,r4,#%d ;@ (Branch is relative to Opcode+2)\n",1<<size);
    ot("  add r4,r4,r0 ;@ r4 = New PC\n");
    ot("\n");
  }

#if USE_CHECKPC_CALLBACK
  if (offset==0 || offset==-1)
  {
    ot(";@ Branch is quite far, so may be a good idea to check Memory Base+pc\n");
    CheckPc();
  }
#endif

  OpEnd();

  if (cc>=2)
  {
    ot("DontBranch%.4x%s\n", op, ms?"":":");
    Cycles+=(size==1)?  2 : -2; // Branch not taken
    OpEnd();
  }

  return 0;
}

