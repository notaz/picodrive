@ vim:filetype=armasm


@ register map:
@ r4:  XXYY
@ r5:  A
@ r6:  STACK and emu flags
@ r7:  SSP context
@ r8:  r0-r2
@ r9:  r4-r6
@ r10: P

.global flush_inval_caches

.text
.align 4

flush_inval_caches:
  mov r2, #0x0  @ must be 0
  swi 0x9f0002
  bx lr


