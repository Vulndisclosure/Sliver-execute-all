PUBLIC VirtualFreeAndExitThread

.CODE

MEM_RELEASE equ 8000h

VirtualFreeAndExitThread PROC
    push r8
    mov rax, rdx
    mov r8, MEM_RELEASE
    mov rdx, 0
    jmp rax
    int 3
VirtualFreeAndExitThread ENDP

END
