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
    const char *unloaded = !peImage->IsRunning ? " (unloaded)" : "";
    PRINTF(
        "%d: ImageBase: %p%s, Available logs: %d, IsRunning: %d, CommandLine: "
        "%s, JobId: %d",
        i, peImage->ImageBase, unloaded, logs, peImage->IsRunning,
        peImage->CommandLineA, peImage->JobId);
  }

  TRACEF("End list loaded PE images.");
}

#ifdef BOF
void go(char *argv, int argc) { InlineListPE(); }
#endif
}
