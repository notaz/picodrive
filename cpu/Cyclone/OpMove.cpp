
#include "app.h"


// Pack our flags into r1, in SR/CCR register format
// trashes r0,r2
void OpFlagsToReg(int high)
{
  ot("  ldrb r0,[r7,#0x45]  ;@ X bit\n");
  ot("  mov r1,r9,lsr #28   ;@ ____NZCV\n");
  ot("  eor r2,r1,r1,ror #1 ;@ Bit 0=C^V\n");
  ot("  tst r2,#1           ;@ 1 if C!=V\n");
  ot("  eorne r1,r1,#3      ;@ ____NZVC\n");
  ot("\n");
  if (high) ot("  ldrb r2,[r7,#0x44]  ;@ Include SR high\n");
  ot("  and r0,r0,#0x02\n");
  ot("  orr r1,r1,r0,lsl #3 ;@ ___XNZVC\n");
  if (high) ot("  orr r1,r1,r2,lsl #8\n");
  ot("\n");
}

// Convert SR/CRR register in r0 to our flags
// trashes r0,r1
void OpRegToFlags(int high)
{
  ot("  eor r1,r0,r0,ror #1 ;@ Bit 0=C^V\n");
  ot("  mov r2,r0,lsr #3    ;@ r2=___XN\n");
  ot("  tst r1,#1           ;@ 1 if C!=V\n");
  ot("  eorne r0,r0,#3      ;@ ___XNZCV\n");
  ot("  strb r2,[r7,#0x45]  ;@ Store X bit\n");
  ot("  mov r9,r0,lsl #28   ;@ r9=NZCV...\n");

  if (high)
  {
    ot("  mov r0,r0,ror #8\n");
    ot("  and r0,r0,#0xa7 ;@ only take defined bits\n");
    ot("  strb r0,[r7,#0x44] ;@ Store SR high\n");
  }
  ot("\n");
}

// checks for supervisor bit, if not set, jumps to SuperEnd()
// also sets r11 to SR high value, SuperChange() uses this
void SuperCheck(int op)
{
  ot("  ldr r11,[r7,#0x44] ;@ Get SR high\n");
  ot("  tst r11,#0x20 ;@ Check we are in supervisor mode\n");
  ot("  beq WrongMode%.4x ;@ No\n",op);
  ot("\n");
}

void SuperEnd(int op)
{
  ot("WrongMode%.4x%s\n",op,ms?"":":");
  ot("  sub r4,r4,#2 ;@ this opcode wasn't executed - go back\n");
  ot("  mov r0,#0x20 ;@ privilege violation\n");
  ot("  bl Exception\n");
  Cycles=34;
  OpEnd();
}

// does OSP and A7 swapping if needed
// new or old SR (not the one already in [r7,#0x44]) should be passed in r11
// trashes r1,r11
void SuperChange(int op)
{
  ot(";@ A7 <-> OSP?\n");
  ot("  ldr r1,[r7,#0x44] ;@ Get other SR high\n");
  ot("  and r11,r11,#0x20\n");
  ot("  and r1,r1,#0x20\n");
  ot("  teq r11,r1 ;@ r11 xor r1\n");
  ot("  beq no_sp_swap%.4x\n",op);
  ot(" ;@ swap OSP and A7:\n");
  ot("  ldr r11,[r7,#0x3C] ;@ Get A7\n");
  ot("  ldr r1, [r7,#0x48] ;@ Get OSP\n");
  ot("  str r11,[r7,#0x48]\n");
  ot("  str r1, [r7,#0x3C]\n");
  ot("no_sp_swap%.4x%s\n", op, ms?"":":");
}



// --------------------- Opcodes 0x1000+ ---------------------
// Emit a Move opcode, 00xxdddd ddssssss
int OpMove(int op)
{
  int sea=0,tea=0;
  int size=0,use=0;
  int movea=0;

  // Get source and target EA
  sea = op&0x003f;
  tea =(op&0x01c0)>>3;
  tea|=(op&0x0e00)>>9;

  if (tea>=8 && tea<0x10) movea=1;

  // Find size extension
  switch (op&0x3000)
  {
    default: return 1;
    case 0x1000: size=0; break;
    case 0x3000: size=1; break;
    case 0x2000: size=2; break;
  }

  if (size<1 && (movea || EaAn(sea))) return 1; // move.b An,* and movea.b * are invalid

  // See if we can do this opcode:
  if (EaCanRead (sea,size)==0) return 1;
  if (EaCanWrite(tea     )==0) return 1;

  use=OpBase(op);
  if (tea<0x38) use&=~0x0e00; // Use same handler for register ?0-7
  
  if (tea>=0x18 && tea<0x28 && (tea&7)==7) use|=0x0e00; // Specific handler for (a7)+ and -(a7)

  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  EaCalc(0,0x003f,sea,size);
  EaRead(0,     1,sea,size,0x003f);

  if (movea==0) {
    ot("  adds r1,r1,#0 ;@ Defines NZ, clears CV\n");
    ot("  mrs r9,cpsr ;@ r9=NZCV flags\n");
    ot("\n");
  }

  if (movea) size=2; // movea always expands to 32-bits

  EaCalc (0,0x0e00,tea,size);
#if SPLIT_MOVEL_PD
  if ((tea&0x38)==0x20 && size==2) { // -(An)
    ot("  mov r10,r0\n");
    ot("  mov r11,r1\n");
    ot("  add r0,r0,#2\n");
    EaWrite(0,     1,tea,1,0x0e00);
    EaWrite(10,   11,tea,1,0x0e00,1);
  } else {
    EaWrite(0,     1,tea,size,0x0e00);
  }
#else
  EaWrite(0,     1,tea,size,0x0e00);
#endif

#if CYCLONE_FOR_GENESIS && !MEMHANDLERS_CHANGE_CYCLES
  // this is a bit hacky
  if ((tea==0x39||(tea&0x38)==0x10)&&size>=1)
    ot("  ldr r5,[r7,#0x5c] ;@ Load Cycles\n");
#endif

  if((tea&0x38)==0x20) Cycles-=2; // less cycles when dest is -(An)

  OpEnd();
  return 0;
}

// --------------------- Opcodes 0x41c0+ ---------------------
// Emit an Lea opcode, 0100nnn1 11aaaaaa
int OpLea(int op)
{
  int use=0;
  int sea=0,tea=0;

  sea= op&0x003f;
  tea=(op&0x0e00)>>9; tea|=8;

  if (EaCanRead(sea,-1)==0) return 1; // See if we can do this opcode

  use=OpBase(op);
  use&=~0x0e00; // Also use 1 handler for target ?0-7
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);

  EaCalc (1,0x003f,sea,0); // Lea
  EaCalc (0,0x0e00,tea,2,1);
  EaWrite(0,     1,tea,2,0x0e00,1);

  Cycles=Ea_add_ns(g_lea_cycle_table,sea);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x40c0+ ---------------------
// Move SR opcode, 01000tt0 11aaaaaa move SR
int OpMoveSr(int op)
{
  int type=0,ea=0;
  int use=0,size=1;

  type=(op>>9)&3; // from SR, from CCR, to CCR, to SR
  ea=op&0x3f;

  if(EaAn(ea)) return 1; // can't use An regs

  switch(type)
  {
    case 0:
      if (EaCanWrite(ea)==0) return 1; // See if we can do this opcode:
      break;

    case 1:
      return 1; // no such op in 68000

	case 2: case 3:
      if (EaCanRead(ea,size)==0) return 1; // See if we can do this opcode:
      break;
  }

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);
  Cycles=12;
  if (type==0) Cycles=(ea>=8)?8:6;

  if (type==3) SuperCheck(op); // 68000 model allows reading whole SR in user mode (but newer models don't)

  if (type==0 || type==1)
  {
    OpFlagsToReg(type==0);
    EaCalc (0,0x003f,ea,size);
    EaWrite(0,     1,ea,size,0x003f);
  }

  if (type==2 || type==3)
  {
    EaCalc(0,0x003f,ea,size);
    EaRead(0,     0,ea,size,0x003f);
    OpRegToFlags(type==3);
    if (type==3) {
	  SuperChange(op);
	  CheckInterrupt(op);
	}
  }

  OpEnd();

  if (type==3) SuperEnd(op);

  return 0;
}


// Ori/Andi/Eori $nnnn,sr 0000t0t0 01111100
int OpArithSr(int op)
{
  int type=0,ea=0;
  int use=0,size=0;

  type=(op>>9)&5; if (type==4) return 1;
  size=(op>>6)&1; // ccr or sr?
  ea=0x3c;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=16;

  if (size) SuperCheck(op);

  EaCalc(0,0x003f,ea,size);
  EaRead(0,    10,ea,size,0x003f);

  OpFlagsToReg(size);
  if (type==0) ot("  orr r0,r1,r10\n");
  if (type==1) ot("  and r0,r1,r10\n");
  if (type==5) ot("  eor r0,r1,r10\n");
  OpRegToFlags(size);
  if (size) {
	SuperChange(op);
    CheckInterrupt(op);
  }

  OpEnd();
  if (size) SuperEnd(op);

  return 0;
}

// --------------------- Opcodes 0x4850+ ---------------------
// Emit an Pea opcode, 01001000 01aaaaaa
int OpPea(int op)
{
  int use=0;
  int ea=0;

  ea=op&0x003f; if (ea<0x10) return 1; // Swap opcode
  if (EaCanRead(ea,-1)==0) return 1; // See if we can do this opcode:

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);

  ot("  ldr r10,[r7,#0x3c]\n");
  EaCalc (1,0x003f, ea,0);
  ot("\n");
  ot("  sub r0,r10,#4 ;@ Predecrement A7\n");
  ot("  str r0,[r7,#0x3c] ;@ Save A7\n");
  ot("\n");
  MemHandler(1,2); // Write 32-bit
  ot("\n");

  Cycles=6+Ea_add_ns(g_pea_cycle_table,ea);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4880+ ---------------------
// Emit a Movem opcode, 01001d00 1xeeeeee regmask
int OpMovem(int op)
{
  int size=0,ea=0,cea=0,dir=0;
  int use=0,decr=0,change=0;

  size=((op>>6)&1)+1; // word, long
  ea=op&0x003f;
  dir=(op>>10)&1; // Direction (1==ea2reg)

  if (dir) {
    if (ea<0x10 || ea>0x3b || (ea&0x38)==0x20) return 1; // Invalid EA
  } else {
    if (ea<0x10 || ea>0x39 || (ea&0x38)==0x18) return 1;
  }

  if ((ea&0x38)==0x18 || (ea&0x38)==0x20) change=1;
  if ((ea&0x38)==0x20) decr=1; // -(An), bitfield is decr

  cea=ea; if (change) cea=0x10;

  use=OpBase(op);
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op);

  ot("  stmdb sp!,{r9} ;@ Push r9\n"); // can't just use r12 or lr here, because memhandlers touch them
  ot("  ldrh r11,[r4],#2 ;@ r11=register mask\n");

  ot("\n");
  ot(";@ Get the address into r9:\n");
  EaCalc(9,0x003f,cea,size);

  ot(";@ r10=Register Index*4:\n");
  if (decr) ot("  mov r10,#0x3c ;@ order reversed for -(An)\n");
  else      ot("  mov r10,#0\n");
  
  ot("\n");
  ot("MoreReg%.4x%s\n",op, ms?"":":");

  ot("  tst r11,#1\n");
  ot("  beq SkipReg%.4x\n",op);
  ot("\n");

  if (decr) ot("  sub r9,r9,#%d ;@ Pre-decrement address\n",1<<size);

  if (dir)
  {
    ot("  ;@ Copy memory to register:\n",1<<size);
    EaRead (9,0,ea,size,0x003f);
    ot("  str r0,[r7,r10] ;@ Save value into Dn/An\n");
  }
  else
  {
    ot("  ;@ Copy register to memory:\n",1<<size);
    ot("  ldr r1,[r7,r10] ;@ Load value from Dn/An\n");
    EaWrite(9,1,ea,size,0x003f);
  }

  if (decr==0) ot("  add r9,r9,#%d ;@ Post-increment address\n",1<<size);

  ot("  sub r5,r5,#%d ;@ Take some cycles\n",2<<size);
  ot("\n");
  ot("SkipReg%.4x%s\n",op, ms?"":":");
  ot("  movs r11,r11,lsr #1;@ Shift mask:\n");
  ot("  add r10,r10,#%d ;@ r10=Next Register\n",decr?-4:4);
  ot("  bne MoreReg%.4x\n",op);
  ot("\n");

  if (change)
  {
    ot(";@ Write back address:\n");
    EaCalc (0,0x0007,8|(ea&7),2);
    EaWrite(0,     9,8|(ea&7),2,0x0007);
  }

  ot("  ldmia sp!,{r9} ;@ Pop r9\n");
  ot("\n");

  if(dir) { // er
         if (ea==0x3a) Cycles=16; // ($nn,PC)
    else if (ea==0x3b) Cycles=18; // ($nn,pc,Rn)
	else Cycles=12;
  } else {
    Cycles=8;
  }

  Cycles+=Ea_add_ns(g_movem_cycle_table,ea);

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0x4e60+ ---------------------
// Emit a Move USP opcode, 01001110 0110dnnn move An to/from USP
int OpMoveUsp(int op)
{
  int use=0,dir=0;

  dir=(op>>3)&1; // Direction
  use=op&~0x0007; // Use same opcode for all An

  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  SuperCheck(op);

  if (dir)
  {
    ot("  ldr r1,[r7,#0x48] ;@ Get from USP\n\n");
    EaCalc (0,0x0007,8,2,1);
    EaWrite(0,     1,8,2,0x0007,1);
  }
  else
  {
    EaCalc (0,0x0007,8,2,1);
    EaRead (0,     0,8,2,0x0007,1);
    ot("  str r0,[r7,#0x48] ;@ Put in USP\n\n");
  }
    
  OpEnd();

  SuperEnd(op);

  return 0;
}

// --------------------- Opcodes 0x7000+ ---------------------
// Emit a Move Quick opcode, 0111nnn0 dddddddd  moveq #dd,Dn
int OpMoveq(int op)
{
  int use=0;

  use=op&0xf100; // Use same opcode for all values
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=4;

  ot("  movs r0,r8,asl #24\n");
  ot("  and r1,r8,#0x0e00\n");
  ot("  mov r0,r0,asr #24 ;@ Sign extended Quick value\n");
  ot("  mrs r9,cpsr ;@ r9=NZ flags\n");
  ot("  str r0,[r7,r1,lsr #7] ;@ Store into Dn\n");
  ot("\n");

  OpEnd();

  return 0;
}

// --------------------- Opcodes 0xc140+ ---------------------
// Emit a Exchange opcode:
// 1100ttt1 01000sss  exg ds,dt
// 1100ttt1 01001sss  exg as,at
// 1100ttt1 10001sss  exg as,dt
int OpExg(int op)
{
  int use=0,type=0;

  type=op&0xf8;

  if (type!=0x40 && type!=0x48 && type!=0x88) return 1; // Not an exg opcode

  use=op&0xf1f8; // Use same opcode for all values
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler

  OpStart(op); Cycles=6;

  ot("  and r10,r8,#0x0e00 ;@ Find T register\n");
  ot("  and r11,r8,#0x000f ;@ Find S register\n");
  if (type==0x48) ot("  orr r10,r10,#0x1000 ;@ T is an address register\n");
  ot("\n");
  ot("  ldr r0,[r7,r10,lsr #7] ;@ Get T\n");
  ot("  ldr r1,[r7,r11,lsl #2] ;@ Get S\n");
  ot("\n");
  ot("  str r0,[r7,r11,lsl #2] ;@ T->S\n");
  ot("  str r1,[r7,r10,lsr #7] ;@ S->T\n");  
  ot("\n");

  OpEnd();
  
  return 0;
}

// ------------------------- movep -------------------------------
// 0000ddd1 0z001sss
// 0000sss1 1z001ddd (to mem)
int OpMovep(int op)
{
  int ea=0;
  int size=1,use=0,dir;

  use=op&0xf1f8;
  if (op!=use) { OpUse(op,use); return 0; } // Use existing handler (for all dests, srcs)

  // Get EA
  ea = (op&0x0007)|0x28;
  dir = (op>>7)&1;

  // Find size extension
  if(op&0x0040) size=2;

  OpStart(op);
  
  if(dir) { // reg to mem
    EaCalc(11,0x0e00,0,size);      // reg number -> r11
    EaRead(11,11,0,size,0x0e00);   // regval -> r11
    EaCalc(10,0x0007,ea,size);
	if(size==2) { // if operand is long
	  ot("  mov r1,r11,lsr #24 ;@ first byte\n");
	  EaWrite(10,1,ea,0,0x0007); // store first byte
	  ot("  add r10,r10,#2\n");
	  ot("  mov r1,r11,lsr #16 ;@ second byte\n");
	  EaWrite(10,1,ea,0,0x0007); // store second byte
	  ot("  add r10,r10,#2\n");
	}
	ot("  mov r1,r11,lsr #8 ;@ first or third byte\n");
	EaWrite(10,1,ea,0,0x0007);
	ot("  add r10,r10,#2\n");
	ot("  and r1,r11,#0xff\n");
	EaWrite(10,1,ea,0,0x0007);
  } else { // mem to reg
    EaCalc(10,0x0007,ea,size,1);
    EaRead(10,11,ea,0,0x0007,1); // read first byte
	ot("  add r10,r10,#2\n");
    EaRead(10,1,ea,0,0x0007,1); // read second byte
	if(size==2) { // if operand is long
      ot("  orr r11,r11,r1,lsr #8 ;@ second byte\n");
	  ot("  add r10,r10,#2\n");
	  EaRead(10,1,ea,0,0x0007,1);
      ot("  orr r11,r11,r1,lsr #16 ;@ third byte\n");
	  ot("  add r10,r10,#2\n");
	  EaRead(10,1,ea,0,0x0007,1);
      ot("  orr r0,r11,r1,lsr #24 ;@ fourth byte\n");
	} else {
      ot("  orr r0,r11,r1,lsr #8 ;@ second byte\n");
	}
	// store the result
    EaCalc(11,0x0e00,0,size,1);      // reg number -> r11
	EaWrite(11,0,0,size,0x0e00,1);
  }

  Cycles=(size==2)?24:16;
  OpEnd();

  return 0;
}

// Emit a Stop/Reset opcodes, 01001110 011100t0 imm
int OpStopReset(int op)
{
  int type=(op>>1)&1; // reset/stop

  OpStart(op);

  SuperCheck(op);

  if(type) {
    // copy immediate to SR, stop the CPU and eat all remaining cycles.
    ot("  ldrh r0,[r4],#2 ;@ Fetch the immediate\n");
    SuperChange(op);
    OpRegToFlags(1);

	ot("\n");

	ot("  mov r0,#1\n");
	ot("  str r0,[r7,#0x58] ;@ stopped\n");
	ot("\n");

	ot("  mov r5,#0 ;@ eat cycles\n");
    Cycles = 4;
	ot("\n");
  }
  else
  {
    Cycles = 132;
#if USE_RESET_CALLBACK
    ot("  str r4,[r7,#0x40] ;@ Save PC\n");
    ot("  mov r1,r9,lsr #28\n");
    ot("  strb r1,[r7,#0x46] ;@ Save Flags (NZCV)\n");
    ot("  str r5,[r7,#0x5c] ;@ Save Cycles\n");
    ot("  ldr r11,[r7,#0x90] ;@ ResetCallback\n");
    ot("  tst r11,r11\n");
    ot("  movne lr,pc\n");
    ot("  movne pc,r11 ;@ call ResetCallback if it is defined\n");
    ot("  ldrb r9,[r7,#0x46] ;@ r9 = Load Flags (NZCV)\n");
    ot("  ldr r5,[r7,#0x5c] ;@ Load Cycles\n");
    ot("  ldr r4,[r7,#0x40] ;@ Load PC\n");
    ot("  mov r9,r9,lsl #28\n");
#endif
  }

  OpEnd();
  SuperEnd(op);

  return 0;
}
