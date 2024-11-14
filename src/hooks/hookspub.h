#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "simplelist.h"

typedef enum {
  HookFeatureRedirectStdio = 0x01,
  HookFeatureStreamingOutput = 0x02,
  HookFeatureCommandLine = 0x04,
  HookFeatureUnload = 0x08,
  HookFeatureVirtualFileSystem = 0x10,

  HookFeatureCpp = 0x0100,
  HookFeatureCppSEH = 0x0200,

  HookFeatureGo = 0x010000,
  HookFeatureGoCobraFakeParentProcess = 0x020000
} HookFeatures;

typedef enum {
  HookVirtualFileInput,
  HookVirtualFileOutput,
  HookVirtualFileOutputPattern
} HookVirtualFileType;

// Represents a virtual file storage or virtual file pattern
typedef struct {
  WCHAR FileName[MAX_PATH];  // Can be a pattern if it is a output file
  HookVirtualFileType Type;
  SimpleList Data;  // Empty if it is a output file or output file pattern
  volatile LONG Lock;
} HookVirtualFile;

// Owned by LoadedPEImage
typedef struct {
  SimpleList *Stdout;           // Moved from StdoutPipe.Buffer
  SimpleList *Stderr;           // Moved from StderrPipe.Buffer
  HANDLE JobOutputWriteHandle;  // Real-time output using job api
  volatile LONG JobOutputLock;
  SimpleList Logs;  // Real-time output fallback when job api unavailable
  volatile LONG LogLock;
  int Type;  // CALLBACK_OUTPUT, CALLBACK_OUTPUT_UTF8
} HookOutput;

typedef struct {
  PVOID ImageBase;
  BOOL IsRunning;
  PVOID JobList;
  int JobId;
  HookOutput *Output;
  HookVirtualFile *VirtualFiles;  // Moved from VirtualFileList
  DWORD VirtualFileCount;
  LPSTR CommandLineA;  // For display only
} LoadedPEImage;

typedef struct {
  int Type;
  int Size;
  char *Text;
} LogEntry;

typedef enum _HookControl {
  HookControlCreate = 1,
  HookControlCreateSelfUnload,
  HookControlDelete,
  HookControlStart,
  HookControlStop
} HookControl;

typedef enum _HookResult {
  HookResultErrorInvalidControl = (int)0x80000000,
  HookResultErrorInvalidArgument,
  HookResultErrorNotYetCreated,
  HookResultErrorDuplicateCommandLine,
  HookResultErrorCreatePipeServer,
  HookResultErrorCreateSimpleList,
  HookResultErrorGetModuleHandle,
  HookResultErrorInitializeVirtualFileSystem,
  HookResultErrorStartNewThread,

  HookResultSuccess = 0,
} HookResult;

typedef HookResult (*HookControlEntryProc)(HookControl id, ...);
