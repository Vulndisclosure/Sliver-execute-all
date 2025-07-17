#include "exguard.h"

#include "hooks.h"

static LONG NTAPI ExGuard_ExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo) {
  DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
  PVOID address = ExceptionInfo->ExceptionRecord->ExceptionAddress;
  TRACEF(
      "ExGuard_ExceptionHandler was called. (ExceptionCode: %08x, "
      "ExceptionAddress: %p)",
      code, address);
  BOOL found = FALSE;
  int index = 0;
  SpinLock(ThreadListLock, {
    SimpleListForEach(&ThreadList, DWORD, entry, {
      if (*entry == (DWORD)(SIZE_T)NtCurrentThreadId()) {
        found = TRUE;
        break;
      }
      index++;
    });
  });
  if (found) {
    // Don't set IsRunning to FALSE which indicates the PE image runs to the end
    // and can be unloaded.
    PEImage->IsCrashed = TRUE;
    PRINTF_ERR("Thread %d (%d) in '%s' encountered an exception: %08x at %p",
               index, (UINT)(SIZE_T)NtCurrentThreadId(), PEImage->CommandLineA,
               code, address);
    ExitThread(code);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

void InitializeExceptionGuard() {
  decltype(SetUnhandledExceptionFilter)* pRtlSetUnhandledExceptionFilter =
      (decltype(SetUnhandledExceptionFilter)*)(PVOID)GetProcAddress(
          GetModuleHandleA("ntdll.dll"), "RtlSetUnhandledExceptionFilter");
  // TODO: Why SetUnhandledExceptionFilter doesn't work when the handler is
  // shellcode?
  if (pRtlSetUnhandledExceptionFilter) {
    pRtlSetUnhandledExceptionFilter(ExGuard_ExceptionHandler);
  } else {
    SetUnhandledExceptionFilter(ExGuard_ExceptionHandler);
  }
  TRACEF("Exception guard initialized. (handler: %p)",
         ExGuard_ExceptionHandler);
}
