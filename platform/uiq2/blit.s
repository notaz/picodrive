@ assembly "optimized" blitter and copy functions
@ all pointers must be word-aligned

@ (c) Copyright 2006, notaz
@ All Rights Reserved


@ Convert 0000bbb0 ggg0rrr0
@ to      0000rrr0 ggg0bbb0

@ r2,r3 - scratch, lr = 0x000F000F
.macro convRGB444 reg
    and     r2,   \reg, lr         @ r2=red
    and     r3,   \reg, lr, lsl #8 @ r3=blue
    and     \reg, \reg, lr, lsl #4 @ green stays in place
    orr     \reg, \reg, r2, lsl #8 @ add red back
    orr     \reg, \reg, r3, lsr #8 @ add blue back
.endm

.global vidConvCpyRGB444 @ void *to, void *from, int pixels

vidConvCpyRGB444:
    stmfd   sp!, {r4-r11,lr}

    mov     r12, r2, lsr #4 @ repeats
    mov     lr, #0xF0000
    orr     lr, lr, #0xF    @ lr == pattern 0x000F000F


.loopRGB444:
	subs    r12, r12, #1

    @ I first thought storing multiple registers would be faster,
    @ but this doesn't seem to be the case, probably because of
    @ slow video memory we are dealing with
 	ldmia	r1!, {r4-r11}
    convRGB444 r4
    str     r4, [r0], #4
    convRGB444 r5
    str     r5, [r0], #4
    convRGB444 r6
    str     r6, [r0], #4
    convRGB444 r7
    str     r7, [r0], #4
    convRGB444 r8
    str     r8, [r0], #4
    convRGB444 r9
    str     r9, [r0], #4
    convRGB444 r10
    str     r10, [r0], #4
    convRGB444 r11
    str     r11, [r0], #4

    bgt     .loopRGB444


    ldmfd   sp!, {r4-r11,lr}
    bx      lr


@ Convert 0000bbb0 ggg0rrr0
@ to      rrr00ggg 000bbb00

@ r2,r3 - scratch, lr = 0x07800780
.macro convRGB565 reg
    and     r2,   \reg, lr,  lsr #7  @ r2=red
    and     r3,   \reg, lr,  lsl #1  @ r3=blue
    and     \reg, lr,   \reg,lsl #3  @ green stays, but needs shifting
    orr     \reg, \reg, r2,  lsl #12 @ add red back
    orr     \reg, \reg, r3,  lsr #7  @ add blue back
.endm

.global vidConvCpyRGB565 @ void *to, void *from, int pixels

vidConvCpyRGB565:
    stmfd   sp!, {r4-r11,lr}

    mov     r12, r2, lsr #4 @ repeats
    mov     lr, #0x07800000
    orr     lr, lr, #0x780  @ lr == pattern 0x07800780

.loopRGB565:
	subs    r12, r12, #1

 	ldmia	r1!, {r4-r11}
    convRGB565 r4
    str     r4, [r0], #4
    convRGB565 r5
    str     r5, [r0], #4
    convRGB565 r6
    str     r6, [r0], #4
    convRGB565 r7
    str     r7, [r0], #4
    convRGB565 r8
    str     r8, [r0], #4
    convRGB565 r9
    str     r9, [r0], #4
    convRGB565 r10
    str     r10, [r0], #4
    convRGB565 r11
    str     r11, [r0], #4

    bgt     .loopRGB565

    ldmfd   sp!, {r4-r11,lr}
    bx      lr


@ Convert 0000bbb0 ggg0rrr0 0000bbb0 ggg0rrr0
@ to      00000000 rrr00000 ggg00000 bbb00000 ...

@ r2,r3 - scratch, lr = 0x0000F000
@ rin - src reg, rout - dest reg (can be same for both; rout can be r3)
.macro convRGB32_l rout rin
    and     r2,    \rin,  lr,   lsr #12 @ r2=red
    and     r3,    \rin,  lr,   lsr #4  @ r3=blue
    orr     r2,    r3,    r2,   lsl #24
    and     \rout, lr,    \rin, lsl #8  @ green stays, but needs shifting
    orr     \rout, \rout, r2,   lsr #4  @ add red+blue back
.endm

@ r2,r3 - scratch, lr = 0x0000F000
@ rin - src reg, rout - dest reg (can be same for both; rout can be r3)
.macro convRGB32_h rout rin
    and     r2,    \rin,  lr,   lsl #4  @ r2=red
    mov     r3,    \rin,        lsr #24 @ r3=blue
    orr     r2,    r3,    r2
    and     \rout, lr,    \rin, lsr #8  @ green
    orr     \rout, \rout, r2,   lsl #4
.endm

@ slightly faster conversion, saves 1 opcode, writes output
@ lr =  0x00F000F0, out: r3=lower_pix, r2=higher_pix; trashes rin
.macro convRGB32_2 rin rethigh=0
    and     r2,  lr, \rin, lsr #4 @ blue
    and     r3,  \rin, lr
    orr     r2,  r2,   r3, lsl #8         @ g0b0g0b0

    mov     r3,  r2,  lsl #16             @ g0b00000
    and     \rin,lr,  \rin, ror #12       @ 00r000r0 (reversed)
    orr     r3,  r3,  \rin, lsr #16       @ g0b000r0
    mov     r3,  r3,  ror #16             @ r3=low

    str     r3, [r0], #4

    mov     r2,  r2,  lsr #16
.if \rethigh
    orr     \rin,r2,  \rin, lsl #16
.else
    orr     r2,  r2,  \rin, lsl #16
    str     r2, [r0], #4
.endif
.endm


.global vidConvCpyRGB32 @ void *to, void *from, int pixels

vidConvCpyRGB32:
    stmfd   sp!, {r4-r7,lr}

    mov     r12, r2, lsr #3 @ repeats
    mov     lr, #0x00F00000
    orr     lr, lr, #0x00F0

.loopRGB32:
	subs    r12, r12, #1

 	ldmia	r1!, {r4-r7}
    convRGB32_2 r4
    convRGB32_2 r5
    convRGB32_2 r6
    convRGB32_2 r7

    bgt     .loopRGB32

    ldmfd   sp!, {r4-r7,lr}
    bx      lr


@ -------- M2 stuff ---------

.bss
tmpstore1d: .long

.text
tmpstore1:  .long tmpstore1d


@ r3 - scratch, ru - reg with 2 pixels from upper col, rl - ... lower col
.macro rot_str16_90 ru rl
    mov     r3, \rl,lsl #16
    mov     r3, r3, lsr #16
    orr     r3, r3, \ru, lsl #16
    str     r3, [r0], #208*2
    mov     r3, \ru,lsr #16
    mov     r3, r3, lsl #16
    orr     r3, r3, \rl, lsr #16
    str     r3, [r0], #208*2
.endm


.global vidConvCpyM2_16_90 @ void *to, void *from, int width

vidConvCpyM2_16_90:
    stmfd   sp!, {r4-r11,lr}

    ldr     r4, =tmpstore1
    str     sp, [r4]               @ save sp, we will need sp reg..
    mov     sp, r0                 @ .. to store our dst

    @ crashing beyond this point will be fatal (phone reboots), as Symbian OS expects sp to always point to stack

    sub     r2,  r2, #1
    mov     r12, #0x00670000
    orr     r12, r12, r2, lsl #24
    orr     r12, r12, r2           @ r12 == ((208-2)/2 << 16) | ((width-1)<<24) | (width-1)

    add     r0,  r0, #206*2
    add     r1,  r1, #8*2          @ skip left border
    add     lr,  r1, #328*2

.loopM2_16_90:
	subs    r12, r12, #1<<24

 	ldmia	r1!, {r4-r7}
 	ldmia	lr!, {r8-r11}
    rot_str16_90 r4 r8
    rot_str16_90 r5 r9
    rot_str16_90 r6 r10
    rot_str16_90 r7 r11

    bpl     .loopM2_16_90

    add     r12, r12, #1<<24
    subs    r12, r12, #0x00010000
    bmi     .loopM2_16_90_end

    add     r0,  sp,  r12, lsr #14 @ calculate new dst pointer
    orr     r12, r12, r12, lsl #24 @ restore the width counter

    @ skip remaining pixels on these 2 lines
    mov     r4, #328/8-1         @ width of mode2 in line_pixels/8
    sub     r4, r4, r12, lsr #24
    add     r1, lr, r4,  lsl #4  @ skip src pixels
    add     lr, r1, #328*2
    b       .loopM2_16_90

.loopM2_16_90_end:
    @ restore sp
    ldr     r4, =tmpstore1
    ldr     sp, [r4]

    ldmfd   sp!, {r4-r11,lr}
    bx      lr



@ r3 - scratch, ru - reg with 2 pixels from upper col, rl - ... lower col (for right-to-left copies)
.macro rot_str16_270 ru rl
    mov     r3, \rl,lsr #16
    mov     r3, r3, lsl #16
    orr     r3, r3, \ru, lsr #16
    str     r3, [r0], #208*2
    mov     r3, \ru,lsl #16
    mov     r3, r3, lsr #16
    orr     r3, r3, \rl, lsl #16
    str     r3, [r0], #208*2
.endm


.global vidConvCpyM2_16_270 @ void *to, void *from, int width

vidConvCpyM2_16_270:
    stmfd   sp!, {r4-r11,lr}

    ldr     r4, =tmpstore1
    str     sp, [r4]               @ save sp, we will need sp reg to store our dst

    sub     r2,  r2, #1
    mov     r12, #0x00670000
    orr     r12, r12, r2, lsl #24
    orr     r12, r12, r2           @ r12 == ((208-2)/2 << 16) | ((width-1)<<24) | (width-1)

    add     r1,  r1, #328*2        @ skip left border+1line
    add     lr,  r1, #328*2
    add     sp,  r0, #206*2        @ adjust for algo

.loopM2_16_270:
	subs    r12, r12, #1<<24

 	ldmdb	r1!, {r4-r7}
 	ldmdb	lr!, {r8-r11}
    rot_str16_270 r7 r11           @ update the screen in incrementing direction, reduces tearing slightly
    rot_str16_270 r6 r10
    rot_str16_270 r5 r9
    rot_str16_270 r4 r8

    bpl     .loopM2_16_270

    add     r12, r12, #1<<24
    subs    r12, r12, #0x00010000
    bmi     .loopM2_16_90_end      @ same end as in 90

    sub     r0,  sp,  r12, lsr #14 @ calculate new dst pointer
    orr     r12, r12, r12, lsl #24 @ restore the width counter

    @ skip remaining pixels on these 2 lines
    mov     r4, #328/8-1         @ width of mode2 in line_pixels/8
    sub     r4, r4, r12, lsr #24
    sub     r1, lr, r4,  lsl #4  @ skip src pixels
    add     r1, r1, #328*2*2
    add     lr, r1, #328*2
    b       .loopM2_16_270



.global vidConvCpyM2_RGB32_90 @ void *to, void *from, int width

vidConvCpyM2_RGB32_90:
    stmfd   sp!, {r4-r10,lr}

    mov     lr, #0x00F00000
    orr     lr, lr, #0x00F0

    mov     r12, #208/4            @ row counter
    mov     r10, r2, lsl #2        @ we do 2 pixel wide copies

    add     r8,  r0, #208*4        @ parallel line
    add     r1,  r1, #0x21000
    add     r1,  r1, #0x00280      @ r1+=328*207*2+8*2
    mov     r9,  r1

.loopM2RGB32_90:
	subs    r12, r12, #1

    @ at first this loop was written differently: src pixels were fetched with ldm's and
    @ dest was not sequential. It ran nearly 2 times slower. It seems it is very important
    @ to do sequential memory access on those items, which we have more (to offload addressing bus?).

    ldr     r4, [r1], #-328*2
    ldr     r5, [r1], #-328*2
    ldr     r6, [r1], #-328*2
    ldr     r7, [r1], #-328*2

    convRGB32_2 r4, 1
    convRGB32_2 r5, 1
    convRGB32_2 r6, 1
    convRGB32_2 r7, 1

    str     r4, [r8], #4
    str     r5, [r8], #4
    str     r6, [r8], #4
    str     r7, [r8], #4

    bne     .loopM2RGB32_90

    subs    r10, r10, #1
    ldmeqfd sp!, {r4-r10,pc}        @ return

    mov     r12, #208/4             @ restore row counter
    mov     r0,  r8                 @ set new dst pointer
    add     r8,  r0,  #208*4
    add     r9,  r9,  #2*2          @ fix src pointer
    mov     r1,  r9
    b       .loopM2RGB32_90



@ converter for vidConvCpyM2_RGB32_270
@ lr =  0x00F000F0, out: r3=lower_pix, r2=higher_pix; trashes rin
.macro convRGB32_3 rin
    and     r2,  lr, \rin, lsr #4 @ blue
    and     r3,  \rin, lr
    orr     r2,  r2,   r3, lsl #8         @ g0b0g0b0

    mov     r3,  r2,  lsl #16             @ g0b00000
    and     \rin,lr,  \rin, ror #12       @ 00r000r0 (reversed)
    orr     r3,  r3,  \rin, lsr #16       @ g0b000r0

    mov     r2,  r2,  lsr #16
    orr     r2,  r2,  \rin, lsl #16
    str     r2, [r0], #4

    mov     \rin,r3,  ror #16             @ r3=low
.endm


.global vidConvCpyM2_RGB32_270 @ void *to, void *from, int width

vidConvCpyM2_RGB32_270:
    stmfd   sp!, {r4-r10,lr}

    mov     lr, #0x00F00000
    orr     lr, lr, #0x00F0

    mov     r12, #208/4            @ row counter
    mov     r10, r2, lsl #2        @ we do 2 pixel wide copies (right to left)

    add     r8,  r0, #208*4        @ parallel line
    add     r1,  r1, #326*2
    mov     r9,  r1

.loopM2RGB32_270:
	subs    r12, r12, #1

    ldr     r4, [r1], #328*2
    ldr     r5, [r1], #328*2
    ldr     r6, [r1], #328*2
    ldr     r7, [r1], #328*2

    convRGB32_3 r4
    convRGB32_3 r5
    convRGB32_3 r6
    convRGB32_3 r7

    str     r4, [r8], #4
    str     r5, [r8], #4
    str     r6, [r8], #4
    str     r7, [r8], #4

    bne     .loopM2RGB32_270

    subs    r10, r10, #1
    ldmeqfd sp!, {r4-r10,pc}        @ return

    mov     r12, #208/4             @ restore row counter
    mov     r0,  r8                 @ set new dst pointer
    add     r8,  r0,  #208*4
    sub     r9,  r9,  #2*2          @ fix src pointer
    mov     r1,  r9
    b       .loopM2RGB32_270

