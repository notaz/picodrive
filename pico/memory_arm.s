@ vim:filetype=armasm

@ (c) Copyright 2006-2009, Grazvydas "notaz" Ignotas
@ All Rights Reserved


@@ .include "port_config.s"

.equ SRR_MAPPED,    (1 <<  0)
.equ SRR_READONLY,  (1 <<  1)
.equ SRF_EEPROM,    (1 <<  1)
.equ POPT_6BTN_PAD, (1 <<  5)
.equ POPT_DIS_32X,  (1 << 20)

.text
.align 4

.global PicoRead8_sram
.global PicoRead8_io
.global PicoRead16_sram
.global PicoRead16_io
.global PicoWrite8_io
.global PicoWrite16_io

PicoRead8_sram: @ u32 a, u32 d
    ldr     r2, =(SRam)
    ldr     r3, =(Pico+0x22200)
    ldr     r1, [r2, #8]    @ SRam.end
    cmp     r0, r1
    bge     m_read8_nosram
    ldr     r1, [r2, #4]    @ SRam.start
    cmp     r0, r1
    blt     m_read8_nosram
    ldrb    r1, [r3, #0x11] @ Pico.m.sram_reg
    tst     r1, #SRR_MAPPED
    beq     m_read8_nosram
    ldr     r1, [r2, #0x0c]
    tst     r1, #SRF_EEPROM
    bne     m_read8_eeprom
    ldr     r1, [r2, #4]    @ SRam.start
    ldr     r2, [r2]        @ SRam.data
    sub     r0, r0, r1
    add     r0, r0, r2
    ldrb    r0, [r0]
    bx      lr

m_read8_nosram:
    ldr     r1, [r3, #4]    @ romsize
    cmp     r0, r1
    movgt   r0, #0
    bxgt    lr              @ bad location
    @ XXX: banking unfriendly
    ldr     r1, [r3]
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr

m_read8_eeprom:
    stmfd   sp!,{r0,lr}
    bl      EEPROM_read
    ldmfd   sp!,{r0,lr}
    tst     r0, #1
    moveq   r0, r0, lsr #8
    bx      lr


PicoRead8_io: @ u32 a, u32 d
    bic     r2, r0, #0x001f   @ most commonly we get i/o port read,
    cmp     r2, #0xa10000     @ so check for it first
    bne     m_read8_not_io

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

m_read8_not_io:
    and     r2, r0, #0xfc00
    cmp     r2, #0x1000
    bne     m_read8_not_brq

    ldr     r3, =(Pico+0x22200)
    mov     r1, r0
    ldr     r0, [r3, #8]      @ Pico.m.rotate
    add     r0, r0, #1
    strb    r0, [r3, #8]
    eor     r0, r0, r0, lsl #6

    tst     r1, #1
    bxne    lr                @ odd addr -> open bus
    bic     r0, r0, #1        @ bit0 defined in this area
    and     r2, r1, #0xff00
    cmp     r2, #0x1100
    bxne    lr                @ not busreq

    ldrb    r1, [r3, #(8+0x01)] @ Pico.m.z80Run
    ldrb    r2, [r3, #(8+0x0f)] @ Pico.m.z80_reset
    orr     r0, r0, r1
    orr     r0, r0, r2
    bx      lr

m_read8_not_brq:
    ldr     r2, =PicoOpt
    ldr     r2, [r2]
    tst     r2, #POPT_DIS_32X
    beq     PicoRead8_32x
    mov     r0, #0
    bx      lr

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

PicoRead16_sram: @ u32 a, u32 d
    ldr     r2, =(SRam)
    ldr     r3, =(Pico+0x22200)
    ldr     r1, [r2, #8]    @ SRam.end
    cmp     r0, r1
    bge     m_read16_nosram
    ldr     r1, [r2, #4]    @ SRam.start
    cmp     r0, r1
    blt     m_read16_nosram
    ldrb    r1, [r3, #0x11] @ Pico.m.sram_reg
    tst     r1, #SRR_MAPPED
    beq     m_read16_nosram
    ldr     r1, [r2, #0x0c]
    tst     r1, #SRF_EEPROM
    bne     EEPROM_read
    ldr     r1, [r2, #4]    @ SRam.start
    ldr     r2, [r2]        @ SRam.data
    sub     r0, r0, r1
    add     r0, r0, r2
    ldrb    r1, [r0], #1
    ldrb    r0, [r0]
    orr     r0, r0, r1, lsl #8
    bx      lr

m_read16_nosram:
    ldr     r1, [r3, #4]    @ romsize
    cmp     r0, r1
    movgt   r0, #0
    bxgt    lr              @ bad location
    @ XXX: banking unfriendly
    ldr     r1, [r3]
    ldrh    r0, [r1, r0]
    bx      lr


PicoRead16_io: @ u32 a, u32 d
    bic     r2, r0, #0x001f   @ most commonly we get i/o port read,
    cmp     r2, #0xa10000     @ so check for it first
    bne     m_read16_not_io
    stmfd   sp!,{lr}
    bl      m_read8_misc_io   @ same as read8
    orr     r0, r0, r0, lsl #8 @ only has bytes mirrored
    ldmfd   sp!,{pc}

m_read16_not_io:
    and     r2, r0, #0xfc00
    cmp     r2, #0x1000
    bne     m_read16_not_brq

    ldr     r3, =(Pico+0x22200)
    and     r2, r0, #0xff00
    ldr     r0, [r3, #8]      @ Pico.m.rotate
    add     r0, r0, #1
    strb    r0, [r3, #8]
    eor     r0, r0, r0, lsl #5
    eor     r0, r0, r0, lsl #8
    bic     r0, r0, #0x100    @ bit8 defined in this area
    cmp     r2, #0x1100
    bxne    lr                @ not busreq

    ldrb    r1, [r3, #(8+0x01)] @ Pico.m.z80Run
    ldrb    r2, [r3, #(8+0x0f)] @ Pico.m.z80_reset
    orr     r0, r0, r1, lsl #8
    orr     r0, r0, r2, lsl #8
    bx      lr

m_read16_not_brq:
    ldr     r2, =PicoOpt
    ldr     r2, [r2]
    tst     r2, #POPT_DIS_32X
    beq     PicoRead16_32x
    mov     r0, #0
    bx      lr

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

PicoWrite8_io: @ u32 a, u32 d
    bic     r2, r0, #0x1e       @ most commonly we get i/o port write,
    eor     r2, r2, #0xa10000   @ so check for it first
    eors    r2, r2, #1
    bne     m_write8_not_io

m_write8_io:
    ldr     r2, =PicoOpt
    and     r0, r0, #0x1e
    ldr     r2, [r2]
    ldr     r3, =(Pico+0x22000) @ Pico.ioports
    tst     r2, #POPT_6BTN_PAD
    beq     m_write8_io_done
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

m_write8_not_io:
    tst     r0, #1
    bne     m_write8_not_z80ctl @ even addrs only
    and     r2, r0, #0xff00
    cmp     r2, #0x1100
    moveq   r0, r1
    beq     ctl_write_z80busreq
    cmp     r2, #0x1200
    moveq   r0, r1
    beq     ctl_write_z80reset

m_write8_not_z80ctl:
    @ unlikely
    eor     r2, r0, #0xa10000
    eor     r2, r2, #0x003000
    eors    r2, r2, #0x0000f1
    bne     m_write8_not_sreg
    ldr     r3, =(Pico+0x22200)
    ldrb    r2, [r3, #(8+9)] @ Pico.m.sram_reg
    and     r1, r1, #(SRR_MAPPED|SRR_READONLY)
    bic     r2, r2, #(SRR_MAPPED|SRR_READONLY)
    orr     r2, r2, r1
    strb    r2, [r3, #(8+9)]
    bx      lr

m_write8_not_sreg:
    ldr     r2, =PicoOpt
    ldr     r2, [r2]
    tst     r2, #POPT_DIS_32X
    beq     PicoWrite8_32x
    bx      lr

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

PicoWrite16_io: @ u32 a, u32 d
    bic     r2, r0, #0x1f    @ most commonly we get i/o port write,
    cmp     r2, #0xa10000    @ so check for it first
    beq     m_write8_io

m_write16_not_io:
    and     r2, r0, #0xff00
    cmp     r2, #0x1100
    moveq   r0, r1, lsr #8
    beq     ctl_write_z80busreq
    cmp     r2, #0x1200
    moveq   r0, r1, lsr #8
    beq     ctl_write_z80reset

m_write16_not_z80ctl:
    @ unlikely
    eor     r2, r0, #0xa10000
    eor     r2, r2, #0x003000
    eors    r2, r2, #0x0000f0
    bne     m_write16_not_sreg
    ldr     r3, =(Pico+0x22200)
    ldrb    r2, [r3, #(8+9)] @ Pico.m.sram_reg
    and     r1, r1, #(SRR_MAPPED|SRR_READONLY)
    bic     r2, r2, #(SRR_MAPPED|SRR_READONLY)
    orr     r2, r2, r1
    strb    r2, [r3, #(8+9)]
    bx      lr

m_write16_not_sreg:
    ldr     r2, =PicoOpt
    ldr     r2, [r2]
    tst     r2, #POPT_DIS_32X
    beq     PicoWrite16_32x
    bx      lr

.pool

