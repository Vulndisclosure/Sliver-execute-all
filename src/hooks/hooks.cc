#include "hooks.h"

#include <process.h>
#include <stdio.h>
#include <stdlib.h>

#include "hash.h"

/*==============================================================================
 * Variables
 *============================================================================*/

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

/*==============================================================================
 * Hook Implementations
 *============================================================================*/

/* Command-line related */

static LPSTR WINAPI MyGetCommandLineA() {
  TRACEF("GetCommandLineA was called.");
  return CommandLineA;
}

static LPWSTR WINAPI MyGetCommandLineW() {
  TRACEF("GetCommandLineW was called.");
  return CommandLineW;
}

/* Stdio related */

PipeServer StdinPipe = {};
PipeServer StdoutPipe = {};
PipeServer StderrPipe = {};

static HANDLE WINAPI MyGetStdHandle(_In_ DWORD nStdHandle) {
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

static BOOL WINAPI MyWriteFile(_In_ HANDLE hFile,
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

static BOOL WINAPI MyProcess32NextW(HANDLE hSnapshot, LPPROCESSENTRY32W lppe) {
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
static uintptr_t __cdecl Mybeginthread(_In_ void *_StartAddress,
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

static VOID WINAPI MyExitProcess(_In_ UINT uExitCode) {
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

static _Ret_maybenull_ PVOID WINAPI MyAddVectoredExceptionHandler(
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

static _Ret_maybenull_ PVOID WINAPI MyAddVectoredContinueHandler(
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

static BOOL WINAPI MySetConsoleCtrlHandler(
    _In_opt_ PHANDLER_ROUTINE HandlerRoutine, _In_ BOOL Add) {
  TRACEF("SetConsoleCtrlHandler was called. (HandlerRoutine: %p, Add: %d)",
         HandlerRoutine, Add);
  return TRUE;
}

/* Allocation related */

SimpleList MemoryList = {};
volatile LONG MemoryListLock = 0;
SimpleList TlsList = {};
volatile LONG TlsListLock = 0;

_Ret_maybenull_ _Post_writable_byte_size_(dwSize) static LPVOID WINAPI
    MyVirtualAlloc(_In_opt_ LPVOID lpAddress, _In_ SIZE_T dwSize,
                   _In_ DWORD flAllocationType, _In_ DWORD flProtect) {
  LPVOID p = VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
  // TRACEF("VirtualAlloc was called. (%p, %x, %x, %x, %p)", lpAddress,
  //        (UINT)dwSize, (UINT)flAllocationType, (UINT)flProtect, p);
  if (flAllocationType & MEM_RESERVE) {
    SpinLock(MemoryListLock, SimpleListAdd(&MemoryList, LPVOID, p));
    TRACEF("Memory %p was added to list.", p);
  }
  return p;
}

static BOOL WINAPI MyVirtualFree(LPVOID lpAddress, _In_ SIZE_T dwSize,
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

static DWORD WINAPI MyTlsAlloc() {
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
