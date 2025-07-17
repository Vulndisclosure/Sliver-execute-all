#include "exguard.h"
#include "hooks.h"
#include "pipeserver.h"
#include "simplelist.h"
#include "vfs.h"

typedef int(WINAPI *ExeEntryProc)(void);

/*
 * Exe entry-point wrapper
 */

static DWORD WINAPI RunExeEntryProc(LPVOID lpThreadParameter) {
  TRACEF("RunExeEntryProc was called. (entryPoint: %p)", lpThreadParameter);
  ((ExeEntryProc)lpThreadParameter)();
  TRACEF("Unreachable.");
  ExitThread(-1);
  return -1;
}

/*==============================================================================
 * Hook Control
 *============================================================================*/

static HookResult HookControlCreateImpl(HookFeatures features, PVOID stdin_,
                                        DWORD stdinSize, LPCSTR commandLineA,
                                        LPCWSTR commandLineW,
                                        HookOutput *output,
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
  // unloading go and exception guard
  if (HasFeatures(HookFeatureUnload | HookFeatureGo) ||
      HasFeatures(HookFeatureExceptionGuard)) {
    if (!SimpleListAlloc(&ThreadList, sizeof(DWORD) * 128)) {
      return HookResultErrorCreateSimpleList;
    }
  }
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
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

static void HookControlCreateSelfUnloadImpl(PVOID hookBase, PVOID imageBase,
                                            PVOID seh, LoadedPEImage *peImage,
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

static void HookControlDeleteImpl() {
  TRACEF("HookControlDeleteImpl was called.");

  // Free new thread list, exception handler list and memory list
  if (HasFeatures(HookFeatureUnload | HookFeatureGo) ||
      HasFeatures(HookFeatureExceptionGuard)) {
    SimpleListFree(&ThreadList);
  }
  if (HasFeatures(HookFeatureUnload | HookFeatureGo)) {
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

static HookResult HookControlStartImpl(void *entryPoint) {
  TRACEF("HookControlStartImpl was called.");

  if (HasFeatures(HookFeatureExceptionGuard)) {
    InitializeExceptionGuard();
  }
  LPTHREAD_START_ROUTINE runExeEntry = (LPTHREAD_START_ROUTINE)RunExeEntryProc;
  HANDLE hThread;
  if (HasFeatures(HookFeatureUnload | HookFeatureGo) ||
      HasFeatures(HookFeatureExceptionGuard)) {
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

static HookResult HookControlStopImpl() {
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
  if (HasFeatures(HookFeatureUnload) ||
      HasFeatures(HookFeatureExceptionGuard)) {
    // TODO: SetUnhandledExceptionFilter does not have a corresponding remove
    // function. If multiple golang programs are executed in memory together,
    // set previous exception filter will fail. (32bit only)
    // If exception guard is enabled, we should also unset the filter.
    if (HasFeatures(HookFeatureExceptionGuard)) {
      decltype(SetUnhandledExceptionFilter) *pRtlSetUnhandledExceptionFilter =
          (decltype(SetUnhandledExceptionFilter) *)(PVOID)GetProcAddress(
              GetModuleHandleA("ntdll.dll"), "RtlSetUnhandledExceptionFilter");
      if (pRtlSetUnhandledExceptionFilter) {
        pRtlSetUnhandledExceptionFilter(NULL);
      }
      RemoveVectoredContinueHandler((PVOID)ExGuard_ExceptionHandler);
    }
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
    HookVirtualFile *virtualFiles = va_arg(args, HookVirtualFile *);
    DWORD virtualFileCount = (DWORD)va_arg(args, unsigned int);
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

    // Initialize the virtual file system
    HasVFS = virtualFiles && virtualFileCount &&
             HasFeatures(HookFeatureVirtualFileSystem);
    if (HasVFS && !VfsInitialize(virtualFiles, virtualFileCount)) {
      HookControlDeleteImpl();
      VfsUninitialize(NULL, NULL);
      return HookResultErrorInitializeVirtualFileSystem;
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
    // } else if (id == HookControlDelete) {
    //   HookControlDeleteImpl();
    //   return HookResultSuccess;
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
    // } else if (id == HookControlStop) {
    //   return HookControlStopImpl();
  }
  return HookResultErrorInvalidControl;
}

/*==============================================================================
 * Self Unload
 *============================================================================*/

#ifndef HOOKS_DLL
#include "../selfunload.h"
#endif

DWORD WINAPI SelfUnloadProc(LPVOID lpThreadParameter) {
  HANDLE hLastThread = (HANDLE)lpThreadParameter;
  WaitForSingleObject(hLastThread, INFINITE);
  CloseHandle(hLastThread);

  /*
   * 7. Free hook and PE image (continuation of inlineexecutepe.cc)
   */

  TRACEF("7. Free hook and PE image");

  // Do HookControl
  if (HasVFS) {
    VfsUninitialize(&PEImage->VirtualFiles, &PEImage->VirtualFileCount);
  }
  HookControlStopImpl();
  HookControlDeleteImpl();
  if (HasFeatures(HookFeatureRedirectStdio)) {
    TRACEF("PE image result: Stdout: %d, Stderr: %d, VirtualFiles: %d",
           Output->Stdout->Size, Output->Stderr->Size,
           PEImage->VirtualFileCount);
  } else {
    TRACEF("PE image result: VirtualFiles: %d", PEImage->VirtualFileCount);
  }

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
  if (Output->JobOutputWriteHandle) {
    // Only when the job api is available, the client will automatically receive
    // the logs.
    PRINTF("Mark to trigger inline-log-pe: 0x%p", ImageBase);
  }

  // Unload hook
  if (CompletionEventHandle) {
    SetEvent(CompletionEventHandle);
  }
#ifndef HOOKS_DLL
  VirtualFreeAndExitThread(HookBase, (void *)VirtualFree, (void *)ExitThread);
#endif
  return 0;
}
