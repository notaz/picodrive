@ vim:filetype=armasm
@ some color conversion and blitting routines

@ (c) Copyright 2006, 2007 notaz
@ All Rights Reserved

@ vim:filetype=armasm

.text
.align 4

@ Convert 0000bbb0 ggg0rrr0 0000bbb0 ggg0rrr0
@ to      00000000 rrr00000 ggg00000 bbb00000 ...

@ lr =  0x00e000e0, out: r3=lower_pix, r2=higher_pix; trashes rin
@ if sh==2, r8=0x00404040 (sh!=0 destroys flags!)
.macro convRGB32_2 rin sh=0
    and     r2,  lr, \rin, lsr #4 @ blue
    and     r3,  \rin, lr
    orr     r2,  r2,   r3, lsl #8         @ g0b0g0b0

    mov     r3,  r2,  lsl #16             @ g0b00000
    and     \rin,lr,  \rin, ror #12       @ 00r000r0 (reversed)
    orr     r3,  r3,  \rin, lsr #16       @ g0b000r0
.if \sh == 1
    mov     r3,  r3,  ror #17             @ shadow mode
.elseif \sh == 2
    adds    r3,  r3,  #0x40000000         @ green
    orrcs   r3,  r3,  #0xe0000000
    mov     r3,  r3,  ror #8
    adds    r3,  r3,  #0x40000000
    orrcs   r3,  r3,  #0xe0000000
    mov     r3,  r3,  ror #16
    adds    r3,  r3,  #0x40000000
    orrcs   r3,  r3,  #0xe0000000
    mov     r3,  r3,  ror #24
.else
    mov     r3,  r3,  ror #16             @ r3=low
.endif

    orr     r3,  r3,   r3, lsr #3
    str     r3, [r0], #4

    mov     r2,  r2,  lsr #16
    orr     r2,  r2,  \rin, lsl #16
.if \sh == 1
    mov     r2,  r2,  lsr #1
.elseif \sh == 2
    mov     r2,  r2,  ror #8
    adds    r2,  r2,  #0x40000000         @ blue
    orrcs   r2,  r2,  #0xe0000000
    mov     r2,  r2,  ror #8
    adds    r2,  r2,  #0x40000000
    orrcs   r2,  r2,  #0xe0000000
    mov     r2,  r2,  ror #8
    adds    r2,  r2,  #0x40000000
    orrcs   r2,  r2,  #0xe0000000
    mov     r2,  r2,  ror #8
.endif

    orr     r2,  r2,   r2,  lsr #3
    str     r2, [r0], #4
.endm


.global vidConvCpyRGB32 @ void *to, void *from, int pixels

vidConvCpyRGB32:
    stmfd   sp!, {r4-r7,lr}

    mov     r12, r2, lsr #3 @ repeats
    mov     lr, #0x00e00000
    orr     lr, lr, #0x00e0

.loopRGB32:
    subs    r12, r12, #1

    ldmia    r1!, {r4-r7}
    convRGB32_2 r4
    convRGB32_2 r5
    convRGB32_2 r6
    convRGB32_2 r7

    bgt     .loopRGB32

    ldmfd   sp!, {r4-r7,lr}
    bx      lr


.global vidConvCpyRGB32sh @ void *to, void *from, int pixels

vidConvCpyRGB32sh:
    stmfd   sp!, {r4-r7,lr}

    mov     r12, r2, lsr #3 @ repeats
    mov     lr, #0x00e00000
    orr     lr, lr, #0x00e0

.loopRGB32sh:
    subs    r12, r12, #1

    ldmia    r1!, {r4-r7}
    convRGB32_2 r4, 1
    convRGB32_2 r5, 1
    convRGB32_2 r6, 1
    convRGB32_2 r7, 1

    bgt     .loopRGB32sh

    ldmfd   sp!, {r4-r7,lr}
    bx      lr


.global vidConvCpyRGB32hi @ void *to, void *from, int pixels

vidConvCpyRGB32hi:
    stmfd   sp!, {r4-r7,lr}

    mov     r12, r2, lsr #3 @ repeats
    mov     lr, #0x00e00000
    orr     lr, lr, #0x00e0

.loopRGB32hi:
     ldmia    r1!, {r4-r7}
    convRGB32_2 r4, 2
    convRGB32_2 r5, 2
    convRGB32_2 r6, 2
    convRGB32_2 r7, 2

    subs    r12, r12, #1
    bgt     .loopRGB32hi

    ldmfd   sp!, {r4-r7,lr}
    bx      lr


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


@ mode2 blitter for 40 cols
.global vidCpyM2_40col @ void *dest, void *src

vidCpyM2_40col:
    stmfd   sp!, {r4-r6,lr}

    mov     r12, #224       @ lines
    add     r1, r1, #8

vidCpyM2_40_loop_out:
    mov     r6, #10
vidCpyM2_40_loop:
    subs    r6, r6, #1
     ldmia    r1!, {r2-r5}
     stmia    r0!, {r2-r5}
     ldmia    r1!, {r2-r5}
     stmia    r0!, {r2-r5}
    bne     vidCpyM2_40_loop
    subs    r12,r12,#1
    add     r1, r1, #8
    bne     vidCpyM2_40_loop_out

    ldmfd   sp!, {r4-r6,lr}
    bx      lr


@ mode2 blitter for 32 cols
.global vidCpyM2_32col @ void *dest, void *src

vidCpyM2_32col:
    stmfd   sp!, {r4-r6,lr}

    mov     r12, #224       @ lines
    add     r1, r1, #8
    add     r0, r0, #32

vidCpyM2_32_loop_out:
    mov     r6, #8
vidCpyM2_32_loop:
    subs    r6, r6, #1
     ldmia    r1!, {r2-r5}
     stmia    r0!, {r2-r5}
     ldmia    r1!, {r2-r5}
     stmia    r0!, {r2-r5}
    bne     vidCpyM2_32_loop
    subs    r12,r12,#1
    add     r0, r0, #64
    add     r1, r1, #8+64
    bne     vidCpyM2_32_loop_out

    ldmfd   sp!, {r4-r6,lr}
    bx      lr


@ mode2 blitter for 32 cols with no borders
.global vidCpyM2_32col_nobord @ void *dest, void *src

vidCpyM2_32col_nobord:
    stmfd   sp!, {r4-r6,lr}

    mov     r12, #224       @ lines
    add     r1, r1, #8
    b       vidCpyM2_32_loop_out


.global spend_cycles @ c

spend_cycles:
    mov     r0, r0, lsr #2  @ 4 cycles/iteration
    sub     r0, r0, #2      @ entry/exit/init
.sc_loop:
    subs    r0, r0, #1
    bpl     .sc_loop

    bx      lr


