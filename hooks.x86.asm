.486
.MODEL FLAT

PUBLIC _getctx
PUBLIC _setctx
PUBLIC @VirtualFreeAndExitThread@12

.CODE

ctx DD -1

_getctx PROC
    call $+5
retaddr:
    pop eax
    add eax, OFFSET ctx - OFFSET retaddr
    mov eax, dword ptr [eax]
    ret
_getctx ENDP

_setctx PROC
    call $+5
retaddr:
    pop eax
    add eax, OFFSET ctx - OFFSET retaddr
    mov ecx, dword ptr [esp + 4]
    mov dword ptr [eax], ecx
    ret
_setctx ENDP

MEM_RELEASE equ 8000h

@VirtualFreeAndExitThread@12 PROC
    push dword ptr [esp + 4]
    push MEM_RELEASE
    push 0
    push ecx
    jmp edx
    int 3
@VirtualFreeAndExitThread@12 ENDP

DefineFunctionStub MACRO name, decorator
    PUBLIC _Get&name&Stub
    _Get&name&Stub PROC
        call $+5
    retaddr:
        pop eax
        add eax, OFFSET stub - OFFSET retaddr
        ret
    stub:
        EXTERN _&name&decorator:PROC
        jmp _&name&decorator
    _Get&name&Stub ENDP
ENDM

DefineFunctionStub MyExitProcess, @4
DefineFunctionStub MyGetCommandLineA, @0
DefineFunctionStub MyGetCommandLineW, @0
DefineFunctionStub MyGetStdHandle, @4
DefineFunctionStub MyProcess32NextW, @8
DefineFunctionStub MyWriteFile, @20
DefineFunctionStub MyCreateThread, @24
DefineFunctionStub MyAddVectoredExceptionHandler, @8
DefineFunctionStub MyAddVectoredContinueHandler, @8
DefineFunctionStub MySetConsoleCtrlHandler, @8
DefineFunctionStub MyVirtualAlloc, @16
DefineFunctionStub MyVirtualFree, @12
DefineFunctionStub MyTlsAlloc, @0
DefineFunctionStub MyGetProcAddress, @8
DefineFunctionStub RunExeEntryProc, @4
DefineFunctionStub HookControlSelfUnloadProc, @4
DefineFunctionStub ReadPipeServerProc, @4
DefineFunctionStub WritePipeServerProc, @4

END
