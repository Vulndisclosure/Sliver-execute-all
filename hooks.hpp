#pragma once
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on
#include "hookspub.h"

/*==============================================================================
 * Import
 *============================================================================*/

// clang-format off
// In hooksvar.*.asm
#ifdef _WIN64
extern HookContext *ctx;
_forceinline HookContext **getctx() { return &ctx; }
#define AddressOfMyExitProcess                 ((void *)(&MyExitProcess))
#define AddressOfMyGetCommandLineA             ((void *)(&MyGetCommandLineA))
#define AddressOfMyGetCommandLineW             ((void *)(&MyGetCommandLineW))
#define AddressOfMyGetStdHandle                ((void *)(&MyGetStdHandle))
#define AddressOfMyProcess32NextW              ((void *)(&MyProcess32NextW))
#define AddressOfMyWriteFile                   ((void *)(&MyWriteFile))
#define AddressOfMyCreateThread                ((void *)(&MyCreateThread))
#define AddressOfMyAddVectoredExceptionHandler ((void *)(&MyAddVectoredExceptionHandler))
#define AddressOfMyAddVectoredContinueHandler  ((void *)(&MyAddVectoredContinueHandler))
#define AddressOfMySetConsoleCtrlHandler       ((void *)(&MySetConsoleCtrlHandler))
#define AddressOfMyVirtualAlloc                ((void *)(&MyVirtualAlloc))
#define AddressOfMyVirtualFree                 ((void *)(&MyVirtualFree))
#define AddressOfMyTlsAlloc                    ((void *)(&MyTlsAlloc))
#define AddressOfMyGetProcAddress              ((void *)(&MyGetProcAddress))
#define AddressOfRunExeEntryProc               ((void *)(&RunExeEntryProc))
#define AddressOfReadPipeServerProc            ((void *)(&ReadPipeServerProc))
#define AddressOfWritePipeServerProc           ((void *)(&WritePipeServerProc))
#else
HookContext **getctx();
void *GetMyExitProcessStub();
void *GetMyGetCommandLineAStub();
void *GetMyGetCommandLineWStub();
void *GetMyGetStdHandleStub();
void *GetMyProcess32NextWStub();
void *GetMyWriteFileStub();
void *GetMyCreateThreadStub();
void *GetMyAddVectoredExceptionHandlerStub();
void *GetMyAddVectoredContinueHandlerStub();
void *GetMySetConsoleCtrlHandlerStub();
void *GetMyVirtualAllocStub();
void *GetMyVirtualFreeStub();
void *GetMyTlsAllocStub();
void *GetMyGetProcAddressStub();
void *GetRunExeEntryProcStub();
void *GetReadPipeServerProcStub();
void *GetWritePipeServerProcStub();
#define AddressOfMyExitProcess                 (GetMyExitProcessStub())
#define AddressOfMyGetCommandLineA             (GetMyGetCommandLineAStub())
#define AddressOfMyGetCommandLineW             (GetMyGetCommandLineWStub())
#define AddressOfMyGetStdHandle                (GetMyGetStdHandleStub())
#define AddressOfMyProcess32NextW              (GetMyProcess32NextWStub())
#define AddressOfMyWriteFile                   (GetMyWriteFileStub())
#define AddressOfMyCreateThread                (GetMyCreateThreadStub())
#define AddressOfMyAddVectoredExceptionHandler (GetMyAddVectoredExceptionHandlerStub())
#define AddressOfMyAddVectoredContinueHandler  (GetMyAddVectoredContinueHandlerStub())
#define AddressOfMySetConsoleCtrlHandler       (GetMySetConsoleCtrlHandlerStub())
#define AddressOfMyVirtualAlloc                (GetMyVirtualAllocStub())
#define AddressOfMyVirtualFree                 (GetMyVirtualFreeStub())
#define AddressOfMyTlsAlloc                    (GetMyTlsAllocStub())
#define AddressOfMyGetProcAddress              (GetMyGetProcAddressStub())
#define AddressOfRunExeEntryProc               (GetRunExeEntryProcStub())
#define AddressOfReadPipeServerProc            (GetReadPipeServerProcStub())
#define AddressOfWritePipeServerProc           (GetWritePipeServerProcStub())
#endif

extern int __cdecl vsprintf_s_proto(char* const _Buffer, size_t const _BufferCount, char const* const _Format, va_list _ArgList);
#define AddVectoredContinueHandler     ((decltype(AddVectoredContinueHandler    )*)((*getctx())->_AddVectoredContinueHandler    ))
#define AddVectoredExceptionHandler    ((decltype(AddVectoredExceptionHandler   )*)((*getctx())->_AddVectoredExceptionHandler   ))
#define CloseHandle                    ((decltype(CloseHandle                   )*)((*getctx())->_CloseHandle                   ))
#define CreateEventA                   ((decltype(CreateEventA                  )*)((*getctx())->_CreateEventA                  ))
#define CreatePipe                     ((decltype(CreatePipe                    )*)((*getctx())->_CreatePipe                    ))
#define CreateThread                   ((decltype(CreateThread                  )*)((*getctx())->_CreateThread                  ))
#define ExitThread                     ((decltype(ExitThread                    )*)((*getctx())->_ExitThread                    ))
#define GetModuleHandleA               ((decltype(GetModuleHandleA              )*)((*getctx())->_GetModuleHandleA              ))
#define GetProcAddress                 ((decltype(GetProcAddress                )*)((*getctx())->_GetProcAddress                ))
#define GetStdHandle                   ((decltype(GetStdHandle                  )*)((*getctx())->_GetStdHandle                  ))
#define GetThreadId                    ((decltype(GetThreadId                   )*)((*getctx())->_GetThreadId                   ))
#define MultiByteToWideChar            ((decltype(MultiByteToWideChar           )*)((*getctx())->_MultiByteToWideChar           ))
#define OpenThread                     ((decltype(OpenThread                    )*)((*getctx())->_OpenThread                    ))
#define Process32NextW                 ((decltype(Process32NextW                )*)((*getctx())->_Process32NextW                ))
#define ReadFile                       ((decltype(ReadFile                      )*)((*getctx())->_ReadFile                      ))
#define RemoveVectoredContinueHandler  ((decltype(RemoveVectoredContinueHandler )*)((*getctx())->_RemoveVectoredContinueHandler ))
#define RemoveVectoredExceptionHandler ((decltype(RemoveVectoredExceptionHandler)*)((*getctx())->_RemoveVectoredExceptionHandler))
#define SetEvent                       ((decltype(SetEvent                      )*)((*getctx())->_SetEvent                      ))
#define SetUnhandledExceptionFilter    ((decltype(SetUnhandledExceptionFilter   )*)((*getctx())->_SetUnhandledExceptionFilter   ))
#define Sleep                          ((decltype(Sleep                         )*)((*getctx())->_Sleep                         ))
#define TerminateThread                ((decltype(TerminateThread               )*)((*getctx())->_TerminateThread               ))
#define TlsAlloc                       ((decltype(TlsAlloc                      )*)((*getctx())->_TlsAlloc                      ))
#define TlsFree                        ((decltype(TlsFree                       )*)((*getctx())->_TlsFree                       ))
#define VirtualAlloc                   ((decltype(VirtualAlloc                  )*)((*getctx())->_VirtualAlloc                  ))
#define VirtualFree                    ((decltype(VirtualFree                   )*)((*getctx())->_VirtualFree                   ))
#define WaitForSingleObject            ((decltype(WaitForSingleObject           )*)((*getctx())->_WaitForSingleObject           ))
#define WideCharToMultiByte            ((decltype(WideCharToMultiByte           )*)((*getctx())->_WideCharToMultiByte           ))
#define WriteFile                      ((decltype(WriteFile                     )*)((*getctx())->_WriteFile                     ))
#define _vscprintf                     ((decltype(_vscprintf                    )*)((*getctx())->__vscprintf                    ))
#define free                           ((decltype(free                          )*)((*getctx())->_free                          ))
#define malloc                         ((decltype(malloc                        )*)((*getctx())->_malloc                        ))
#define realloc                        ((decltype(realloc                       )*)((*getctx())->_realloc                       ))
#define vsprintf_s                     ((decltype(vsprintf_s_proto              )*)((*getctx())->_vsprintf_s                    ))
#ifdef _DEBUG
#define OutputDebugStringA             ((decltype(OutputDebugStringA            )*)((*getctx())->_OutputDebugStringA            ))
#else
#define OutputDebugStringA
#endif

/* Output Functions */
#define CALLBACK_OUTPUT      0x0
#define CALLBACK_OUTPUT_OEM  0x1e
#define CALLBACK_OUTPUT_UTF8 0x20
#define CALLBACK_ERROR       0x0d
// clang-format on

/*==============================================================================
 * Helpers
 *============================================================================*/

#define NtCurrentProcessId() (((PHANDLE)NtCurrentTeb())[8])
#define NtCurrentThreadId() (((PHANDLE)NtCurrentTeb())[9])
#define NtLastError() (*(PDWORD)((PSIZE_T)NtCurrentTeb() + 13))

#define SpinLock(lock, code)                                                   \
  do {                                                                         \
    while (InterlockedExchange(&(lock), 1) == 1) {                             \
      Sleep(1);                                                                \
    }                                                                          \
    code;                                                                      \
    InterlockedExchange(&(lock), 0);                                           \
  } while (0)

_forceinline void _memcpy(void *dst, const void *src, size_t size) {
  for (size_t i = 0; i < size; i++)
    ((unsigned char *)dst)[i] = ((unsigned char *)src)[i];
}

/*==============================================================================
 * Log
 *============================================================================*/

extern void BeaconPrintfPostMsg(int type, const char *fmt, ...);
#define _BeaconPrint(a, b, ...) BeaconPrintfPostMsg(a, b, ##__VA_ARGS__)

#define PRINTF(a, ...) _BeaconPrint(CALLBACK_OUTPUT, xorstr_(a), ##__VA_ARGS__)
#define PRINTF_ERR(a, ...)                                                     \
  _BeaconPrint(CALLBACK_ERROR, xorstr_(a), ##__VA_ARGS__)

/*==============================================================================
 * Trace
 *============================================================================*/

#ifdef _DEBUG
#define _TRACE
#endif

#ifdef _TRACE
#define TRACEF(a, ...)                                                         \
  _BeaconPrint(CALLBACK_OUTPUT, xorstr_("[DBG] " a), ##__VA_ARGS__)
#else
#define TRACEF(a, ...)
#endif
