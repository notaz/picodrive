@ some color conversion and blitting routines

@ (c) Copyright 2006, notaz
@ All Rights Reserved


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
    orr     r3,  r3,   r3, lsr #3
.else
    mov     r3,  r3,  ror #16             @ r3=low
    orr     r3,  r3,   r3, lsr #3
.endif

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
    orr     r2,  r2,   r2,  lsr #3
.else
    orr     r2,  r2,   r2,  lsr #3
.endif

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

@ -------- M2 stuff ---------
/*
.global vidConvCpy_90 @ void *to, void *from, int width

vidConvCpy_90:
    stmfd   sp!, {r4-r10,lr}

    mov     lr, #0x00F00000
    orr     lr, lr, #0x00F0

    mov     r12, #224/4            @ row counter
    mov     r10, r2, lsl #2        @ we do 2 pixel wide copies

    add     r8,  r0, #256*4        @ parallel line
    add     r1,  r1, #0x23000
    add     r1,  r1, #0x00B80      @ r1+=328*223*2+8*2
    mov     r9,  r1

    mov     r4,  #0                @ fill bottom border
    mov     r5,  #0
    mov     r6,  #0
    mov     r7,  #0
    stmia   r0!, {r4-r7}
    stmia   r0!, {r4-r7}
    stmia   r8!, {r4-r7}
    stmia   r8!, {r4-r7}

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

    mov     r4,  #0                @ top border
    mov     r5,  #0
    mov     r6,  #0
    stmia   r0!, {r4-r6,r12}
    stmia   r0!, {r4-r6,r12}
    stmia   r8!, {r4-r6,r12}
    stmia   r8!, {r4-r6,r12}

    subs    r10, r10, #1
    ldmeqfd sp!, {r4-r10,pc}        @ return

    add     r0,  r8,  #16*4         @ set new dst pointer
    add     r8,  r0,  #256*4
    add     r9,  r9,  #2*2          @ fix src pointer
    mov     r1,  r9

    stmia   r0!, {r4-r6,r12}        @ bottom border
    stmia   r0!, {r4-r6,r12}
    stmia   r8!, {r4-r6,r12}
    stmia   r8!, {r4-r6,r12}

    mov     r12, #224/4             @ restore row counter
    b       .loopM2RGB32_90



@ converter for vidConvCpy_270
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
*/
@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


@ takes byte-sized pixels from r3-r6, fetches from pal and stores to r7,r8,r10,lr
@ r2=pal
.macro mode2_4pix shift
    and     r7, r11, r3, lsr #\shift
    ldr     r7, [r2, r7, lsl #2]

    and     r8, r11, r4, lsr #\shift
    ldr     r8, [r2, r8, lsl #2]

    and     r10,r11, r5, lsr #\shift
    ldr     r10,[r2, r10,lsl #2]

    and     lr, r11, r6, lsr #\shift
    ldr     lr, [r2, lr, lsl #2]
.endm

@ r2=pal, r11=0xff
.macro mode2_4pix_getpix0 dreg sreg
    and     \dreg, r11, \sreg
    ldr     \dreg, [r2, \dreg, lsl #2]
.endm

.macro mode2_4pix_getpix1 dreg sreg
    and     \dreg, r11, \sreg, lsr #8
    ldr     \dreg, [r2, \dreg, lsl #2]
.endm

.macro mode2_4pix_getpix2 dreg sreg
    and     \dreg, r11, \sreg, lsr #16
    ldr     \dreg, [r2, \dreg, lsl #2]
.endm

.macro mode2_4pix_getpix3 dreg sreg
    and     \dreg, r11, \sreg, lsr #24
    ldr     \dreg, [r2, \dreg, lsl #2]
.endm

@ takes byte-sized pixels from reg, fetches from pal and stores to r3-r6
@ r11=0xFF, r2=pal
.macro mode2_4pix2_0 reg
    mode2_4pix_getpix0 r3, \reg
    mode2_4pix_getpix1 r4, \reg
    mode2_4pix_getpix2 r5, \reg
    mode2_4pix_getpix3 r6, \reg
.endm

@ ...
.macro mode2_4pix2_180 reg
    mode2_4pix_getpix3 r3, \reg
    mode2_4pix_getpix2 r4, \reg
    mode2_4pix_getpix1 r5, \reg
    mode2_4pix_getpix0 r6, \reg
.endm

@ takes byte-sized pixels from reg, fetches from pal and stores to r3-r5
@ r11=0xFF, r2=pal, r10=0xfcfcfc, r6=tmp
.macro mode2_4pix_to3 reg is180
.if \is180
    mode2_4pix_getpix3 r3, \reg
    mode2_4pix_getpix2 r4, \reg
.else
    mode2_4pix_getpix0 r3, \reg     @ gathering loads cause a weird-hang
    mode2_4pix_getpix1 r4, \reg
.endif

    sub     r3, r3,  r3, lsr #2     @ r3 *= 0.75
    add     r3, r3,  r4, lsr #2     @ r3 += r4 * 0.25
    and     r3, r3,  r10

.if \is180
    mode2_4pix_getpix1 r5, \reg
    mode2_4pix_getpix0 r6, \reg
.else
    mode2_4pix_getpix2 r5, \reg
    mode2_4pix_getpix3 r6, \reg
.endif

    mov     r4, r4,  lsr #1
    add     r4, r4,  r5, lsr #1     @ r4 = (r4 + r5) / 2;
@    and     r4, r4,  r10
    sub     r6, r6,  r6, lsr #2     @ r6 *= 0.75
    add     r5, r6,  r5, lsr #2     @ r5 = r6 + r5 * 0.25
    and     r5, r5,  r10
.endm


@ void *to, void *from, void *pal, int width
.macro vidConvCpyM2_landscape is270
    stmfd   sp!, {r4-r11,lr}

    mov     r11, #0xff

    mov     r12, #(224/4-1)<<16    @ row counter
    orr     r12, r12, r3, lsl #1   @ we do 4 pixel wide copies (right to left)

.if \is270
    add     r1,  r1, #324
.else
    add     r1,  r1, #0x11c00
    add     r1,  r1, #0x00308      @ 328*224+8
.endif
    mov     r9,  r1

    mov     r3,  #0                @ fill top border
    mov     r4,  #0
    mov     r5,  #0
    mov     r6,  #0
    stmia   r0!, {r3-r6}
    stmia   r0!, {r3-r6}
    add     r7,  r0, #256*4-8*4
    stmia   r7!, {r3-r6}
    stmia   r7!, {r3-r6}
    add     r7,  r7, #256*4-8*4
    stmia   r7!, {r3-r6}
    stmia   r7!, {r3-r6}
    add     r7,  r7, #256*4-8*4
    stmia   r7!, {r3-r6}
    stmia   r7!, {r3-r6}

0: @ .loopM2RGB32_270:
	subs    r12, r12, #1<<16

.if \is270
    ldr     r3, [r1], #328
    ldr     r4, [r1], #328
    ldr     r5, [r1], #328
    ldr     r6, [r1], #328
.else
    ldr     r3, [r1, #-328]!
    ldr     r4, [r1, #-328]!
    ldr     r5, [r1, #-328]!
    ldr     r6, [r1, #-328]!
.endif

.if \is270
    mode2_4pix 24
.else
    mode2_4pix  0
.endif
    stmia   r0, {r7,r8,r10,lr}
    add     r0, r0, #256*4

.if \is270
    mode2_4pix 16
.else
    mode2_4pix  8
.endif
    stmia   r0, {r7,r8,r10,lr}
    add     r0, r0, #256*4

.if \is270
    mode2_4pix  8
.else
    mode2_4pix 16
.endif
    stmia   r0, {r7,r8,r10,lr}
    add     r0, r0, #256*4

.if \is270
    mode2_4pix  0
.else
    mode2_4pix 24
.endif
    stmia   r0!,{r7,r8,r10,lr}
    sub     r0, r0, #256*4*3

    bpl     0b @ .loopM2RGB32_270

    mov     r3,  #0                @ bottom border
    mov     r4,  #0
    mov     r5,  #0
    mov     r6,  #0
    stmia   r0!, {r3-r6}
    stmia   r0!, {r3-r6}
    add     r0,  r0, #256*4-8*4
    stmia   r0!, {r3-r6}
    stmia   r0!, {r3-r6}
    add     r0,  r0, #256*4-8*4
    stmia   r0!, {r3-r6}
    stmia   r0!, {r3-r6}
    add     r0,  r0, #256*4-8*4
    stmia   r0!, {r3-r6}
    nop                             @ phone crashes if this is commented out. Do I stress it too much?
    stmia   r0!, {r3-r6}

    add     r12, r12, #1<<16
    subs    r12, r12, #1
    ldmeqfd sp!, {r4-r11,pc}        @ return

    add     r0,  r0, #16*4
.if \is270
    sub     r9,  r9, #4            @ fix src pointer
.else
    add     r9,  r9, #4
.endif
    mov     r1,  r9

    stmia   r0!, {r3-r6}            @ top border
    stmia   r0!, {r3-r6}
    add     r7,  r0, #256*4-8*4
    stmia   r7!, {r3-r6}
    stmia   r7!, {r3-r6}
    add     r7,  r7, #256*4-8*4
    stmia   r7!, {r3-r6}
    stmia   r7!, {r3-r6}
    add     r7,  r7, #256*4-8*4
    stmia   r7!, {r3-r6}
    stmia   r7!, {r3-r6}

    orr     r12, r12, #(224/4-1)<<16 @ restore row counter
    b       0b @ .loopM2RGB32_270
.endm


.global vidConvCpy_90 @ void *to, void *from, void *pal, int width

vidConvCpy_90:
    vidConvCpyM2_landscape 0


.global vidConvCpy_270 @ void *to, void *from, void *pal, int width

vidConvCpy_270:
    vidConvCpyM2_landscape 1


.global vidConvCpy_center_0 @ void *to, void *from, void *pal

vidConvCpy_center_0:
    stmfd   sp!, {r4-r6,r11,lr}

    mov     r11, #0xff
    add     r1,  r1, #8     @ not border (centering 32col here)

    mov     r12, #(240/4-1)<<16
    orr     r12, r12, #224

.loopRGB32_c0:
    ldr     lr, [r1], #4
	subs    r12, r12, #1<<16

    mode2_4pix2_0 lr
    stmia   r0!, {r3-r6}
    bpl     .loopRGB32_c0

	sub     r12, r12, #1
	adds    r12, r12, #1<<16
    ldmeqfd sp!, {r4-r6,r11,pc} @ return
    add     r0,  r0, #16*4
    add     r1,  r1, #88
    orr     r12, #(240/4-1)<<16
    b       .loopRGB32_c0


.global vidConvCpy_center_180 @ void *to, void *from, void *pal

vidConvCpy_center_180:
    stmfd   sp!, {r4-r6,r11,lr}

    mov     r11, #0xff
    add     r1,  r1, #0x11c00
    add     r1,  r1, #0x002B8 @ #328*224-72

    mov     r12, #(240/4-1)<<16
    orr     r12, r12, #224

.loopRGB32_c180:
    ldr     lr, [r1, #-4]!
	subs    r12, r12, #1<<16

    mode2_4pix2_180 lr
    stmia   r0!, {r3-r6}
    bpl     .loopRGB32_c180

	sub     r12, r12, #1
	adds    r12, r12, #1<<16
    ldmeqfd sp!, {r4-r6,r11,pc} @ return
    add     r0,  r0, #16*4
    sub     r1,  r1, #88
    orr     r12, #(240/4-1)<<16
    b       .loopRGB32_c180


@ note: the following code assumes that (pal[x] & 0x030303) == 0

.global vidConvCpy_center2_40c_0 @ void *to, void *from, void *pal, int lines

vidConvCpy_center2_40c_0:
    stmfd   sp!, {r4-r6,r10,r11,lr}

    mov     r11, #0xff
    mov     r10, #0xfc
    orr     r10, r10, lsl #8
    orr     r10, r10, lsl #8
    add     r1,  r1, #8     @ border

    mov     r12, #(240/3-1)<<16
    orr     r12, r12, r3

.loopRGB32_c2_40c_0:
    ldr     lr, [r1], #4
	subs    r12, r12, #1<<16

    mode2_4pix_to3 lr, 0

    stmia   r0!, {r3-r5}
    bpl     .loopRGB32_c2_40c_0

	sub     r12, r12, #1
	adds    r12, r12, #1<<16
    ldmeqfd sp!, {r4-r6,r10,r11,pc} @ return
    add     r0,  r0, #16*4
    add     r1,  r1, #8
    orr     r12, #(240/3-1)<<16
    b       .loopRGB32_c2_40c_0


.global vidConvCpy_center2_40c_180 @ void *to, void *from, void *pal, int lines

vidConvCpy_center2_40c_180:
    stmfd   sp!, {r4-r6,r10,r11,lr}

    mov     r11, #0xff
    mov     r10, #0xfc
    orr     r10, r10, lsl #8
    orr     r10, r10, lsl #8

    mov     r4,  #328
    mla     r1,  r3, r4, r1
@    add     r1,  r1, #0x11000
@    add     r1,  r1, #0x00f00 @ #328*224

    mov     r12, #(240/3-1)<<16
    orr     r12, r12, r3

.loop_c2_40c_180:
    ldr     lr, [r1, #-4]!
	subs    r12, r12, #1<<16

    mode2_4pix_to3 lr, 1

    stmia   r0!, {r3-r5}
    bpl     .loop_c2_40c_180

	sub     r12, r12, #1
	adds    r12, r12, #1<<16
    ldmeqfd sp!, {r4-r6,r10,r11,pc} @ return
    add     r0,  r0, #16*4
    sub     r1,  r1, #8
    orr     r12, #(240/3-1)<<16
    b       .loop_c2_40c_180


.global vidConvCpy_center2_32c_0 @ void *to, void *from, void *pal, int lines

vidConvCpy_center2_32c_0:
    stmfd   sp!, {r4-r11,lr}

    mov     r10, #0xfc
    orr     r10, r10, lsl #8
    orr     r10, r10, lsl #8
    mov     r11, #0xff
    add     r1,  r1, #8     @ border

    mov     r12, #(240/15-1)<<16
    orr     r12, r12, r3

.loop_c2_32c_0:
    ldmia   r1!, {r7-r9,lr}
	subs    r12, r12, #1<<16

    mode2_4pix2_0 r7
    stmia   r0!, {r3-r6}
    mode2_4pix2_0 r8
    stmia   r0!, {r3-r6}
    mode2_4pix2_0 r9
    stmia   r0!, {r3-r6}
    mode2_4pix_to3 lr, 0
    stmia   r0!, {r3-r5}
    bpl     .loop_c2_32c_0

	sub     r12, r12, #1
	adds    r12, r12, #1<<16
    ldmeqfd sp!, {r4-r11,pc} @ return
    add     r0,  r0, #16*4
    add     r1,  r1, #64+8
    orr     r12, #(240/15-1)<<16
    b       .loop_c2_32c_0


.global vidConvCpy_center2_32c_180 @ void *to, void *from, void *pal, int lines

vidConvCpy_center2_32c_180:
    stmfd   sp!, {r4-r11,lr}

    mov     r10, #0xfc
    orr     r10, r10, lsl #8
    orr     r10, r10, lsl #8
    mov     r11, #0xff

    mov     r4,  #328
    mla     r1,  r3, r4, r1
@    add     r1,  r1, #0x11000
@    add     r1,  r1, #0x00f00 @ #328*224

    mov     r12, #(240/15-1)<<16
    orr     r12, r12, r3

.loop_c2_32c_180:
    ldmdb   r1!, {r7-r9,lr}
	subs    r12, r12, #1<<16

    mode2_4pix2_180 lr
    stmia   r0!, {r3-r6}
    mode2_4pix2_180 r9
    stmia   r0!, {r3-r6}
    mode2_4pix2_180 r8
    stmia   r0!, {r3-r6}
    mode2_4pix_to3 r7, 1
    stmia   r0!, {r3-r5}
    bpl     .loop_c2_32c_180

	sub     r12, r12, #1
	adds    r12, r12, #1<<16
    ldmeqfd sp!, {r4-r11,pc} @ return
    add     r0,  r0, #16*4
    sub     r1,  r1, #64+8
    orr     r12, #(240/15-1)<<16
    b       .loop_c2_32c_180


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


.global vidClear @ void *to, int lines

vidClear:
    stmfd   sp!, {lr}
    mov     r12, #240/16-1
    orr     r12, r1, r12, lsl #16
    mov     r1, #0
    mov     r2, #0
    mov     r3, #0
    mov     lr, #0

.loopVidClear:
	subs    r12, r12, #1<<16

    stmia   r0!, {r1-r3,lr}
    stmia   r0!, {r1-r3,lr}
    stmia   r0!, {r1-r3,lr}
    stmia   r0!, {r1-r3,lr}
    bpl     .loopVidClear

	sub     r12, r12, #1
	adds    r12, r12, #1<<16
    ldmeqfd sp!, {pc}        @ return
    add     r0,  r0, #16*4
    orr     r12, #(240/16-1)<<16
    b       .loopVidClear

