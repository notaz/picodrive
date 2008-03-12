@ vim:filetype=armasm

.if 0
#include "compiler.h"
.endif

.global tcache

.global flush_inval_caches
.global ssp_regfile_load
.global ssp_regfile_store
.global ssp_drc_entry
.global ssp_drc_next
.global ssp_hle_800

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

ssp_regfile_load:
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


ssp_regfile_store:
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


#define SSP_OFFS_GR         0x400
#define SSP_PM0                 8
#define SSP_PC                  6
#define SSP_OFFS_EMUSTAT    0x484 // emu_status
#define SSP_OFFS_IRAM_DIRTY 0x494
#define SSP_OFFS_IRAM_CTX   0x498 // iram_context
#define SSP_OFFS_BLTAB      0x49c // block_table
#define SSP_OFFS_BLTAB_IRAM 0x4a0
#define SSP_OFFS_TMP0       0x4a4
#define SSP_WAIT_PM0       0x2000


ssp_drc_entry:
    stmfd   sp!, {r4-r11, lr}
    mov     r11, r0
    bl      ssp_regfile_load

ssp_drc_next:
    cmp     r11, #0
    bmi     ssp_drc_end

    ldr     r0, [r7, #(SSP_OFFS_GR+SSP_PC*4)]
    mov     r0, r0, lsr #16
    str     r0, [r7, #SSP_OFFS_TMP0]
    cmp     r0, #0x400
    blt     ssp_de_iram

    ldr     r1, [r7, #SSP_OFFS_BLTAB]
    ldr     r1, [r1, r0, lsl #2]
    tst     r1, r1
    bxne    r1
    bl      ssp_translate_block
    ldr     r2, [r7, #SSP_OFFS_TMP0]		@ entry PC
    ldr     r1, [r7, #SSP_OFFS_BLTAB]
    str     r0, [r1, r2, lsl #2]
    bx      r0

ssp_de_iram:
    ldr     r1, [r7, #SSP_OFFS_IRAM_DIRTY]
    tst     r1, r1
    ldreq   r1, [r7, #SSP_OFFS_IRAM_CTX]
    beq     ssp_de_iram_ctx

    bl      ssp_get_iram_context
    mov     r1, #0
    str     r1, [r7, #SSP_OFFS_IRAM_DIRTY]
    mov     r1, r0
    str     r1, [r7, #SSP_OFFS_IRAM_CTX]
    ldr     r0, [r7, #SSP_OFFS_TMP0]		@ entry PC
    
ssp_de_iram_ctx:
    ldr     r2, [r7, #SSP_OFFS_BLTAB_IRAM]
    add     r2, r2, r1, lsl #12			@ block_tab_iram + iram_context * 0x800/2*4
    add     r2, r2, r0, lsl #2
    ldr     r1, [r2]
    tst     r1, r1
    bxne    r1
    str     r2, [r7, #SSP_OFFS_TMP0]
    bl      ssp_translate_block
    ldr     r2, [r7, #SSP_OFFS_TMP0]		@ &block_table_iram[iram_context][rPC]
    str     r0, [r2]
    bx      r0

ssp_drc_end:
    bl      ssp_regfile_store
    mov     r0, r11
    ldmfd   sp!, {r4-r11, lr}
    bx      lr



@ ld      A, PM0
@ andi    2
@ bra     z=1, gloc_0800
ssp_hle_800:
    @ block prologue
@    stmfd   sp!, {r4-r11, lr}
@    bl      regfile_load
@    mov     r11, #0
    
    ldr     r0, [r7, #(SSP_OFFS_GR+SSP_PM0*4)]
    ldr     r1, [r7, #SSP_OFFS_EMUSTAT]
    tst     r0, #0x20000
    orreq   r1, r1, #SSP_WAIT_PM0
    addeq   r11,r11, #1024
    streq   r1, [r7, #SSP_OFFS_EMUSTAT]
    movne   r0,     #0x04000000
    orrne   r0, r0, #0x00040000
    strne   r0, [r7, #(SSP_OFFS_GR+SSP_PC*4)]

    bl      ssp_drc_next
@    bl      regfile_store
@    add     r0, r11, #3
@    ldmfd   sp!, {r4-r11, lr}
@    bx      lr


