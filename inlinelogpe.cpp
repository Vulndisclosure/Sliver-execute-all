#ifdef _DEBUG
#define xorstr_(s) s
#else
#include "xorstr.hpp"
#endif

extern "C" {
#include "inlinelogpe.hpp"

#include "globalshareddata.c"

void InlineLogPE(void *imageBase) {
  TRACEF("Begin fetch log.");

  HookContext *hookCtx = NULL;
  PVOID jobList = NULL;
  GlobalSharedData *sharedData = NULL;
  int index = 0;

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
  jobList = sharedData->PEImages[index].JobList;

  /*
   * Copied from inlineexecutepe.cpp
   */

  /*
   * 6. Print async log
   */

  TRACEF("6. Print async log");

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
  } else {
    PRINTF_ERR("Do NOT call 'inline-log-pe' when the PE image was already "
               "added to the job list.");
  }

out:
  TRACEF("End fetch log.");
}

#ifdef BOF
void go(char *argv, int argc) {
  // Parse arguments
  datap parser;
  BeaconDataParse(&parser, argv, argc);
  int imageBaseStrSize = 0;
  char *imageBaseStr = BeaconDataExtract(&parser, &imageBaseStrSize);
  void *imageBase = (void *)(size_t)_strtoui64(imageBaseStr, NULL, 16);
  TRACEF("Arguments was parsed. (imageBase: %p)", imageBase);

  InlineLogPE(imageBase);
}
#endif
}
