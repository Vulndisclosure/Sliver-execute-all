.486
.MODEL FLAT

PUBLIC MyNtQueryVirtualMemory

MEM_IMAGE equ 1000000h

MEMORY_BASIC_INFORMATION STRUCT
  BaseAddress       DWORD      ?
  AllocationBase    DWORD      ?
  AllocationProtect DWORD      ?
  RegionSize        DWORD      ?
  State             DWORD      ?
  Protect           DWORD      ?
  lType             DWORD      ?
MEMORY_BASIC_INFORMATION ENDS

NtQueryVirtualMemory PROTO STDCALL ProcessHandle:DWORD, BaseAddress:DWORD, MemoryInformationClass:DWORD, MemoryInformation:DWORD, MemoryInformationLength:DWORD, ReturnLength:DWORD
NtQueryVirtualMemoryTrampoline PROTO STDCALL ProcessHandle:DWORD, BaseAddress:DWORD, MemoryInformationClass:DWORD, MemoryInformation:DWORD, MemoryInformationLength:DWORD, ReturnLength:DWORD

.CODE

ImageBaseList DWORD 16 DUP(0)

MyNtQueryVirtualMemory PROC STDCALL ProcessHandle:DWORD, BaseAddress:DWORD, MemoryInformationClass:DWORD, MemoryInformation:DWORD, MemoryInformationLength:DWORD, ReturnLength:DWORD
    push ebx

    INVOKE NtQueryVirtualMemoryTrampoline, ProcessHandle, BaseAddress, MemoryInformationClass, MemoryInformation, MemoryInformationLength, ReturnLength

    ; if (eax >= 0) and (ProcessHandle == NtCurrentProcess) and (MemoryInformationClass == MemoryBasicInformation)
    .IF (SDWORD PTR eax >= 0) && (ProcessHandle == -1) && (MemoryInformationClass == 0)
        ; get ImageBaseList
        call $+5
    retaddr:
        pop ebx
        add ebx, OFFSET ImageBaseList - OFFSET retaddr

        ; get mbi.AllocationBase
        mov edx, MemoryInformation
        ASSUME edx:PTR MEMORY_BASIC_INFORMATION
        mov edx, [edx].AllocationBase
        ASSUME edx:NOTHING

        ; Check up to 16 elements in the ImageBaseList
        mov ecx, 0
        .REPEAT
            ; if AllocationBase == ImageBaseList[i]
            .IF edx == [ebx + ecx * 4]
                ; mbi.Type = MEM_IMAGE
                mov edx, MemoryInformation
                ASSUME edx:PTR MEMORY_BASIC_INFORMATION
                mov [edx].lType, MEM_IMAGE
                .BREAK
            .ENDIF

            inc ecx
        .UNTIL ecx >= 16
    .ENDIF

    pop ebx
    ret
MyNtQueryVirtualMemory ENDP

OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

NtQueryVirtualMemoryTrampoline PROC STDCALL ProcessHandle:DWORD, BaseAddress:DWORD, MemoryInformationClass:DWORD, MemoryInformation:DWORD, MemoryInformationLength:DWORD, ReturnLength:DWORD
    mov eax, 0
    jmp NtQueryVirtualMemory + 5
NtQueryVirtualMemoryTrampoline ENDP

OPTION PROLOGUE:PROLOGUEDEF
OPTION EPILOGUE:EPILOGUEDEF

END