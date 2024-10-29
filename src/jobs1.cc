#include "jobs.h"

JobEntry *RemoveJob(int id, JobEntry **list) {
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

BOOL SimpleRemoveJob(JobEntry **list, int id) {
  if (!list || id == -1) {
    return FALSE;
  }

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
