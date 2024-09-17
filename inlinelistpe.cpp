#ifdef _DEBUG
#define xorstr_(s) s
#else
#include "xorstr.hpp"
#endif

extern "C" {
#include "inlinelistpe.hpp"

#include "globalshareddata.c"

void InlineListPE() {
  TRACEF("Begin list loaded PE images.");

  PRINTF("Loaded PE images:");
  GlobalSharedData *data = GetGlobalSharedData();
  for (int i = 0; i < LoadedPEImageMax; i++) {
    LoadedPEImage *peImage = &data->PEImages[i];
    if (!peImage->ImageBase)
      continue;

    HookContext *ctx;
    SpinLock(peImage->HookContextLock, ctx = peImage->HookContext);
    int logs = 0;
    SpinLock(peImage->Output->LogLock, {
      SimpleListForEach(&peImage->Output->Logs, LogEntry, log, {
        if (log->Text)
          logs++;
      });
    });

    if (ctx)
      PRINTF("%d: ImageBase: %p, Features: %x, Available logs: %d, IsRunning: "
             "%d, CommandLine: %s, JobId: %d",
             i, peImage->ImageBase, ctx->Features, logs, peImage->IsRunning,
             ctx->CommandLineA, peImage->JobId);
    else
      PRINTF("%d: ImageBase: %p (unloaded), Available logs: %d, IsRunning: %d, "
             "JobId: %d",
             i, peImage->ImageBase, logs, peImage->IsRunning, peImage->JobId);
  }

  TRACEF("End list loaded PE images.");
}

#ifdef BOF
void go(char *argv, int argc) { InlineListPE(); }
#endif
}
