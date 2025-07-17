#include <intrin.h>
#include <windows.h>

EXTERN_C NTSTATUS NTAPI NtGetContextThreadProto(_In_ HANDLE ThreadHandle,
                                                _Inout_ PCONTEXT ThreadContext);

#define VP_NtCurrentThread() ((HANDLE)(LONG_PTR)(-2))
#define VP_NtCurrentThreadId() (((PHANDLE)NtCurrentTeb())[9])
#define VP_NtLastError() (*(PDWORD)((PSIZE_T)NtCurrentTeb() + 13))
#define VP_STATE_RUNNING 2
#define VP_STATE_EXCEPTION 3
#define VP_STATE_ENDED 4
#define VP_EXCEPTION_HANDLED 0x4898068a

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
PVOID VP_ExceptionAddress = (PVOID)(SIZE_T)1;
volatile DWORD VP_State = 1;

LONG NTAPI VP_ExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo) {
  DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
  PVOID address = ExceptionInfo->ExceptionRecord->ExceptionAddress;
  // Skip DBG_* exceptions: FACILITY_DEBUGGER
  if (((code >> 16) & 0xFFF) == 1) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  TRACEF(
      "VP_ExceptionHandler was called. (ExceptionCode: %08x, "
      "ExceptionAddress: %p, Flags: %08x, ThreadId: %d)",
      code, address, ExceptionInfo->ExceptionRecord->ExceptionFlags,
      (UINT)(SIZE_T)VP_NtCurrentThreadId());
  if (VP_State == VP_STATE_RUNNING &&
      VP_ThreadId == (DWORD)(SIZE_T)VP_NtCurrentThreadId()) {
    TRACEF("VP_ExceptionHandler: continue execution.");
    ExceptionInfo->ExceptionRecord->ExceptionCode = VP_EXCEPTION_HANDLED;
    *ExceptionInfo->ContextRecord = VP_Context;
    VP_State = VP_STATE_EXCEPTION;
    VP_ExceptionCode = code;
    VP_ExceptionAddress = address;
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
#define VP_CaptureContext() \
  VP_NtGetContextThread(VP_NtCurrentThread(), &VP_Context)
#else
/*
In 32-bit x86 mode, the ESP register within the function stack frame is not
determined at the beginning. It may change with function calls. Directly calling
NtCurrentThread might cause the compiler to produces different code:

1.
push offset Context
push -2
call NtCurrentThread

2.
mov [esp+0x??], offset Context
mov [esp+0x??], -2
call NtCurrentThread
push reg
push reg

In the first case, the ESP obtained needs a +12 offset, while in the second
case, the ESP only requires a +4 offset. Therefore, using a noinline naked
function ensures that the compiler produces a fixed stack behavior.
*/
#ifdef _MSC_VER
__declspec(naked) void VP_CaptureContext() {
  __asm {
    push offset VP_Context
    push -2  // VP_NtCurrentThread()
    call VP_NtGetContextThread
    ret
  }
}
#else
__declspec(naked) void VP_CaptureContext() {
  __asm__(
      "push %0\n"
      "push $-2\n"  // VP_NtCurrentThread()
      "call *%1\n"
      "ret\n"
      :
      : "r"(&VP_Context), "r"(VP_NtGetContextThread));
}
#endif
#endif

#ifdef _WIN64
#define VP_SetIp(c, ip) c.Rip = (DWORD64)(SIZE_T)(ip)
#define VP_AdjustSp(c) c.Rsp += 8  // + 1 return address size
#else
#define VP_SetIp(c, ip) c.Eip = (DWORD)(SIZE_T)(ip)
#define VP_AdjustSp(c) \
  c.Esp += 16  // + 2 parameters size + 2 return address size
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
    VP_Handle = VP_AddVectoredExceptionHandler(TRUE, VP_ExceptionHandler);     \
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

#define VP_TRY                                         \
  VP_State = VP_STATE_RUNNING;                         \
  VP_ThreadId = (DWORD)(SIZE_T)VP_NtCurrentThreadId(); \
  VP_Context.ContextFlags = CONTEXT_FULL;              \
  VP_CaptureContext();                                 \
  VP_AdjustSp(VP_Context);                             \
  VP_SetIp(VP_Context, VP_GetReturnAddress());         \
  if (VP_State == VP_STATE_RUNNING)

#define VP_CATCH else if (VP_State == VP_STATE_EXCEPTION)

#define VP_END VP_State = VP_STATE_ENDED
