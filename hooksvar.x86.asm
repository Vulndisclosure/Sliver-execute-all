.486
.MODEL FLAT

PUBLIC _getctx

.CODE

_getctx PROC
    call $+5
retaddr:
    pop eax
    add eax, OFFSET ctxdata - OFFSET retaddr
    ret
ctxdata:
    dd -1
_getctx ENDP

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
DefineFunctionStub ReadPipeServerProc, @4
DefineFunctionStub WritePipeServerProc, @4

END
