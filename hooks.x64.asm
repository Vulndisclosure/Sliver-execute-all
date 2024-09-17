PUBLIC ctx
PUBLIC VirtualFreeAndExitThread

.CODE

ctx DQ -1

MEM_RELEASE equ 8000h

VirtualFreeAndExitThread PROC
    sub rsp, 20h
    push r8
    mov r9, MEM_RELEASE
    mov r8, 0
    jmp rdx
    int 3
VirtualFreeAndExitThread ENDP

END
