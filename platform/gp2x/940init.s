.global code940

code940:                          @ interrupt table:
    b .b_reset                    @ reset
    b .b_undef                    @ undefined instructions
    b .b_swi                      @ software interrupt
    b .b_pabort                   @ prefetch abort
    b .b_dabort                   @ data abort
    b .b_reserved                 @ reserved
    b .b_irq                      @ IRQ
    b .b_fiq                      @ FIQ

@ test
.b_reset:
    mov     r12, #0
    b       .Begin
.b_undef:
    mov     r12, #1
    b       .Begin
.b_swi:
    mov     r12, #2
    b       .Begin
.b_pabort:
    mov     r12, #3
    b       .Begin
.b_dabort:
    mov     r12, #4
    b       .Begin
.b_reserved:
    mov     r12, #5
    b       .Begin
.b_irq:
    mov     r12, #6
    mov     sp, #0x100000       @ reset stack
    sub     sp, sp, #4
    mov     r1, #0xbe000000     @ assume we live @ 0x2000000 bank
    orr     r2, r1, #0x3B00
    orr     r2, r2, #0x0046
    mvn     r3, #0
    strh    r3, [r2]            @ clear any pending interrupts from the DUALCPU unit
    orr     r2, r1, #0x4500
    str     r3, [r2]            @ clear all pending interrupts in irq controller's SRCPND register
    orr     r2, r2, #0x0010
    str     r3, [r2]            @ clear all pending interrupts in irq controller's INTPND register
    b       .Enter
.b_fiq:
    mov     r12, #7
    b       .Begin

.Begin:
    mov sp, #0x100000           @ set the stack top (1M)
    sub sp, sp, #4              @ minus 4

    @ set up memory region 0 -- the whole 4GB address space
    mov r0, #(0x1f<<1)|1        @ region data
    mcr p15, 0, r0, c6, c0, 0   @ opcode2 ~ data/instr
    mcr p15, 0, r0, c6, c0, 1

    @ set up region 1 which is the first 2 megabytes.
    mov r0, #(0x14<<1)|1        @ region data
    mcr p15, 0, r0, c6, c1, 0
    mcr p15, 0, r0, c6, c1, 1

    @ set up region 2: 64k 0x200000-0x210000
    mov r0, #(0x0f<<1)|1
    orr r0, r0, #0x200000
    mcr p15, 0, r0, c6, c2, 0
    mcr p15, 0, r0, c6, c2, 1

    @ set up region 3: 64k 0xbe000000-0xbe010000 (hw control registers)
    mov r0, #(0x0f<<1)|1
    orr r0, r0, #0xbe000000
    mcr p15, 0, r0, c6, c3, 0
    mcr p15, 0, r0, c6, c3, 1

    @ set region 1 to be cacheable (so the first 2M will be cacheable)
    mov r0, #2
    mcr p15, 0, r0, c2, c0, 0
    mcr p15, 0, r0, c2, c0, 1

    @ set region 1 to be bufferable too (only data)
    mcr p15, 0, r0, c3, c0, 0

    @ set protection, allow accsess only to regions 1 and 2
    mov r0, #(3<<6)|(3<<4)|(3<<2)|(0)  @ data: [full, full, full, no access] for regions [3 2 1 0]
    mcr p15, 0, r0, c5, c0, 0
    mov r0, #(0<<6)|(0<<4)|(3<<2)|(0)  @ instructions: [no access, no, full, no]
    mcr p15, 0, r0, c5, c0, 1

    mrc p15, 0, r0, c1, c0, 0   @ fetch current control reg
    orr r0, r0, #1              @ 0x00000001: enable protection unit
    orr r0, r0, #4              @ 0x00000004: enable D cache
    orr r0, r0, #0x1000         @ 0x00001000: enable I cache
    orr r0, r0, #0xC0000000     @ 0xC0000000: async+fastbus
    mcr p15, 0, r0, c1, c0, 0   @ set control reg

    @ flush (invalidate) the cache (just in case)
    mov r0, #0
    mcr p15, 0, r0, c7, c6, 0

.Enter:
    mov r0, r12
    bl Main940

    @ we should never get here
.b_deadloop:
    b .b_deadloop



@ so asm utils are also defined here:
.global spend_cycles @ c

spend_cycles:
    mov     r0, r0, lsr #2  @ 4 cycles/iteration
    sub     r0, r0, #2      @ entry/exit/init
.sc_loop:
    subs    r0, r0, #1
    bpl     .sc_loop

    bx      lr


@ clean-flush function from ARM940T technical reference manual
.global cache_clean_flush

cache_clean_flush:
    mov     r1, #0                  @ init line counter
ccf_outer_loop:
    mov     r0, #0                  @ segment counter
ccf_inner_loop:
    orr     r2, r1, r0              @ make segment and line address
    mcr     p15, 0, r2, c7, c14, 2  @ clean and flush that line
    add     r0, r0, #0x10           @ incremet secment counter
    cmp     r0, #0x40               @ complete all 4 segments?
    bne     ccf_inner_loop
    add     r1, r1, #0x04000000     @ increment line counter
    cmp     r1, #0                  @ complete all lines?
    bne     ccf_outer_loop
    bx      lr


@ clean-only version
.global cache_clean

cache_clean:
    mov     r1, #0                  @ init line counter
cf_outer_loop:
    mov     r0, #0                  @ segment counter
cf_inner_loop:
    orr     r2, r1, r0              @ make segment and line address
    mcr     p15, 0, r2, c7, c10, 2  @ clean that line
    add     r0, r0, #0x10           @ incremet secment counter
    cmp     r0, #0x40               @ complete all 4 segments?
    bne     cf_inner_loop
    add     r1, r1, #0x04000000     @ increment line counter
    cmp     r1, #0                  @ complete all lines?
    bne     cf_outer_loop
    bx      lr


.global wait_irq

wait_irq:
    mrs     r0, cpsr
    bic     r0, r0, #0x80
    msr     cpsr_c, r0               @ enable interrupts

    mov     r0, #0
    mcr     p15, 0, r0, c7, c0, 4   @ wait for IRQ
@    mcr     p15, 0, r0, c15, c8, 2
    b       .b_reserved

.pool

@ vim:filetype=ignored:
