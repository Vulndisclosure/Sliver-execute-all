#include "hooks.h"

#include <process.h>
#include <stdio.h>
#include <stdlib.h>

#include "pipeserver.h"
#include "simplelist.h"

/*==============================================================================
 * Hook Foundation
 *============================================================================*/

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

template <unsigned v>
constexpr unsigned __force_compile_time_result() {
  return v;
}

#define HashCT(s) __force_compile_time_result<__hash(s)>()

template <class CharT>
__forceinline unsigned HashRT(const CharT *s) {
  unsigned h = 0;
  do {
    h = (h >> __shift) | (h << (32 - __shift));
    h += *s;
  } while (*++s);
  return h;
}

// Basic (In)
HookFeatures Features = (HookFeatures)0;
LPSTR CommandLineA = NULL;
LPWSTR CommandLineW = NULL;
HookOutput *Output = NULL;  // owned by LoadedPEImage

// Self Unload (In)
PVOID HookBase = NULL;
PVOID ImageBase = NULL;
#ifdef _WIN64
PVOID FunctionTable = NULL;
#else
PVOID SEH_ImageBase = NULL;
#endif
LoadedPEImage *PEImage = NULL;
HANDLE CompletionEventHandle = NULL;

#define HasFeatures(x) ((Features & (x)) == (x))

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
    SpinLock(Output->JobOutputLock,
             result = WriteFile(Output->JobOutputWriteHandle, str, strCb - 1,
                                NULL, NULL));
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

/*==============================================================================
 * Self Unload
 *============================================================================*/

#ifndef HOOKS_DLL
#include "../selfunload.h"
#endif

HookResult HookControlStopImpl();
void HookControlDeleteImpl();

DWORD WINAPI SelfUnloadProc(LPVOID lpThreadParameter) {
  HANDLE hLastThread = (HANDLE)lpThreadParameter;
  WaitForSingleObject(hLastThread, INFINITE);
  CloseHandle(hLastThread);

  /*
   * 7. Free hook and PE image (continuation of inlineexecutepe.cc)
   */

  TRACEF("7. Free hook and PE image");

  // Do HookControl
  HookControlStopImpl();
  HookControlDeleteImpl();

  // Unload PE image
  PEImage->IsRunning = FALSE;
  if (HasFeatures(HookFeatureCppSEH)) {
#ifdef _WIN64
    RtlDeleteFunctionTable((PRUNTIME_FUNCTION)FunctionTable);
#else
    *(PVOID *)SEH_ImageBase = NULL;
#endif
    TRACEF("SEH was disabled.");
  }
  VirtualFree(ImageBase, 0, MEM_RELEASE);
  TRACEF("The entire PE image was unloaded.");

  // Unload hook
  if (CompletionEventHandle) {
    SetEvent(CompletionEventHandle);
  }
#ifndef HOOKS_DLL
  VirtualFreeAndExitThread(HookBase, (void *)VirtualFree, (void *)ExitThread);
#endif
  return 0;
}

/*==============================================================================
 * Hook Implementations
 *============================================================================*/

/* Command-line related */

LPSTR WINAPI MyGetCommandLineA() {
  TRACEF("GetCommandLineA was called.");
  return CommandLineA;
}

LPWSTR WINAPI MyGetCommandLineW() {
  TRACEF("GetCommandLineW was called.");
  return CommandLineW;
}

/* Stdio related */

PipeServer StdinPipe = {};
PipeServer StdoutPipe = {};
PipeServer StderrPipe = {};

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
    if (Output->JobOutputWriteHandle) {
      SpinLock(Output->JobOutputLock,
               WriteFile(Output->JobOutputWriteHandle, s, len, NULL, NULL));
#ifdef _DEBUG
      char *buffer = (char *)malloc(len + 1);
      if (buffer) {
        memcpy(buffer, s, len);
        buffer[len] = '\0';
        OutputDebugStringA(buffer);
        free(buffer);
      }
#endif
    } else {
      int type =
          HasFeatures(HookFeatureCpp) ? CALLBACK_OUTPUT : CALLBACK_OUTPUT_UTF8;
      if (len >= 1 && s[len - 1] == '\n') {
        len -= 1;
      }
      if (len >= 1 && s[len - 1] == '\r') {
        len -= 1;
      }
      _BeaconPrint(type, "%.*s", len, s);
    }
  }
  return WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite,
                   lpNumberOfBytesWritten, lpOverlapped);
}

/* Hacking for golang cobra command line parser */

DWORD ParentProcessId = 0;

BOOL WINAPI MyProcess32NextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe) {
  // CreateToolhelp32Snapshot will be called twice in
  // https://github.com/spf13/cobra/blob/main/command_win.go
  BOOL b = Process32NextW(hSnapshot, lppe);
  if (!b) {
    return FALSE;
  }
  // TRACEF("Process32NextW was called. (th32ProcessID: %d, szExeFile: %ws)",
  //        lppe->th32ProcessID, lppe->szExeFile);
  if (lppe->th32ProcessID == (DWORD)(SIZE_T)NtCurrentProcessId()) {
    ParentProcessId = lppe->th32ParentProcessID;
  }
  if (lppe->th32ProcessID == ParentProcessId &&
      HashRT(lppe->szExeFile) == HashCT(L"explorer.exe")) {
    lppe->szExeFile[0] = '_';
  }
  return b;
}

/* Thread/Process related */

SimpleList ThreadList = {};
volatile LONG ThreadListLock = 0;

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
  if (!hThread) {
    return NULL;
  }
  DWORD threadId = GetThreadId(hThread);
  SpinLock(ThreadListLock, SimpleListAdd(&ThreadList, DWORD, threadId));
  TRACEF("Thread %d was created at %p with parameter %p.", (UINT)threadId,
         lpStartAddress, lpParameter);
  return hThread;
}

// cgo only
uintptr_t __cdecl Mybeginthread(_In_ void *_StartAddress,
                                _In_ unsigned _StackSize,
                                _In_opt_ void *_ArgList) {
  HANDLE hThread = (HANDLE)_beginthread((_beginthread_proc_type)_StartAddress,
                                        _StackSize, _ArgList);
  if (hThread == NULL || hThread == INVALID_HANDLE_VALUE) {
    return (uintptr_t)hThread;
  }
  DWORD threadId = GetThreadId(hThread);
  SpinLock(ThreadListLock, SimpleListAdd(&ThreadList, DWORD, threadId));
  TRACEF("Thread %d was created at %p with parameter %p. (cgo)", (UINT)threadId,
         _StartAddress, _ArgList);
  return (uintptr_t)hThread;
}

VOID WINAPI MyExitProcess(_In_ UINT uExitCode) {
  TRACEF("ExitProcess was called.");
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
    SpinLock(ThreadListLock, {
      SimpleListForEach(&ThreadList, DWORD, entry, {
        DWORD id = *entry;
        if (id == (DWORD)(SIZE_T)NtCurrentThreadId()) {
          continue;
        }
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
  if (HasFeatures(HookFeatureUnload)) {
    HANDLE hLastThread = OpenThread(THREAD_ALL_ACCESS, FALSE,
                                    (DWORD)(SIZE_T)NtCurrentThreadId());
    HANDLE hSelfUnloadThread = CreateThread(
        NULL, 0, (LPTHREAD_START_ROUTINE)SelfUnloadProc, hLastThread, 0, NULL);
    if (hSelfUnloadThread) {
      CloseHandle(hSelfUnloadThread);
      TRACEF("A new thread was created to self-unload.");
    } else {
      PRINTF_ERR("Can't create a new thread to self-unload.");
    }
  }
  PRINTF("Process exited with code %d.", uExitCode);
  ExitThread(uExitCode);
  __debugbreak();
}

/* System callback related */

SimpleList ExceptionHandlerList = {};

_Ret_maybenull_ PVOID WINAPI MyAddVectoredExceptionHandler(
    _In_ ULONG First, _In_ PVECTORED_EXCEPTION_HANDLER Handler) {
  PVOID handle = AddVectoredExceptionHandler(First, Handler);
  TRACEF(
      "AddVectoredExceptionHandler was called. (First: %d, Handler: %p, "
      "handle: %p)",
      (UINT)First, Handler, handle);
  ExceptionHandlerEntry entry = {EHTYPE_VEH, handle};
  SimpleListAdd(&ExceptionHandlerList, ExceptionHandlerEntry, entry);
  return handle;
}

_Ret_maybenull_ PVOID WINAPI MyAddVectoredContinueHandler(
    _In_ ULONG First, _In_ PVECTORED_EXCEPTION_HANDLER Handler) {
  PVOID handle = AddVectoredContinueHandler(First, Handler);
  TRACEF(
      "AddVectoredContinueHandler was called. (First: %d, Handler: %p, "
      "handle: %p)",
      (UINT)First, Handler, handle);
  ExceptionHandlerEntry entry = {EHTYPE_VCH, handle};
  SimpleListAdd(&ExceptionHandlerList, ExceptionHandlerEntry, entry);
  return handle;
}

BOOL WINAPI MySetConsoleCtrlHandler(_In_opt_ PHANDLER_ROUTINE HandlerRoutine,
                                    _In_ BOOL Add) {
  TRACEF("SetConsoleCtrlHandler was called. (HandlerRoutine: %p, Add: %d)",
         HandlerRoutine, Add);
  return TRUE;
}

/* Allocation related */

SimpleList MemoryList = {};
volatile LONG MemoryListLock = 0;
SimpleList TlsList = {};
volatile LONG TlsListLock = 0;

_Ret_maybenull_ _Post_writable_byte_size_(dwSize)
LPVOID WINAPI MyVirtualAlloc(_In_opt_ LPVOID lpAddress, _In_ SIZE_T dwSize,
                             _In_ DWORD flAllocationType,
                             _In_ DWORD flProtect) {
  LPVOID p = VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
  // TRACEF("VirtualAlloc was called. (%p, %x, %x, %x, %p)", lpAddress,
  //        (UINT)dwSize, (UINT)flAllocationType, (UINT)flProtect, p);
  if (flAllocationType & MEM_RESERVE) {
    SpinLock(MemoryListLock, SimpleListAdd(&MemoryList, LPVOID, p));
    TRACEF("Memory %p was added to list.", p);
  }
  return p;
}

BOOL WINAPI MyVirtualFree(LPVOID lpAddress, _In_ SIZE_T dwSize,
                          _In_ DWORD dwFreeType) {
  BOOL b = VirtualFree(lpAddress, dwSize, dwFreeType);
  // TRACEF("VirtualFree was called. (%p, %x, %x, %d)", lpAddress, dwSize,
  //        dwFreeType, b);
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

/* GetProcAddress */

HMODULE Kernel32Base = NULL;
HMODULE MsvcrtBase = NULL;

FARPROC WINAPI MyGetProcAddress(_In_ HMODULE hModule, _In_ LPCSTR lpProcName) {
  if ((SIZE_T)lpProcName < 0x10000) {
    // TRACEF("GetProcAddress was called. (hModule: %p, nOrdinal: %d)", hModule,
    //        (WORD)(SIZE_T)lpProcName);
    return GetProcAddress(hModule, lpProcName);
  }
  // TRACEF("GetProcAddress was called. (hModule: %p, lpProcName: %s)", hModule,
  //        lpProcName);
  unsigned hash = HashRT(lpProcName);
  if (hModule == Kernel32Base) {
    // Prevent from exiting process (see
    // https://github.com/golang/go/blob/go1.23.0/src/runtime/proc.go#L296)
    if (hash == HashCT("ExitProcess")) {
      return (FARPROC)(PVOID)MyExitProcess;
    }

    // Masquerade command-line
    if (HasFeatures(HookFeatureCommandLine)) {
      if (hash == HashCT("GetCommandLineA")) {
        return (FARPROC)(PVOID)MyGetCommandLineA;
      }
      if (hash == HashCT("GetCommandLineW")) {
        return (FARPROC)(PVOID)MyGetCommandLineW;
      }
    }

    // Redirect stdio
    if (HasFeatures(HookFeatureRedirectStdio) &&
        hash == HashCT("GetStdHandle")) {
      return (FARPROC)(PVOID)MyGetStdHandle;
    }

    // For cobra command line parser (see
    // https://github.com/spf13/cobra/blob/main/command_win.go) (used by frp)
    if (HasFeatures(HookFeatureGoCobraFakeParentProcess) &&
        hash == HashCT("Process32NextW")) {
      return (FARPROC)(PVOID)MyProcess32NextW;
    }

    // Streaming stdout/stderr
    if (HasFeatures(HookFeatureStreamingOutput) &&
        hash == HashCT("WriteFile")) {
      return (FARPROC)(PVOID)MyWriteFile;
    }

    if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
      // Hook CreateThread to collect all go routines
      if (hash == HashCT("CreateThread")) {
        return (FARPROC)(PVOID)MyCreateThread;
      }
      // Hook VEH functions to collect VEHs (see
      // https://github.com/golang/go/blob/go1.23.0/src/runtime/signal_windows.go#L50)
      if (hash == HashCT("AddVectoredExceptionHandler")) {
        return (FARPROC)(PVOID)MyAddVectoredExceptionHandler;
      }
      if (hash == HashCT("AddVectoredContinueHandler")) {
        return (FARPROC)(PVOID)MyAddVectoredContinueHandler;
      }
      // Prevent program from crashing when closing console window (see
      // https://github.com/golang/go/blob/go1.23.0/src/runtime/os_windows.go#L543)
      if (hash == HashCT("SetConsoleCtrlHandler")) {
        return (FARPROC)(PVOID)MySetConsoleCtrlHandler;
      }
      // Track pages allocated from VirtualAlloc
      if (hash == HashCT("VirtualAlloc")) {
        return (FARPROC)(PVOID)MyVirtualAlloc;
      }
      if (hash == HashCT("VirtualFree")) {
        return (FARPROC)(PVOID)MyVirtualFree;
      }
      if (hash == HashCT("TlsAlloc")) {
        return (FARPROC)(PVOID)MyTlsAlloc;
      }
    }

    // Dynamic resolution (e.g. UPX)
    if (hash == HashCT("GetProcAddress")) {
      return (FARPROC)(PVOID)MyGetProcAddress;
    }
  }
  // cgo only
  if (hash == HashCT("_beginthread") && HasFeatures(HookFeatureGo) &&
      hModule == GetModuleHandleA("msvcrt.dll")) {
    return (FARPROC)(PVOID)Mybeginthread;
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

/*==============================================================================
 * Hook Control
 *============================================================================*/

HookResult HookControlCreateImpl(HookFeatures features, PVOID stdin_,
                                 DWORD stdinSize, LPCSTR commandLineA,
                                 LPCWSTR commandLineW, HookOutput *output,
                                 PVOID *myGetProcAddress) {
  Features = features;
  Output = output;

  TRACEF("HookControlCreateImpl was called.");

  // Copy command-line Ansi and Unicode
  if (HasFeatures(HookFeatureCommandLine)) {
    CommandLineA = _strdup(commandLineA);
    if (!CommandLineA) {
      return HookResultErrorDuplicateCommandLine;
    }
    CommandLineW = _wcsdup(commandLineW);
    if (!CommandLineW) {
      return HookResultErrorDuplicateCommandLine;
    }
  }

  // Create pipe servers
  if (HasFeatures(HookFeatureRedirectStdio)) {
    if (!CreateWritePipeServer(&StdinPipe, stdin_, stdinSize) ||
        !CreateReadPipeServer(&StdoutPipe) ||
        !CreateReadPipeServer(&StderrPipe)) {
      return HookResultErrorCreatePipeServer;
    }
  }

  // Allocate new thread list, exception handler list and memory list to support
  // unloading go
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
    if (!SimpleListAlloc(&ThreadList, sizeof(DWORD) * 128)) {
      return HookResultErrorCreateSimpleList;
    }
    if (!SimpleListAlloc(&ExceptionHandlerList,
                         sizeof(ExceptionHandlerEntry) * 8)) {
      return HookResultErrorCreateSimpleList;
    }
    if (!SimpleListAlloc(&MemoryList, sizeof(LPVOID) * 64)) {
      return HookResultErrorCreateSimpleList;
    }
    if (!SimpleListAlloc(&TlsList, sizeof(DWORD) * 8)) {
      return HookResultErrorCreateSimpleList;
    }
  }

  // Get kernel32.dll and msvcrt.dll image base for MyGetProcAddress
  Kernel32Base = GetModuleHandleA("kernel32.dll");
  MsvcrtBase = GetModuleHandleA("msvcrt.dll");
  if (!Kernel32Base || !MsvcrtBase) {
    return HookResultErrorGetModuleHandle;
  }

  // Return detours
  *myGetProcAddress = (PVOID)MyGetProcAddress;

  return HookResultSuccess;
}

void HookControlCreateSelfUnloadImpl(PVOID hookBase, PVOID imageBase, PVOID seh,
                                     LoadedPEImage *peImage,
                                     HANDLE hCompletionEvent) {
  HookBase = hookBase;
  ImageBase = imageBase;
#ifdef _WIN64
  FunctionTable = seh;
#else
  SEH_ImageBase = seh;
#endif
  PEImage = peImage;
  CompletionEventHandle = hCompletionEvent;
}

void HookControlDeleteImpl() {
  TRACEF("HookControlDeleteImpl was called.");

  // Free new thread list, exception handler list and memory list
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
    SimpleListFree(&ThreadList);
    SimpleListFree(&ExceptionHandlerList);
    SimpleListFree(&MemoryList);
    SimpleListFree(&TlsList);
  }

  // Close pipe servers
  if (HasFeatures(HookFeatureRedirectStdio)) {
    CloseWritePipeServer(&StdinPipe);
    CloseReadPipeServer(&StdoutPipe, FALSE);
    CloseReadPipeServer(&StderrPipe, FALSE);
    Output->Stdout = StdoutPipe.Buffer;
    Output->Stderr = StderrPipe.Buffer;
  }

  // Free command-line buffers
  if (HasFeatures(HookFeatureCommandLine)) {
    free(CommandLineA);
    free(CommandLineW);
  }
}

HookResult HookControlStartImpl(void *entryPoint) {
  TRACEF("HookControlStartImpl was called.");

  LPTHREAD_START_ROUTINE runExeEntry = (LPTHREAD_START_ROUTINE)RunExeEntryProc;
  HANDLE hThread;
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
    hThread = MyCreateThread(NULL, 0, runExeEntry, entryPoint, 0, NULL);
  } else {
    hThread = CreateThread(NULL, 0, runExeEntry, entryPoint, 0, NULL);
  }
  if (!hThread) {
    return HookResultErrorStartNewThread;
  }
  CloseHandle(hThread);
  return HookResultSuccess;
}

HookResult HookControlStopImpl() {
  TRACEF("HookControlStopImpl was called.");

  // Remove all top level exception handlers and release memory pages
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
    SimpleListForEach(&ExceptionHandlerList, ExceptionHandlerEntry, eh, {
      PVOID result = NULL;
      if (eh->Type == EHTYPE_VEH) {
        result = (PVOID)(SIZE_T)RemoveVectoredExceptionHandler(eh->Handle);
      } else if (eh->Type == EHTYPE_VCH) {
        result = (PVOID)(SIZE_T)RemoveVectoredContinueHandler(eh->Handle);
      }
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
  if (HasFeatures(HookFeatureUnload)) {
    // TODO: SetUnhandledExceptionFilter does not have a corresponding remove
    // function. If multiple golang programs are executed in memory together,
    // set previous exception filter will fail. (32bit only)
    SetUnhandledExceptionFilter(NULL);
  }

  return HookResultSuccess;
}

#ifdef _MSC_VER
#pragma code_seg(".text$A")
#else
extern "C" __attribute__((section(".text.entry")))
#endif
#ifdef HOOKS_DLL
extern "C" __declspec(dllexport)
#endif
HookResult
HookControlEntry(HookControl id, ...) {
  if (id == HookControlCreate) {
    // Get arguments
    va_list args;
    va_start(args, id);
    HookFeatures features = (HookFeatures)va_arg(args, int);
    PVOID stdin_ = va_arg(args, void *);
    DWORD stdinSize = (DWORD)va_arg(args, unsigned int);
    LPSTR commandLineA = va_arg(args, char *);
    LPWSTR commandLineW = va_arg(args, wchar_t *);
    HookOutput *output = (HookOutput *)va_arg(args, void *);
    PVOID *myGetProcAddress = va_arg(args, void **);
    va_end(args);
    if (!output || !commandLineA || !commandLineW || !myGetProcAddress) {
      return HookResultErrorInvalidArgument;
    }

    // Initialize global variables
    HookResult result =
        HookControlCreateImpl(features, stdin_, stdinSize, commandLineA,
                              commandLineW, output, myGetProcAddress);
    if (result != HookResultSuccess) {
      HookControlDeleteImpl();
      return result;
    }

    return HookResultSuccess;
  } else if (id == HookControlCreateSelfUnload) {
    // Get arguments
    va_list args;
    va_start(args, id);
    PVOID hookBase = va_arg(args, void *);
    PVOID imageBase = va_arg(args, void *);
    PVOID seh = va_arg(args, void *);
    LoadedPEImage *peImage = (LoadedPEImage *)va_arg(args, void *);
    HANDLE hCompletionEvent = (HANDLE)va_arg(args, void *);
    va_end(args);
    if (!imageBase || !peImage) {
      return HookResultErrorInvalidArgument;
    }

    // Initialize global variables (self unload)
    HookControlCreateSelfUnloadImpl(hookBase, imageBase, seh, peImage,
                                    hCompletionEvent);
    return HookResultSuccess;
  } else if (id == HookControlDelete) {
    HookControlDeleteImpl();
    return HookResultSuccess;
  } else if (id == HookControlStart) {
    // Get arguments
    va_list args;
    va_start(args, id);
    void *entryPoint = va_arg(args, void *);
    va_end(args);
    if (!entryPoint) {
      return HookResultErrorInvalidArgument;
    }

    return HookControlStartImpl(entryPoint);
  } else if (id == HookControlStop) {
    return HookControlStopImpl();
  }
  return HookResultErrorInvalidControl;
}
