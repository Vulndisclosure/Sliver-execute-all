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

_forceinline JobEntry *RemoveJob(int id, JobEntry **list) {
  JobEntry **previous = list;
  JobEntry *current = *list;
  while (current) {
    if (current->JobId == id) {
      *previous = current->Next;
      return current;
    }
    previous = &current->Next;
    current = current->Next;
  }
  return NULL;
}

_forceinline BOOL SimpleRemoveJob(JobEntry **list, int id) {
  if (!list || id == -1)
    return FALSE;

  // Remove the job from the job list
  JobEntry *entry = RemoveJob(id, list);
  if (!entry) {
    TRACEF("JobEntry was not found: %d", id);
    return FALSE;
  }

  // Close the pipe handles
  CloseHandle(entry->OutputWriteHandle);
  CloseHandle(entry->OutputReadHandle);
  free(entry);
  TRACEF("JobEntry was removed: %d", id);
  return TRUE;
}
