#define INLINE_WAIT_PE

#ifdef _DEBUG
#define xorstr_(s) s
#else
#include "xorstr.hpp"
#endif

extern "C" {
#include "inlinewaitpe.hpp"

#include "globalshareddata.c"
#include "jobs.c"
#include "seh.c"

HookResult InlineWaitPE(void *imageBase, int waitingTimeoutMs) {
  TRACEF("Begin wait pe.");

  int runningTimeoutMs = waitingTimeoutMs;
  HookContext *hookCtx = NULL;
  HookControlEntryProc hookControlEntry = NULL;
  JobEntry **jobList = NULL;
  int jobId = -1;
  HookResult result = HookResultSuccess;
  GlobalSharedData *sharedData = NULL;
  int index = 0;
  int outputType = 0;
  SimpleList *stdout_ = NULL;
  SimpleList *stderr_ = NULL;

  sharedData = GetGlobalSharedData();
  if (!imageBase) {
    imageBase = sharedData->LastImageBase;
    PRINTF("Use the last image base %p.", imageBase);
  }
  for (index = 0; index < LoadedPEImageMax; index++) {
    if (sharedData->PEImages[index].ImageBase == imageBase)
      break;
  }
  if (index == LoadedPEImageMax) {
    PRINTF_ERR("Can't find %p in PEImages.", imageBase);
    goto out;
  }
  hookCtx = sharedData->PEImages[index].HookContext;
  hookControlEntry = (HookControlEntryProc)hookCtx->HookControlEntry;
  jobList = (JobEntry **)sharedData->PEImages[index].JobList;
  jobId = sharedData->PEImages[index].JobId;

  /*
   * Copied from inlineexecutepe.cpp
   */

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

out:
  TRACEF("End wait pe.");

  return result;
}

#ifdef BOF
void go(char *argv, int argc) {
  // Parse arguments
  datap parser;
  BeaconDataParse(&parser, argv, argc);
  int imageBaseStrSize = 0;
  char *imageBaseStr = BeaconDataExtract(&parser, &imageBaseStrSize);
  void *imageBase = (void *)(size_t)_strtoui64(imageBaseStr, NULL, 16);
  int waitingTimeoutMs = BeaconDataInt(&parser);
  TRACEF("Arguments was parsed. (imageBase: %p, waitingTimeoutMs: %d)",
         imageBase, waitingTimeoutMs);

  InlineWaitPE(imageBase, waitingTimeoutMs);
}
#endif
}
