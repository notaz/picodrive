/*
 * PicoDrive
 * (C) notaz, 2006
 * (C) kub, 2020	added SSG-EG and simple output rate interpolation
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */

@ this is a rewrite of MAME's ym2612 code, in particular this is only the main sample-generatin loop.
@ it does not seem to give much performance increase (if any at all), so don't use it if it causes trouble.
@ - notaz, 2006

@ vim:filetype=armasm

#include "../arm_features.h"

@ very simple YM2612 output rate to sample rate adaption (~500k cycles @44100)
#define INTERPOL

.equiv SLOT1, 0
.equiv SLOT2, 2
.equiv SLOT3, 1
.equiv SLOT4, 3
.equiv SLOT_STRUCT_SIZE, 0x38

.equiv TL_TAB_LEN, 0x1A00

.equiv EG_ATT, 4
.equiv EG_DEC, 3
.equiv EG_SUS, 2
.equiv EG_REL, 1
.equiv EG_OFF, 0

.equiv EG_SH,		  16             @ 16.16 fixed point (envelope generator timing)
.equiv EG_TIMER_OVERFLOW, (3*(1<<EG_SH)) @ envelope generator timer overflows every 3 samples (on real chip)
.equiv LFO_SH,            24  /*  8.24 fixed point (LFO calculations)       */

.equiv ENV_QUIET,	  (2*13*256/8)

.text
.align 2
    PIC_LDR_INIT()

@ r5=slot, r1=eg_cnt, trashes: r0,r2,r3
@ writes output to routp, but only if vol_out changes
.macro update_eg_phase_slot
#if defined(INTERPOL)
    ldrh    r0, [r5,#0x34]       @ vol_out
#endif
    ldrb    r2, [r5,#0x17]       @ state
    add     r3, r5, #0x1c
#if defined(INTERPOL)
    strh    r0, [r5,#0x36]       @ vol_ipol
#endif
    tst     r2, r2
    beq     0f                   @ EG_OFF

    ldr     r2, [r3, r2, lsl #2] @ pack
    mov     r3, #1
    mov     r0, r2, lsr #24      @ shift
    mov     r3, r3, lsl r0
    sub     r3, r3, #1

    tst     r1, r3
    bne     0f                   @ no volume change

    mov     r3, r1, lsr r0
    ldrb    r0, [r5,#0x30]       @ ssg
    and     r3, r3, #7
    add     r3, r3, r3, lsl #1
    mov     r3, r2, lsr r3
    and     r3, r3, #7           @ eg_inc_val shift, may be 0
    ldrb    r2, [r5,#0x17]       @ state

    tst     r0, #0x08            @ ssg enabled?
    bne     9f

    @ non-SSG-EG mode
    cmp     r2, #4               @ EG_ATT
    ldrh    r0, [r5,#0x1a]       @ volume, unsigned (0-1023)
    beq     4f

    cmp     r2, #2
    mov     r2, #1
    mov     r2, r2, lsl r3
    mov     r2, r2, lsr #1       @ eg_inc_val
    add     r0, r0, r2
    blt     1f                   @ EG_REL
    beq     2f                   @ EG_SUS

3:  @ EG_DEC
    ldr     r2, [r5,#0x1c]       @ sl (can be 16bit?)
    mov     r3, #EG_SUS
    cmp     r0, r2               @ if ( volume >= (INT32) SLOT->sl )
    strgeb  r3, [r5,#0x17]       @ state
    b       10f

4:  @ EG_ATT
    subs    r3, r3, #1           @ eg_inc_val_shift - 1
    mvnpl   r2, r0
    movpl   r2, r2, lsl r3
    addpl   r0, r0, r2, asr #4
    cmp     r0, #0               @ if (volume <= MIN_ATT_INDEX)
    bgt     10f
    ldr     r2, [r5,#0x1c]
    mov     r0, #0
    cmp     r2, #0
    movne   r3, #EG_DEC
    moveq   r3, #EG_SUS
    strb    r3, [r5,#0x17]       @ state
    b       10f

2:  @ EG_SUS
    mov     r2, #1024
    sub     r2, r2, #1           @ r2 = MAX_ATT_INDEX
    cmp     r0, r2               @ if ( volume >= MAX_ATT_INDEX )
    movge   r0, r2
    b       10f

1:  @ EG_REL
    mov     r2, #1024
    sub     r2, r2, #1           @ r2 = MAX_ATT_INDEX
    cmp     r0, r2               @ if ( volume >= MAX_ATT_INDEX )
    movge   r0, r2
    movge   r3, #EG_OFF
    strgeb  r3, [r5,#0x17]       @ state

10: @ finish
    strh    r0, [r5,#0x1a]       @ volume
    b       11f

9:  @ SSG-EG mode
    ldrh    r0, [r5,#0x1a]       @ volume, unsigned (0-1023)
    cmp     r2, #4               @ EG_ATT
    beq     4f

    cmp     r0, #0x200           @ if ( volume < 0x200 )
    movlt   r0, #1
    movlt   r3, r0, lsl r3
    ldrlth  r0, [r5,#0x1a]       @ volume, unsigned (0-1023)
    movlt   r3, r3, lsr #1       @ eg_inc_val
    addlt   r0, r0, r3, lsr #2

    cmp     r2, #2
    blt     1f                   @ EG_REL
    beq     10f                  @ EG_SUS - nothing more to do

3:  @ EG_DEC
    ldr     r2, [r5,#0x1c]       @ sl (can be 16bit?)
    mov     r3, #EG_SUS
    cmp     r0, r2               @ if ( volume >= (INT32) SLOT->sl )
    strgeb  r3, [r5,#0x17]       @ state
    b       10f

4:  @ EG_ATT
    subs    r3, r3, #1           @ eg_inc_val_shift - 1
    mvnpl   r2, r0
    movpl   r2, r2, lsl r3
    addpl   r0, r0, r2, asr #4
    cmp     r0, #0               @ if (volume <= MIN_ATT_INDEX)
    bgt     10f
    ldr     r2, [r5,#0x1c]
    mov     r0, #0
    cmp     r2, #0
    movne   r3, #EG_DEC
    moveq   r3, #EG_SUS
    strb    r3, [r5,#0x17]       @ state
    b       10f

1:  @ EG_REL
    mov     r2, #0x200
    cmp     r0, r2               @ if ( volume >= 0x200 )
    movge   r0, #1024
    subge   r0, #1
    movge   r3, #EG_OFF
    strgeb  r3, [r5,#0x17]       @ state

10: @ finish
    ldrb    r2, [r5,#0x30]       @ ssg
    ldrb    r3, [r5,#0x17]       @ state
    strh    r0, [r5,#0x1a]       @ volume
    cmp     r2, #0x0c            @ if ( ssg&0x04 && state > EG_REL )
    cmpge   r3, #EG_REL+1
    rsbge   r0, r0, #0x200       @ volume = (0x200-volume) & MAX_ATT
    lslge   r0, r0, #10
    lsrge   r0, r0, #10

11:
    ldrh    r3, [r5,#0x18]       @ tl
    add     r0, r0, r3           @ volume += tl
    strh    r0, [r5,#0x34]       @ vol_out

0: @ EG_OFF
.endm

@ r5=slot, trashes: r0,r2,r3
.macro update_ssg_eg
    ldrh    r0, [r5,#0x30]                @ ssg+ssgn
    ldrb    r2, [r5,#0x17]                @ state
    ldrh    r3, [r5,#0x1a]                @ volume
    tst     r0, #0x08                     @ ssg enabled &&
    beq     9f
    cmp     r2, #EG_REL+1                 @   state > EG_REL &&
    cmpge   r3, #0x200                    @   volume >= 0x200?
    blt     9f

    tst     r0, #0x01
    beq     1f

    tst     r0, #0x02
    eorne   r0, r0, lsr #8                @ ssg ^= ssgn ^ 4
    eorne   r0, r0, #0x4
    orrne   r0, r0, #0x400                @ ssgn = 4
    strneh  r0, [r5,#0x30]

    eor     r0, r0, #0x4                  @ if ( !(ssg&0x04) )
    tst     r0, #0x4
    cmpne   r2, #EG_ATT                   @ if ( state != EG_ATT )
    movne   r3, #0x400
    subne   r3, r3, #1
    strneh  r3, [r5,#0x1a]                @ volume = MAX_ATT
    b       9f

1:  tst     r0, #0x02
    eorne   r0, r0, #0x4                  @ ssg ^= 4
    eorne   r0, r0, #0x400                @ ssgn ^= 4
    strneh  r0, [r5,#0x30]
    moveq   r0, #0
    streq   r0, [r5,#0x0c]                @ phase = 0

    cmp     r2, #EG_ATT                   @ if ( state != EG_ATT )
    beq     9f

    ldr     r0, [r5,#0x1c]                @ sl
    mov     r2, #EG_SUS                   @ state = sl==MIN_ATT ? EG_SUS:EG_DEC
    cmp     r0, #0

    ldrh    r0, [r5,#0x32]                @ ar+ksr
    movne   r2, #EG_DEC
    cmp     r0, #32+62                    @ if ( ar+ksr >= 32+62 )
    movge   r3, #0
    strgeh  r3, [r5,#0x1a]                @ volume = MIN_ATT
    bge     9f

    cmp     r3, #0
    movgt   r2, #EG_ATT
    strb    r2, [r5,#0x17]                @ state
9:
.endm

@ r12=lfo_ampm[31:16], r1=lfo_cnt_old, r2=lfo_cnt, r3=scratch
.macro advance_lfo_m
    mov     r2, r2, lsr #LFO_SH
    cmp     r2, r1, lsr #LFO_SH
    beq     0f
    and     r3, r2, #0x3f
    cmp     r2, #0x40
    eorlt   r3, r3, #0x3f
    bic     r12,r12, #0xff000000          @ lfo_ampm &= 0xff
    orr     r12,r12, r3, lsl #1+24

    mov     r2, r2, lsr #2
    cmp     r2, r1, lsr #LFO_SH+2
    bicne   r12,r12, #0xff0000
    orrne   r12,r12, r2, lsl #16

0:
.endm


@ result goes to r1, trashes r2
.macro make_eg_out slot
    tst     r12, #8
    tstne   r12, #(1<<(\slot+8))
.if     \slot == SLOT1
    mov     r1, r6, lsl #16
    mov     r1, r1, lsr #16
.elseif \slot == SLOT2
    mov     r1, r6, lsr #16
.elseif \slot == SLOT3
    mov     r1, r7, lsl #16
    mov     r1, r1, lsr #16
.elseif \slot == SLOT4
    mov     r1, r7, lsr #16
.endif
    andne   r2, r12, #0xc0
    movne   r2, r2,  lsr #6
    addne   r2, r2,  #24
    addne   r1, r1,  r12, lsr r2
    bic     r1, r1,  #1
.endm


@ \r=sin/result, r1=env, r3=ym_tl_tab
.macro lookup_tl r
    tst     \r, #0x100
    eorne   \r, \r, #0xff   @ if (sin & 0x100) sin = 0xff - (sin&0xff);
    tst     \r, #0x200
    and     \r, \r, #0xff
    orr     \r, \r, r1, lsl #7
    mov     \r, \r, lsl #1
    ldrh    \r, [r3, \r]    @ 2ci if ne
    rsbne   \r, \r, #0
.endm


@ lr=context, r12=pack (stereo, lastchan, disabled, lfo_enabled | pan_r, pan_l, ams[2] | AMmasks[4] | FB[4] | lfo_ampm[16])
@ r0-r2=scratch, r3=sin_tab, r5=scratch, r6-r7=vol_out[4], r10=op1_out
.macro upd_algo0_m

    @ SLOT3
    make_eg_out SLOT3
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     0f
    ldr     r2, [lr, #0x18]
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r2, r2, lsr #16
    add     r0, r2, r0, lsr #1
    lookup_tl r0                  @ r0=c2

0:

    @ SLOT4
    make_eg_out SLOT4
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     1f
    ldr     r2, [lr, #0x1c]
    mov     r0, r0, lsr #1
    add     r0, r0, r2, lsr #16
    lookup_tl r0                  @ r0=output smp

1:
    @ SLOT2
    make_eg_out SLOT2
    cmp     r1, #ENV_QUIET
    movcs   r2, #0
    bcs     2f
    ldr     r2, [lr, #0x14]       @ 1ci
    mov     r5, r10, lsr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2                  @ r2=mem

2:
    str     r2, [lr, #0x38] @ mem
.endm


.macro upd_algo1_m

    @ SLOT3
    make_eg_out SLOT3
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     0f
    ldr     r2, [lr, #0x18]
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r2, r2, lsr #16
    add     r0, r2, r0, lsr #1
    lookup_tl r0                 @ r0=c2

0:
    @ SLOT4
    make_eg_out SLOT4
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     1f
    ldr     r2, [lr, #0x1c]
    mov     r0, r0, lsr #1
    add     r0, r0, r2, lsr #16
    lookup_tl r0                 @ r0=output smp

1:
    @ SLOT2
    make_eg_out SLOT2
    cmp     r1, #ENV_QUIET
    movcs   r2, #0
    bcs     2f
    ldr     r2, [lr, #0x14]      @ 1ci
    mov     r2, r2, lsr #16
    lookup_tl r2                 @ r2=mem

2:
    add     r2, r2, r10, asr #16
    str     r2, [lr, #0x38]
.endm


.macro upd_algo2_m

    @ SLOT3
    make_eg_out SLOT3
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     0f
    ldr     r2, [lr, #0x18]
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r2, r2, lsr #16
    add     r0, r2, r0, lsr #1
    lookup_tl r0                 @ r0=c2

0:
    add     r0, r0, r10, asr #16

    @ SLOT4
    make_eg_out SLOT4
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     1f
    ldr     r2, [lr, #0x1c]
    mov     r0, r0, lsr #1
    add     r0, r0, r2, lsr #16
    lookup_tl r0                 @ r0=output smp

1:
    @ SLOT2
    make_eg_out SLOT2
    cmp     r1, #ENV_QUIET
    movcs   r2, #0
    bcs     2f
    ldr     r2, [lr, #0x14]
    mov     r2, r2, lsr #16      @ 1ci
    lookup_tl r2                 @ r2=mem

2:
    str     r2, [lr, #0x38] @ mem
.endm


.macro upd_algo3_m

    @ SLOT3
    make_eg_out SLOT3
    cmp     r1, #ENV_QUIET
    ldr     r2, [lr, #0x38] @ mem (for future)
    mov     r0, #0
    bcs     0f
    ldr     r0, [lr, #0x18]      @ phase3
    mov     r0, r0, lsr #16
    lookup_tl r0                 @ r0=c2

0:
    add     r0, r0, r2

    @ SLOT4
    make_eg_out SLOT4
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     1f
    ldr     r2, [lr, #0x1c]
    mov     r0, r0, lsr #1
    add     r0, r0, r2, lsr #16
    lookup_tl r0                 @ r0=output smp

1:
    @ SLOT2
    make_eg_out SLOT2
    cmp     r1, #ENV_QUIET
    movcs   r2, #0
    bcs     2f
    ldr     r2, [lr, #0x14]      @ phase2
    mov     r5, r10, lsr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2                 @ r2=mem

2:
    str     r2, [lr, #0x38]      @ mem
.endm


.macro upd_algo4_m

    @ SLOT3
    make_eg_out SLOT3
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     0f
    ldr     r0, [lr, #0x18]
    mov     r0, r0, lsr #16      @ 1ci
    lookup_tl r0                 @ r0=c2

0:
    @ SLOT4
    make_eg_out SLOT4
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     1f
    ldr     r2, [lr, #0x1c]
    mov     r0, r0, lsr #1
    add     r0, r0, r2, lsr #16
    lookup_tl r0                 @ r0=output smp

1:
    @ SLOT2
    make_eg_out SLOT2
    cmp     r1, #ENV_QUIET
    bcs     2f
    ldr     r2, [lr, #0x14]
    mov     r5, r10, lsr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2
    add     r0, r0, r2            @ add to smp

2:
.endm


.macro upd_algo5_m

    @ SLOT3
    make_eg_out SLOT3
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     0f
    ldr     r2, [lr, #0x18]
    ldr     r0, [lr, #0x38] @ mem (signed)
    mov     r2, r2, lsr #16
    add     r0, r2, r0, lsr #1
    lookup_tl r0                 @ r0=output smp

0:
    @ SLOT4
    make_eg_out SLOT4
    cmp     r1, #ENV_QUIET
    bcs     1f
    ldr     r2, [lr, #0x1c]
    mov     r5, r10, lsr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    make_eg_out SLOT2
    cmp     r1, #ENV_QUIET
    bcs     2f
    ldr     r2, [lr, #0x14]
    mov     r5, r10, lsr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2
    add     r0, r0, r2           @ add to smp

2:
    mov     r1, r10, asr #16
    str     r1, [lr, #0x38] @ mem
.endm


.macro upd_algo6_m

    @ SLOT3
    make_eg_out SLOT3
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     0f
    ldr     r0, [lr, #0x18]
    mov     r0, r0, lsr #16      @ 1ci
    lookup_tl r0                 @ r0=output smp

0:
    @ SLOT4
    make_eg_out SLOT4
    cmp     r1, #ENV_QUIET
    bcs     1f
    ldr     r2, [lr, #0x1c]
    mov     r2, r2, lsr #16      @ 1ci
    lookup_tl r2
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    make_eg_out SLOT2
    cmp     r1, #ENV_QUIET
    bcs     2f
    ldr     r2, [lr, #0x14]
    mov     r5, r10, lsr #17
    add     r2, r5, r2, lsr #16
    lookup_tl r2
    add     r0, r0, r2           @ add to smp

2:
.endm


.macro upd_algo7_m

    @ SLOT3
    make_eg_out SLOT3
    cmp     r1, #ENV_QUIET
    movcs   r0, #0
    bcs     0f
    ldr     r0, [lr, #0x18]
    mov     r0, r0, lsr #16      @ 1ci
    lookup_tl r0                 @ r0=output smp

0:
    add     r0, r0, r10, asr #16

    @ SLOT4
    make_eg_out SLOT4
    cmp     r1, #ENV_QUIET
    bcs     1f
    ldr     r2, [lr, #0x1c]
    mov     r2, r2, lsr #16      @ 1ci
    lookup_tl r2
    add     r0, r0, r2           @ add to smp

1:  @ SLOT2
    make_eg_out SLOT2
    cmp     r1, #ENV_QUIET
    bcs     2f
    ldr     r2, [lr, #0x14]
    mov     r2, r2, lsr #16      @ 1ci
    lookup_tl r2
    add     r0, r0, r2           @ add to smp

2:
.endm


.macro upd_slot1_m

    make_eg_out SLOT1
    cmp     r1, #ENV_QUIET
    movcs   r10, r10, lsl #16     @ ct->op1_out <<= 16; // op1_out0 = op1_out1; op1_out1 = 0;
    bcs     0f
    ands    r2, r12, #0xf000
    moveq   r0, #0
    movne   r2, r2, lsr #12
    addne   r0, r10, r10, lsl #16
    movne   r0, r0, asr #16
    movne   r0, r0, lsl r2

    ldr     r2, [lr, #0x10]     @ phase1
    add     r0, r0, r2
    mov     r0, r0, lsr #16
    lookup_tl r0
    mov     r10,r10,lsl #16     @ ct->op1_out <<= 16;
    mov     r0, r0, lsl #16
    orr     r10,r10, r0, lsr #16

0:
.endm


@ lr=context, r12=pack (stereo, lastchan, disabled, lfo_enabled | pan_r, pan_l, ams[2] | AMmasks[4] | FB[4] | lfo_ampm[16])
@ r0-r2=scratch, r3=sin_tab/scratch, r4=(length<<8)|unused[4],was_update,algo[3], r5=tl_tab/slot,
@ r6-r7=vol_out[4], r8=eg_timer, r9=eg_timer_add[31:16], r10=op1_out, r11=buffer
.global chan_render_loop @ chan_rend_context *ct, int *buffer, int length

chan_render_loop:
    stmfd   sp!, {r4-r11,lr}
    mov     lr,  r0
    mov     r4,  r2, lsl #8      @ no more 24 bits here
    ldr     r12, [lr, #0x4c]
    ldr     r0,  [lr, #0x50]
    mov     r11, r1
    and     r0,  r0, #7
    orr     r4,  r4, r0          @ (length<<8)|algo
    ldr     r8, [lr, #0x44]      @ eg_timer
    ldr     r9, [lr, #0x48]      @ eg_timer_add
    ldr     r10, [lr, #0x54]     @ op1_out

    tst     r12, #8              @ lfo?
    beq     crl_loop

crl_loop_lfo:
    ldr     r1, [lr, #0x30]      @ lfo_cnt
    ldr     r2, [lr, #0x34]      @ lfo_inc

    subs    r4, r4, #0x100
    bmi     crl_loop_end

    add     r2, r2, r1
    str     r2, [lr, #0x30]

    @ r12=lfo_ampm[31:16], r1=lfo_cnt_old, r2=lfo_cnt
    advance_lfo_m

    add     r4, r4, #0x100

crl_loop:
    subs    r4, r4, #0x100
    bmi     crl_loop_end

    @ -- SSG --
    ldr     r5, [lr, #0x40]      @ CH

    @ r5=slot, trashes: r0,r2,r3
    mov     r6, #4
ssg_upd_loop:
    update_ssg_eg
#if 0
    subs    r6, r6, #1
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_ssg_eg
    subs    r6, r6, #2
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     ssg_upd_loop
    sub     r5, r5, #SLOT_STRUCT_SIZE*3

    @ -- EG --
    add     r8, r8, r9
    cmp     r8, #EG_TIMER_OVERFLOW
    bcc     eg_done
    ldr     r1, [lr, #0x3c]     @ eg_cnt
eg_loop:
    sub     r8, r8, #EG_TIMER_OVERFLOW
    add     r1, r1, #1
    cmp     r1, #4096
    movge   r1, #1

    mov     r6, #4
eg_upd_loop:
    update_eg_phase_slot
#if 1
    subs    r6, r6, #1
    addne   r5, r5, #SLOT_STRUCT_SIZE
#else
    add     r5, r5, #SLOT_STRUCT_SIZE*2
    update_eg_phase_slot
    subs    r6, r6, #2
    subne   r5, r5, #SLOT_STRUCT_SIZE
#endif
    bne     eg_upd_loop

    cmp     r8, #EG_TIMER_OVERFLOW
    sub     r5, r5, #SLOT_STRUCT_SIZE*3
    bhs     eg_loop
    str     r1, [lr, #0x3c]

eg_done:

    @ -- disabled? --
    and     r0, r12, #0xC
    cmp     r0, #0xC
    beq     crl_loop_lfo
    cmp     r0, #0x4
    beq     crl_loop

    @ output interpolation
#if defined(INTERPOL)
#if 1 // possibly too expensive for slow platforms?
    @ basic interpolator, interpolate in middle region, else use closer value
    mov     r3, r8, lsr #EG_SH      @ eg_timer, [0..3<<EG_SH) after loop
    cmp     r3, #(EG_TIMER_OVERFLOW>>EG_SH)/2
    bne     0f                      @ mix is vol_out

    ldr     r6, [r5, #0x34]      @ vol_out, vol_ipol for all slots
    ldr     r2, [r5, #0x34+SLOT_STRUCT_SIZE*2]
    ldr     r7, [r5, #0x34+SLOT_STRUCT_SIZE]
    ldr     r3, [r5, #0x34+SLOT_STRUCT_SIZE*3]
    add     r6, r6, r6, lsl #16
    lsr     r6, r6, #17
    add     r2, r2, r2, lsl #16
    lsr     r2, r2, #17
    add     r7, r7, r7, lsl #16
    lsr     r7, r7, #17
    add     r3, r3, r3, lsl #16
    lsr     r3, r3, #17
    b       1f
#else
    @ super-basic... just take value closest to sample point
    mov     r3, r8, lsr #EG_SH-1    @ eg_timer, [0..3<<EG_SH) after loop
    cmp     r3, #(EG_TIMER_OVERFLOW>>EG_SH)
#endif

0:  ldrgeh  r6, [r5, #0x34]      @ vol_out values for all slots
    ldrlth  r6, [r5, #0x36]      @ vol_ipol values for all slots
    ldrgeh  r2, [r5, #0x34+SLOT_STRUCT_SIZE*2]
    ldrlth  r2, [r5, #0x36+SLOT_STRUCT_SIZE*2]
    ldrgeh  r7, [r5, #0x34+SLOT_STRUCT_SIZE]
    ldrlth  r7, [r5, #0x36+SLOT_STRUCT_SIZE]
    ldrgeh  r3, [r5, #0x34+SLOT_STRUCT_SIZE*3]
    ldrlth  r3, [r5, #0x36+SLOT_STRUCT_SIZE*3]

#else
    ldrh    r6, [r5, #0x34]      @ vol_out values for all slots
    ldrh    r2, [r5, #0x34+SLOT_STRUCT_SIZE*2]
    ldrh    r7, [r5, #0x34+SLOT_STRUCT_SIZE]
    ldrh    r3, [r5, #0x34+SLOT_STRUCT_SIZE*3]
#endif
1:  orr     r6, r6, r2, lsl #16
    orr     r7, r7, r3, lsl #16

    @ -- SLOT1 --
    PIC_LDR(r3, r2, ym_tl_tab)

    @ lr=context, r12=pack (stereo, lastchan, disabled, lfo_enabled | pan_r, pan_l, ams[2] | AMmasks[4] | FB[4] | lfo_ampm[16])
    @ r0-r2=scratch, r3=tl_tab, r5=scratch, r6-r7=vol_out[4], r10=op1_out
    upd_slot1_m

    @ -- SLOT2+ --
    and     r0, r4, #7
    PIC_XB(,r0, lsl #2)
    nop
    PIC_BT(crl_algo0)
    PIC_BT(crl_algo1)
    PIC_BT(crl_algo2)
    PIC_BT(crl_algo3)
    PIC_BT(crl_algo4)
    PIC_BT(crl_algo5)
    PIC_BT(crl_algo6)
    PIC_BT(crl_algo7)
    .pool

crl_algo0:
    upd_algo0_m
    b       crl_algo_done
    .pool

crl_algo1:
    upd_algo1_m
    b       crl_algo_done
    .pool

crl_algo2:
    upd_algo2_m
    b       crl_algo_done
    .pool

crl_algo3:
    upd_algo3_m
    b       crl_algo_done
    .pool

crl_algo4:
    upd_algo4_m
    b       crl_algo_done
    .pool

crl_algo5:
    upd_algo5_m
    b       crl_algo_done
    .pool

crl_algo6:
    upd_algo6_m
    b       crl_algo_done
    .pool

crl_algo7:
    upd_algo7_m


crl_algo_done:
    @ -- WRITE SAMPLE --
    tst     r0, r0
    beq     ctl_sample_skip
    orr     r4, r4, #8              @ have_output
    tst     r12, #1
    beq     ctl_sample_mono

    tst     r12, #0x20              @ L
    ldrne   r1, [r11]
    addeq   r11, r11, #4
    addne   r1, r0, r1
    strne   r1, [r11], #4
    tst     r12, #0x10              @ R
    ldrne   r1, [r11]
    addeq   r11, r11, #4
    addne   r1, r0, r1
    strne   r1, [r11], #4
    b       crl_do_phase

ctl_sample_mono:
    ldr     r1, [r11]
    add     r1, r0, r1
    str     r1, [r11], #4
    b       crl_do_phase

ctl_sample_skip:
    and     r1, r12, #1
    add     r1, r1,  #1
    add     r11,r11, r1, lsl #2

crl_do_phase:
    @ -- PHASE UPDATE --
    add     r5, lr, #0x10
    ldmia   r5, {r0-r3,r6-r7}
    add     r0, r0, r6
    add     r1, r1, r7
    ldr     r6, [r5, #0x18]
    ldr     r7, [r5, #0x1c]
    add     r2, r2, r6
    add     r3, r3, r7
    stmia   r5, {r0-r3}

    tst     r12, #8
    bne     crl_loop_lfo
    b       crl_loop


crl_loop_end:
    str     r8,  [lr, #0x44]     @ eg_timer
    str     r12, [lr, #0x4c]     @ pack (for lfo_ampm)
    str     r4,  [lr, #0x50]     @ was_update
    str     r10, [lr, #0x54]     @ op1_out
    ldmfd   sp!, {r4-r11,pc}

.pool

@ vim:filetype=armasm
