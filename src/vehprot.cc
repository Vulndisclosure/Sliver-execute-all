#include <intrin.h>
#include <windows.h>

EXTERN_C NTSTATUS NTAPI NtGetContextThreadProto(_In_ HANDLE ThreadHandle,
                                                _Inout_ PCONTEXT ThreadContext);

#define VP_NtCurrentThread() ((HANDLE)(LONG_PTR) - 2)
#define VP_NtCurrentThreadId() (((PHANDLE)NtCurrentTeb())[9])
#define VP_NtLastError() (*(PDWORD)((PSIZE_T)NtCurrentTeb() + 13))
#define VP_STATE_RUNNING 2
#define VP_STATE_EXCEPTION 3
#define VP_STATE_ENDED 4

// For compatibility, .bss is supported since Cobalt Strike 4.7
PVOID VP_Handle = (PVOID)(SIZE_T)1;
decltype(AddVectoredExceptionHandler)* VP_AddVectoredExceptionHandler =
    (decltype(AddVectoredExceptionHandler)*)(SIZE_T)1;
decltype(RemoveVectoredExceptionHandler)* VP_RemoveVectoredExceptionHandler =
    (decltype(RemoveVectoredExceptionHandler)*)(SIZE_T)1;
decltype(NtGetContextThreadProto)* VP_NtGetContextThread =
    (decltype(NtGetContextThreadProto)*)(SIZE_T)1;
DWORD VP_ThreadId = 1;
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
CONTEXT VP_Context = {1};
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif
DWORD VP_ExceptionCode = 1;
volatile DWORD VP_State = 1;

LONG NTAPI VP_ExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo) {
  if (VP_State == VP_STATE_RUNNING &&
      VP_ThreadId == (DWORD)(SIZE_T)VP_NtCurrentThreadId()) {
    *ExceptionInfo->ContextRecord = VP_Context;
    VP_State = VP_STATE_EXCEPTION;
    VP_ExceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

DECLSPEC_NOINLINE PVOID VP_GetReturnAddress() {
#ifdef _MSC_VER
  return (PVOID)_ReturnAddress();
#else
  return (PVOID)__builtin_return_address(0);
#endif
}
#ifdef _WIN64
#define VP_SetIp(c, ip) c.Rip = (DWORD64)(SIZE_T)(ip)
#define VP_AdjustSp(c) c.Rsp += 8 // + 1 return address size
#else
#define VP_SetIp(c, ip) c.Eip = (DWORD)(SIZE_T)(ip)
#define VP_AdjustSp(c) c.Esp += 12 // + 2 parameters size + 1 return address size
#endif

// AddVectoredExceptionHandler: The continue handler is called only if a valid
// SEH frame is present.
#define VP_INIT                                                                \
  VP_AddVectoredExceptionHandler =                                             \
      (decltype(AddVectoredExceptionHandler)*)(PVOID)GetProcAddress(           \
          GetModuleHandleA("kernel32.dll"), "AddVectoredExceptionHandler");    \
  VP_RemoveVectoredExceptionHandler =                                          \
      (decltype(RemoveVectoredExceptionHandler)*)(PVOID)GetProcAddress(        \
          GetModuleHandleA("kernel32.dll"), "RemoveVectoredExceptionHandler"); \
  VP_NtGetContextThread =                                                      \
      (decltype(NtGetContextThreadProto)*)(PVOID)GetProcAddress(               \
          GetModuleHandleA("ntdll.dll"), "NtGetContextThread");                \
  if (VP_AddVectoredExceptionHandler && VP_RemoveVectoredExceptionHandler) {   \
    VP_Handle = VP_AddVectoredExceptionHandler(FALSE, VP_ExceptionHandler);    \
    if (!VP_Handle) {                                                          \
      PRINTF_ERR("Failed to add vectored continue handler: %d",                \
                 (UINT)VP_NtLastError());                                      \
    }                                                                          \
  }

#define VP_UNINIT                                   \
  if (VP_Handle && VP_Handle != (PVOID)(SIZE_T)1) { \
    VP_RemoveVectoredExceptionHandler(VP_Handle);   \
    VP_Handle = NULL;                               \
  }

#define VP_TRY                                              \
  VP_State = VP_STATE_RUNNING;                              \
  VP_ThreadId = (DWORD)(SIZE_T)VP_NtCurrentThreadId();      \
  VP_Context.ContextFlags = CONTEXT_FULL;                   \
  VP_NtGetContextThread(VP_NtCurrentThread(), &VP_Context); \
  VP_AdjustSp(VP_Context);                                  \
  VP_SetIp(VP_Context, VP_GetReturnAddress());              \
  if (VP_State == VP_STATE_RUNNING)

#define VP_CATCH else if (VP_State == VP_STATE_EXCEPTION)

#define VP_END VP_State = VP_STATE_ENDED
