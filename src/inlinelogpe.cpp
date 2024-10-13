#ifdef _DEBUG
#define xorstr_(s) s
#else
#include "xorstr.hpp"
#endif

extern "C" {
#include "inlinecommon.hpp"

#include "globalshareddata.c"
#include "jobs1.c"
#include "simplelist1.c"

void InlineLogPE(void *imageBase) {
  TRACEF("Begin fetch log.");

  GlobalSharedData *sharedData = NULL;
  int index = 0;
  LoadedPEImage *peImage = NULL;
  JobEntry **jobList = NULL;
  int jobId = 0;
  HookOutput *output = NULL;

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
  peImage = &sharedData->PEImages[index];
  jobList = (JobEntry **)peImage->JobList;
  jobId = peImage->JobId;
  output = peImage->Output;

  TRACEF("Print async log");

  SpinLock(output->LogLock, {
    SimpleListForEach(&output->Logs, LogEntry, log, {
      if (!log->Text)
        continue;
      _BeaconPrint(log->Type, xorstr_("%.*s"), log->Size, log->Text);
      free(log->Text);
      log->Text = NULL;
      // TODO: Trim list?
    });
  });

  TRACEF("Print stdout/stderr");

  if (peImage->IsRunning) {
    PRINTF("The PE image %p is still running. Skip printing stdout/stderr.",
           imageBase);
    goto out;
  }
  if (output->Stdout) {
    _BeaconPrint(output->Type, xorstr_("Stdout: %.*s"), output->Stdout->Size,
                 (char *)output->Stdout->Buffer);
    SimpleListFree(output->Stdout);
  }
  if (output->Stderr) {
    _BeaconPrint(output->Type, xorstr_("Stderr: %.*s"), output->Stderr->Size,
                 (char *)output->Stderr->Buffer);
    SimpleListFree(output->Stderr);
  }
  SimpleListFree(&output->Logs);
  free(output);
  if (jobList) {
    SimpleRemoveJob(jobList, jobId);
    TRACEF("Job %d was removed.", jobId);
  }
  *peImage = {};
  TRACEF("PE image %p was removed from global shared data.", imageBase);

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
