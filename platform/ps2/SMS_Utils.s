/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# MUL64 is pulled from some binary library (I don't remember which one).
# mips_memcpy routine is pulled from 'sde' library from MIPS.
#
*/
.set noat
.set noreorder
.set nomacro

.globl MUL64
.globl mips_memcpy
.globl mips_memset

.text

MUL64:
    pmultuw	$v0, $a0, $a1
    dsra32	$a2, $a0, 0
    dsra32  $v1, $a1, 0
    mult    $v1, $a0, $v1
    mult1   $a2, $a2, $a1
    addu    $v1, $v1, $a2
    dsll32  $v1, $v1, 0
    jr      $ra
    daddu   $v0, $v0, $v1

mips_memcpy:
    addu    $v0, $a0, $zero
    beqz    $a2, 1f
    sltiu   $t2, $a2, 12
    bnez    $t2, 2f
    xor     $v1, $a1, $a0
    andi    $v1, $v1, 7
    negu    $a3, $a0
    beqz    $v1, 3f
    andi    $a3, $a3, 7
    beqz    $a3, 4f
    subu    $a2, $a2, $a3
    ldr     $v1, 0($a1)
    ldl     $v1, 7($a1)
    addu    $a1, $a1, $a3
    sdr     $v1, 0($a0)
    addu    $a0, $a0, $a3
4:
    andi    $v1, $a2, 31
    subu    $a3, $a2, $v1
    beqz    $a3, 5f
    addu    $a2, $v1, $zero
    addu    $a3, $a3, $a1
6:
    ldr     $v1,  0($a1)
    ldl     $v1,  7($a1)
    ldr     $t0,  8($a1)
    ldl     $t0, 15($a1)
    ldr     $t1, 16($a1)
    ldl     $t1, 23($a1)
    ldr     $t2, 24($a1)
    ldl     $t2, 31($a1)
    sd      $v1,  0($a0)
    sd      $t0,  8($a0)
    sd      $t1, 16($a0)
    addiu   $a1, $a1, 32
    addiu   $a0, $a0, 32
    bne     $a1, $a3, 6b
    sd      $t2, -8($a0)
5:
    andi    $v1, $a2, 7
    subu    $a3, $a2, $v1
    beqz    $a3, 2f
    addu    $a2, $v1, $zero
    addu    $a3, $a3, $a1
7:
    ldr     $v1, 0($a1)
    ldl     $v1, 7($a1)
    addiu   $a1, $a1, 8
    addiu   $a0, $a0, 8
    nop
    bne     $a1, $a3, 7b
    sd      $v1, -8($a0)
    beq     $zero, $zero, 2f
    nop
3:
    beqz    $a3, 8f
    subu    $a2, $a2, $a3
    ldr     $v1, 0($a1)
    addu    $a1, $a1, $a3
    sdr     $v1, 0($a0)
    addu    $a0, $a0, $a3
8:
    andi    $v1, $a2, 31
    subu    $a3, $a2, $v1
    beqz    $a3, 9f
    addu    $a2, $v1, $zero
    addu    $a3, $a3, $a1
10:
    ld      $v1,  0($a1)
    ld      $t0,  8($a1)
    ld      $t1, 16($a1)
    ld      $t2, 24($a1)
    sd      $v1,  0($a0)
    sd      $t0,  8($a0)
    sd      $t1, 16($a0)
    addiu   $a1, $a1, 32
    addiu   $a0, $a0, 32
    bne     $a1, $a3, 10b
    sd      $t2, -8($a0)
9:
    andi    $v1, $a2, 7
    subu    $a3, $a2, $v1
    beqz    $a3, 2f
    addu    $a2, $v1, $zero
    addu    $a3, $a3, $a1
11:
    ld      $v1, 0($a1)
    addiu   $a1, $a1, 8
    addiu   $a0, $a0, 8
    nop
    nop
    bne     $a1, $a3, 11b
    sd      $v1, -8($a0)
2:
    beqz    $a2, 1f
    addu    $a3, $a2, $a1
12:
    lbu     $v1, 0($a1)
    addiu   $a1, $a1, 1
    addiu   $a0, $a0, 1
    nop
    nop
    bne     $a1, $a3, 12b
    sb      $v1, -1($a0)
1:
    jr      $ra
    nop

mips_memset:
    beqz    $a2, 1f
    sltiu   $at, $a2, 16
    bnez    $at, 2f
    andi    $a1, $a1, 0xFF
    dsll    $at, $a1, 0x8
    or      $a1, $a1, $at
    dsll    $at, $a1, 0x10
    or      $a1, $a1, $at
    dsll32  $at, $a1, 0x0
    or      $a1, $a1, $at
    andi    $v1, $a0, 0x7
    beqz    $v1, 3f
    li      $a3, 8
    subu    $a3, $a3, $v1
    subu    $a2, $a2, $a3
    sdr     $a1, 0($a0)
    addu    $a0, $a0, $a3
3:
    andi    $v1, $a2, 0x1f
    subu    $a3, $a2, $v1
    beqz    $a3, 4f
    move    $a2, $v1
    addu    $a3, $a3, $a0
5:
    sd      $a1,  0($a0)
    sd      $a1,  8($a0)
    sd      $a1, 16($a0)
    addiu   $a0, $a0, 32
    sd      $a1, -8($a0)
    bne     $a0, $a3, 5b
4:
    andi    $v1, $a2, 0x7
    subu    $a3, $a2, $v1
    beqz    $a3, 2f
    move    $a2, $v1
    addu    $a3, $a3, $a0
6:
    addiu   $a0, $a0, 8
    beq     $a0, $a3, 2f
    sd      $a1, -8($a0)
    addiu   $a0, $a0, 8
    beq     $a0, $a3, 2f
    sd      $a1, -8($a0)
    addiu   $a0, $a0, 8
    bne     $a0, $a3, 6b
    sd      $a1, -8($a0)
2:
    beqz    $a2, 1f
    addu    $a3, $a2, $a0
7:
    addiu   $a0, $a0, 1
    beq     $a0, $a3, 1f
    sb      $a1, -1($a0)
    addiu   $a0, $a0, 1
    beq     $a0, $a3, 1f
    sb      $a1, -1($a0)
    addiu   $a0, $a0, 1
    bne     $a0, $a3, 7b
    sb      $a1, -1($a0)
1:
    jr  $ra
    nop
