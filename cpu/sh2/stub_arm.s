@ vim:filetype=armasm
.text

.global sh2_drc_entry @ SH2 *sh2, void *block

sh2_drc_entry:
    stmfd   sp!, {r7,lr}
    mov     r7, r0
    bx      r1


.global sh2_drc_exit

sh2_drc_exit:
    ldmfd   sp!, {r7,pc}

