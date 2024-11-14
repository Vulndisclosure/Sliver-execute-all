#pragma once
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on
#include "hookspub.h"
#include "pipeserver.h"
#include "simplelist.h"

/*==============================================================================
 * Import
 *============================================================================*/

// clang-format off
/* Output Functions */
#define CALLBACK_OUTPUT      0x0
#define CALLBACK_OUTPUT_OEM  0x1e
#define CALLBACK_OUTPUT_UTF8 0x20
#define CALLBACK_ERROR       0x0d
// clang-format on

/*==============================================================================
 * Declarations
 *============================================================================*/

// Basic (In)

extern HookFeatures Features;
extern LPSTR CommandLineA;
extern LPWSTR CommandLineW;
extern BOOL HasVFS;
extern HookOutput *Output;  // owned by LoadedPEImage

#define HasFeatures(x) ((Features & (x)) == (x))

// Self Unload (In)

extern PVOID HookBase;
extern PVOID ImageBase;
#ifdef _WIN64
extern PVOID FunctionTable;
#else
extern PVOID SEH_ImageBase;
#endif
extern LoadedPEImage *PEImage;
extern HANDLE CompletionEventHandle;

DWORD WINAPI SelfUnloadProc(LPVOID lpThreadParameter);

// Stdio related

extern PipeServer StdinPipe;
extern PipeServer StdoutPipe;
extern PipeServer StderrPipe;

// Hacking for golang cobra command line parser

extern DWORD ParentProcessId;

// Thread/Process related

extern SimpleList ThreadList;
extern volatile LONG ThreadListLock;

HANDLE WINAPI MyCreateThread(_In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
                             _In_ SIZE_T dwStackSize,
                             _In_ LPTHREAD_START_ROUTINE lpStartAddress,
                             _In_opt_ __drv_aliasesMem LPVOID lpParameter,
                             _In_ DWORD dwCreationFlags,
                             _Out_opt_ LPDWORD lpThreadId);

// System callback related

#define EHTYPE_VEH 1
#define EHTYPE_VCH 2
// #define EHTYPE_UEF 3

typedef struct {
  int Type;
  PVOID Handle;
} ExceptionHandlerEntry;

extern SimpleList ExceptionHandlerList;

// Allocation related

extern SimpleList MemoryList;
extern volatile LONG MemoryListLock;
extern SimpleList TlsList;
extern volatile LONG TlsListLock;

// GetProcAddress

extern HMODULE Kernel32Base;
extern HMODULE MsvcrtBase;

FARPROC WINAPI MyGetProcAddress(_In_ HMODULE hModule, _In_ LPCSTR lpProcName);

/*==============================================================================
 * Helpers
 *============================================================================*/

#define NtCurrentProcessId() (((PHANDLE)NtCurrentTeb())[8])
#define NtCurrentThreadId() (((PHANDLE)NtCurrentTeb())[9])
#define NtLastError() (*(PDWORD)((PSIZE_T)NtCurrentTeb() + 13))

#define SpinLock(lock, code)                               \
  do {                                                     \
    while (InterlockedExchange(&(lock), 1) == 1) Sleep(1); \
    code;                                                  \
    InterlockedExchange(&(lock), 0);                       \
  } while (0)

/*==============================================================================
 * Log
 *============================================================================*/

void BeaconPrintfPostMsg(int type, const char *fmt, ...);
#define _BeaconPrint(a, b, ...) BeaconPrintfPostMsg(a, b, ##__VA_ARGS__)

#define PRINTF(a, ...) _BeaconPrint(CALLBACK_OUTPUT, a, ##__VA_ARGS__)
#define PRINTF_ERR(a, ...) _BeaconPrint(CALLBACK_ERROR, a, ##__VA_ARGS__)

/*==============================================================================
 * Trace
 *============================================================================*/

#ifdef _DEBUG
#define _TRACE
#endif

#ifdef _TRACE
#define TRACEF(a, ...) _BeaconPrint(CALLBACK_OUTPUT, "[DBG] " a, ##__VA_ARGS__)
#else
#define TRACEF(a, ...)
#endif
