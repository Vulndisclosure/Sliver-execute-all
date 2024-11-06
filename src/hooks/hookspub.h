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

  HookFeatureCpp = 0x0100,
  HookFeatureCppSEH = 0x0200,

  HookFeatureGo = 0x010000,
  HookFeatureGoCobraFakeParentProcess = 0x020000
} HookFeatures;

// The HookOutput should be both allocated and freed outside of the hooks
typedef struct {
  SimpleList *Stdout;
  SimpleList *Stderr;
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
  LPSTR CommandLineA;  // For display only
} LoadedPEImage;

typedef struct {
  int Type;
  int Size;
  char *Text;
} LogEntry;

#define EHTYPE_VEH 1
#define EHTYPE_VCH 2
// #define EHTYPE_UEF 3

typedef struct {
  int Type;
  PVOID Handle;
} ExceptionHandlerEntry;

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
  HookResultErrorStartNewThread,

  HookResultSuccess = 0,
} HookResult;

typedef HookResult (*HookControlEntryProc)(HookControl id, ...);
