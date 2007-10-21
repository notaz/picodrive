# vim:filetype=mips

# only CLUT for now..

.set noreorder # don't reorder any instructions
.set noat      # don't use $at


# void amips_clut(unsigned short *dst, unsigned char *src, unsigned short *pal, int count)

.global amips_clut

amips_clut:
    srl     $a3, 2
amips_clut_loop:
    lbu     $t0, 0($a1)           # tried lw here, no improvement noticed
    lbu     $t1, 1($a1)
    lbu     $t2, 2($a1)
    lbu     $t3, 3($a1)
    sll     $t0, 1
    sll     $t1, 1
    sll     $t2, 1
    sll     $t3, 1
    addu    $t0, $a2
    addu    $t1, $a2
    addu    $t2, $a2
    addu    $t3, $a2
    lhu     $t0, 0($t0)
    lhu     $t1, 0($t1)
    lhu     $t2, 0($t2)
    lhu     $t3, 0($t3)
    ins     $t0, $t1, 16, 16      # ins rt, rs, pos, size - Insert size bits starting
    ins     $t2, $t3, 16, 16      #  from the LSB of rs into rt starting at position pos
    sw      $t0, 0($a0)
    sw      $t2, 4($a0)
    addiu   $a0, 8
    addiu   $a3, -1
    bnez    $a3, amips_clut_loop
    addiu   $a1, 4
    jr      $ra
    nop

