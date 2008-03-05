@ vim:filetype=armasm

.if 0
#include "compiler.h"
.endif

.global tcache

.global flush_inval_caches
.global regfile_load
.global regfile_store

@ translation cache buffer
.text
.align 12 @ 4096
.size tcache, TCACHE_SIZE
tcache:
 .space TCACHE_SIZE


.text
.align 2


flush_inval_caches:
    mov     r2, #0x0  @ must be 0
    swi     0x9f0002
    bx      lr


@       SSP_GR0, SSP_X,     SSP_Y,   SSP_A,
@       SSP_ST,  SSP_STACK, SSP_PC,  SSP_P,
@       SSP_PM0, SSP_PM1,   SSP_PM2, SSP_XST,
@       SSP_PM4, SSP_gr13,  SSP_PMC, SSP_AL

@ register map:
@ r4:  XXYY
@ r5:  A
@ r6:  STACK and emu flags: sss0 * .uu. .lll NZCV (NZCV is PSR bits from ARM)
@ r7:  SSP context
@ r8:  r0-r2 (.210)
@ r9:  r4-r6 (.654)
@ r10: P
@ r11: cycles

@ trashes r2,r3

regfile_load:
    ldr     r7, =ssp
    ldr     r7, [r7]
    add     r2, r7, #0x400
    add     r2, r2, #4
    ldmia   r2, {r3,r4,r5,r6,r8}
    mov     r3, r3, lsr #16
    mov     r3, r3, lsl #16
    orr     r4, r3, r4, lsr #16         @ XXYY

    and     r8, r8, #0x0f0000
    mov     r8, r8, lsl #13             @ sss0 *
    and     r9, r6, #0x670000
    tst     r6,     #0x80000000
    orrne   r8, r8, #0x8
    tst     r6,     #0x20000000
    orrne   r8, r8, #0x4                @ sss0 *           NZ..
    orr     r6, r8, r9, lsr #12         @ sss0 * .uu. .lll NZ..

    ldr     r8, [r7, #0x440]            @ r0-r2
    ldr     r9, [r7, #0x444]            @ r4-r6
    ldr     r10,[r7, #(0x400+7*4)]      @ P
    bx      lr


regfile_store:
    str     r10,[r7, #(0x400+7*4)]      @ P
    str     r8, [r7, #0x440]            @ r0-r2
    str     r9, [r7, #0x444]            @ r4-r6

    mov     r9, r6, lsr #13
    and     r9, r9, #(7<<16)            @ STACK
    mov     r3, r6, lsl #28
    msr     cpsr_flg, r3                @ to to ARM PSR
    and     r6, r6, #0x670
    mov     r6, r6, lsl #12
    orrmi   r6, r6, #0x80000000         @ N
    orreq   r6, r6, #0x20000000         @ Z

    mov     r3, r4, lsl #16             @ Y
    mov     r2, r4, lsr #16
    mov     r2, r2, lsl #16             @ X
    add     r8, r7, #0x400
    add     r8, r8, #4
    stmia   r8, {r2,r3,r5,r6,r9}
    bx      lr



