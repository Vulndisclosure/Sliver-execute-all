#ifdef _DEBUG
#define xorstr_(s) s
#else
#include "xorstr.hpp"
#endif

extern "C" {
#include "inlinecommon.hpp"
#include <string.h>

#include "globalshareddata.c"

void InlineListPE() {
  TRACEF("Begin list loaded PE images.");

  PRINTF("Loaded PE images:");
  GlobalSharedData *data = GetGlobalSharedData();
  for (int i = 0; i < LoadedPEImageMax; i++) {
    LoadedPEImage *peImage = &data->PEImages[i];
    if (!peImage->ImageBase)
      continue;

    // Copy hook context
    BOOL hasCtx = FALSE;
    HookFeatures features = (HookFeatures)0;
    char *commandLineA = NULL;
    SpinLock(peImage->HookContextLock, {
      HookContext *ctx = peImage->HookContext;
      if (ctx) {
        hasCtx = TRUE;
        features = ctx->Features;
        if (ctx->CommandLineA)
          commandLineA = _strdup(ctx->CommandLineA);
      }
    });

    // Count the number of logs
    int logs = 0;
    SpinLock(peImage->Output->LogLock, {
      SimpleListForEach(&peImage->Output->Logs, LogEntry, log, {
        if (log->Text)
          logs++;
      });
    });

    // Print the information
    if (hasCtx)
      PRINTF("%d: ImageBase: %p, Features: %x, Available logs: %d, IsRunning: "
             "%d, CommandLine: %s, JobId: %d",
             i, peImage->ImageBase, features, logs, peImage->IsRunning,
             commandLineA, peImage->JobId);
    else
      PRINTF("%d: ImageBase: %p (unloaded), Available logs: %d, IsRunning: %d, "
             "JobId: %d",
             i, peImage->ImageBase, logs, peImage->IsRunning, peImage->JobId);

    if (commandLineA)
      free(commandLineA);
  }

  TRACEF("End list loaded PE images.");
}

#ifdef BOF
void go(char *argv, int argc) { InlineListPE(); }
#endif
}
