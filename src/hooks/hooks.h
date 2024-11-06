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
