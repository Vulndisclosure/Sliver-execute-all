#pragma once
#include "hookspub.h"
#include <stdio.h>
#include <windows.h>

#ifdef BOF
#define _static
#else
#define _static static
#endif

#ifdef _MSC_VER
#define _forceinline __forceinline
#else
#define _forceinline __attribute__((always_inline)) inline
#endif

/*==============================================================================
 * Import
 *============================================================================*/

// clang-format off
#ifdef BOF
#define DFR(mod, func) DECLSPEC_IMPORT decltype(func) mod##$##func;
DFR(KERNEL32, GetEnvironmentVariableA)
#define GetEnvironmentVariableA KERNEL32$GetEnvironmentVariableA
DFR(KERNEL32, SetEnvironmentVariableA)
#define SetEnvironmentVariableA KERNEL32$SetEnvironmentVariableA
DFR(KERNEL32, Sleep)
#define Sleep KERNEL32$Sleep
DFR(MSVCRT, _strtoui64)
#define _strtoui64 MSVCRT$_strtoui64
DFR(MSVCRT, _ui64toa)
#define _ui64toa MSVCRT$_ui64toa
DFR(MSVCRT, _ultoa)
#define _ultoa MSVCRT$_ultoa
DFR(MSVCRT, free)
#define free MSVCRT$free
DFR(MSVCRT, malloc)
#define malloc MSVCRT$malloc
#ifdef _DEBUG
DFR(KERNEL32, OutputDebugStringA)
#define OutputDebugStringA KERNEL32$OutputDebugStringA
#endif
#ifdef _DEBUG
extern int __cdecl vsprintf_s_proto(char* const _Buffer, size_t const _BufferCount, char const* const _Format, va_list _ArgList);
#define DFR_PROTO(mod, proto, func) DECLSPEC_IMPORT decltype(proto) mod##$##func;
DFR(KERNEL32, MultiByteToWideChar)
#define MultiByteToWideChar KERNEL32$MultiByteToWideChar
DFR(KERNEL32, WideCharToMultiByte)
#define WideCharToMultiByte KERNEL32$WideCharToMultiByte
DFR(MSVCRT, _vscprintf)
#define _vscprintf MSVCRT$_vscprintf
DFR_PROTO(MSVCRT, vsprintf_s_proto, vsprintf_s)
#define vsprintf_s MSVCRT$vsprintf_s
#endif
#endif
// clang-format on

#ifdef BOF
#include "beacon.h"
#else
#include "beacon_compatibility.h"
#endif

/*==============================================================================
 * Helpers
 *============================================================================*/

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

_forceinline void _memset(void *dst, int value, size_t size) {
  for (size_t i = 0; i < size; i++)
    ((unsigned char *)dst)[i] = (unsigned char)value;
}

/*==============================================================================
 * Log
 *============================================================================*/

#if defined(BOF) && defined(_DEBUG)
void BeaconPrintfDebug(int type, const char *fmt, ...) {
  char *str = NULL;
  int strCb = 0;
  wchar_t *wstr = NULL;
  int wstrCch = 0;
  char newline[] = "\n";
  va_list args;

  // Format string
  va_start(args, fmt);
  strCb = _vscprintf(fmt, args);
  va_end(args);
  if (strCb < 0)
    goto err;
  strCb += 1;
  str = (char *)malloc(strCb);
  if (!str)
    goto err;
  va_start(args, fmt);
  vsprintf_s(str, strCb, fmt, args);
  va_end(args);

  if (type == CALLBACK_OUTPUT_UTF8) {
    // Convert UTF-8 to Unicode first
    wstrCch = MultiByteToWideChar(CP_UTF8, 0, str, strCb, NULL, 0);
    if (!wstrCch)
      goto err;
    wstr = (wchar_t *)malloc(wstrCch * sizeof(wchar_t));
    if (!wstr)
      goto err;
    wstrCch = MultiByteToWideChar(CP_UTF8, 0, str, strCb, wstr, wstrCch);
    if (!wstrCch)
      goto err;

    // Then convert Unicode to Ansi back
    strCb = WideCharToMultiByte(CP_ACP, 0, wstr, wstrCch, NULL, 0, NULL, NULL);
    if (!strCb)
      goto err;
    free(str);
    str = (char *)malloc(strCb);
    if (!str)
      goto err;
    strCb =
        WideCharToMultiByte(CP_ACP, 0, wstr, wstrCch, str, strCb, NULL, NULL);
    if (!strCb)
      goto err;

    // Set type to CALLBACK_OUTPUT
    type = CALLBACK_OUTPUT;
  }

  // Print to Beacon
  BeaconOutput(type, str, strCb);
  OutputDebugStringA(str);
  OutputDebugStringA(newline);
  goto out;

err:
  __debugbreak();
out:
  if (str)
    free(str);
  if (wstr)
    free(wstr);
}
#define _BeaconPrint(a, b, ...) BeaconPrintfDebug(a, b, ##__VA_ARGS__)
#else
#define _BeaconPrint(a, b, ...) BeaconPrintf(a, b, ##__VA_ARGS__)
#endif

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
