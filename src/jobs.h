#pragma once
#include <windows.h>

#define JobDescriptionMax 64

typedef struct _JobEntry {
  int JobId;
  PROCESS_INFORMATION __Padding1;
  HANDLE OutputReadHandle;
  HANDLE OutputWriteHandle;
  struct _JobEntry *Next;
} JobEntry;

typedef struct _JobEntryV1 {
  int JobId;
  PROCESS_INFORMATION Process;
  HANDLE OutputReadHandle;
  HANDLE OutputWriteHandle;
  struct _JobEntryV1 *Next;
  short IsNamedPipe;
  short IsCompleted;
  DWORD ProcessId;
  int CallbackType;
  short IsPacket;
  char Description[JobDescriptionMax];
} JobEntryV1;

// Add RequestId (4.9+)
// COMMAND_TASK_CB = 114
// https://www.cobaltstrike.com/blog/cobalt-strike-49-take-me-to-your-loader
typedef struct _JobEntryV2 {
  int JobId;
  PROCESS_INFORMATION Process;
  HANDLE OutputReadHandle;
  HANDLE OutputWriteHandle;
  struct _JobEntryV2 *Next;
  short IsNamedPipe;
  short IsCompleted;
  DWORD ProcessId;
  int CallbackType;
  int RequestId;
  short IsPacket;
  char Description[JobDescriptionMax];
} JobEntryV2;
