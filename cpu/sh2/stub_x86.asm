section .text

global sh2_drc_entry ; SH2 *sh2, void *block

sh2_drc_entry:
    push    ebp
    mov     ebp, [esp+4+4]	; context
    mov     eax, [esp+4+8]
    jmp     eax

global sh2_drc_exit

sh2_drc_exit:
    pop     ebp
    ret

