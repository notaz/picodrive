
#include "app.h"

static FILE *AsmFile=NULL;

static int CycloneVer=0x0086; // Version number of library
int *CyJump=NULL; // Jump table
int ms=USE_MS_SYNTAX; // If non-zero, output in Microsoft ARMASM format
char *Narm[4]={ "b", "h","",""}; // Normal ARM Extensions for operand sizes 0,1,2
char *Sarm[4]={"sb","sh","",""}; // Sign-extend ARM Extensions for operand sizes 0,1,2
int Cycles; // Current cycles for opcode


void ot(const char *format, ...)
{
  va_list valist=NULL;
  int i, len;

  // notaz: stop me from leaving newlines in the middle of format string
  // and generating bad code
  for(i=0, len=strlen(format); i < len && format[i] != '\n'; i++);
  if(i < len-1 && format[len-1] != '\n') printf("\nWARNING: possible improper newline placement:\n%s\n", format);

  va_start(valist,format);
  if (AsmFile) vfprintf(AsmFile,format,valist);
  va_end(valist);
}

void ltorg()
{
  if (ms) ot("  LTORG\n");
  else    ot("  .ltorg\n");
}

// trashes all temp regs
static void PrintException(int ints)
{
  if(!ints) {
    ot("  ;@ Cause an Exception - Vector address in r0\n");
    ot("  mov r11,r0\n");
  }

  ot(";@ swap OSP <-> A7?\n");
  ot("  ldr r0,[r7,#0x44] ;@ Get SR high\n");
  ot("  tst r0,#0x20\n");
  ot("  bne no_sp_swap%i\n",ints);
  ot(";@ swap OSP and A7:\n");
  ot("  ldr r0,[r7,#0x3C] ;@ Get A7\n");
  ot("  ldr r1,[r7,#0x48] ;@ Get OSP\n");
  ot("  str r0,[r7,#0x48]\n");
  ot("  str r1,[r7,#0x3C]\n");
  ot("no_sp_swap%i%s\n",ints,ms?"":":");

  ot("  ldr r10,[r7,#0x60] ;@ Get Memory base\n");
  ot("  mov r1,r4,lsl #8\n");
  ot("  sub r1,r1,r10,lsl #8 ;@ r1 = Old PC\n");
  ot("  mov r1,r1,asr #8 ;@ push sign extended\n");
  OpPush32();
  OpPushSr(1);
  ot("  mov r0,r11\n");
  ot(";@ Read IRQ Vector:\n");
  MemHandler(0,2);
  if(ints) {
    ot("  tst r0,r0 ;@ uninitialized int vector?\n");
    ot("  moveq r0,#0x3c\n");
    ot("  moveq lr,pc\n");
    ot("  ldreq pc,[r7,#0x70] ;@ Call read32(r0) handler\n");
  }
#if USE_CHECKPC_CALLBACK
  ot("  add r0,r0,r10 ;@ r0 = Memory Base + New PC\n");
  ot("  mov lr,pc\n");
  ot("  ldr pc,[r7,#0x64] ;@ Call checkpc()\n");
  ot("  mov r4,r0\n");
#endif
  ot("\n");

  if(!ints) {
    ot("  ldr r0,[r7,#0x44] ;@ Get SR high\n");
    ot("  bic r0,r0,#0xd8 ;@ clear trace and unused flags\n");
    ot("  orr r0,r0,#0x20 ;@ set supervisor mode\n");
    ot("  strb r0,[r7,#0x44]\n");
  }
}

// Trashes r0,r1
void CheckInterrupt(int op)
{
  ot(";@ CheckInterrupt:\n");
  ot("  ldr r0,[r7,#0x44]\n"); // same as  ldrb r0,[r7,#0x47]
  ot("  movs r0,r0,lsr #24 ;@ Get IRQ level (loading word is faster)\n");
  ot("  beq NoInts%x\n",op);
  ot("  cmp r0,#6 ;@ irq>6 ?\n");
  ot("  ldrleb r1,[r7,#0x44] ;@ Get SR high: T_S__III\n");
  ot("  andle r1,r1,#7 ;@ Get interrupt mask\n");
  ot("  cmple r0,r1 ;@ irq<=6: Is irq<=mask ?\n");
  ot("  blgt DoInterrupt\n");
  ot("NoInts%x%s\n", op,ms?"":":");
  ot("\n");
}

static void PrintFramework()
{
  ot(";@ --------------------------- Framework --------------------------\n");
  if (ms) ot("CycloneRun\n");
  else    ot("CycloneRun:\n");

  ot("  stmdb sp!,{r4-r11,lr}\n");

  ot("  mov r7,r0          ;@ r7 = Pointer to Cpu Context\n");
  ot("                     ;@ r0-3 = Temporary registers\n");
  ot("  ldrb r9,[r7,#0x46] ;@ r9 = Flags (NZCV)\n");
  ot("  ldr r6,=JumpTab    ;@ r6 = Opcode Jump table\n");
  ot("  ldr r5,[r7,#0x5c]  ;@ r5 = Cycles\n");
  ot("  ldr r4,[r7,#0x40]  ;@ r4 = Current PC + Memory Base\n");
  ot("                     ;@ r8 = Current Opcode\n");
  ot("  ldr r0,[r7,#0x44]\n");
  ot("  mov r9,r9,lsl #28  ;@ r9 = Flags 0xf0000000, cpsr format\n");
  ot("                     ;@ r10 = Source value / Memory Base\n");
  ot("\n");
  ot(";@ CheckInterrupt:\n");
  ot("  movs r0,r0,lsr #24 ;@ Get IRQ level\n"); // same as  ldrb r0,[r7,#0x47]
  ot("  beq NoInts0\n");
  ot("  cmp r0,#6 ;@ irq>6 ?\n");
  ot("  ldrleb r1,[r7,#0x44] ;@ Get SR high: T_S__III\n");
  ot("  andle r1,r1,#7 ;@ Get interrupt mask\n");
  ot("  cmple r0,r1 ;@ irq<=6: Is irq<=mask ?\n");
  ot("  blgt DoInterrupt\n");
  ot(";@ Check if interrupt used up all the cycles:\n");
  ot("  subs r5,r5,#0\n");
  ot("  blt CycloneEndNoBack\n");
  ot("NoInts0%s\n", ms?"":":");
  ot("\n");
  ot(";@ Check if our processor is in stopped state and jump to opcode handler if not\n");
  ot("  ldr r0,[r7,#0x58]\n");
  ot("  ldrh r8,[r4],#2 ;@ Fetch first opcode\n");
  ot("  tst r0,r0 ;@ stopped?\n");
  ot("  bne CycloneStopped\n");
  ot("  ldr pc,[r6,r8,asl #2] ;@ Jump to opcode handler\n");
  ot("\n");
  ot("\n");

  ot(";@ We come back here after execution\n");
  ot("CycloneEnd%s\n", ms?"":":");
  ot("  sub r4,r4,#2\n");
  ot("CycloneEndNoBack%s\n", ms?"":":");
  ot("  mov r9,r9,lsr #28\n");
  ot("  str r4,[r7,#0x40]  ;@ Save Current PC + Memory Base\n");
  ot("  str r5,[r7,#0x5c]  ;@ Save Cycles\n");
  ot("  strb r9,[r7,#0x46] ;@ Save Flags (NZCV)\n");
  ot("  ldmia sp!,{r4-r11,pc}\n");
  ot("\n");
  ot("CycloneStopped%s\n", ms?"":":");
  ot("  mov r5,#0\n");
  ot("  str r5,[r7,#0x5C]  ;@ eat all cycles\n");
  ot("  ldmia sp!,{r4-r11,pc} ;@ we are stopped, do nothing!\n");
  ot("\n");

  ltorg();

#if COMPRESS_JUMPTABLE
    ot(";@ uncompress jump table\n");
    if (ms) ot("CycloneInit\n");
    else    ot("CycloneInit:\n");
    ot("  ldr r12,=JumpTab\n");
    ot("  add r0,r12,#0xe000*4 ;@ ctrl code pointer\n");
    ot("  ldr r1,[r0,#-4]\n");
    ot("  tst r1,r1\n");
    ot("  movne pc,lr ;@ already uncompressed\n");
	ot("  add r3,r12,#0xa000*4 ;@ handler table pointer, r12=dest\n");
    ot("unc_loop%s\n", ms?"":":");
    ot("  ldrh r1,[r0],#2\n");
    ot("  and r2,r1,#0xf\n");
    ot("  bic r1,r1,#0xf\n");
    ot("  ldr r1,[r3,r1,lsr #2] ;@ r1=handler\n");
    ot("  cmp r2,#0xf\n");
    ot("  addeq r2,r2,#1 ;@ 0xf is really 0x10\n");
    ot("  tst r2,r2\n");
    ot("  ldreqh r2,[r0],#2 ;@ counter is in next word\n");
    ot("  tst r2,r2\n");
    ot("  beq unc_finish ;@ done decompressing\n");
    ot("  tst r1,r1\n");
    ot("  addeq r12,r12,r2,lsl #2 ;@ 0 handler means we should skip those bytes\n");
    ot("  beq unc_loop\n");
    ot("unc_loop_in%s\n", ms?"":":");
    ot("  subs r2,r2,#1\n");
    ot("  str r1,[r12],#4\n");
    ot("  bgt unc_loop_in\n");
    ot("  b unc_loop\n");
    ot("unc_finish%s\n", ms?"":":");
    ot("  ldr r12,=JumpTab\n");
    ot("  ;@ set a-line and f-line handlers\n");
    ot("  add r0,r12,#0xa000*4\n");
    ot("  ldr r1,[r0,#4] ;@ a-line handler\n");
    ot("  ldr r3,[r0,#8] ;@ f-line handler\n");
    ot("  mov r2,#0x1000\n");
    ot("unc_fill3%s\n", ms?"":":");
    ot("  subs r2,r2,#1\n");
    ot("  str r1,[r0],#4\n");
    ot("  bgt unc_fill3\n");
    ot("  add r0,r12,#0xf000*4\n");
    ot("  mov r2,#0x1000\n");
    ot("unc_fill4%s\n", ms?"":":");
    ot("  subs r2,r2,#1\n");
    ot("  str r3,[r0],#4\n");
    ot("  bgt unc_fill4\n");
    ot("  bx lr\n");
    ltorg();
    ot("\n");
#else
    ot(";@ do nothing\n");
    if (ms) ot("CycloneInit\n");
    else    ot("CycloneInit:\n");
    ot("  bx lr\n");
    ot("\n");
#endif
  if (ms) ot("CycloneSetSr\n");
  else    ot("CycloneSetSr:\n");
  ot("  mov r2,r1,lsr #8\n");
  ot("  ldrb r3,[r0,#0x44] ;@ get SR high\n");
  ot("  eor r3,r3,r2\n");
  ot("  tst r3,#0x20\n");
  ot("  and r2,r2,#0xa7 ;@ only nonzero bits\n");
  ot("  strb r2,[r0,#0x44] ;@ set SR high\n");
  ot("  bne setsr_noswap\n");
  ot("  ldr r2,[r0,#0x3C] ;@ Get A7\n");
  ot("  ldr r3,[r0,#0x48] ;@ Get OSP\n");
  ot("  str r3,[r0,#0x3C]\n");
  ot("  str r2,[r0,#0x48]\n");
  ot("setsr_noswap%s\n",ms?"":":");
  ot("  mov r2,r1,lsr #3\n");
  ot("  strb r2,[r0,#0x45] ;@ the X flag\n");
  ot("  bic r2,r1,#0xf3\n");
  ot("  tst r1,#1\n");
  ot("  orrne r2,r2,#2\n");
  ot("  tst r1,#2\n");
  ot("  orrne r2,r2,#1\n");
  ot("  strb r2,[r0,#0x46] ;@ flags\n");
  ot("  bx lr\n");
  ot("\n");

  if (ms) ot("CycloneGetSr\n");
  else    ot("CycloneGetSr:\n");
  ot("  ldrb r1,[r0,#0x46] ;@ flags\n");
  ot("  bic r2,r1,#0xf3\n");
  ot("  tst r1,#1\n");
  ot("  orrne r2,r2,#2\n");
  ot("  tst r1,#2\n");
  ot("  orrne r2,r2,#1\n");
  ot("  ldrb r1,[r0,#0x45] ;@ the X flag\n");
  ot("  tst r1,#2\n");
  ot("  orrne r2,r2,#0x10\n");
  ot("  ldrb r1,[r0,#0x44] ;@ the SR high\n");
  ot("  orr r0,r2,r1,lsl #8\n");
  ot("  bx lr\n");
  ot("\n");

  ot(";@ DoInterrupt - r0=IRQ number\n");
  ot("DoInterrupt%s\n", ms?"":":");
  ot("  stmdb sp!,{lr} ;@ Push ARM return address\n");

  ot(";@ Get IRQ Vector address:\n");
  ot("  mov r0,r0,asl #2\n");
  ot("  add r11,r0,#0x60\n");
  PrintException(1);
  
  ot("  ldrb r0,[r7,#0x47] ;@ IRQ\n");
  ot("  mov r2,#0\n");
  ot("  orr r1,r0,#0x20 ;@ Supervisor mode + IRQ number\n");
  ot("  strb r1,[r7,#0x44] ;@ Put SR high\n");

  ot(";@ Clear stopped states:\n");
  ot("  str r2,[r7,#0x58]\n");
  ot("  sub r5,r5,#%d ;@ Subtract cycles\n",44);
  ot("\n");
#if USE_INT_ACK_CALLBACK
#if INT_ACK_NEEDS_STUFF
  ot("  str r4,[r7,#0x40] ;@ Save PC\n");
  ot("  mov r1,r9,lsr #28\n");
  ot("  strb r1,[r7,#0x46] ;@ Save Flags (NZCV)\n");
  ot("  str r5,[r7,#0x5c] ;@ Save Cycles\n");
#endif
  ot("  ldr r11,[r7,#0x8c] ;@ IrqCallback\n");
  ot("  tst r11,r11\n");
  ot("  movne lr,pc\n");
  ot("  movne pc,r11 ;@ call IrqCallback if it is defined\n");
#if INT_ACK_CHANGES_STUFF
  ot("  ldr r5,[r7,#0x5c] ;@ Load Cycles\n");
  ot("  ldrb r9,[r7,#0x46] ;@ r9 = Load Flags (NZCV)\n");
  ot("  mov r9,r9,lsl #28\n");
  ot("  ldr r4,[r7,#0x40] ;@ Load PC\n");
#endif
#else // not USE_INT_ACK_CALLBACK
  ot(";@ Clear irq:\n");
  ot("  strb r1,[r7,#0x47]\n");
#endif
  ot("  ldmia sp!,{pc} ;@ Return\n");
  ot("\n");
  
  ot("Exception%s\n", ms?"":":");
  ot("\n");
  ot("  stmdb sp!,{lr} ;@ Preserve ARM return address\n");
  PrintException(0);
  ot("  ldmia sp!,{pc} ;@ Return\n");
  ot("\n");
}

// ---------------------------------------------------------------------------
// Call Read(r0), Write(r0,r1) or Fetch(r0)
// Trashes r0-r3,r12,lr
int MemHandler(int type,int size)
{
  int func=0;
  func=0x68+type*0xc+(size<<2); // Find correct offset

#if MEMHANDLERS_NEED_PC
  ot("  str r4,[r7,#0x40] ;@ Save PC\n");
#endif
#if MEMHANDLERS_NEED_FLAGS
  ot("  mov r3,r9,lsr #28\n");
  ot("  strb r3,[r7,#0x46] ;@ Save Flags (NZCV)\n");
#endif
#if MEMHANDLERS_NEED_CYCLES
  ot("  str r5,[r7,#0x5c] ;@ Save Cycles\n");
#endif

  ot("  mov lr,pc\n");
  ot("  ldr pc,[r7,#0x%x] ;@ Call ",func);

  // Document what we are calling:
  if (type==0) ot("read");
  if (type==1) ot("write");
  if (type==2) ot("fetch");

  if (type==1) ot("%d(r0,r1)",8<<size);
  else         ot("%d(r0)",   8<<size);
  ot(" handler\n");

#if MEMHANDLERS_CHANGE_CYCLES
  ot("  ldr r5,[r7,#0x5c] ;@ Load Cycles\n");
#endif
#if MEMHANDLERS_CHANGE_FLAGS
  ot("  ldrb r9,[r7,#0x46] ;@ r9 = Load Flags (NZCV)\n");
  ot("  mov r9,r9,lsl #28\n");
#endif
#if MEMHANDLERS_CHANGE_PC
  ot("  ldr r4,[r7,#0x40] ;@ Load PC\n");
#endif

  return 0;
}

static void PrintOpcodes()
{
  int op=0;
 
  printf("Creating Opcodes: [");

  ot(";@ ---------------------------- Opcodes ---------------------------\n");

  // Emit null opcode:
  ot("Op____%s ;@ Called if an opcode is not recognised\n", ms?"":":");
  ot("  sub r4,r4,#2\n");
#if USE_UNRECOGNIZED_CALLBACK
  ot("  str r4,[r7,#0x40] ;@ Save PC\n");
  ot("  mov r1,r9,lsr #28\n");
  ot("  strb r1,[r7,#0x46] ;@ Save Flags (NZCV)\n");
  ot("  str r5,[r7,#0x5c] ;@ Save Cycles\n");
  ot("  ldr r11,[r7,#0x94] ;@ UnrecognizedCallback\n");
  ot("  tst r11,r11\n");
  ot("  movne lr,pc\n");
  ot("  movne pc,r11 ;@ call UnrecognizedCallback if it is defined\n");
  ot("  ldrb r9,[r7,#0x46] ;@ r9 = Load Flags (NZCV)\n");
  ot("  ldr r5,[r7,#0x5c] ;@ Load Cycles\n");
  ot("  ldr r4,[r7,#0x40] ;@ Load PC\n");
  ot("  mov r9,r9,lsl #28\n");
  ot("  tst r0,r0\n");
  ot("  moveq r0,#0x10\n");
  ot("  bleq Exception\n");
#else
  ot("  mov r0,#0x10\n");
  ot("  bl Exception\n");
#endif
  Cycles=34;
  OpEnd();

  // Unrecognised a-line and f-line opcodes throw an exception:
  ot("Op__al%s ;@ Unrecognised a-line opcode\n", ms?"":":");
  ot("  sub r4,r4,#2\n");
#if USE_AFLINE_CALLBACK
  ot("  str r4,[r7,#0x40] ;@ Save PC\n");
  ot("  mov r1,r9,lsr #28\n");
  ot("  strb r1,[r7,#0x46] ;@ Save Flags (NZCV)\n");
  ot("  str r5,[r7,#0x5c] ;@ Save Cycles\n");
  ot("  ldr r11,[r7,#0x94] ;@ UnrecognizedCallback\n");
  ot("  tst r11,r11\n");
  ot("  movne lr,pc\n");
  ot("  movne pc,r11 ;@ call UnrecognizedCallback if it is defined\n");
  ot("  ldrb r9,[r7,#0x46] ;@ r9 = Load Flags (NZCV)\n");
  ot("  ldr r5,[r7,#0x5c] ;@ Load Cycles\n");
  ot("  ldr r4,[r7,#0x40] ;@ Load PC\n");
  ot("  mov r9,r9,lsl #28\n");
  ot("  tst r0,r0\n");
  ot("  moveq r0,#0x28\n");
  ot("  bleq Exception\n");
#else
  ot("  mov r0,#0x28\n");
  ot("  bl Exception\n");
#endif
  Cycles=4;
  OpEnd();

  ot("Op__fl%s ;@ Unrecognised f-line opcode\n", ms?"":":");
  ot("  sub r4,r4,#2\n");
#if USE_AFLINE_CALLBACK
  ot("  str r4,[r7,#0x40] ;@ Save PC\n");
  ot("  mov r1,r9,lsr #28\n");
  ot("  strb r1,[r7,#0x46] ;@ Save Flags (NZCV)\n");
  ot("  str r5,[r7,#0x5c] ;@ Save Cycles\n");
  ot("  ldr r11,[r7,#0x94] ;@ UnrecognizedCallback\n");
  ot("  tst r11,r11\n");
  ot("  movne lr,pc\n");
  ot("  movne pc,r11 ;@ call UnrecognizedCallback if it is defined\n");
  ot("  ldrb r9,[r7,#0x46] ;@ r9 = Load Flags (NZCV)\n");
  ot("  ldr r5,[r7,#0x5c] ;@ Load Cycles\n");
  ot("  ldr r4,[r7,#0x40] ;@ Load PC\n");
  ot("  mov r9,r9,lsl #28\n");
  ot("  tst r0,r0\n");
  ot("  moveq r0,#0x2c\n");
  ot("  bleq Exception\n");
#else
  ot("  mov r0,#0x2c\n");
  ot("  bl Exception\n");
#endif
  Cycles=4;
  OpEnd();


  for (op=0;op<0x10000;op++)
  {
    if ((op&0xfff)==0) { printf("%x",op>>12); fflush(stdout); } // Update progress

    OpAny(op);
  }

  ot("\n");

  printf("]\n");
}

// helper
static void ott(const char *str, int par, const char *nl, int nlp, int counter, int size)
{
  switch(size) {
    case 0: if((counter&7)==0) ot(ms?"  dcb ":"  .byte ");  break;
    case 1: if((counter&7)==0) ot(ms?"  dcw ":"  .hword "); break;
    case 2: if((counter&7)==0) ot(ms?"  dcd ":"  .long ");  break;
  }
  ot(str, par);
  if((counter&7)==7) ot(nl,nlp); else ot(",");
}

static void PrintJumpTable()
{
  int i=0,op=0,len=0;

  ot(";@ -------------------------- Jump Table --------------------------\n");

#if COMPRESS_JUMPTABLE
    int handlers=0,reps=0,*indexes,ip,u,out;
    // use some weird compression on the jump table
	indexes=(int *)malloc(0x10000*4);
	if(!indexes) { printf("ERROR: out of memory\n"); exit(1); }
	len=0x10000;

	// space for decompressed table
	ot(ms?"  area |.data|, data\n":"  .data\n  .align 4\n\n");

	ot("JumpTab%s\n", ms?"":":");
	if(ms) {
	  for(i = 0; i < 0xa000/8; i++)
	    ot("  dcd 0,0,0,0,0,0,0,0\n");
	} else
	  ot("  .rept 0x%x\n  .long 0,0,0,0,0,0,0,0\n  .endr\n", 0xa000/8);

    // hanlers live in "a-line" part of the table
	// first output nop,a-line,f-line handlers
	ot(ms?"  dcd Op____,Op__al,Op__fl,":"  .long Op____,Op__al,Op__fl,");
	handlers=3;

	for(i=0;i<len;i++)
    {
      op=CyJump[i];

	  for(u=i-1; u>=0; u--) if(op == CyJump[u]) break; // already done with this op?
	  if(u==-1 && op >= 0) {
		ott("Op%.4x",op," ;@ %.4x\n",i,handlers,2);
		indexes[op] = handlers;
	    handlers++;
      }
	}
	if(handlers&7) {
	  fseek(AsmFile, -1, SEEK_CUR); // remove last comma
	  for(i = 8-(handlers&7); i > 0; i--)
	    ot(",000000");
	  ot("\n");
	}
	if(ms) {
	  for(i = (0x4000-handlers)/8; i > 0; i--)
	    ot("  dcd 0,0,0,0,0,0,0,0\n");
	} else {
	  ot(ms?"":"  .rept 0x%x\n  .long 0,0,0,0,0,0,0,0\n  .endr\n", (0x4000-handlers)/8);
	}
    printf("total distinct hanlers: %i\n",handlers);
	// output data
	for(i=0,ip=0; i < 0xf000; i++, ip++) {
      op=CyJump[i];
	  if(op == -2) {
	    // it must skip a-line area, because we keep our data there
	    ott("0x%.4x", handlers<<4, "\n",0,ip++,1);
	    ott("0x%.4x", 0x1000, "\n",0,ip,1);
		i+=0xfff;
	    continue;
	  }
	  for(reps=1; i < 0xf000; i++, reps++) if(op != CyJump[i+1]) break;
	  if(op>=0) out=indexes[op]<<4; else out=0; // unrecognised
	  if(reps <= 0xe || reps==0x10) {
	    if(reps!=0x10) out|=reps; else out|=0xf; // 0xf means 0x10 (0xf appeared to be unused anyway)
	    ott("0x%.4x", out, "\n",0,ip,1);
      } else {
	    ott("0x%.4x", out, "\n",0,ip++,1);
	    ott("0x%.4x", reps,"\n",0,ip,1);
	  }
    }
	if(ip&1) ott("0x%.4x", 0, "\n",0,ip++,1);
	if(ip&7) fseek(AsmFile, -1, SEEK_CUR); // remove last comma
	ot("\n");
	if(ip&7) {
	  for(i = 8-(ip&7); i > 0; i--)
	    ot(",0x0000");
	  ot("\n");
	}
	if(ms) {
	  for(i = (0x2000-ip/2)/8+1; i > 0; i--)
	    ot("  dcd 0,0,0,0,0,0,0,0\n");
	} else {
	  ot("  .rept 0x%x\n  .long 0,0,0,0,0,0,0,0\n  .endr\n", (0x2000-ip/2)/8+1);
	}
	ot("\n");
	free(indexes);
#else
	ot("JumpTab%s\n", ms?"":":");
    len=0xfffe; // Hmmm, armasm 2.50.8684 messes up with a 0x10000 long jump table
                // notaz: same thing with GNU as 2.9-psion-98r2 (reloc overflow)
                // this is due to COFF objects using only 2 bytes for reloc count

    for (i=0;i<len;i++)
    {
      op=CyJump[i];
    
           if(op>=0)  ott("Op%.4x",op," ;@ %.4x\n",i-7,i,2);
      else if(op==-2) ott("Op__al",0, " ;@ %.4x\n",i-7,i,2);
      else if(op==-3) ott("Op__fl",0, " ;@ %.4x\n",i-7,i,2);
      else            ott("Op____",0, " ;@ %.4x\n",i-7,i,2);
    }
	if(i&7) fseek(AsmFile, -1, SEEK_CUR); // remove last comma

    ot("\n");
    ot(";@ notaz: we don't want to crash if we run into those 2 missing opcodes\n");
    ot(";@ so we leave this pattern to patch it later\n");
    ot("%s 0x78563412\n", ms?"  dcd":"  .long");
    ot("%s 0x56341290\n", ms?"  dcd":"  .long");
#endif
}

static int CycloneMake()
{
  int i;
  char *name="Cyclone.s";
  
  // Open the assembly file
  if (ms) name="Cyclone.asm";
  AsmFile=fopen(name,"wt"); if (AsmFile==NULL) return 1;
  
  printf("Making %s...\n",name);

  ot("\n;@ Dave's Cyclone 68000 Emulator v%x.%.3x - Assembler Output\n\n",CycloneVer>>12,CycloneVer&0xfff);

  ot(";@ (c) Copyright 2003 Dave, All rights reserved.\n");
  ot(";@ some code (c) Copyright 2005-2006 notaz, All rights reserved.\n");
  ot(";@ Cyclone 68000 is free for non-commercial use.\n\n");
  ot(";@ For commercial use, separate licencing terms must be obtained.\n\n");

  CyJump=(int *)malloc(0x40000); if (CyJump==NULL) return 1;
  memset(CyJump,0xff,0x40000); // Init to -1
  for(i=0xa000; i<0xb000;  i++) CyJump[i] = -2; // a-line emulation
  for(i=0xf000; i<0x10000; i++) CyJump[i] = -3; // f-line emulation

  if (ms)
  {
    ot("  area |.text|, code\n");
    ot("  export CycloneInit\n");
    ot("  export CycloneRun\n");
    ot("  export CycloneSetSr\n");
    ot("  export CycloneGetSr\n");
    ot("  export CycloneVer\n");
    ot("\n");
    ot("CycloneVer dcd 0x%.4x\n",CycloneVer);
  }
  else
  {
    ot("  .global CycloneInit\n");
    ot("  .global CycloneRun\n");
    ot("  .global CycloneSetSr\n");
    ot("  .global CycloneGetSr\n");
    ot("  .global CycloneVer\n");
    ot("CycloneVer: .long 0x%.4x\n",CycloneVer);
  }
  ot("\n");

  PrintFramework();
  PrintOpcodes();
  PrintJumpTable();

  if (ms) ot("  END\n");

  fclose(AsmFile); AsmFile=NULL;

#if 0
  printf("Assembling...\n");
  // Assemble the file
  if (ms) system("armasm Cyclone.asm");
  else    system("as -o Cyclone.o Cyclone.s");
  printf("Done!\n\n");
#endif

  free(CyJump);
  return 0;
}

int main()
{
  printf("\n  Dave's Cyclone 68000 Emulator v%x.%.3x - Core Creator\n\n",CycloneVer>>12,CycloneVer&0xfff);

  // Make GAS or ARMASM version
  CycloneMake();
  return 0;
}

