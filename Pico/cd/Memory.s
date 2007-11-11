@ vim:filetype=armasm

@ Memory I/O handlers for Sega/Mega CD emulation
@ (c) Copyright 2007, Grazvydas "notaz" Ignotas



.equiv PCM_STEP_SHIFT, 11
.equiv POLL_LIMIT, 16

@ jump tables
.data
.align 4

.altmacro
.macro mk_m68k_jump_table on sz @ operation name, size
    .long   m_m68k_&\on&\sz&_bios           @ 0x000000 - 0x01ffff
    .long   m_m68k_&\on&\sz&_prgbank        @ 0x020000 - 0x03ffff
    .long   m_&\on&_null, m_&\on&_null      @ 0x040000 - 0x07ffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0x080000 - 0x0fffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0x100000 - 0x17ffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0x180000 - 0x1fffff
    .long   m_m68k_&\on&\sz&_wordram0_2M    @ 0x200000 - 0x21ffff
    .long   m_m68k_&\on&\sz&_wordram1_2M    @ 0x220000 - 0x23ffff
    .long   m_&\on&_null, m_&\on&_null      @ 0x240000 - 0x27ffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0x280000 - 0x2fffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0x300000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @          - 0x3fffff
    .long   m_m68k_&\on&\sz&_bcram_size     @ 0x400000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null               @ 0x420000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @          - 0x4fffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0x500000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @          - 0x5fffff
    .long   m_m68k_&\on&\sz&_bcram          @ 0x600000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null               @ 0x620000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @          - 0x6fffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0x700000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null               @          - 0x7dffff
    .long   m_m68k_&\on&\sz&_bcram_reg                             @ 0x7e0000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0x800000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @          - 0x8fffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0x900000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @          - 0x9fffff
    .long   m_m68k_&\on&\sz&_system_io      @ 0xa00000 - 0xa1ffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null               @ 0xa20000 - 0xa7ffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0xa80000 - 0xafffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0xb00000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @          - 0xbfffff
    .long   m_m68k_&\on&\sz&_vdp, m_m68k_&\on&\sz&_vdp, m_m68k_&\on&\sz&_vdp, m_m68k_&\on&\sz&_vdp @ 0xc00000
    .long   m_m68k_&\on&\sz&_vdp, m_m68k_&\on&\sz&_vdp, m_m68k_&\on&\sz&_vdp, m_m68k_&\on&\sz&_vdp @          - 0xcfffff
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @ 0xd00000
    .long   m_&\on&_null, m_&\on&_null, m_&\on&_null, m_&\on&_null @          - 0xdfffff
    .long   m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram @ 0xe00000
    .long   m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram @          - 0xefffff
    .long   m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram @ 0xf00000
    .long   m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram, m_m68k_&\on&\sz&_ram @          - 0xffffff
.endm

.macro mk_s68k_jump_table on sz @ operation name, size
    .long   m_s68k_&\on&\sz&_prg, m_s68k_&\on&\sz&_prg, m_s68k_&\on&\sz&_prg, m_s68k_&\on&\sz&_prg            @ 0x000000 - 0x07ffff
    .long   m_s68k_&\on&\sz&_wordram_2M     @ 0x080000 - 0x09ffff
    .long   m_s68k_&\on&\sz&_wordram_2M     @ 0x0a0000 - 0x0bffff
    .long   m_&\on&_null                    @ 0x0c0000 - 0x0dffff, 1M area
    .long   m_&\on&_null                    @ 0x0e0000 - 0x0fffff
.endm


@ the jumptables themselves.
m_m68k_read8_table:   mk_m68k_jump_table read 8
m_m68k_read16_table:  mk_m68k_jump_table read 16
m_m68k_read32_table:  mk_m68k_jump_table read 32
m_m68k_write8_table:  mk_m68k_jump_table write 8
m_m68k_write16_table: mk_m68k_jump_table write 16
m_m68k_write32_table: mk_m68k_jump_table write 32

m_s68k_read8_table:   mk_s68k_jump_table read 8
m_s68k_read16_table:  mk_s68k_jump_table read 16
m_s68k_read32_table:  mk_s68k_jump_table read 32
m_s68k_write8_table:  mk_s68k_jump_table write 8
m_s68k_write16_table: mk_s68k_jump_table write 16
m_s68k_write32_table: mk_s68k_jump_table write 32

m_s68k_decode_write_table:
    .long m_s68k_write8_2M_decode_b0_m0
    .long m_s68k_write16_2M_decode_b0_m0
    .long m_s68k_write32_2M_decode_b0_m0
    .long m_s68k_write8_2M_decode_b0_m1
    .long m_s68k_write16_2M_decode_b0_m1
    .long m_s68k_write32_2M_decode_b0_m1
    .long m_s68k_write8_2M_decode_b0_m2
    .long m_s68k_write16_2M_decode_b0_m2
    .long m_s68k_write32_2M_decode_b0_m2
    .long m_s68k_write8_2M_decode_b1_m0
    .long m_s68k_write16_2M_decode_b1_m0
    .long m_s68k_write32_2M_decode_b1_m0
    .long m_s68k_write8_2M_decode_b1_m1
    .long m_s68k_write16_2M_decode_b1_m1
    .long m_s68k_write32_2M_decode_b1_m1
    .long m_s68k_write8_2M_decode_b1_m2
    .long m_s68k_write16_2M_decode_b1_m2
    .long m_s68k_write32_2M_decode_b1_m2


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.text
.align 4

.global PicoMemResetCD
.global PicoMemResetCDdecode
.global PicoReadM68k8
.global PicoReadM68k16
.global PicoReadM68k32
.global PicoWriteM68k8
.global PicoWriteM68k16
.global PicoWriteM68k32
.global PicoReadS68k8
.global PicoReadS68k16
.global PicoReadS68k32
.global PicoWriteS68k8
.global PicoWriteS68k16
.global PicoWriteS68k32

@ externs, just for reference
.extern Pico
.extern z80Read8
.extern OtherRead16
.extern PicoVideoRead
.extern Read_CDC_Host
.extern m68k_reg_write8
.extern OtherWrite8
.extern OtherWrite16
.extern gfx_cd_read
.extern s68k_reg_read16
.extern SRam
.extern gfx_cd_write16
.extern s68k_reg_write8
.extern s68k_poll_adclk
.extern PicoCpuMS68k
.extern s68k_poll_detect
.extern SN76496Write
.extern m_m68k_read8_misc
.extern m_m68k_write8_misc


@ r0=reg3, r1-r3=temp
.macro mk_update_table on sz @ operation name, size
    @ we only set word-ram handlers
    ldr     r1, =m_m68k_&\on&\sz&_table
    ldr     r12,=m_s68k_&\on&\sz&_table
    tst     r0, #4
    bne     0f @ pmr_8_1M

@ pmr_8_2M:
    ldr     r2, =m_m68k_&\on&\sz&_wordram0_2M
    ldr     r3, =m_s68k_&\on&\sz&_wordram_2M
    str     r2, [r1, #16*4]
    str     r2, [r1, #17*4]
    ldr     r2, =m_&\on&_null
    str     r3, [r12,#4*4]
    str     r3, [r12,#5*4]
    str     r2, [r12,#6*4]
    b       9f @ pmr_8_done

0: @ pmr_8_1M:
    tst     r0, #1
    bne     1f @ pmr_8_1M1

@ pmr_8_1M0:
    ldr     r2, =m_m68k_&\on&\sz&_wordram0_1M_b0
    ldr     r3, =m_m68k_&\on&\sz&_wordram1_1M_b0
    str     r2, [r1, #16*4]
    str     r3, [r1, #17*4]
    ldr     r3, =m_s68k_&\on&\sz&_wordram_1M_b1
.ifeqs "\on", "read"
    ldr     r2, =m_s68k_&\on&\sz&_wordram_2M_decode_b1
    str     r2, [r12,#4*4]
    str     r2, [r12,#5*4]
.endif
    str     r3, [r12,#6*4]
    b       9f @ pmr_8_done

1: @ pmr_8_1M1:
    ldr     r2, =m_m68k_&\on&\sz&_wordram0_1M_b1
    ldr     r3, =m_m68k_&\on&\sz&_wordram1_1M_b1
    str     r2, [r1, #16*4]
    str     r3, [r1, #17*4]
    ldr     r3, =m_s68k_&\on&\sz&_wordram_1M_b0
.ifeqs "\on", "read"
    ldr     r2, =m_s68k_&\on&\sz&_wordram_2M_decode_b0
    str     r2, [r12,#4*4]
    str     r2, [r12,#5*4]
.endif
    str     r3, [r12,#6*4]

9: @ pmr_8_done:
.endm


PicoMemResetCD: @ r3
    mk_update_table read 8
    mk_update_table read 16
    mk_update_table read 32
    mk_update_table write 8
    mk_update_table write 16
    mk_update_table write 32
    bx      lr


PicoMemResetCDdecode: @reg3
    tst     r0, #4
    bxeq    lr                 @ we should not be called in 2M mode
    ldr     r1, =m_s68k_write8_table
    ldr     r3, =m_s68k_decode_write_table
    and     r2, r0, #0x18
    mov     r2, r2, lsr #3
    cmp     r2, #3
    moveq   r2, #2             @ mode3 is same as mode2?
    tst     r0, #1
    addeq   r2, r2, #3         @ bank1 (r2=0..5)
    add     r2, r2, r2, lsl #1 @ *= 3
    add     r2, r3, r2, lsl #2
    ldmia   r2, {r0,r3,r12}
    str     r0, [r1, #4*4]
    str     r0, [r1, #5*4]
    str     r3, [r1, #4*4+8*4]
    str     r3, [r1, #5*4+8*4]
    str     r12,[r1, #4*4+8*4*2]
    str     r12,[r1, #5*4+8*4*2]
    bx      lr


.pool

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

.macro mk_entry_m68k table
    ldr     r2, =\table
    bic     r0, r0, #0xff000000
    and     r3, r0, #0x00fe0000
    ldr     pc, [r2, r3, lsr #15]
.endm

PicoReadM68k8: @ u32 a
    mk_entry_m68k m_m68k_read8_table

PicoReadM68k16: @ u32 a
    mk_entry_m68k m_m68k_read16_table

PicoReadM68k32: @ u32 a
    mk_entry_m68k m_m68k_read32_table

PicoWriteM68k8: @ u32 a, u8 d
    mk_entry_m68k m_m68k_write8_table

PicoWriteM68k16: @ u32 a, u16 d
    mk_entry_m68k m_m68k_write16_table

PicoWriteM68k32: @ u32 a, u32 d
    mk_entry_m68k m_m68k_write32_table


.macro mk_entry_s68k on sz
    bic     r0, r0, #0xff000000
    cmp     r0, #0x00080000
    blt     m_s68k_&\on&\sz&_prg
    cmp     r0, #0x000e0000
    ldrlt   r2, =m_s68k_&\on&\sz&_table
    andlt   r3, r0, #0x000e0000
    ldrlt   pc, [r2, r3, lsr #15]
    mov     r3,     #0x00ff0000
    orr     r3, r3, #0x00008000
    cmp     r0, r3
    bge     m_s68k_&\on&\sz&_regs
    cmp     r0, #0x00ff0000
    bge     m_s68k_&\on&\sz&_pcm
    cmp     r0, #0x00fe0000
    bge     m_s68k_&\on&\sz&_backup
    mov     r0, #0
    bx      lr
.endm

PicoReadS68k8: @ u32 a
    mk_entry_s68k read 8

PicoReadS68k16: @ u32 a
    mk_entry_s68k read 16

PicoReadS68k32: @ u32 a
    mk_entry_s68k read 32

PicoWriteS68k8: @ u32 a, u8 d
    mk_entry_s68k write 8

PicoWriteS68k16: @ u32 a, u16 d
    mk_entry_s68k write 16

PicoWriteS68k32: @ u32 a, u32 d
    mk_entry_s68k write 32


.pool

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

@ utilities

@ r0=addr[in,out], r1,r2=tmp
.macro cell_map
    ands    r1, r0, #0x01c000
    ldrne   pc, [pc, r1, lsr #12]
    beq     0f                          @ most common?
    .long   0f
    .long   0f
    .long   0f
    .long   0f
    .long   1f
    .long   1f
    .long   2f
    .long   3f
1: @ x16 cells
    and     r1, r0, #0x7e00             @ col
    and     r2, r0, #0x01fc             @ row
    orr     r2, r2, #0x0400
    orr     r1, r2, r1, ror #13
    b       9f
2: @ x8 cells
    and     r1, r0, #0x3f00             @ col
    and     r2, r0, #0x00fc             @ row
    orr     r2, r2, #0x0600
    orr     r1, r2, r1, ror #12
    b       9f
3: @ x4 cells
    and     r1, r0, #0x1f80             @ col
    and     r2, r0, #0x007c             @ row
    orr     r1, r2, r1, ror #11
    and     r2, r0,#0x1e000
    orr     r1, r1, r2, lsr #6
    b       9f
0: @ x32 cells
    and     r1, r0, #0xfc00             @ col
    and     r2, r0, #0x03fc             @ row
    orr     r1, r2, r1, ror #14
9:
    and     r0, r0, #3
    orr     r0, r0, r1, ror #26         @ rol 4+2
.endm


@ r0=prt1, r1=ptr2; unaligned ptr MUST be r0
.macro m_read32_gen
    tst     r0, #2
    ldrneh  r0, [r1, r0]!
    ldrneh  r1, [r1, #2]
    ldreq   r0, [r1, r0]
    moveq   r0, r0, ror #16
    orrne   r0, r1, r0, lsl #16
.endm


@ r0=prt1, r1=data, r2=ptr2; unaligned ptr MUST be r0
.macro m_write32_gen
    tst     r0, #2
    mov     r1, r1, ror #16
    strneh  r1, [r2, r0]!
    movne   r1, r1, lsr #16
    strneh  r1, [r2, #2]
    streq   r1, [r2, r0]
.endm

@
.macro bcram_reg_rw is_read addr_check
    rsb     r0, r0, #0x800000
    ldr     r2, =(Pico+0x22200)
    cmp     r0, #(0x800000-\addr_check)
    ldreq   r2, [r2]
.if \is_read
    movne   r0, #0
.endif
    bxne    lr
    add     r2, r2, #0x110000
    add     r2, r2, #0x002200
.if \is_read
    ldrb    r0, [r2, #0x18]      @ Pico_mcd->m.bcram_reg
.else
    strb    r1, [r2, #0x18]
.endif
    bx      lr
.endm

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


m_read_null:
    mov     r0, #0
    bx      lr


m_m68k_read8_bios:
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #0xfe0000
    ldr     r1, [r1]
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr


m_m68k_read8_prgbank:
    ldr     r1, =(Pico+0x22200)
    eor     r0, r0, #1
    ldr     r1, [r1]
    mov     r2, #0x110000
    orr     r3, r2, #0x002200
    ldr     r3, [r1, r3]
    ldr     r2, [r1, r2]
    and     r3, r3, #0x00030000
    cmp     r3,     #0x00010000         @ have bus or in reset state?
    moveq   r0, #0
    bxeq    lr
    and     r2, r2, #0xc0000000		@ r3 & 0xC0
    add     r1, r1, r2, lsr #12
    ldrb    r0, [r1, r0]
    bx      lr


m_m68k_read8_wordram0_2M:               @ 0x200000 - 0x21ffff
m_m68k_read8_wordram1_2M:               @ 0x220000 - 0x23ffff
    ldr     r1, =(Pico+0x22200)
    sub     r0, r0, #0x160000           @ map to our offset, which is 0x0a0000
    ldr     r1, [r1]
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr


m_m68k_read8_wordram0_1M_b0:            @ 0x200000 - 0x21ffff
    ldr     r1, =(Pico+0x22200)
    sub     r0, r0, #0x140000           @ map to our offset, which is 0x0c0000
    ldr     r1, [r1]
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr


m_m68k_read8_wordram0_1M_b1:            @ 0x200000 - 0x21ffff
    ldr     r1, =(Pico+0x22200)
    sub     r0, r0, #0x120000           @ map to our offset, which is 0x0e0000
    ldr     r1, [r1]
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr


m_m68k_read8_wordram1_1M_b0:            @ 0x220000 - 0x23ffff, cell arranged
    cell_map
    ldr     r1, =(Pico+0x22200)
    add     r0, r0, #0x0c0000
    ldr     r1, [r1]
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr


m_m68k_read8_wordram1_1M_b1:            @ 0x220000 - 0x23ffff, cell arranged
    cell_map
    ldr     r1, =(Pico+0x22200)
    add     r0, r0, #0x0e0000
    ldr     r1, [r1]
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr


m_m68k_read8_bcram_size:                @ 0x400000
    sub     r0, r0, #1
    cmp     r0, #0x400000
    ldreq   r1, =SRam
    mov     r0, #0
    ldreq   r1, [r1]
    bxne    lr
    tst     r1, r1
    movne   r0, #3                      @ pretend to be a 64k cart (8<<3)
    bx      lr


m_m68k_read8_bcram:                     @ 0x600000 - 0x61ffff
    ldr     r1, =SRam
    bic     r0, r0, #0xfe0000
    ldr     r1, [r1]
    mov     r0, r0, lsr #1
    tst     r1, r1
    moveq   r0, #0
    bxeq    lr
    add     r1, r1, #0x2000
    ldrb    r0, [r1, r0]
    bx      lr


m_m68k_read8_bcram_reg:                 @ 0x7fffff
    bcram_reg_rw 1, 0x7fffff


m_m68k_read8_system_io:
    bic     r2, r0, #0xfe0000
    bic     r2, r2, #0x3f
    cmp     r2, #0x012000
    bne     m_m68k_read8_misc         @ now from Pico/Memory.s

    ldr     r1, =(Pico+0x22200)
    and     r0, r0, #0x3f
    ldr     r1, [r1]                  @ Pico.mcd (used everywhere)
    cmp     r0, #0x0e
    ldrlt   pc, [pc, r0, lsl #2]
    b       m_m68k_read8_hi
    .long   m_m68k_read8_r00
    .long   m_m68k_read8_r01
    .long   m_m68k_read8_r02
    .long   m_m68k_read8_r03
    .long   m_m68k_read8_r04
    .long   m_read_null               @ unused bits
    .long   m_m68k_read8_r06
    .long   m_m68k_read8_r07
    .long   m_m68k_read8_r08
    .long   m_m68k_read8_r09
    .long   m_read_null               @ reserved
    .long   m_read_null
    .long   m_m68k_read8_r0c
    .long   m_m68k_read8_r0d
m_m68k_read8_r00:
    add     r1, r1, #0x110000
    ldr     r0, [r1, #0x30]
    and     r0, r0, #0x04000000       @ we need irq2 mask state
    mov     r0, r0, lsr #19
    bx      lr
m_m68k_read8_r01:
    add     r1, r1, #0x110000
    add     r1, r1, #0x002200
    ldrb    r0, [r1, #2]              @ Pico_mcd->m.busreq
    bx      lr
m_m68k_read8_r02:
    add     r1, r1, #0x110000
    ldrb    r0, [r1, #2]
    bx      lr
m_m68k_read8_r03:
    add     r1, r1, #0x110000
    ldrb    r0, [r1, #3]
    add     r1, r1, #0x002200
    ldr     r1, [r1, #4]
    and     r0, r0, #0xc7
    tst     r1, #2                    @ DMNA pending?
    bxeq    lr
    bic     r0, r0, #1
    orr     r0, r0, #2
    bx      lr
m_m68k_read8_r04:
    add     r1, r1, #0x110000
    ldrb    r0, [r1, #4]
    bx      lr
m_m68k_read8_r06:
    ldrb    r0, [r1, #0x73]           @ IRQ vector
    bx      lr
m_m68k_read8_r07:
    ldrb    r0, [r1, #0x72]
    bx      lr
m_m68k_read8_r08:
    mov     r0, #0
    bl      Read_CDC_Host             @ TODO: make it local
    mov     r0, r0, lsr #8
    bx      lr
m_m68k_read8_r09:
    mov     r0, #0
    b       Read_CDC_Host
m_m68k_read8_r0c:
    add     r1, r1, #0x110000
    add     r1, r1, #0x002200
    ldr     r0, [r1, #0x14]           @ Pico_mcd->m.timer_stopwatch
    mov     r0, r0, lsr #24
    bx      lr
m_m68k_read8_r0d:
    add     r1, r1, #0x110000
    add     r1, r1, #0x002200
    ldr     r0, [r1, #0x14]
    mov     r0, r0, lsr #16
    bx      lr
m_m68k_read8_hi:
    cmp     r0, #0x30
    movge   r0, #0
    bxeq    lr
    add     r1, r1, #0x110000
    ldrb    r0, [r1, r0]
    bx      lr

/*
m_m68k_read8_misc:
    bic     r2, r0, #0x00ff
    bic     r2, r2, #0xbf00
    cmp     r2, #0xa00000  @ Z80 RAM?
    beq     z80Read8
@    ldreq   r2, =z80Read8
@    bxeq    r2
    stmfd   sp!,{r0,lr}
    bic     r0, r0, #1
    mov     r1, #8
    bl      OtherRead16                 @ non-MCD version should be ok too
    ldmfd   sp!,{r1,lr}
    tst     r1, #1
    moveq   r0, r0, lsr #8
    bx      lr
*/

m_m68k_read8_vdp:
    tst     r0, #0x70000
    tsteq   r0, #0x000e0
    bxne    lr              @ invalid read
    stmfd   sp!,{r0,lr}
    bic     r0, r0, #1
    bl      PicoVideoRead               @ TODO: implement it in asm
    ldmfd   sp!,{r1,lr}
    tst     r1, #1
    moveq   r0, r0, lsr #8
    bx      lr


m_m68k_read8_ram:
    ldr     r1, =Pico
    bic     r0, r0, #0xff0000
    eor     r0, r0, #1
    ldrb    r0, [r1, r0]
    bx      lr


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


m_m68k_read16_bios:
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #0xfe0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    ldrh    r0, [r1, r0]
    bx      lr


m_m68k_read16_prgbank:
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #1
    ldr     r1, [r1]
    mov     r2, #0x110000
    orr     r3, r2, #0x002200
    ldr     r3, [r1, r3]
    ldr     r2, [r1, r2]
    and     r3, r3, #0x00030000
    cmp     r3,     #0x00010000         @ have bus or in reset state?
    moveq   r0, #0
    bxeq    lr
    and     r2, r2, #0xc0000000		@ r3 & 0xC0
    add     r1, r1, r2, lsr #12
    ldrh    r0, [r1, r0]
    bx      lr


m_m68k_read16_wordram0_2M:              @ 0x200000 - 0x21ffff
m_m68k_read16_wordram1_2M:              @ 0x220000 - 0x23ffff
    ldr     r1, =(Pico+0x22200)
    sub     r0, r0, #0x160000           @ map to our offset, which is 0x0a0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    ldrh    r0, [r1, r0]
    bx      lr


m_m68k_read16_wordram0_1M_b0:           @ 0x200000 - 0x21ffff
    ldr     r1, =(Pico+0x22200)
    sub     r0, r0, #0x140000           @ map to our offset, which is 0x0c0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    ldrh    r0, [r1, r0]
    bx      lr


m_m68k_read16_wordram0_1M_b1:           @ 0x200000 - 0x21ffff
    ldr     r1, =(Pico+0x22200)
    sub     r0, r0, #0x120000           @ map to our offset, which is 0x0e0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    ldrh    r0, [r1, r0]
    bx      lr


m_m68k_read16_wordram1_1M_b0:           @ 0x220000 - 0x23ffff, cell arranged
    @ Warning: read32 relies on NOT using r3 and r12 here
    cell_map
    ldr     r1, =(Pico+0x22200)
    add     r0, r0, #0x0c0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    ldrh    r0, [r1, r0]
    bx      lr


m_m68k_read16_wordram1_1M_b1:           @ 0x220000 - 0x23ffff, cell arranged
    cell_map
    ldr     r1, =(Pico+0x22200)
    add     r0, r0, #0x0e0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    ldrh    r0, [r1, r0]
    bx      lr


m_m68k_read16_bcram_size:               @ 0x400000
    cmp     r0, #0x400000
    ldreq   r1, =SRam
    mov     r0, #0
    ldreq   r1, [r1]
    bxne    lr
    tst     r1, r1
    movne   r0, #3                      @ pretend to be a 64k cart
    bx      lr


@ m_m68k_read16_bcram:                  @ 0x600000 - 0x61ffff
.equiv m_m68k_read16_bcram, m_m68k_read8_bcram


m_m68k_read16_bcram_reg:                @ 0x7fffff
    bcram_reg_rw 1, 0x7ffffe


m_m68k_read16_system_io:
    bic     r1, r0, #0xfe0000
    bic     r1, r1, #0x3f
    cmp     r1, #0x012000
    bne     m_m68k_read16_misc

m_m68k_read16_m68k_regs:
    ldr     r1, =(Pico+0x22200)
    and     r0, r0, #0x3e
    ldr     r1, [r1]                  @ Pico.mcd (used everywhere)
    cmp     r0, #0x0e
    ldrlt   pc, [pc, r0, lsl #1]
    b       m_m68k_read16_hi
    .long   m_m68k_read16_r00
    .long   m_m68k_read16_r02
    .long   m_m68k_read16_r04
    .long   m_m68k_read16_r06
    .long   m_m68k_read16_r08
    .long   m_read_null               @ reserved
    .long   m_m68k_read16_r0c
m_m68k_read16_r00:
    add     r1, r1, #0x110000
    ldr     r0, [r1, #0x30]
    add     r1, r1, #0x002200
    ldrb    r1, [r1, #2]              @ Pico_mcd->m.busreq
    and     r0, r0, #0x04000000       @ we need irq2 mask state
    orr     r0, r1, r0, lsr #11
    bx      lr
m_m68k_read16_r02:
    add     r1, r1, #0x110000
    ldrb    r0, [r1, #2]
    ldrb    r2, [r1, #3]
    add     r1, r1, #0x002200
    ldr     r1, [r1, #4]
    and     r2, r2, #0xc7
    orr     r0, r2, r0, lsl #8
    tst     r1, #2                    @ DMNA pending?
    bxeq    lr
    bic     r0, r0, #1
    orr     r0, r0, #2
    bx      lr
m_m68k_read16_r04:
    add     r1, r1, #0x110000
    ldrb    r0, [r1, #4]
    mov     r0, r0, lsl #8
    bx      lr
m_m68k_read16_r06:
    ldrh    r0, [r1, #0x72]           @ IRQ vector
    bx      lr
m_m68k_read16_r08:
    mov     r0, #0
    b       Read_CDC_Host
m_m68k_read16_r0c:
    add     r1, r1, #0x110000
    add     r1, r1, #0x002200
    ldr     r0, [r1, #0x14]
    mov     r0, r0, lsr #16
    bx      lr
m_m68k_read16_hi:
    cmp     r0, #0x30
    addlt   r1, r1, #0x110000
    ldrlth  r1, [r1, r0]
    movge   r0, #0
    bxge    lr
    mov     r0, r1, lsr #8
    and     r1, r1, #0xff
    orr     r0, r0, r1, lsl #8
    bx      lr


m_m68k_read16_misc:
    bic     r0, r0, #1
    mov     r1, #16
    b       OtherRead16


m_m68k_read16_vdp:
    tst     r0, #0x70000
    tsteq   r0, #0x000e0
    bxne    lr              @ invalid read
    bic     r0, r0, #1
    b       PicoVideoRead


m_m68k_read16_ram:
    ldr     r1, =Pico
    bic     r0, r0, #0xff0000
    bic     r0, r0, #1
    ldrh    r0, [r1, r0]
    bx      lr


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


m_m68k_read32_bios:
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #0xfe0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    m_read32_gen
    bx      lr


m_m68k_read32_prgbank:
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #1
    ldr     r1, [r1]
    mov     r2, #0x110000
    orr     r3, r2, #0x002200
    ldr     r3, [r1, r3]
    ldr     r2, [r1, r2]
    and     r3, r3, #0x00030000
    cmp     r3,     #0x00010000         @ have bus or in reset state?
    moveq   r0, #0
    bxeq    lr
    and     r2, r2, #0xc0000000		@ r3 & 0xC0
    add     r1, r1, r2, lsr #12
    m_read32_gen
    bx      lr


m_m68k_read32_wordram0_2M:              @ 0x200000 - 0x21ffff
m_m68k_read32_wordram1_2M:              @ 0x220000 - 0x23ffff
    ldr     r1, =(Pico+0x22200)
    sub     r0, r0, #0x160000           @ map to our offset, which is 0x0a0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    m_read32_gen
    bx      lr


m_m68k_read32_wordram0_1M_b0:           @ 0x200000 - 0x21ffff
    ldr     r1, =(Pico+0x22200)
    sub     r0, r0, #0x140000           @ map to our offset, which is 0x0c0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    m_read32_gen
    bx      lr


m_m68k_read32_wordram0_1M_b1:           @ 0x200000 - 0x21ffff
    ldr     r1, =(Pico+0x22200)
    sub     r0, r0, #0x120000           @ map to our offset, which is 0x0e0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    m_read32_gen
    bx      lr


m_m68k_read32_wordram1_1M_b0:           @ 0x220000 - 0x23ffff, cell arranged
    tst     r0, #2
    bne     m_m68k_read32_wordram1_1M_b0_unal
    cell_map
    ldr     r1, =(Pico+0x22200)
    add     r0, r0, #0x0c0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    m_read32_gen
    bx      lr
m_m68k_read32_wordram1_1M_b0_unal:
    @ hopefully this doesn't happen too often
    mov     r12,lr
    mov     r3, r0
    bl      m_m68k_read16_wordram1_1M_b0 @ must not trash r12 and r3
    add     r1, r3, #2
    mov     r3, r0
    mov     r0, r1
    bl      m_m68k_read16_wordram1_1M_b0
    orr     r0, r0, r3, lsl #16
    bx      r12


m_m68k_read32_wordram1_1M_b1:            @ 0x220000 - 0x23ffff, cell arranged
    tst     r0, #2
    bne     m_m68k_read32_wordram1_1M_b1_unal
    cell_map
    ldr     r1, =(Pico+0x22200)
    add     r0, r0, #0x0e0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    m_read32_gen
    bx      lr
m_m68k_read32_wordram1_1M_b1_unal:
    mov     r12,lr
    mov     r3, r0
    bl      m_m68k_read16_wordram1_1M_b1 @ must not trash r12 and r3
    add     r1, r3, #2
    mov     r3, r0
    mov     r0, r1
    bl      m_m68k_read16_wordram1_1M_b1
    orr     r0, r0, r3, lsl #16
    bx      r12


m_m68k_read32_bcram_size:               @ 0x400000
    cmp     r0, #0x400000
    ldreq   r1, =SRam
    mov     r0, #0
    ldreq   r1, [r1]
    bxne    lr
    tst     r1, r1
    movne   r0, #0x30000                @ pretend to be a 64k cart
    bx      lr


m_m68k_read32_bcram:                    @ 0x600000 - 0x61ffff, not likely to be called
    mov     r12,lr
    add     r3, r0, #2
    bl      m_m68k_read8_bcram
    mov     r1, r0
    mov     r0, r3
    mov     r3, r1
    bl      m_m68k_read8_bcram
    orr     r0, r0, r3, lsl #16
    bx      r12


m_m68k_read32_bcram_reg:                @ 0x7fffff
    bcram_reg_rw 1, 0x7ffffc


@ it is not very practical to use long access on hw registers, so I assume it is not used too much.
m_m68k_read32_system_io:
    bic     r1, r0, #0xfe0000
    bic     r1, r1, #0x3f
    cmp     r1, #0x012000
    bne     m_m68k_read32_misc
    and     r1, r0, #0x3e
    cmp     r1, #0x0e
    blt     m_m68k_read32_misc
    cmp     r1, #0x30
    movge   r0, #0
    bxge    lr
    @ I have seen the range 0x0e-0x2f accessed quite frequently with long i/o, so here is some code for that
    mov     r0, r1
    ldr     r1, =(Pico+0x22200)
    mov     r2, #0xff
    ldr     r1, [r1]
    orr     r2, r2, r2, lsl #16
    add     r1, r1, #0x110000
    m_read32_gen
    and     r1, r2, r0                @ data is big-endian read as little, have to byteswap
    and     r0, r2, r0, lsr #8
    orr     r0, r0, r1, lsl #8
    bx      lr

m_m68k_read32_misc:
    add     r1, r0, #2
    stmfd   sp!,{r1,lr}
    bl      m_m68k_read16_system_io
    swp     r0, r0, [sp]
    bl      m_m68k_read16_system_io
    ldmfd   sp!,{r1,lr}
    orr     r0, r0, r1, lsl #16
    bx      lr


m_m68k_read32_vdp:
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


m_m68k_read32_ram:
    ldr     r1, =Pico
    bic     r0, r0, #0xff0000
    bic     r0, r0, #1
    m_read32_gen
    bx      lr

.pool

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


m_write_null:
m_m68k_write8_bios:
m_m68k_write8_bcram_size:               @ 0x400000
    bx      lr


m_m68k_write8_prgbank:
    ldr     r2, =(Pico+0x22200)
    eor     r0, r0, #1
    ldr     r2, [r2]
    mov     r12,#0x110000
    orr     r3, r12, #0x002200
    ldr     r3, [r2, r3]
    ldr     r12,[r2, r12]
    and     r3, r3, #0x00030000
    cmp     r3,     #0x00010000         @ have bus or in reset state?
    bxeq    lr
    and     r12,r12,#0xc0000000		@ r3 & 0xC0
    add     r2, r2, r12, lsr #12
    strb    r1, [r2, r0]
    bx      lr


m_m68k_write8_wordram0_2M:              @ 0x200000 - 0x21ffff
m_m68k_write8_wordram1_2M:              @ 0x220000 - 0x23ffff
    ldr     r2, =(Pico+0x22200)
    sub     r0, r0, #0x160000           @ map to our offset, which is 0x0a0000
    ldr     r2, [r2]
    eor     r0, r0, #1
    strb    r1, [r2, r0]
    bx      lr


m_m68k_write8_wordram0_1M_b0:           @ 0x200000 - 0x21ffff
    ldr     r2, =(Pico+0x22200)
    sub     r0, r0, #0x140000           @ map to our offset, which is 0x0c0000
    ldr     r2, [r2]
    eor     r0, r0, #1
    strb    r1, [r2, r0]
    bx      lr


m_m68k_write8_wordram0_1M_b1:           @ 0x200000 - 0x21ffff
    ldr     r2, =(Pico+0x22200)
    sub     r0, r0, #0x120000           @ map to our offset, which is 0x0e0000
    ldr     r2, [r2]
    eor     r0, r0, #1
    strb    r1, [r2, r0]
    bx      lr


m_m68k_write8_wordram1_1M_b0:           @ 0x220000 - 0x23ffff, cell arranged
    mov     r3, r1
    cell_map
    ldr     r2, =(Pico+0x22200)
    add     r0, r0, #0x0c0000
    ldr     r2, [r2]
    eor     r0, r0, #1
    strb    r3, [r2, r0]
    bx      lr


m_m68k_write8_wordram1_1M_b1:           @ 0x220000 - 0x23ffff, cell arranged
    mov     r3, r1
    cell_map
    ldr     r2, =(Pico+0x22200)
    add     r0, r0, #0x0e0000
    ldr     r2, [r2]
    eor     r0, r0, #1
    strb    r3, [r2, r0]
    bx      lr


m_m68k_write8_bcram:                    @ 0x600000 - 0x61ffff
    @ can't use r3 or r12, because of write32
    ldr     r2, =SRam
    bic     r0, r0, #0xfe0000
    ldr     r2, [r2]
    tst     r2, r2
    bxeq    lr
    add     r0, r2, r0, lsr #1
    ldr     r2, =(Pico+0x22200)
    ldr     r2, [r2]
    add     r0, r0, #0x2000
    add     r2, r2, #0x110000
    add     r2, r2, #0x002200
    ldr     r2, [r2, #0x18]
    tst     r2, #1                      @ check bcram reg
    bxeq    lr
    strb    r1, [r0]
    ldr     r2, =SRam
    mov     r0, #1
    strb    r0, [r2, #0x0e]             @ SRam.changed = 1
    bx      lr


m_m68k_write8_bcram_reg:                @ 0x7fffff
    bcram_reg_rw 0, 0x7fffff


m_m68k_write8_system_io:
    bic     r2, r0, #0xfe0000
    bic     r2, r2, #0x3f
    cmp     r2, #0x012000
    beq     m68k_reg_write8
    mov     r2, #8
@    b       OtherWrite8
    b       m_m68k_write8_misc


m_m68k_write8_vdp:
    tst     r0, #0x70000
    tsteq   r0, #0x000e0
    bxne    lr                          @ invalid
    and     r2, r0, #0x19
    cmp     r2, #0x11
    andeq   r0, r1, #0xff
    beq     SN76496Write
    and     r1, r1, #0xff
    orr     r1, r1, r1, lsl #8          @ byte access gets mirrored
    b       PicoVideoWrite


m_m68k_write8_ram:
    ldr     r2, =Pico
    bic     r0, r0, #0xff0000
    eor     r0, r0, #1
    strb    r1, [r2, r0]
    bx      lr


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


m_m68k_write16_bios:
m_m68k_write16_bcram_size:              @ 0x400000
    bx      lr


m_m68k_write16_prgbank:
    ldr     r2, =(Pico+0x22200)
    bic     r0, r0, #1
    ldr     r2, [r2]
    mov     r12,#0x110000
    orr     r3, r12, #0x002200
    ldr     r3, [r2, r3]
    ldr     r12,[r2, r12]
    and     r3, r3, #0x00030000
    cmp     r3,     #0x00010000         @ have bus or in reset state?
    bxeq    lr
    and     r12,r12,#0xc0000000		@ r3 & 0xC0
    add     r2, r2, r12, lsr #12
    strh    r1, [r2, r0]
    bx      lr


m_m68k_write16_wordram0_2M:             @ 0x200000 - 0x21ffff
m_m68k_write16_wordram1_2M:             @ 0x220000 - 0x23ffff
    ldr     r2, =(Pico+0x22200)
    sub     r0, r0, #0x160000           @ map to our offset, which is 0x0a0000
    ldr     r2, [r2]
    bic     r0, r0, #1
    strh    r1, [r2, r0]
    bx      lr


m_m68k_write16_wordram0_1M_b0:          @ 0x200000 - 0x21ffff
    ldr     r2, =(Pico+0x22200)
    sub     r0, r0, #0x140000           @ map to our offset, which is 0x0c0000
    ldr     r2, [r2]
    bic     r0, r0, #1
    strh    r1, [r2, r0]
    bx      lr


m_m68k_write16_wordram0_1M_b1:          @ 0x200000 - 0x21ffff
    ldr     r2, =(Pico+0x22200)
    sub     r0, r0, #0x120000           @ map to our offset, which is 0x0e0000
    ldr     r2, [r2]
    bic     r0, r0, #1
    strh    r1, [r2, r0]
    bx      lr


m_m68k_write16_wordram1_1M_b0:           @ 0x220000 - 0x23ffff, cell arranged
    @ Warning: write32 relies on NOT using r12 and and keeping data in r3
    mov     r3, r1
    cell_map
    ldr     r1, =(Pico+0x22200)
    add     r0, r0, #0x0c0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    strh    r3, [r1, r0]
    bx      lr


m_m68k_write16_wordram1_1M_b1:           @ 0x220000 - 0x23ffff, cell arranged
    mov     r3, r1
    cell_map
    ldr     r1, =(Pico+0x22200)
    add     r0, r0, #0x0e0000
    ldr     r1, [r1]
    bic     r0, r0, #1
    strh    r3, [r1, r0]
    bx      lr


@ m_m68k_write16_bcram:                  @ 0x600000 - 0x61ffff
.equiv m_m68k_write16_bcram, m_m68k_write8_bcram


m_m68k_write16_bcram_reg:                @ 0x7fffff
    bcram_reg_rw 0, 0x7ffffe


m_m68k_write16_system_io:
    bic     r0, r0, #1
    bic     r2, r0, #0xfe0000
    bic     r2, r2, #0x3f
    cmp     r2, #0x012000
    bne     OtherWrite16

m_m68k_write16_regs:
    and     r0, r0, #0x3e
    cmp     r0, #0x0e
    beq     m_m68k_write16_regs_spec
    and     r3, r1, #0xff
    add     r2, r0, #1
    stmfd   sp!,{r2,r3,lr}
    mov     r1, r1, lsr #8
    bl      m68k_reg_write8
    ldmfd   sp!,{r0,r1,lr}
    b       m68k_reg_write8

m_m68k_write16_regs_spec:               @ special case
    ldr     r2, =(Pico+0x22200)
    ldr     r3, =s68k_poll_adclk
    mov     r0, #0x110000
    ldr     r2, [r2]
    add     r0, r0, #0x00000e
    mov     r1, r1, lsr #8
    strb    r1, [r2, r0]                @ if (a == 0xe) s68k_regs[0x0e] = d >> 8;
    ldr     r2, [r3]
    mov     r1, #0
    and     r2, r2, #0xfe
    cmp     r2, #0x0e
    bxne    lr
    ldr     r0, =PicoCpuCS68k
    str     r1, [r0, #0x58]             @ push s68k out of stopped state
    str     r1, [r3]
    bx      lr


m_m68k_write16_vdp:
    tst     r0, #0x70000
    tsteq   r0, #0x000e0
    bxne    lr              @ invalid
    bic     r0, r0, #1
    and     r2, r0, #0x18
    cmp     r2, #0x10
    bne     PicoVideoWrite
    and     r0, r1, #0xff
    b       SN76496Write    @ lsb goes to 0x11


m_m68k_write16_ram:
    ldr     r2, =Pico
    bic     r0, r0, #0xff0000
    bic     r0, r0, #1
    strh    r1, [r2, r0]
    bx      lr


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


m_m68k_write32_bios:
m_m68k_write32_bcram_size:              @ 0x400000
    bx      lr


m_m68k_write32_prgbank:
    ldr     r2, =(Pico+0x22200)
    bic     r0, r0, #1
    ldr     r2, [r2]
    mov     r12,#0x110000
    orr     r3, r12, #0x002200
    ldr     r3, [r2, r3]
    ldr     r12,[r2, r12]
    and     r3, r3, #0x00030000
    cmp     r3,     #0x00010000         @ have bus or in reset state?
    bxeq    lr
    and     r12,r12,#0xc0000000		@ r3 & 0xC0
    add     r2, r2, r12, lsr #12
    m_write32_gen
    bx      lr


m_m68k_write32_wordram0_2M:             @ 0x200000 - 0x21ffff
m_m68k_write32_wordram1_2M:             @ 0x220000 - 0x23ffff
    ldr     r2, =(Pico+0x22200)
    sub     r0, r0, #0x160000           @ map to our offset, which is 0x0a0000
    ldr     r2, [r2]
    bic     r0, r0, #1
    m_write32_gen
    bx      lr


m_m68k_write32_wordram0_1M_b0:           @ 0x200000 - 0x21ffff
    ldr     r2, =(Pico+0x22200)
    sub     r0, r0, #0x140000           @ map to our offset, which is 0x0c0000
    ldr     r2, [r2]
    bic     r0, r0, #1
    m_write32_gen
    bx      lr


m_m68k_write32_wordram0_1M_b1:           @ 0x200000 - 0x21ffff
    ldr     r2, =(Pico+0x22200)
    sub     r0, r0, #0x120000           @ map to our offset, which is 0x0e0000
    ldr     r2, [r2]
    bic     r0, r0, #1
    m_write32_gen
    bx      lr


m_m68k_write32_wordram1_1M_b0:           @ 0x220000 - 0x23ffff, cell arranged
    tst     r0, #2
    bne     m_m68k_write32_wordram1_1M_b0_unal
    mov     r3, r1
    cell_map
    ldr     r2, =(Pico+0x22200)
    add     r0, r0, #0x0c0000
    ldr     r2, [r2]
    bic     r0, r0, #1
    mov     r1, r3
    m_write32_gen
    bx      lr
m_m68k_write32_wordram1_1M_b0_unal:
    @ hopefully this doesn't happen too often
    add     r12,r0, #2
    mov     r1, r1, ror #16
    stmfd   sp!,{lr}
    bl      m_m68k_write16_wordram1_1M_b0 @ must not trash r12 and keep data in r3
    ldmfd   sp!,{lr}
    mov     r0, r12
    mov     r1, r3, lsr #16
    b       m_m68k_write16_wordram1_1M_b0


m_m68k_write32_wordram1_1M_b1:           @ 0x220000 - 0x23ffff, cell arranged
    tst     r0, #2
    bne     m_m68k_write32_wordram1_1M_b1_unal
    mov     r3, r1
    cell_map
    ldr     r2, =(Pico+0x22200)
    add     r0, r0, #0x0e0000
    ldr     r2, [r2]
    bic     r0, r0, #1
    mov     r1, r3
    m_write32_gen
    bx      lr
m_m68k_write32_wordram1_1M_b1_unal:
    add     r12,r0, #2
    mov     r1, r1, ror #16
    stmfd   sp!,{lr}
    bl      m_m68k_write16_wordram1_1M_b1 @ same as above
    ldmfd   sp!,{lr}
    mov     r0, r12
    mov     r1, r3, lsr #16
    b       m_m68k_write16_wordram1_1M_b1


m_m68k_write32_bcram:                   @ 0x600000 - 0x61ffff, not likely to be called
    mov     r12,lr
    add     r3, r0, #2
    mov     r1, r1, ror #16
    bl      m_m68k_write8_bcram
    mov     r0, r3
    mov     r1, r1, ror #16
    bl      m_m68k_write8_bcram
    bx      r12


m_m68k_write32_bcram_reg:               @ 0x7fffff
    bcram_reg_rw 0, 0x7ffffc



@ it is not very practical to use long access on hw registers, so I assume it is not used too much.
m_m68k_write32_system_io:
    bic     r2, r0, #0xfe0000
    bic     r2, r2, #0x3f
    cmp     r2, #0x012000
    bne     m_m68k_write32_misc
    and     r2, r0, #0x3e
    cmp     r2, #0x20
    bxge    lr
    cmp     r2, #0x10
    bge     m_m68k_write32_regs_comm
    cmp     r2, #0x0c
    bge     m_m68k_write32_regs_spec  @ hits the nasty comm reg qiurk

    bic     r0, r0, #1
    stmfd   sp!,{r0,r1,lr}
    mov     r1, r1, lsr #24
    bl      m68k_reg_write8
    ldr     r0, [sp]
    ldr     r1, [sp, #4]
    add     r0, r0, #1
    mov     r1, r1, lsr #16
    bl      m68k_reg_write8
    ldr     r0, [sp]
    ldr     r1, [sp, #4]
    add     r0, r0, #2
    mov     r1, r1, lsr #8
    bl      m68k_reg_write8
    ldmfd   sp!,{r0,r1,lr}
    add     r0, r0, #3
    b       m68k_reg_write8

m_m68k_write32_regs_comm:             @ Handle the 0x10-0x1f range
    ldr     r0, =(Pico+0x22200)
    mov     r3, #0xff
    ldr     r0, [r0]
    orr     r3, r3, r3, lsl #16
    add     r0, r0, #0x110000
    and     r12,r3, r1, ror #16       @ data is big-endian to be written as little, have to byteswap
    and     r1, r3, r1, ror #24
    orr     r1, r1, r12,lsl #8        @ end of byteswap
    cmp     r2, #0x1e
    strh    r1, [r2, r0]!
    ldr     r3, =s68k_poll_adclk
    ldr     r0, [r3]
    movne   r1, r1, lsr #16
    strneh  r1, [r2, #2]
    cmp     r0, #0x10
    bxlt    lr
    ldr     r0, =PicoCpuCS68k         @ remove poll detected state for s68k
    mov     r1, #0
    str     r1, [r0, #0x58]
    str     r1, [r3]
    bx      lr

m_m68k_write32_misc:
    bic     r0, r0, #1
    stmfd   sp!,{r0,r1,lr}
    mov     r1, r1, lsr #16
    bl      OtherWrite16
    ldmfd   sp!,{r0,r1,lr}
    add     r0, r0, #2
    b       OtherWrite16

m_m68k_write32_regs_spec:
    bic     r0, r0, #1
    stmfd   sp!,{r0,r1,lr}
    mov     r1, r1, lsr #16
    bl      m_m68k_write16_regs
    ldmfd   sp!,{r0,r1,lr}
    add     r0, r0, #2
    b       m_m68k_write16_regs


m_m68k_write32_vdp:
    tst     r0, #0x70000
    tsteq   r0, #0x000e0
    bxne    lr              @ invalid
    and     r2, r0, #0x18
    cmp     r2, #0x10
    moveq   r0, r1, lsr #16
    beq     SN76496Write    @ which game is crazy enough to do that?
    stmfd   sp!,{r0,r1,lr}
    mov     r1, r1, lsr #16
    bl      PicoVideoWrite
    ldmfd   sp!,{r0,r1,lr}
    add     r0, r0, #2
    b       PicoVideoWrite


m_m68k_write32_ram:
    ldr     r2, =Pico
    bic     r0, r0, #0xff0000
    bic     r0, r0, #1
    m_write32_gen
    bx      lr

.pool


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


.macro m_s68k_read8_ram map_addr
    ldr     r1, =(Pico+0x22200)
    eor     r0, r0, #1
    ldr     r1, [r1]
.if \map_addr
    add     r0, r0, #\map_addr          @ map to our address
.endif
    ldrb    r0, [r1, r0]
    bx      lr
.endm

.macro m_s68k_read8_wordram_2M_decode map_addr
    ldr     r2, =(Pico+0x22200)
    eor     r0, r0, #2
    ldr     r2, [r2]
    movs    r0, r0, lsr #1              @ +4-6 <<16
    add     r2, r2, #\map_addr          @ map to our address
    ldrb    r0, [r2, r0]
    movcc   r0, r0, lsr #4
    andcs   r0, r0, #0xf
    bx      lr
.endm


m_s68k_read8_prg:                       @ 0x000000 - 0x07ffff
m_s68k_read8_wordram_2M:                @ 0x080000 - 0x0bffff
m_s68k_read8_wordram_1M_b1:             @ 0x0c0000 - 0x0dffff, maps to 0x0e0000
    m_s68k_read8_ram 0x020000


m_s68k_read8_wordram_2M_decode_b0:      @ 0x080000 - 0x0bffff
    m_s68k_read8_wordram_2M_decode 0x080000 @ + ^ / 2


m_s68k_read8_wordram_2M_decode_b1:      @ 0x080000 - 0x0bffff
    m_s68k_read8_wordram_2M_decode 0x0a0000 @ + ^ / 2


m_s68k_read8_wordram_1M_b0:             @ 0x0c0000 - 0x0dffff (same as our offset :)
    m_s68k_read8_ram 0


m_s68k_read8_backup:                    @ 0xfe0000 - 0xfe3fff (repeated?)
    @ must not trash r3 and r12
    ldr     r1, =(Pico+0x22200)
    mov     r0, r0, lsr #1
    ldr     r1, [r1]
    bic     r0, r0, #0xff0000
    bic     r0, r0, #0x00e000
    add     r1, r1, #0x110000
    add     r1, r1, #0x000200
    ldrb    r0, [r1, r0]
    bx      lr


m_s68k_read8_pcm:
    @ must not trash r3 and r12
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #0xff0000
@    bic     r0, r0, #0x008000
    ldr     r1, [r1]
    mov     r2, #0x110000
    orr     r2, r2, #0x002200
    cmp     r0, #0x2000
    bge     m_s68k_read8_pcm_ram
    cmp     r0, #0x20
    movlt   r0, #0
    bxlt    lr
    orr     r2, r2, #(0x48+8)           @ pcm.ch + addr_offset
    add     r1, r1, r2
    and     r2, r0, #0x1c
    ldr     r1, [r1, r2, lsl #2]
    tst     r0, #2
    moveq   r0, r1, lsr #PCM_STEP_SHIFT
    movne   r0, r1, lsr #(PCM_STEP_SHIFT+8)
    and     r0, r0, #0xff
    bx      lr

m_s68k_read8_pcm_ram:
    orr     r2, r2, #0x40
    ldr     r2, [r1, r2]
    add     r1, r1, #0x100000           @ pcm_ram
    and     r2, r2, #0x0f000000         @ bank
    add     r1, r1, r2, lsr #12
    bic     r0, r0, #0x00e000
    mov     r0, r0, lsr #1
    ldrb    r0, [r1, r0]
    bx      lr


m_s68k_read8_regs:
    bic     r0, r0, #0xff0000
    bic     r0, r0, #0x008000
    tst     r0, #0x7e00
    movne   r0, #0
    bxne    lr
    sub     r2, r0, #0x0e
    cmp     r2, #(0x30-0x0e)
    blo     m_s68k_read8_comm
    sub     r2, r0, #0x58
    cmp     r2, #0x10
    ldrlo   r2, =gfx_cd_read
    ldrhs   r2, =s68k_reg_read16
    stmfd   sp!,{r0,lr}
    bic     r0, r0, #1
    mov     lr, pc
    bx      r2
    ldmfd   sp!,{r1,lr}
    tst     r1, #1
    moveq   r0, r0, lsr #8
    and     r0, r0, #0xff
    bx      lr

m_s68k_read8_comm:
    ldr     r1, =(Pico+0x22200)
    ldr     r1, [r1]
    add     r1, r1, #0x110000
    ldrb    r1, [r1, r0]
    b       s68k_poll_detect


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


.macro m_s68k_read16_ram map_addr
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #1
    ldr     r1, [r1]
.if \map_addr
    add     r0, r0, #\map_addr          @ map to our address
.endif
    ldrh    r0, [r1, r0]
    bx      lr
.endm

.macro m_s68k_read16_wordram_2M_decode map_addr
    ldr     r2, =(Pico+0x22200)
    eor     r0, r0, #2
    ldr     r2, [r2]
    mov     r0, r0, lsr #1              @ +4-6 <<16
    add     r2, r2, #\map_addr          @ map to our address
    ldrb    r0, [r2, r0]
    orr     r0, r0, r0, lsl #4
    bic     r0, r0, #0xf0
    bx      lr
.endm


m_s68k_read16_prg:                      @ 0x000000 - 0x07ffff
m_s68k_read16_wordram_2M:               @ 0x080000 - 0x0bffff
m_s68k_read16_wordram_1M_b1:            @ 0x0c0000 - 0x0dffff, maps to 0x0e0000
    m_s68k_read16_ram 0x020000


m_s68k_read16_wordram_2M_decode_b0:     @ 0x080000 - 0x0bffff
    m_s68k_read16_wordram_2M_decode 0x080000


m_s68k_read16_wordram_2M_decode_b1:     @ 0x080000 - 0x0bffff
    m_s68k_read16_wordram_2M_decode 0x0a0000


m_s68k_read16_wordram_1M_b0:            @ 0x0c0000 - 0x0dffff (same as our offset :)
    m_s68k_read16_ram 0


@ m_s68k_read16_backup:                 @ 0xfe0000 - 0xfe3fff (repeated?)
@ bram is not meant to be accessed by words, does any game do this?
.equiv m_s68k_read16_backup, m_s68k_read8_backup


@ m_s68k_read16_pcm:
@ pcm is on 8-bit bus, would this be same as byte access?
.equiv m_s68k_read16_pcm, m_s68k_read8_pcm


m_s68k_read16_regs:
    bic     r0, r0, #0xff0000
    bic     r0, r0, #0x008000
    bic     r0, r0, #0x000001
    tst     r0, #0x7e00
    movne   r0, #0
    bxne    lr
    sub     r2, r0, #0x58
    cmp     r2, #0x10
    blo     gfx_cd_read
    cmp     r0, #8
    bne     s68k_reg_read16
    mov     r0, #1
    b       Read_CDC_Host


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


.macro m_s68k_read32_ram map_addr
    ldr     r1, =(Pico+0x22200)
    bic     r0, r0, #1
    ldr     r1, [r1]
.if \map_addr
    add     r0, r0, #\map_addr          @ map to our address
.endif
    m_read32_gen
    bx      lr
.endm

.macro m_s68k_read32_wordram_2M_decode map_addr
    ldr     r2, =(Pico+0x22200)
    eor     r0, r0, #2
    ldr     r2, [r2]
    mov     r0, r0, lsr #1              @ +4-6 <<16
    add     r2, r2, #\map_addr          @ map to our address
    ldrb    r1, [r2, r0]!
    tst     r0, #1
    ldrneb  r0, [r2, #-1]
    ldreqb  r0, [r2, #2]
    orr     r1, r1, r1, lsl #4
    bic     r1, r1, #0xf0
    orr     r0, r0, r0, lsl #4
    bic     r0, r0, #0xf0
    orr     r0, r0, r1, lsl #16
    bx      lr
.endm


m_s68k_read32_prg:                      @ 0x000000 - 0x07ffff
m_s68k_read32_wordram_2M:               @ 0x080000 - 0x0bffff
m_s68k_read32_wordram_1M_b1:            @ 0x0c0000 - 0x0dffff, maps to 0x0e0000
    m_s68k_read32_ram 0x020000


m_s68k_read32_wordram_2M_decode_b0:     @ 0x080000 - 0x0bffff
    m_s68k_read32_wordram_2M_decode 0x080000


m_s68k_read32_wordram_2M_decode_b1:     @ 0x080000 - 0x0bffff
    m_s68k_read32_wordram_2M_decode 0x0a0000


m_s68k_read32_wordram_1M_b0:            @ 0x0c0000 - 0x0dffff (same as our offset :)
    m_s68k_read32_ram 0


m_s68k_read32_backup:                   @ 0xfe0000 - 0xfe3fff (repeated?)
    @ bram is not meant to be accessed by words, does any game do this?
    mov     r12,lr
    mov     r3, r0
    bl      m_s68k_read8_backup         @ must preserve r3 and r12
    mov     r1, r0
    add     r0, r3, #2
    mov     r3, r1
    bl      m_s68k_read8_backup
    orr     r0, r0, r3, lsl #16
    bx      r12


m_s68k_read32_pcm:
    mov     r12,lr
    mov     r3, r0
    bl      m_s68k_read8_pcm            @ must preserve r3 and r12
    mov     r1, r0
    add     r0, r3, #2
    mov     r3, r1
    bl      m_s68k_read8_pcm
    orr     r0, r0, r3, lsl #16
    bx      r12


m_s68k_read32_regs:
    bic     r0, r0, #0xff0000
    bic     r0, r0, #0x008000
    bic     r0, r0, #0x000001
    tst     r0, #0x7e00
    movne   r0, #0
    bxne    lr
    sub     r2, r0, #0x58
    cmp     r2, #0x10
    add     r1, r0, #2
    blo     m_s68k_read32_regs_gfx
    stmfd   sp!,{r1,lr}
    bl      s68k_reg_read16
    swp     r0, r0, [sp]
    bl      s68k_reg_read16
    ldmfd   sp!,{r1,lr}
    orr     r0, r0, r1, lsl #16
    bx      lr


m_s68k_read32_regs_gfx:
    stmfd   sp!,{r1,lr}
    bl      gfx_cd_read
    swp     r0, r0, [sp]
    bl      gfx_cd_read
    ldmfd   sp!,{r1,lr}
    orr     r0, r0, r1, lsl #16
    bx      lr

.pool

@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


.macro m_s68k_write8_ram map_addr
    ldr     r2, =(Pico+0x22200)
    eor     r0, r0, #1
    ldr     r2, [r2]
.if \map_addr
    add     r0, r0, #\map_addr          @ map to our address
.endif
    strb    r1, [r2, r0]
    bx      lr
.endm

.macro m_s68k_write8_2M_decode map_addr
    ldr     r2, =(Pico+0x22200)
    eor     r0, r0, #2
    ldr     r2, [r2]
    movs    r0, r0, lsr #1              @ +4-6 <<16
    add     r2, r2, #\map_addr          @ map to our address
.endm

.macro m_s68k_write8_2M_decode_m0 map_addr @ mode off
    m_s68k_write8_2M_decode \map_addr
    ldrb    r0, [r2, r0]!
    and     r1, r1, #0x0f
    movcc   r1, r1, lsl #4
    andcc   r3, r0, #0x0f
    andcs   r3, r0, #0xf0
    orr     r3, r3, r1
    cmp     r0, r3                      @ avoid writing if result is same
    strneb  r3, [r2]
    bx      lr
.endm

.macro m_s68k_write8_2M_decode_m1 map_addr @ mode underwrite
    ands    r1, r1, #0x0f
    bxeq    lr
    m_s68k_write8_2M_decode \map_addr
    ldrb    r0, [r2, r0]!
    movcc   r1, r1, lsl #4
    andcc   r3, r0, #0x0f
    andcs   r3, r0, #0xf0
    tst     r3, r3
    bxeq    lr
    orr     r3, r3, r1
    cmp     r0, r3
    strneb  r3, [r2]
    bx      lr
.endm

.macro m_s68k_write8_2M_decode_m2 map_addr @ mode overwrite
    ands    r1, r1, #0x0f
    bxeq    lr
    m_s68k_write8_2M_decode_m0 \map_addr   @ same as in off mode
.endm



m_s68k_write8_prg:                      @ 0x000000 - 0x07ffff
    ldr     r2, =(Pico+0x22200)
    eor     r0, r0, #1
    ldr     r2, [r2]
    add     r3, r0, #0x020000           @ map to our address
    add     r12,r2, #0x110000
    ldr     r12,[r12]
    and     r12,r12,#0x00ff0000         @ wp
    cmp     r0, r12, lsr #8
    strgeb  r1, [r2, r3]
    bx      lr


m_s68k_write8_wordram_2M:               @ 0x080000 - 0x0bffff
m_s68k_write8_wordram_1M_b1:            @ 0x0c0000 - 0x0dffff, maps to 0x0e0000
    m_s68k_write8_ram 0x020000


m_s68k_write8_2M_decode_b0_m0:          @ 0x080000 - 0x0bffff
    m_s68k_write8_2M_decode_m0 0x080000

m_s68k_write8_2M_decode_b0_m1:
    m_s68k_write8_2M_decode_m1 0x080000

m_s68k_write8_2M_decode_b0_m2:
    m_s68k_write8_2M_decode_m2 0x080000

m_s68k_write8_2M_decode_b1_m0:
    m_s68k_write8_2M_decode_m0 0x0a0000

m_s68k_write8_2M_decode_b1_m1:
    m_s68k_write8_2M_decode_m1 0x0a0000

m_s68k_write8_2M_decode_b1_m2:
    m_s68k_write8_2M_decode_m2 0x0a0000


m_s68k_write8_wordram_1M_b0:            @ 0x0c0000 - 0x0dffff (same as our offset :)
    m_s68k_write8_ram 0


m_s68k_write8_backup:                   @ 0xfe0000 - 0xfe3fff (repeated?)
    @ must not trash r3 and r12
    ldr     r2, =(Pico+0x22200)
    mov     r0, r0, lsr #1
    ldr     r2, [r2]
    bic     r0, r0, #0xff0000
    bic     r0, r0, #0x00e000
    add     r2, r2, #0x110000
    add     r2, r2, #0x000200
    strb    r1, [r2, r0]
    ldr     r1, =SRam
    mov     r0, #1
    strb    r0, [r1, #0x0e]             @ SRam.changed = 1
    bx      lr


m_s68k_write8_pcm:
    bic     r0, r0, #0xff0000
    cmp     r0, #0x12
    movlt   r0, r0, lsr #1
    blt     pcm_write

    cmp     r0, #0x2000
    bxlt    lr

m_s68k_write8_pcm_ram:
    ldr     r3, =(Pico+0x22200)
    bic     r0, r0, #0x00e000
    ldr     r3, [r3]
    mov     r0, r0, lsr #1
    add     r2, r3, #0x110000
    add     r2, r2, #0x002200
    add     r2, r2, #0x000040
    ldr     r2, [r2]
    add     r3, r3, #0x100000           @ pcm_ram
    and     r2, r2, #0x0f000000         @ bank
    add     r3, r3, r2, lsr #12
    strb    r1, [r3, r0]
    bx      lr


m_s68k_write8_regs:
    bic     r0, r0, #0xff0000
    bic     r0, r0, #0x008000
    tst     r0, #0x7e00
    movne   r0, #0
    bxne    lr
    sub     r2, r0, #0x58
    cmp     r2, #0x10
    bhs     s68k_reg_write8
    bic     r0, r0, #1
    orr     r1, r1, r1, lsl #8
    b       gfx_cd_write16


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


.macro m_s68k_write16_ram map_addr
    ldr     r2, =(Pico+0x22200)
    bic     r0, r0, #1
    ldr     r2, [r2]
.if \map_addr
    add     r0, r0, #\map_addr          @ map to our address
.endif
    strh    r1, [r2, r0]
    bx      lr
.endm

.macro m_s68k_write16_2M_decode map_addr
    ldr     r2, =(Pico+0x22200)
    eor     r0, r0, #2
    ldr     r2, [r2]
    mov     r0, r0, lsr #1              @ +4-6 <<16
    add     r2, r2, #\map_addr          @ map to our address
.endm

.macro m_s68k_write16_2M_decode_m0 map_addr @ mode off
    m_s68k_write16_2M_decode \map_addr
    bic     r1, r1, #0xf0
    orr     r1, r1, r1, lsr #4
    strb    r1, [r2, r0]
    bx      lr
.endm

.macro m_s68k_write16_2M_decode_m1 map_addr @ mode underwrite
    bics    r1, r1, #0xf000
    bicnes  r1, r1, #0x00f0
    bxeq    lr
    orr     r1, r1, r1, lsr #4
    m_s68k_write16_2M_decode \map_addr
    ldrb    r0, [r2, r0]!
    and     r3, r1, #0x0f
    and     r1, r1, #0xf0
    tst     r0, #0x0f
    orreq   r0, r0, r3
    tst     r0, #0xf0
    orreq   r0, r0, r1
    strb    r0, [r2]
    bx      lr
.endm

.macro m_s68k_write16_2M_decode_m2 map_addr @ mode overwrite
    bics    r1, r1, #0xf000
    bicnes  r1, r1, #0x00f0
    bxeq    lr
    orr     r1, r1, r1, lsr #4
    m_s68k_write16_2M_decode \map_addr
    ldrb    r0, [r2, r0]!
    ands    r3, r1, #0x0f
    andne   r0, r0, #0xf0
    orrne   r0, r0, r3
    ands    r1, r1, #0xf0
    andne   r0, r0, #0x0f
    orrne   r0, r0, r1
    strb    r0, [r2]
    bx      lr
.endm



m_s68k_write16_prg:                     @ 0x000000 - 0x07ffff
    ldr     r2, =(Pico+0x22200)
    bic     r0, r0, #1
    ldr     r2, [r2]
    add     r3, r0, #0x020000           @ map to our address
    add     r12,r2, #0x110000
    ldr     r12,[r12]
    and     r12,r12,#0x00ff0000         @ wp
    cmp     r0, r12, lsr #8
    strgeh  r1, [r2, r3]
    bx      lr


m_s68k_write16_wordram_2M:              @ 0x080000 - 0x0bffff
m_s68k_write16_wordram_1M_b1:           @ 0x0c0000 - 0x0dffff, maps to 0x0e0000
    m_s68k_write16_ram 0x020000


m_s68k_write16_2M_decode_b0_m0:         @ 0x080000 - 0x0bffff
    m_s68k_write16_2M_decode_m0 0x080000

m_s68k_write16_2M_decode_b0_m1:
    m_s68k_write16_2M_decode_m1 0x080000

m_s68k_write16_2M_decode_b0_m2:
    m_s68k_write16_2M_decode_m2 0x080000

m_s68k_write16_2M_decode_b1_m0:
    m_s68k_write16_2M_decode_m0 0x0a0000

m_s68k_write16_2M_decode_b1_m1:
    m_s68k_write16_2M_decode_m1 0x0a0000

m_s68k_write16_2M_decode_b1_m2:
    m_s68k_write16_2M_decode_m2 0x0a0000


m_s68k_write16_wordram_1M_b0:           @ 0x0c0000 - 0x0dffff (same as our offset :)
    m_s68k_write16_ram 0


@ m_s68k_write16_backup:
.equiv m_s68k_write16_backup, m_s68k_write8_backup


@ m_s68k_write16_pcm:
.equiv m_s68k_write16_pcm, m_s68k_write8_pcm


m_s68k_write16_regs:
    bic     r0, r0, #0xff0000
    bic     r0, r0, #0x008000
    bic     r0, r0, #1
    tst     r0, #0x7e00
    movne   r0, #0
    bxne    lr
    cmp     r0, #0x0e
    beq     m_s68k_write16_regs_spec
    sub     r2, r0, #0x58
    cmp     r2, #0x10
    blo     gfx_cd_write16
    and     r3, r1, #0xff
    add     r2, r0, #1
    stmfd   sp!,{r2,r3,lr}
    mov     r1, r1, lsr #8
    bl      s68k_reg_write8
    ldmfd   sp!,{r0,r1,lr}
    b       s68k_reg_write8

m_s68k_write16_regs_spec:               @ special case
    ldr     r2, =(Pico+0x22200)
    mov     r0, #0x110000
    ldr     r2, [r2]
    add     r0, r0, #0x00000f
    strb    r1, [r2, r0]                @ if (a == 0xe) s68k_regs[0xf] = d;
    bx      lr


@ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@


.macro m_s68k_write32_ram map_addr
    ldr     r2, =(Pico+0x22200)
    bic     r0, r0, #1
    ldr     r2, [r2]
.if \map_addr
    add     r0, r0, #\map_addr          @ map to our address
.endif
    m_write32_gen
    bx      lr
.endm

.macro m_s68k_write32_2M_decode map_addr
    ldr     r2, =(Pico+0x22200)
    eor     r0, r0, #2
    ldr     r2, [r2]
    mov     r0, r0, lsr #1              @ +4-6 <<16
    add     r2, r2, #\map_addr          @ map to our address
.endm

.macro m_s68k_write32_2M_decode_m0 map_addr @ mode off
    m_s68k_write32_2M_decode \map_addr
    bic     r1, r1, #0x000000f0
    bic     r1, r1, #0x00f00000
    orr     r1, r1, r1, lsr #4
    mov     r3, r1, lsr #16
    strb    r3, [r2, r0]!
    tst     r0, #1
    strneb  r1, [r2, #-1]
    streqb  r1, [r2, #3]
    bx      lr
.endm

.macro m_s68k_write32_2M_decode_m1 map_addr @ mode underwrite
    bics    r1, r1, #0x000000f0
    bicnes  r1, r1, #0x0000f000
    bicnes  r1, r1, #0x00f00000
    bicnes  r1, r1, #0xf0000000
    bxeq    lr
    orr     r1, r1, r1, lsr #4
    m_s68k_write32_2M_decode \map_addr
    ldrb    r3, [r2, r0]!
    tst     r0, #1
    ldrneb  r0, [r2, #-1]
    ldreqb  r0, [r2, #3]
    and     r12,r1, #0x0000000f
    orr     r0, r0, r3, lsl #16
    orrne   r0, r0, #0x80000000          @ remember addr lsb bit
    tst     r0,     #0x0000000f
    orreq   r0, r0, r12
    tst     r0,     #0x000000f0
    andeq   r12,r1, #0x000000f0
    orreq   r0, r0, r12
    tst     r0,     #0x000f0000
    andeq   r12,r1, #0x000f0000
    orreq   r0, r0, r12
    tst     r0,     #0x00f00000
    andeq   r12,r1, #0x00f00000
    orreq   r0, r0, r12
    tst     r0, #0x80000000
    strneb  r0, [r2, #-1]
    streqb  r0, [r2, #3]
    mov     r0, r0, lsr #16
    strb    r0, [r2]
    bx      lr
.endm

.macro m_s68k_write32_2M_decode_m2 map_addr @ mode overwrite
    bics    r1, r1, #0x000000f0
    bicnes  r1, r1, #0x0000f000
    bicnes  r1, r1, #0x00f00000
    bicnes  r1, r1, #0xf0000000
    bxeq    lr
    orr     r1, r1, r1, lsr #4
    m_s68k_write32_2M_decode \map_addr
    ldrb    r3, [r2, r0]!
    tst     r0, #1
    ldrneb  r0, [r2, #-1]
    ldreqb  r0, [r2, #3]
    orrne   r1, r1, #0x80000000          @ remember addr lsb bit
    orr     r0, r0, r3, lsl #16
    tst     r1,     #0x0000000f
    andeq   r12,r0, #0x0000000f
    orreq   r1, r1, r12
    tst     r1,     #0x000000f0
    andeq   r12,r0, #0x000000f0
    orreq   r1, r1, r12
    tst     r1,     #0x000f0000
    andeq   r12,r0, #0x000f0000
    orreq   r1, r1, r12
    tst     r1,     #0x00f00000
    andeq   r12,r0, #0x00f00000
    orreq   r1, r1, r12
    cmp     r0, r1
    bxeq    lr
    tst     r1, #0x80000000
    strneb  r1, [r2, #-1]
    streqb  r1, [r2, #3]
    mov     r1, r1, lsr #16
    strb    r1, [r2]
    bx      lr
.endm



m_s68k_write32_prg:                     @ 0x000000 - 0x07ffff
    ldr     r2, =(Pico+0x22200)
    bic     r0, r0, #1
    ldr     r2, [r2]
    add     r3, r0, #0x020000           @ map to our address
    add     r12,r2, #0x110000
    ldr     r12,[r12]
    and     r12,r12,#0x00ff0000         @ wp
    cmp     r0, r12, lsr #8
    bxlt    lr
    mov     r0, r1, lsr #16
    strh    r0, [r2, r3]!
    strh    r1, [r2, #2]
    bx      lr


m_s68k_write32_wordram_2M:              @ 0x080000 - 0x0bffff
m_s68k_write32_wordram_1M_b1:           @ 0x0c0000 - 0x0dffff, maps to 0x0e0000
    m_s68k_write32_ram 0x020000


m_s68k_write32_2M_decode_b0_m0:  @ 0x080000 - 0x0bffff
    m_s68k_write32_2M_decode_m0 0x080000

m_s68k_write32_2M_decode_b0_m1:
    m_s68k_write32_2M_decode_m1 0x080000

m_s68k_write32_2M_decode_b0_m2:
    m_s68k_write32_2M_decode_m2 0x080000

m_s68k_write32_2M_decode_b1_m0:
    m_s68k_write32_2M_decode_m0 0x0a0000

m_s68k_write32_2M_decode_b1_m1:
    m_s68k_write32_2M_decode_m1 0x0a0000

m_s68k_write32_2M_decode_b1_m2:
    m_s68k_write32_2M_decode_m2 0x0a0000


m_s68k_write32_wordram_1M_b0:           @ 0x0c0000 - 0x0dffff (same as our offset :)
    m_s68k_write32_ram 0


m_s68k_write32_backup:
    add     r12,r0, #2
    mov     r3, r1
    mov     r1, r1, lsr #16
    stmfd   sp!,{lr}
    bl      m_s68k_write8_backup        @ must preserve r3 and r12
    ldmfd   sp!,{lr}
    mov     r0, r12
    mov     r1, r3
    b       m_s68k_write8_backup


m_s68k_write32_pcm:
    bic     r0, r0, #0xff0000
    cmp     r0, #0x12
    blt     m_s68k_write32_pcm_reg

    cmp     r0, #0x2000
    bxlt    lr

m_s68k_write32_pcm_ram:
    ldr     r3, =(Pico+0x22200)
    bic     r0, r0, #0x00e000
    ldr     r3, [r3]
    mov     r0, r0, lsr #1
    add     r2, r3, #0x110000
    add     r2, r2, #0x002200
    add     r2, r2, #0x000040
    ldr     r2, [r2]
    add     r3, r3, #0x100000           @ pcm_ram
    and     r2, r2, #0x0f000000         @ bank
    add     r3, r3, r2, lsr #12
    mov     r1, r1, ror #16
    strb    r1, [r3, r0]!
    mov     r1, r1, ror #16
    strb    r1, [r3]
    bx      lr

m_s68k_write32_pcm_reg:
    mov     r0, r0, lsr #1
    stmfd   sp!,{r0,r1,lr}
    mov     r1, r1, lsr #16
    bl      pcm_write
    ldmfd   sp!,{r0,r1,lr}
    add     r0, r0, #1
    b       pcm_write


m_s68k_write32_regs:
    bic     r0, r0, #0xff0000
    bic     r0, r0, #0x008000
    bic     r0, r0, #1
    tst     r0, #0x7e00
    bxne    lr
    sub     r2, r0, #0x58
    cmp     r2, #0x10
    blo     m_s68k_write32_regs_gfx
    and     r2, r0, #0x1fc
    cmp     r2, #0x0c
    beq     m_s68k_write32_regs_spec   @ hits 0x0f
    and     r2, r0, #0x1f0
    cmp     r2, #0x20
    beq     m_s68k_write32_regs_comm

    stmfd   sp!,{r0,r1,lr}
    mov     r1, r1, lsr #24
    bl      s68k_reg_write8
    ldr     r0, [sp]
    ldr     r1, [sp, #4]
    add     r0, r0, #1
    mov     r1, r1, lsr #16
    bl      s68k_reg_write8
    ldr     r0, [sp]
    ldr     r1, [sp, #4]
    add     r0, r0, #2
    mov     r1, r1, lsr #8
    bl      s68k_reg_write8
    ldmfd   sp!,{r0,r1,lr}
    add     r0, r0, #3
    b       s68k_reg_write8

m_s68k_write32_regs_gfx:
    stmfd   sp!,{r0,r1,lr}
    mov     r1, r1, lsr #16
    bl      gfx_cd_write16
    ldmfd   sp!,{r0,r1,lr}
    add     r0, r0, #2
    b       gfx_cd_write16

m_s68k_write32_regs_comm:             @ Handle the 0x20-0x2f range
    ldr     r2, =(Pico+0x22200)
    mov     r3, #0xff
    ldr     r2, [r2]
    orr     r3, r3, r3, lsl #16
    add     r2, r2, #0x110000
    and     r12,r3, r1, ror #16       @ data is big-endian to be written as little, have to byteswap
    and     r1, r3, r1, ror #24
    orr     r1, r1, r12,lsl #8        @ end of byteswap
    cmp     r0, #0x2e
    strh    r1, [r0, r2]!
    movne   r1, r1, lsr #16
    strneh  r1, [r0, #2]
    bx      lr

m_s68k_write32_regs_spec:
    stmfd   sp!,{r0,r1,lr}
    mov     r1, r1, lsr #16
    bl      m_s68k_write16_regs
    ldmfd   sp!,{r0,r1,lr}
    add     r0, r0, #2
    b       m_s68k_write16_regs


