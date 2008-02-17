@ vim:filetype=armasm


@ register map:
@ r4:  XXYY
@ r5:  A
@ r6:  STACK and emu flags
@ r7:  SSP context
@ r8:  r0-r2
@ r9:  r4-r6
@ r10: P

.global flush_inval_dcache
.global flush_inval_icache

.text
.align 4

flush_inval_dcache:
  mov r2, #0x0  @ ??
  swi 0x9f0002
  bx lr

flush_inval_icache:
  mov r2, #0x1
  swi 0x9f0002
  bx lr

