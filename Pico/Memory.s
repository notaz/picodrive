@ vim:filetype=armasm

@ memory handlers with banking support for SSF II - The New Challengers
@ mostly based on Gens code

@ (c) Copyright 2006-2007, Grazvydas "notaz" Ignotas
@ All Rights Reserved


.include "port_config.s"

.text
.align 4

@ default jump tables

m_read8_def_table:
    .long   m_read8_rom0    @ 0x000000 - 0x07FFFF
    .long   m_read8_rom1    @ 0x080000 - 0x0FFFFF
    .long   m_read8_rom2    @ 0x100000 - 0x17FFFF
    .long   m_read8_rom3    @ 0x180000 - 0x1FFFFF
    .long   m_read8_rom4    @ 0x200000 - 0x27FFFF
    .long   m_read8_rom5    @ 0x280000 - 0x2FFFFF
    .long   m_read8_rom6    @ 0x300000 - 0x37FFFF
    .long   m_read8_rom7    @ 0x380000 - 0x3FFFFF
    .long   m_read8_rom8    @ 0x400000 - 0x47FFFF - for all those large ROM hacks
    .long   m_read8_rom9    @ 0x480000 - 0x4FFFFF
    .long   m_read8_romA    @ 0x500000 - 0x57FFFF
    .long   m_read8_romB    @ 0x580000 - 0x5FFFFF
    .long   m_read8_romC    @ 0x600000 - 0x67FFFF
    .long   m_read8_romD    @ 0x680000 - 0x6FFFFF
    .long   m_read8_romE    @ 0x700000 - 0x77FFFF
    .long   m_read8_romF    @ 0x780000 - 0x7FFFFF
    .long   m_read8_rom10   @ 0x800000 - 0x87FFFF
    .long   m_read8_rom11   @ 0x880000 - 0x8FFFFF
    .long   m_read8_rom12   @ 0x900000 - 0x97FFFF
    .long   m_read8_rom13   @ 0x980000 - 0x9FFFFF
    .long   m_read8_misc    @ 0xA00000 - 0xA7FFFF
    .long   m_read_null     @ 0xA80000 - 0xAFFFFF
    .long   m_read_null     @ 0xB00000 - 0xB7FFFF
    .long   m_read_null     @ 0xB80000 - 0xBFFFFF
    .long   m_read8_vdp     @ 0xC00000 - 0xC7FFFF
    .long   m_read8_vdp     @ 0xC80000 - 0xCFFFFF
    .long   m_read8_vdp     @ 0xD00000 - 0xD7FFFF
    .long   m_read8_vdp     @ 0xD80000 - 0xDFFFFF
    .long   m_read8_ram     @ 0xE00000 - 0xE7FFFF
    .long   m_read8_ram     @ 0xE80000 - 0xEFFFFF
    .long   m_read8_ram     @ 0xF00000 - 0xF7FFFF
    .long   m_read8_ram     @ 0xF80000 - 0xFFFFFF

m_read16_def_table:
    .long   m_read16_rom0    @ 0x000000 - 0x07FFFF
    .long   m_read16_rom1    @ 0x080000 - 0x0FFFFF
    .long   m_read16_rom2    @ 0x100000 - 0x17FFFF
    .long   m_read16_rom3    @ 0x180000 - 0x1FFFFF
    .long   m_read16_rom4    @ 0x200000 - 0x27FFFF
    .long   m_read16_rom5    @ 0x280000 - 0x2FFFFF
    .long   m_read16_rom6    @ 0x300000 - 0x37FFFF
    .long   m_read16_rom7    @ 0x380000 - 0x3FFFFF
    .long   m_read16_rom8    @ 0x400000 - 0x47FFFF
    .long   m_read16_rom9    @ 0x480000 - 0x4FFFFF
    .long   m_read16_romA    @ 0x500000 - 0x57FFFF
    .long   m_read16_romB    @ 0x580000 - 0x5FFFFF
    .long   m_read16_romC    @ 0x600000 - 0x67FFFF
    .long   m_read16_romD    @ 0x680000 - 0x6FFFFF
    .long   m_read16_romE    @ 0x700000 - 0x77FFFF
    .long   m_read16_romF    @ 0x780000 - 0x7FFFFF
    .long   m_read16_rom10   @ 0x800000 - 0x87FFFF
    .long   m_read16_rom11   @ 0x880000 - 0x8FFFFF
    .long   m_read16_rom12   @ 0x900000 - 0x97FFFF
    .long   m_read16_rom13   @ 0x980000 - 0x9FFFFF
    .long   m_read16_misc    @ 0xA00000 - 0xA7FFFF
    .long   m_read_null      @ 0xA80000 - 0xAFFFFF
    .long   m_read_null      @ 0xB00000 - 0xB7FFFF
    .long   m_read_null      @ 0xB80000 - 0xBFFFFF
    .long   m_read16_vdp     @ 0xC00000 - 0xC7FFFF
    .long   m_read16_vdp     @ 0xC80000 - 0xCFFFFF
    .long   m_read16_vdp     @ 0xD00000 - 0xD7FFFF
    .long   m_read16_vdp     @ 0xD80000 - 0xDFFFFF
    .long   m_read16_ram     @ 0xE00000 - 0xE7FFFF
    .long   m_read16_ram     @ 0xE80000 - 0xEFFFFF
    .long   m_read16_ram     @ 0xF00000 - 0xF7FFFF
    .long   m_read16_ram     @ 0xF80000 - 0xFFFFFF

m_read32_def_table:
    .long   m_read32_rom0    @ 0x000000 - 0x07FFFF
    .long   m_read32_rom1    @ 0x080000 - 0x0FFFFF
    .long   m_read32_rom2    @ 0x100000 - 0x17FFFF
    .long   m_read32_rom3    @ 0x180000 - 0x1FFFFF
    .long   m_read32_rom4    @ 0x200000 - 0x27FFFF
    .long   m_read32_rom5    @ 0x280000 - 0x2FFFFF
    .long   m_read32_rom6    @ 0x300000 - 0x37FFFF
    .long   m_read32_rom7    @ 0x380000 - 0x3FFFFF
    .long   m_read32_rom8    @ 0x400000 - 0x47FFFF
    .long   m_read32_rom9    @ 0x480000 - 0x4FFFFF
    .long   m_read32_romA    @ 0x500000 - 0x57FFFF
    .long   m_read32_romB    @ 0x580000 - 0x5FFFFF
    .long   m_read32_romC    @ 0x600000 - 0x67FFFF
    .long   m_read32_romD    @ 0x680000 - 0x6FFFFF
    .long   m_read32_romE    @ 0x700000 - 0x77FFFF
    .long   m_read32_romF    @ 0x780000 - 0x7FFFFF
    .long   m_read32_rom10   @ 0x800000 - 0x87FFFF
    .long   m_read32_rom11   @ 0x880000 - 0x8FFFFF
    .long   m_read32_rom12   @ 0x900000 - 0x97FFFF
    .long   m_read32_rom13   @ 0x980000 - 0x9FFFFF
    .long   m_read32_misc    @ 0xA00000 - 0xA7FFFF
    .long   m_read_null      @ 0xA80000 - 0xAFFFFF
    .long   m_read_null      @ 0xB00000 - 0xB7FFFF
    .long   m_read_null      @ 0xB80000 - 0xBFFFFF
    .long   m_read32_vdp     @ 0xC00000 - 0xC7FFFF
    .long   m_read32_vdp     @ 0xC80000 - 0xCFFFFF
    .long   m_read32_vdp     @ 0xD00000 - 0xD7FFFF
    .long   m_read32_vdp     @ 0xD80000 - 0xDFFFFF
    .long   m_read32_ram     @ 0xE00000 - 0xE7FFFF
    .long   m_read32_ram     @ 0xE80000 - 0xEFFFFF
    .long   m_read32_ram     @ 0xF00000 - 0xF7FFFF
    .long   m_read32_ram     @ 0xF80000 - 0xFFFFFF


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.bss
.align 4
@.section .bss, "brw"
@.data

@ used tables
m_read8_table:
    .skip 32*4

m_read16_table:
    .skip 32*4

m_read32_table:
    .skip 32*4


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.text
.align 4

.global PicoMemReset
.global PicoRead8
.global PicoRead16
.global PicoRead32
.global PicoWrite8
.global PicoWriteRomHW_SSF2
.global m_m68k_read8_misc
.global m_m68k_write8_misc


PicoMemReset:
    ldr     r12,=(Pico+0x22204)
    ldr     r12,[r12]                @ romsize
    add     r12,r12,#0x80000
    sub     r12,r12,#1
    mov     r12,r12,lsr #19

    ldr     r0, =m_read8_table
    ldr     r1, =m_read8_def_table
    mov     r2, #32
1:
    ldr     r3, [r1], #4
    str     r3, [r0], #4
    subs    r2, r2, #1
    bne     1b

    ldr     r0, =m_read16_table
    ldr     r1, =m_read16_def_table
    mov     r2, #32
1:
    subs    r2, r2, #1
    ldr     r3, [r1], #4
    str     r3, [r0], #4
    bne     1b

    ldr     r0, =m_read32_table
    ldr     r1, =m_read32_def_table
    mov     r2, #32
1:
    subs    r2, r2, #1
    ldr     r3, [r1], #4
    str     r3, [r0], #4
    bne     1b

    @ update memhandlers according to ROM size
    ldr     r1, =m_read8_above_rom
    ldr     r0, =m_read8_table
    mov     r2, #20
1:
    sub     r2, r2, #1
    cmp     r2, r12
    blt     2f
    cmp     r2, #4
    beq     1b                      @ do not touch the SRAM area
    str     r1, [r0, r2, lsl #2]
    b       1b
2:
    ldr     r1, =m_read16_above_rom
    ldr     r0, =m_read16_table
    mov     r2, #20
1:
    sub     r2, r2, #1
    cmp     r2, r12
    blt     2f
    cmp     r2, #4
    beq     1b
    str     r1, [r0, r2, lsl #2]
    b       1b
2:
    ldr     r1, =m_read32_above_rom
    ldr     r0, =m_read32_table
    mov     r2, #20
1:
    sub     r2, r2, #1
    cmp     r2, r12
    blt     2f
    cmp     r2, #4
    beq     1b
    str     r1, [r0, r2, lsl #2]
    b       1b
2:
    bx      lr

.pool

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

PicoRead8: @ u32 a
    ldr     r2, =m_read8_table
    bic     r0, r0, #0xff000000
    and     r1, r0, #0x00f80000
    ldr     pc, [r2, r1, lsr #17]

PicoRead16: @ u32 a
    ldr     r2, =m_read16_table
    bic     r0, r0, #0xff000000
    and     r1, r0, #0x00f80000
    ldr     pc, [r2, r1, lsr #17]

PicoRead32: @ u32 a
    ldr     r2, =m_read32_table
    bic     r0, r0, #0xff000000
    and     r1, r0, #0x00f80000
    ldr     pc, [r2, r1, lsr #17]

.pool

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

m_read_null:
    mov     r0, #0
    bx      lr


.macro m_read8_rom sect
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #0xf80000
    ldr     r1, [r1]
.if \sect
    orr     r0, r0, #0x080000*\sect
.endif
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr
.endm


m_read8_rom0: @ 0x000000 - 0x07ffff
    m_read8_rom 0

m_read8_rom1: @ 0x080000 - 0x0fffff
    m_read8_rom 1

m_read8_rom2: @ 0x100000 - 0x17ffff
    m_read8_rom 2

m_read8_rom3: @ 0x180000 - 0x1fffff
    m_read8_rom 3

m_read8_rom4: @ 0x200000 - 0x27ffff, SRAM area
    ldr     r2, =(SRam)
    ldr     r3, =(Pico+0x22200)
    ldr     r1, [r2, #8]    @ SRam.end
    bic     r0, r0, #0xf80000
    orr     r0, r0, #0x200000
    cmp     r0, r1
    bgt     m_read8_nosram
    ldr     r1, [r2, #4]    @ SRam.start
    cmp     r0, r1
    blt     m_read8_nosram
    ldrb    r1, [r3, #0x11] @ Pico.m.sram_reg
    tst     r1, #5
    bne     SRAMRead
m_read8_nosram:
    ldr     r1, [r3, #4]    @ romsize
    cmp     r0, r1
    movgt   r0, #0
    bxgt    lr              @ bad location
    ldr     r1, [r3]
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr

m_read8_rom5: @ 0x280000 - 0x2fffff
    m_read8_rom 5

m_read8_rom6: @ 0x300000 - 0x37ffff
    m_read8_rom 6

m_read8_rom7: @ 0x380000 - 0x3fffff
    m_read8_rom 7

m_read8_rom8: @ 0x400000 - 0x47ffff
    m_read8_rom 8

m_read8_rom9: @ 0x480000 - 0x4fffff
    m_read8_rom 9

m_read8_romA: @ 0x500000 - 0x57ffff
    m_read8_rom 0xA

m_read8_romB: @ 0x580000 - 0x5fffff
    m_read8_rom 0xB

m_read8_romC: @ 0x600000 - 0x67ffff
    m_read8_rom 0xC

m_read8_romD: @ 0x680000 - 0x6fffff
    m_read8_rom 0xD

m_read8_romE: @ 0x700000 - 0x77ffff
    m_read8_rom 0xE

m_read8_romF: @ 0x780000 - 0x7fffff
    m_read8_rom 0xF

m_read8_rom10: @ 0x800000 - 0x87ffff
    m_read8_rom 0x10

m_read8_rom11: @ 0x880000 - 0x8fffff
    m_read8_rom 0x11

m_read8_rom12: @ 0x900000 - 0x97ffff
    m_read8_rom 0x12

m_read8_rom13: @ 0x980000 - 0x9fffff
    m_read8_rom 0x13


m_m68k_read8_misc:
m_read8_misc:
    bic     r2, r0, #0x001f @ most commonly we get i/o port read,
    cmp     r2, #0xa10000   @ so check for it first
    bne     m_read8_misc2
m_read8_misc_io:
    ands    r0, r0, #0x1e
    beq     m_read8_misc_hwreg
    cmp     r0, #4
    movlt   r0, #0
    moveq   r0, #1
    ble     PadRead
    ldr     r3, =(Pico+0x22000)
    mov     r0, r0, lsr #1  @ other IO ports (Pico.ioports[a])
    ldrb    r0, [r3, r0]
    bx      lr

m_read8_misc_hwreg:
    ldr     r3, =(Pico+0x22200)
    ldrb    r0, [r3, #0x0f] @ Pico.m.hardware
    bx      lr

m_read8_misc2:
    mov     r2,     #0xa10000 @ games also like to poll busreq,
    orr     r2, r2, #0x001100 @ so we'll try it now
    cmp     r0, r2
    beq     z80ReadBusReq

    and     r2, r0, #0xff0000 @ finally it might be
    cmp     r2,     #0xa00000 @ z80 area
    bne     m_read8_misc3
    tst     r0, #0x4000
    beq     z80Read8          @ z80 RAM
    and     r2, r0, #0x6000
    cmp     r2, #0x4000
    mvnne   r0, #0
    bxne    lr                @ invalid
    ldr     r1, =PicoOpt
    ldr     r1, [r1]
    tst     r1, #1

    ldrne   r1, =ym2612_st
    ldrne   r1, [r1]
    ldrneb  r0, [r1, #0x11]   @ ym2612_st->status
    bxne    lr

m_read8_fake_ym2612:
    ldr     r3, =(Pico+0x22200)
    ldrb    r0, [r3, #8]      @ Pico.m.rotate
    add     r1, r0, #1
    strb    r1, [r3, #8]
    and     r0, r0, #3
    bx      lr

m_read8_misc3:
    @ if everything else fails, use generic handler
    stmfd   sp!,{r0,lr}
    bic     r0, r0, #1
    mov     r1, #8
    bl      OtherRead16
    ldmfd   sp!,{r1,lr}
    tst     r1, #1
    moveq   r0, r0, lsr #8
    bx      lr


m_read8_vdp:
    tst     r0, #0x70000
    tsteq   r0, #0x000e0
    bxne    lr              @ invalid read
    stmfd   sp!,{r0,lr}
    bic     r0, r0, #1
    bl      PicoVideoRead
    ldmfd   sp!,{r1,lr}
    tst     r1, #1
    moveq   r0, r0, lsr #8
    bx      lr

m_read8_ram:
    ldr     r1, =Pico
    bic     r0, r0, #0xff0000
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr

m_read8_above_rom:
    @ might still be SRam (Micro Machines, HardBall '95)
    ldr     r2, =(SRam)
    ldr     r3, =(Pico+0x22200)
    ldr     r1, [r2, #8]    @ SRam.end
    cmp     r0, r1
    bgt     m_read8_ar_nosram
    ldr     r1, [r2, #4]    @ SRam.start
    cmp     r0, r1
    blt     m_read8_ar_nosram
    ldrb    r1, [r3, #0x11] @ Pico.m.sram_reg
    tst     r1, #5
    bne     SRAMRead
m_read8_ar_nosram:
    stmfd   sp!,{r0,lr}
    bic     r0, r0, #1
    mov     r1, #8
    bl      OtherRead16End
    ldmfd   sp!,{r1,lr}
    tst     r1, #1
    moveq   r0, r0, lsr #8
    bx      lr

.pool

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.macro m_read16_rom sect
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #0xf80000
    ldr     r1, [r1]
    bic     r0, r0, #1
.if \sect
    orr     r0, r0, #0x080000*\sect
.endif
    ldrh    r0, [r1, r0]
    bx      lr
.endm


m_read16_rom0: @ 0x000000 - 0x07ffff
    m_read16_rom 0

m_read16_rom1: @ 0x080000 - 0x0fffff
    m_read16_rom 1

m_read16_rom2: @ 0x100000 - 0x17ffff
    m_read16_rom 2

m_read16_rom3: @ 0x180000 - 0x1fffff
    m_read16_rom 3

m_read16_rom4: @ 0x200000 - 0x27ffff, SRAM area (NBA Live 95)
    ldr     r2, =(SRam)
    ldr     r3, =(Pico+0x22200)
    ldr     r1, [r2, #8]    @ SRam.end
    bic     r0, r0, #0xf80000
    bic     r0, r0, #1
    orr     r0, r0, #0x200000
    cmp     r0, r1
    bgt     m_read16_nosram
    ldr     r1, [r2, #4]    @ SRam.start
    cmp     r0, r1
    blt     m_read16_nosram
    ldrb    r1, [r3, #0x11] @ Pico.m.sram_reg
    tst     r1, #5
    beq     m_read16_nosram
    stmfd   sp!,{lr}
    bl      SRAMRead16
    ldmfd   sp!,{pc}
m_read16_nosram:
    ldr     r1, [r3, #4]    @ romsize
    cmp     r0, r1
    movgt   r0, #0
    bxgt    lr              @ bad location
    ldr     r1, [r3]        @ 1ci
    ldrh    r0, [r1, r0]
    bx      lr

m_read16_rom5: @ 0x280000 - 0x2fffff
    m_read16_rom 5

m_read16_rom6: @ 0x300000 - 0x37ffff
    m_read16_rom 6

m_read16_rom7: @ 0x380000 - 0x3fffff
    m_read16_rom 7

m_read16_rom8: @ 0x400000 - 0x47ffff
    m_read16_rom 8

m_read16_rom9: @ 0x480000 - 0x4fffff
    m_read16_rom 9

m_read16_romA: @ 0x500000 - 0x57ffff
    m_read16_rom 0xA

m_read16_romB: @ 0x580000 - 0x5fffff
    m_read16_rom 0xB

m_read16_romC: @ 0x600000 - 0x67ffff
    m_read16_rom 0xC

m_read16_romD: @ 0x680000 - 0x6fffff
    m_read16_rom 0xD

m_read16_romE: @ 0x700000 - 0x77ffff
    m_read16_rom 0xE

m_read16_romF: @ 0x780000 - 0x7fffff
    m_read16_rom 0xF

m_read16_rom10: @ 0x800000 - 0x87ffff
    m_read16_rom 0x10

m_read16_rom11: @ 0x880000 - 0x8fffff
    m_read16_rom 0x11

m_read16_rom12: @ 0x900000 - 0x97ffff
    m_read16_rom 0x12

m_read16_rom13: @ 0x980000 - 0x9fffff
    m_read16_rom 0x13

m_read16_misc:
    bic     r0, r0, #1
    mov     r1, #16
    b       OtherRead16

m_read16_vdp:
    tst     r0, #0x70000    @ if ((a&0xe700e0)==0xc00000)
    tsteq   r0, #0x000e0
    bxne    lr              @ invalid read
    bic     r0, r0, #1
    b       PicoVideoRead

m_read16_ram:
    ldr     r1, =Pico
    bic     r0, r0, #0xff0000
    bic     r0, r0, #1
    ldrh    r0, [r1, r0]
    bx      lr

m_read16_above_rom:
    @ might still be SRam
    ldr     r2, =(SRam)
    ldr     r3, =(Pico+0x22200)
    ldr     r1, [r2, #8]    @ SRam.end
    bic     r0, r0, #1
    cmp     r0, r1
    bgt     m_read16_ar_nosram
    ldr     r1, [r2, #4]    @ SRam.start
    cmp     r0, r1
    blt     m_read16_ar_nosram
    ldrb    r1, [r3, #0x11] @ Pico.m.sram_reg
    tst     r1, #5
    beq     m_read16_ar_nosram
    stmfd   sp!,{lr}
    bl      SRAMRead16
    ldmfd   sp!,{pc}
m_read16_ar_nosram:
    mov     r1, #16
    b       OtherRead16End

.pool

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.macro m_read32_rom sect
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #0xf80000
    ldr     r1, [r1]
    bic     r0, r0, #1
.if \sect
    orr     r0, r0, #0x080000*\sect
.endif
    ldrh    r0, [r1, r0]!
    ldrh    r1, [r1, #2]           @ 1ci
    orr     r0, r1, r0, lsl #16
    bx      lr
.endm


m_read32_rom0: @ 0x000000 - 0x07ffff
    m_read32_rom 0

m_read32_rom1: @ 0x080000 - 0x0fffff
    m_read32_rom 1

m_read32_rom2: @ 0x100000 - 0x17ffff
    m_read32_rom 2

m_read32_rom3: @ 0x180000 - 0x1fffff
    m_read32_rom 3

m_read32_rom4: @ 0x200000 - 0x27ffff, SRAM area (does any game do long reads?)
    ldr     r2, =(SRam)
    ldr     r3, =(Pico+0x22200)
    ldr     r1, [r2, #8]    @ SRam.end
    bic     r0, r0, #0xf80000
    bic     r0, r0, #1
    orr     r0, r0, #0x200000
    cmp     r0, r1
    bgt     m_read32_nosram
    ldr     r1, [r2, #4]    @ SRam.start
    cmp     r0, r1
    blt     m_read32_nosram
    ldrb    r1, [r3, #0x11] @ Pico.m.sram_reg
    tst     r1, #5
    beq     m_read32_nosram
    stmfd   sp!,{r0,lr}
    bl      SRAMRead16
    ldmfd   sp!,{r1,lr}
    stmfd   sp!,{r0,lr}
    add     r0, r1, #2
    bl      SRAMRead16
    ldmfd   sp!,{r1,lr}
    orr     r0, r0, r1, lsl #16
    bx      lr
m_read32_nosram:
    ldr     r1, [r3, #4]    @ romsize
    cmp     r0, r1
    movgt   r0, #0
    bxgt    lr              @ bad location
    ldr     r1, [r3]        @ (1ci)
    ldrh    r0, [r1, r0]!
    ldrh    r1, [r1, #2]    @ (2ci)
    orr     r0, r1, r0, lsl #16
    bx      lr

m_read32_rom5: @ 0x280000 - 0x2fffff
    m_read32_rom 5

m_read32_rom6: @ 0x300000 - 0x37ffff
    m_read32_rom 6

m_read32_rom7: @ 0x380000 - 0x3fffff
    m_read32_rom 7

m_read32_rom8: @ 0x400000 - 0x47ffff
    m_read32_rom 8

m_read32_rom9: @ 0x480000 - 0x4fffff
    m_read32_rom 9

m_read32_romA: @ 0x500000 - 0x57ffff
    m_read32_rom 0xA

m_read32_romB: @ 0x580000 - 0x5fffff
    m_read32_rom 0xB

m_read32_romC: @ 0x600000 - 0x67ffff
    m_read32_rom 0xC

m_read32_romD: @ 0x680000 - 0x6fffff
    m_read32_rom 0xD

m_read32_romE: @ 0x700000 - 0x77ffff
    m_read32_rom 0xE

m_read32_romF: @ 0x780000 - 0x7fffff
    m_read32_rom 0xF

m_read32_rom10: @ 0x800000 - 0x87ffff
    m_read32_rom 0x10

m_read32_rom11: @ 0x880000 - 0x8fffff
    m_read32_rom 0x11

m_read32_rom12: @ 0x900000 - 0x97ffff
    m_read32_rom 0x12

m_read32_rom13: @ 0x980000 - 0x9fffff
    m_read32_rom 0x13

m_read32_misc:
    bic     r0, r0, #1
    stmfd   sp!,{r0,lr}
    mov     r1, #32
    bl      OtherRead16
    mov     r1, r0
    ldmfd   sp!,{r0}
    stmfd   sp!,{r1}
    add     r0, r0, #2
    mov     r1, #32
    bl      OtherRead16
    ldmfd   sp!,{r1,lr}
    orr     r0, r0, r1, lsl #16
    bx      lr

m_read32_vdp:
    tst     r0, #0x70000
    tsteq   r0, #0x000e0
    bxne    lr              @ invalid read
    bic     r0, r0, #1
    add     r1, r0, #2
    stmfd   sp!,{r1,lr}
    bl      PicoVideoRead
    swp     r0, r0, [sp]
    bl      PicoVideoRead
    ldmfd   sp!,{r1,lr}
    orr     r0, r0, r1, lsl #16
    bx      lr

m_read32_ram:
    ldr     r1, =Pico
    bic     r0, r0, #0xff0000
    bic     r0, r0, #1
    ldrh    r0, [r1, r0]!
    ldrh    r1, [r1, #2]    @ 2ci
    orr     r0, r1, r0, lsl #16
    bx      lr

m_read32_above_rom:
    bic     r0, r0, #1
    stmfd   sp!,{r0,lr}
    mov     r1, #32
    bl      OtherRead16End
    mov     r1, r0
    ldmfd   sp!,{r0}
    stmfd   sp!,{r1}
    add     r0, r0, #2
    mov     r1, #32
    bl      OtherRead16End
    ldmfd   sp!,{r1,lr}
    orr     r0, r0, r1, lsl #16
    bx      lr

.pool

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

PicoWriteRomHW_SSF2: @ u32 a, u32 d
    and     r0, r0, #0xe
    movs    r0, r0, lsr #1
    bne     pwr_banking

    @ sram register
    ldr     r2, =(Pico+0x22211) @ Pico.m.sram_reg
    ldrb    r0, [r2]
    and     r1, r1, #3
    bic     r0, r0, #3
    orr     r0, r0, r1
    strb    r0, [r2]
    bx      lr

pwr_banking:
    and     r1, r1, #0x1f

    ldr     r3, =m_read8_def_table
    ldr     r2, =m_read8_table
    ldr     r12, [r3, r1, lsl #2]
    str     r12, [r2, r0, lsl #2]

    ldr     r3, =m_read16_def_table
    ldr     r2, =m_read16_table
    ldr     r12, [r3, r1, lsl #2]
    str     r12, [r2, r0, lsl #2]

    ldr     r3, =m_read32_def_table
    ldr     r2, =m_read32_table
    ldr     r12, [r3, r1, lsl #2]
    str     r12, [r2, r0, lsl #2]
 
    bx      lr

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

@ Here we only handle most often used locations,
@ everything else is passed to generic handlers

PicoWrite8: @ u32 a, u8 d
    bic     r0, r0, #0xff000000
    and     r2, r0, #0x00e00000
    cmp     r2,     #0x00e00000 @ RAM?
    ldr     r3, =Pico
    biceq   r0, r0, #0x00ff0000
    eoreq   r0, r0, #1
    streqb  r1, [r3, r0]
    bxeq    lr

m_m68k_write8_misc:
    bic     r2, r0, #0x1f    @ most commonly we get i/o port write,
    cmp     r2, #0xa10000    @ so check for it first
    bne     m_write8_misc2
m_write8_io:
    ldr     r2, =PicoOpt
    and     r0, r0, #0x1e
    ldr     r2, [r2]
    ldr     r3, =(Pico+0x22000) @ Pico.ioports
    tst     r2, #0x20        @ 6 button pad?
    streqb  r1, [r3, r0, lsr #1]
    bxeq    lr
    cmp     r0, #2
    cmpne   r0, #4
    bne     m_write8_io_done @ not likely to happen
    add     r2, r3, #0x200   @ Pico+0x22200
    mov     r12,#0
    cmp     r0, #2
    streqb  r12,[r2,#0x18]
    strneb  r12,[r2,#0x19]   @ Pico.m.padDelay[i] = 0
    tst     r1, #0x40        @ TH
    beq     m_write8_io_done
    ldrb    r12,[r3, r0, lsr #1]
    tst     r12,#0x40
    bne     m_write8_io_done
    cmp     r0, #2
    ldreqb  r12,[r2,#0x0a]
    ldrneb  r12,[r2,#0x0b]   @ Pico.m.padTHPhase
    add     r12,r12,#1
    streqb  r12,[r2,#0x0a]
    strneb  r12,[r2,#0x0b]   @ Pico.m.padTHPhase
m_write8_io_done:
    strb    r1, [r3, r0, lsr #1]
    bx      lr


m_write8_misc2:
    and     r2, r0, #0xff0000
    cmp     r2,     #0xa00000 @ z80 area?
    bne     m_write8_not_z80
    tst     r0, #0x4000
    bne     m_write8_z80_not_ram
    ldr     r3, =(Pico+0x20000) @ Pico.zram
    add     r2, r3, #0x02200 @ Pico+0x22200
    ldrb    r2, [r2, #9]     @ Pico.m.z80Run
    bic     r0, r0, #0xff0000
    bic     r0, r0, #0x00e000
    tst     r2, #1
    streqb  r1, [r3, r0]     @ zram
    bx      lr

m_write8_z80_not_ram:
    and     r2, r0, #0x6000
    cmp     r2,     #0x4000
    bne     m_write8_z80_not_ym2612
    ldr     r2, =PicoOpt
    and     r0, r0, #3
    ldr     r2, [r2]
    tst     r2, #1
    bxeq    lr
    stmfd   sp!,{lr}
.if EXTERNAL_YM2612
    tst     r2, #0x200
    ldreq   r2, =YM2612Write_
    ldrne   r2, =YM2612Write_940
    mov     lr, pc
    bx      r2
.else
    bl      YM2612Write_
.endif
    ldr     r2, =emustatus
    ldmfd   sp!,{lr}
    ldr     r1, [r2]
    and     r0, r0, #1
    orr     r1, r0, r1
    str     r1, [r2]         @ emustatus|=YM2612Write(a&3, d);
    bx      lr

m_write8_z80_not_ym2612:     @ not too likely
    mov     r2, r0, lsl #17
    bic     r2, r2, #6<<17
    mov     r3,     #0x7f00
    orr     r3, r3, #0x0011
    cmp     r3, r2, lsr #17  @ psg @ z80 area?
    beq     m_write8_psg
    and     r2, r0, #0x7f00
    cmp     r2,     #0x6000  @ bank register?
    bxne    lr               @ invalid write

m_write8_z80_bank_reg:
    ldr     r3, =(Pico+0x22208) @ Pico.m
    ldrh    r2, [r3, #0x0a]
    mov     r1, r1, lsr #8
    orr     r2, r1, r2, lsr #1
    bic     r2, r2, #0xfe00
    strh    r2, [r3, #0x0a]
    bx      lr


m_write8_not_z80:
    and     r2, r0, #0xe70000
    cmp     r2, #0xc00000    @ VDP area?
    bne     OtherWrite8      @ passthrough
    and     r2, r0, #0xf9
    cmp     r2, #0x11
    bne     OtherWrite8
m_write8_psg:
    ldr     r2, =PicoOpt
    mov     r0, r1
    ldr     r2, [r2]
    tst     r2, #2
    bxeq    lr
    b       SN76496Write

