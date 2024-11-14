extern "C" {
// clang-format off
#include "inlinecommon.h"
// clang-format on

#include <string.h>

#include "globalshareddata.cc"

void InlineListPE() {
  TRACEF("Begin list loaded PE images.");

  PRINTF("Loaded PE images:");
  GlobalSharedData *data = GetGlobalSharedData();
  for (int i = 0; i < LoadedPEImageMax; i++) {
    LoadedPEImage *peImage = &data->PEImages[i];
    if (!peImage->ImageBase) {
      continue;
    }

    // Count the number of logs
    int logs = 0;
    SpinLock(peImage->Output->LogLock, {
      SimpleListForEach(&peImage->Output->Logs, LogEntry, log,
                        if (log->Text) logs++;);
    });

    // Print the information
    if (peImage->IsRunning) {
      PRINTF("%d: ImageBase: %p, Logs: %d, CommandLine: %s, JobId: %d", i,
             peImage->ImageBase, logs, peImage->CommandLineA, peImage->JobId);
    } else {
      PRINTF(
          "%d: ImageBase: %p (unloaded), Logs: %d, CommandLine: %s, JobId: %d, "
          "Stdout: %d, Stderr: %d, VirtualFiles: %d",
          i, peImage->ImageBase, logs, peImage->CommandLineA, peImage->JobId,
          peImage->Output->Stdout->Size, peImage->Output->Stderr->Size,
          peImage->VirtualFileCount);
    }
  }

  TRACEF("End list loaded PE images.");
}

#ifdef BOF
void go(char *argv, int argc) { InlineListPE(); }
#endif
}
