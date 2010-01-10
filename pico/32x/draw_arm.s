@ vim:filetype=armasm

.extern Pico32x
.extern PicoDraw2FB
.extern HighPal

.equiv P32XV_PRI,  (1<< 7)

.bss
.align 2
.global Pico32xNativePal
Pico32xNativePal:
    .word 0

.text
.align 2


.macro call_scan_prep cond
.if \cond
    ldr     r4, =PicoScan32xBegin
    ldr     r5, =PicoScan32xEnd
    ldr     r6, =DrawLineDest
    ldr     r4, [r4]
    ldr     r5, [r5]
    stmfd   sp!, {r4,r5,r6}
.endif
.endm

.macro call_scan_fin_ge cond
.if \cond
    addge   sp, sp, #4*3
.endif
.endm

.macro call_scan_begin cond
.if \cond
    stmfd   sp!, {r1-r3}
    and     r0, r2, #0xff
    add     r0, r0, r4
    mov     lr, pc
    ldr     pc, [sp, #(3+0)*4]
    ldr     r0, [sp, #(3+2)*4] @ &DrawLineDest
    ldmfd   sp!, {r1-r3}
    ldr     r0, [r0]
.endif
.endm

.macro call_scan_end cond
.if \cond
    stmfd   sp!, {r0-r3}
    and     r0, r2, #0xff
    add     r0, r0, r4
    mov     lr, pc
    ldr     pc, [sp, #(4+1)*4]
    ldmfd   sp!, {r0-r3}
.endif
.endm

@ direct color
@ unsigned short *dst, unsigned short *dram, int lines_offs, int mdbg
.macro make_do_loop_dc name call_scan do_md
.global \name
\name:
    stmfd   sp!, {r4-r11,lr}

    ldr     r10,=Pico32x
    ldr     r11,=PicoDraw2FB
    ldr     r10,[r10, #0x40] @ Pico32x.vdp_regs[0]
    ldr     r11,[r11]
    ldr     r9, =HighPal     @ palmd
    add     r11,r11,#(328*8) @ r11 = pmd: md data
    tst     r10,#P32XV_PRI
    moveq   r10,#0
    movne   r10,#0x8000      @ r10 = inv_bit
    call_scan_prep \call_scan

    mov     r4, #0           @ line
    b       1f @ loop_outer_entry

0: @ loop_outer:
    call_scan_end \call_scan
    add     r4, r4, #1
    sub     r11,r11,#1       @ adjust for prev read
    cmp     r4, r2, lsr #16
    call_scan_fin_ge \call_scan
    ldmgefd sp!, {r4-r11,pc}

1: @ loop_outer_entry:
    call_scan_begin \call_scan
    mov     r12,r4, lsl #1
    ldrh    r12,[r1, r12]
    add     r11,r11,#8
    mov     r6, #320
    add     r5, r1, r12, lsl #1 @ p32x = dram + dram[l]

2: @ loop_inner:
    ldrb    r7, [r11], #1    @ MD pixel
    subs    r6, r6, #1
    blt     0b @ loop_outer
    ldrh    r8, [r5], #2     @ 32x pixel
    cmp     r7, r3           @ MD has bg pixel?
    beq     3f @ draw32x
    eor     r12,r8, r10
    ands    r12,r12,#0x8000  @ !((t ^ inv) & 0x8000)
.if \do_md
    mov     r7, r7, lsl #1
    ldreqh  r12,[r9, r7]
    streqh  r12,[r0], #2     @ *dst++ = palmd[*pmd]
.endif
    beq     2b @ loop_inner

3: @ draw32x:
    and     r12,r8, #0x03e0
    mov     r8, r8, lsl #11
    orr     r8, r8, r8, lsr #(10+11)
    orr     r8, r8, r12,lsl #1
    bic     r8, r8, #0x0020  @ kill prio bit
    strh    r8, [r0], #2     @ *dst++ = bgr2rgb(*p32x++)
    b       2b @ loop_inner
.endm


@ packed pixel
.macro do_pixel_pp do_md
    ldrb    r7, [r11], #1    @ MD pixel
    eor     r12,r5, #1
    ldrb    r8, [r12]        @ palette index
    cmp     r7, r3           @ MD has bg pixel?
    mov     r12,r8,lsl #1
    ldrh    r8, [r10,r12]    @ t = 32x pixel
    mov     r7, r7, lsl #1
    add     r5, r5, #1
    eorne   r12,r8, #0x20
    tstne   r12, #0x20
.if \do_md
    ldrneh  r8, [r9, r7]     @ t = palmd[*pmd]
    subs    r6, r6, #1
    strh    r8, [r0], #2     @ *dst++ = t
.else
    streqh  r8, [r0], #2
    addne   r0, r0, #2
    subs    r6, r6, #1
.endif
.endm

@ unsigned short *dst, unsigned short *dram, int lines_offs, int mdbg
.macro make_do_loop_pp name call_scan do_md
.global \name
\name:
    stmfd   sp!, {r4-r11,lr}

    ldr     r11,=PicoDraw2FB
    ldr     r10,=Pico32xNativePal
    ldr     r11,[r11]
    ldr     r10,[r10]
    ldr     r9, =HighPal     @ palmd
    add     r11,r11,#(328*8) @ r11 = pmd: md data
    call_scan_prep \call_scan

    mov     r4, #0           @ line
    b       1f @ loop_outer_entry

0: @ loop_outer:
    call_scan_end \call_scan
    add     r4, r4, #1
    cmp     r4, r2, lsr #16
    call_scan_fin_ge \call_scan
    ldmgefd sp!, {r4-r11,pc}

1: @ loop_outer_entry:
    call_scan_begin \call_scan
    mov     r12,r4, lsl #1
    ldrh    r12,[r1, r12]
    add     r11,r11,#8
    mov     r6, #320
    add     r5, r1, r12, lsl #1 @ p32x = dram + dram[l]

2: @ loop_inner:
    do_pixel_pp \do_md
    do_pixel_pp \do_md
    bgt     2b @ loop_inner
    b       0b @ loop_outer
.endm


@ run length
@ unsigned short *dst, unsigned short *dram, int lines_offs, int mdbg
.macro make_do_loop_rl name call_scan do_md
.global \name
\name:
    stmfd   sp!, {r4-r11,lr}

    ldr     r11,=PicoDraw2FB
    ldr     r10,=Pico32xNativePal
    ldr     r11,[r11]
    ldr     r10,[r10]
    ldr     r9, =HighPal     @ palmd
    add     r11,r11,#(328*8) @ r11 = pmd: md data
    call_scan_prep \call_scan

    mov     r4, #0           @ line
    b       1f @ loop_outer_entry

0: @ loop_outer:
    call_scan_end \call_scan
    add     r4, r4, #1
    sub     r11,r11,#1       @ adjust for prev read
    cmp     r4, r2, lsr #16
    call_scan_fin_ge \call_scan
    ldmgefd sp!, {r4-r11,pc}

1: @ loop_outer_entry:
    call_scan_begin \call_scan
    mov     r12,r4, lsl #1
    ldrh    r12,[r1, r12]
    add     r11,r11,#8
    mov     r6, #320
    add     r5, r1, r12, lsl #1 @ p32x = dram + dram[l]

2: @ loop_inner:
    ldrh    r8, [r5], #2     @ control word
    and     r12,r8, #0xff
    mov     r12,r12,lsl #1
    ldrh    lr, [r10,r12]    @ t = 32x pixel
    eor     lr, lr, #0x20

3: @ loop_innermost:
    ldrb    r7, [r11], #1    @ MD pixel
    subs    r6, r6, #1
    blt     0b @ loop_outer
    cmp     r7, r3           @ MD has bg pixel?
    mov     r7, r7, lsl #1
    tstne   lr, #0x20
.if \do_md
    ldrneh  r12,[r9, r7]     @ t = palmd[*pmd]
    streqh  lr, [r0], #2
    strneh  r12,[r0], #2     @ *dst++ = t
.else
    streqh  lr, [r0]
    add     r0, r0, #2
.endif
    subs    r8, r8, #0x100
    bge     3b @ loop_innermost
    b       2b @ loop_inner
.endm


make_do_loop_dc do_loop_dc,         0, 0
make_do_loop_dc do_loop_dc_md,      0, 1
make_do_loop_dc do_loop_dc_scan,    1, 0
make_do_loop_dc do_loop_dc_scan_md, 1, 1

make_do_loop_pp do_loop_pp,         0, 0
make_do_loop_pp do_loop_pp_md,      0, 1
make_do_loop_pp do_loop_pp_scan,    1, 0
make_do_loop_pp do_loop_pp_scan_md, 1, 1

make_do_loop_rl do_loop_rl,         0, 0
make_do_loop_rl do_loop_rl_md,      0, 1
make_do_loop_rl do_loop_rl_scan,    1, 0
make_do_loop_rl do_loop_rl_scan_md, 1, 1

