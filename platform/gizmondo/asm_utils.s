@ vim:filetype=armasm


.global vidCpy8to16_40 @ void *dest, void *src, short *pal, int lines

vidCpy8to16_40:
    stmfd   sp!, {r4-r9,lr}

    mov     r3, r3, lsr #1
    orr     r3, r3, r3, lsl #8
    orr     r3, r3, #(320/8-1)<<24
    add     r1, r1, #8
    mov     lr, #0xff
    mov     lr, lr, lsl #1

    @ even lines
vcloop_40_aligned:
    ldr     r12, [r1], #4
    ldr     r7,  [r1], #4

    and     r4, lr, r12, lsl #1
    ldrh    r4, [r2, r4]
    and     r5, lr, r12, lsr #7
    ldrh    r5, [r2, r5]
    and     r6, lr, r12, lsr #15
    ldrh    r6, [r2, r6]
    orr     r4, r4, r5, lsl #16

    and     r5, lr, r12, lsr #23
    ldrh    r5, [r2, r5]
    and     r8, lr, r7, lsl #1
    ldrh    r8, [r2, r8]
    orr     r5, r6, r5, lsl #16

    and     r6, lr, r7, lsr #7
    ldrh    r6, [r2, r6]
    and     r12,lr, r7, lsr #15
    ldrh    r12,[r2, r12]
    and     r9, lr, r7, lsr #23
    ldrh    r9, [r2, r9]
    orr     r8, r8, r6, lsl #16

    subs    r3, r3, #1<<24
    orr     r12,r12, r9, lsl #16

    stmia   r0!, {r4,r5,r8,r12}
    bpl     vcloop_40_aligned

    add     r1, r1, #336             @ skip a line and 1 col
    add     r0, r0, #320*2+2*2
    add     r3, r3, #(320/8)<<24
    sub     r3, r3, #1
    tst     r3, #0xff
    bne     vcloop_40_aligned

    and     r4, r3, #0xff00
    orr     r3, r3, r4, lsr #8
    mov     r4, r4, lsr #7
    sub     r6, r4, #1
    mov     r5, #320*2
    add     r5, r5, #2
    mul     r4, r5, r6
    sub     r0, r0, r4
    mov     r5, #328
    mul     r4, r5, r6
    sub     r1, r1, r4

vcloop_40_unaligned_outer:
    ldr     r12, [r1], #4
    ldr     r7,  [r1], #4

    and     r4, lr, r12, lsl #1
    ldrh    r4, [r2, r4]
    and     r5, lr, r12, lsr #7
    ldrh    r5, [r2, r5]
    strh    r4, [r0], #2
    b       vcloop_40_unaligned_enter

vcloop_40_unaligned:
    ldr     r12, [r1], #4
    ldr     r7,  [r1], #4

    and     r6, lr, r12, lsl #1
    ldrh    r6, [r2, r6]
    and     r5, lr, r12, lsr #7
    ldrh    r5, [r2, r5]
    orr     r4, r4, r6, lsl #16
    str     r4, [r0], #4

vcloop_40_unaligned_enter:
    and     r6, lr, r12, lsr #15
    ldrh    r6, [r2, r6]

    and     r4, lr, r12, lsr #23
    ldrh    r4, [r2, r4]
    orr     r5, r5, r6, lsl #16

    and     r8, lr, r7, lsl #1
    ldrh    r8, [r2, r8]
    and     r6, lr, r7, lsr #7
    ldrh    r6, [r2, r6]
    orr     r8, r4, r8, lsl #16

    and     r12,lr, r7, lsr #15
    ldrh    r12,[r2, r12]

    and     r4, lr, r7, lsr #23
    ldrh    r4, [r2, r4]
    orr     r12,r6, r12,lsl #16
    subs    r3, r3, #1<<24

    stmia   r0!, {r5,r8,r12}
    bpl     vcloop_40_unaligned

    strh    r4, [r0], #2

    add     r1, r1, #336             @ skip a line and 1 col
    add     r0, r0, #320*2+2*2
    add     r3, r3, #(320/8)<<24
    sub     r3, r3, #1
    tst     r3, #0xff
    bne     vcloop_40_unaligned_outer

    ldmfd   sp!, {r4-r9,lr}
    bx      lr


