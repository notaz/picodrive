#include "PicoInt.h"

typedef unsigned char  u8;

static unsigned int pppc, ops=0;
extern unsigned int lastread_a, lastread_d[16], lastwrite_cyc_d[16], lastwrite_mus_d[16];
extern int lrp_cyc, lrp_mus, lwp_cyc, lwp_mus;
unsigned int old_regs[16], old_sr, ppop;

//static
void dumpPCandExit()
{
  char buff[128];
  int i;

  m68k_disassemble(buff, pppc, M68K_CPU_TYPE_68000);
  dprintf("PC: %06x: %04x: %s", pppc, ppop, buff);
  for(i=0; i < 8; i++)
    dprintf("d%i=%08x, a%i=%08x | d%i=%08x, a%i=%08x", i, PicoCpu.d[i], i, PicoCpu.a[i], i, old_regs[i], i, old_regs[i+8]);
  dprintf("SR: %04x | %04x (??s? 0iii 000x nzvc)", CycloneGetSr(&PicoCpu), old_sr);
  dprintf("last_read: %08x @ %06x", lastread_d[--lrp_cyc&15], lastread_a);
  dprintf("ops done: %i", ops);
  exit(1);
}

int CM_compareRun(int cyc)
{
  char *str;
  int cyc_done=0, cyc_cyclone, cyc_musashi, err=0;
  unsigned int i, mu_sr;

  lrp_cyc = lrp_mus = 0;

  while(cyc > cyc_done) {
    pppc = SekPc;
	ppop = m68k_read_disassembler_16(pppc);
	memcpy(old_regs, PicoCpu.d, 4*16);
	old_sr = CycloneGetSr(&PicoCpu);

	PicoCpu.cycles=1;
    CycloneRun(&PicoCpu);
	cyc_cyclone=1-PicoCpu.cycles;
	cyc_musashi=m68k_execute(1);

	if(cyc_cyclone != cyc_musashi) {
	  dprintf("cycles: %i vs %i", cyc_cyclone, cyc_musashi);
	  err=1;
	}

	if(lrp_cyc != lrp_mus) {
	  dprintf("lrp: %i vs %i", lrp_cyc&15, lrp_mus&15);
	  err=1;
	}

	if(lwp_cyc != lwp_mus) {
	  dprintf("lwp: %i vs %i", lwp_cyc&15, lwp_mus&15);
	  err=1;
	}

	for(i=0; i < 16; i++) {
	  if(lastwrite_cyc_d[i] != lastwrite_mus_d[i]) {
	    dprintf("lastwrite: [%i]= %08x vs %08x", i, lastwrite_cyc_d[i], lastwrite_mus_d[i]);
 	    err=1;
		break;
	  }
	}

	// compare PC
	if( SekPc != (m68ki_cpu.pc&0xffffff) ) {
	  dprintf("PC: %06x vs %06x", SekPc, m68ki_cpu.pc&0xffffff);
	  err=1;
	}

#if 0
	if( SekPc > Pico.romsize || SekPc < 0x200 ) {
	  dprintf("PC out of bounds: %06x", SekPc);
	  err=1;
	}
#endif

    // compare regs
	for(i=0; i < 16; i++) {
	  if(PicoCpu.d[i] != m68ki_cpu.dar[i]) {
	    str = (i < 8) ? "d" : "a";
	    dprintf("reg: %s%i: %08x vs %08x", str, i&7, PicoCpu.d[i], m68ki_cpu.dar[i]);
 	    err=1;
		break;
	  }
	}

	// SR
	if((CycloneGetSr(&PicoCpu)) != (mu_sr = m68k_get_reg(NULL, M68K_REG_SR))) {
	  dprintf("SR: %04x vs %04x (??s? 0iii 000x nzvc)", CycloneGetSr(&PicoCpu), mu_sr);
	  err=1;
	}

	// IRQl
	if(PicoCpu.irq != (m68ki_cpu.int_level>>8)) {
	  dprintf("IRQ: %i vs %i", PicoCpu.irq, (m68ki_cpu.int_level>>8));
	  err=1;
	}

	// OSP/USP
	if(PicoCpu.osp != m68ki_cpu.sp[((mu_sr>>11)&4)^4]) {
	  dprintf("OSP: %06x vs %06x", PicoCpu.osp, m68ki_cpu.sp[0]);
	  err=1;
	}

	// stopped
	if((PicoCpu.stopped && !m68ki_cpu.stopped) || (!PicoCpu.stopped && m68ki_cpu.stopped)) {
	  dprintf("stopped: %i vs %i", PicoCpu.stopped, m68ki_cpu.stopped);
	  err=1;
	}

    if(err) dumpPCandExit();

#if 0
    m68k_set_reg(M68K_REG_SR, ((mu_sr-1)&~0x2000)|(mu_sr&0x2000)); // broken
	CycloneSetSr(&PicoCpu, ((mu_sr-1)&~0x2000)|(mu_sr&0x2000));
    PicoCpu.stopped = m68ki_cpu.stopped = 0;
	if(SekPc > 0x400 && (PicoCpu.a[7] < 0xff0000 || PicoCpu.a[7] > 0xffffff)) 
	  PicoCpu.a[7] = m68ki_cpu.dar[15] = 0xff8000;
#endif

	cyc_done += cyc_cyclone;
	ops++;
  }

  return cyc_done;
}
