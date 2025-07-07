#include "hooks.h"

void BeaconPrintfPostMsg(int type, const char *fmt, ...) {
  char *str = NULL;
  int strCb = 0;
  wchar_t *wstr = NULL;
  int wstrCch = 0;
  BOOL result = FALSE;
  LogEntry log;
  va_list args;

  // Format string
  va_start(args, fmt);
  strCb = _vscprintf(fmt, args);
  va_end(args);
  if (strCb < 0) goto err;
  strCb += 2;
  str = (char *)malloc(strCb);
  if (!str) goto err;
  va_start(args, fmt);
  vsprintf_s(str, strCb, fmt, args);
  str[strCb - 2] = '\n';
  str[strCb - 1] = '\0';
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

  // Post message
  OutputDebugStringA(str);
  if (Output->JobOutputWriteHandle) {
    DWORD t = 0;  // From MSDN: Windows 7: This parameter can not be NULL.
    SpinLock(Output->JobOutputLock,
             result = WriteFile(Output->JobOutputWriteHandle, str, strCb - 1,
                                &t, NULL));
    if (!result) goto err;
  } else {
    log = {type, strCb - 2, str};
    SpinLock(Output->LogLock, SimpleListAdd(&Output->Logs, LogEntry, log));
    str = NULL;  // Don't free
  }
  goto out;

err:
#ifdef _DEBUG
  __debugbreak();
#endif
out:
  if (str) free(str);
  if (wstr) free(wstr);
}
