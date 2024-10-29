#pragma once
#include <stdio.h>
#include <windows.h>

#undef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT static
#include "beacon.h"
#undef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT __declspec(dllimport)

static BOOL toWideChar(char *src, wchar_t *dst, int max) {
  int size = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
  if (size == -1 || size >= max) {
    return FALSE;
  }

  MultiByteToWideChar(CP_ACP, 0, src, -1, dst, max);
  return TRUE;
}

static void BeaconPrintf(int type, const char *fmt, ...) {
  char *str = NULL;
  int strCb = 0;
  wchar_t *wstr = NULL;
  int wstrCch = 0;
  char format[] = "%.*s\n";
  va_list args;

  // Format string
  va_start(args, fmt);
  strCb = _vscprintf(fmt, args);
  va_end(args);
  if (strCb < 0) goto err;
  strCb += 1;
  str = (char *)malloc(strCb);
  if (!str) goto err;
  va_start(args, fmt);
  vsprintf_s(str, strCb, fmt, args);
  va_end(args);

  if (type == CALLBACK_OUTPUT_UTF8) {
    // Convert UTF-8 to Unicode first
    wstrCch = MultiByteToWideChar(CP_UTF8, 0, str, strCb, NULL, 0);
    if (!wstrCch) goto err;
    wstr = (wchar_t *)malloc(wstrCch * sizeof(wchar_t));
    if (!wstr) goto err;
    wstrCch = MultiByteToWideChar(CP_UTF8, 0, str, strCb, wstr, wstrCch);
    if (!wstrCch) goto err;

    // Then convert Unicode to Ansi back
    strCb = WideCharToMultiByte(CP_ACP, 0, wstr, wstrCch, NULL, 0, NULL, NULL);
    if (!strCb) goto err;
    free(str);
    str = (char *)malloc(strCb);
    if (!str) goto err;
    strCb =
        WideCharToMultiByte(CP_ACP, 0, wstr, wstrCch, str, strCb, NULL, NULL);
    if (!strCb) goto err;

    // Set type to CALLBACK_OUTPUT
    type = CALLBACK_OUTPUT;
  }

  printf(format, strCb, str);
  goto out;

err:
#ifdef _DEBUG
  __debugbreak();
#endif
out:
  if (str) free(str);
  if (wstr) free(wstr);
}

static void BeaconOutput(int type, const char *data, int len) {
  if (type == CALLBACK_OUTPUT_UTF8) {
    char format[] = "%.*s";
    BeaconPrintf(type, format, len, data);
  } else {
    char format[] = "%.*s\n";
    printf(format, len, data);
  }
}
