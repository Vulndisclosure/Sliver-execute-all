#define JobDescriptionMax 64

typedef struct _JobEntry {
  int JobId;
  PROCESS_INFORMATION __Padding1;
  HANDLE OutputReadHandle;
  HANDLE OutputWriteHandle;
  struct _JobEntry *Next;
} JobEntry;

#ifndef INLINE_WAIT_PE
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

_static _forceinline BOOL IsExecutableRegion(PVOID address,
                                             PMEMORY_BASIC_INFORMATION mbi) {
  if (!address || !mbi)
    return FALSE;

  SIZE_T result = VirtualQuery(address, mbi, sizeof(MEMORY_BASIC_INFORMATION));
  if (!result)
    return FALSE;

  return mbi->State == MEM_COMMIT &&
         (mbi->Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                          PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}

_static BOOL FindExecutableRegion(PVOID hint, PVOID *address, PSIZE_T size) {
  if (!hint || !address || !size)
    return FALSE;

  *address = NULL;
  *size = 0;

  // Check if the hint address is in an executable region
  MEMORY_BASIC_INFORMATION mbi;
  if (!IsExecutableRegion(hint, &mbi))
    return FALSE;

  // Initialize the start and end addresses of the region
  PBYTE regionStart = (PBYTE)mbi.BaseAddress;
  PBYTE regionEnd = regionStart + mbi.RegionSize;

  // Find the start address of the executable region
  PBYTE addr = regionStart;
  while (addr > (PBYTE)0) {
    if (!IsExecutableRegion(addr - 1, &mbi))
      break;
    addr = (PBYTE)mbi.BaseAddress;
    regionStart = addr;
  }

  // Find the end address of the executable region
  addr = regionEnd;
  while (addr <= (PBYTE)MAXLONG_PTR) {
    if (!IsExecutableRegion(addr, &mbi))
      break;

    regionEnd = (PBYTE)mbi.BaseAddress + mbi.RegionSize;
    addr = regionEnd;
  }

  // Return the start address and size of the executable region
  *address = regionStart;
  *size = regionEnd - regionStart;

  return TRUE;
}

_static _forceinline SIZE_T FindPattern(PVOID address, SIZE_T size,
                                        const USHORT *pattern,
                                        SIZE_T patternSize) {
  if (!address || !pattern || patternSize == 0 || size < patternSize)
    return SIZE_MAX;

  PBYTE p = (PBYTE)address;
  for (SIZE_T i = 0; i <= size - patternSize; i++) {
    BOOL found = TRUE;
    for (SIZE_T j = 0; j < patternSize; j++) {
      BYTE wildcardMask = (BYTE)(pattern[j] >> 8);
      BYTE comparand = (BYTE)(pattern[j]);
      if ((p[i + j] & ~wildcardMask) != comparand) {
        found = FALSE;
        break;
      }
    }
    if (found)
      return i;
  }

  return SIZE_MAX;
}

#define DECODE_REL32_OP(op)                                                    \
  (void *)(((char *)(op) + 4) + *(int *)((char *)(op)))
#define DECODE_IMM32_OP(op) (void *)(*(unsigned int *)(op))

_static BOOL FindJobList(PVOID hint, JobEntry ***ppJobList,
                         int **ppJobIdCounter) {
  if (!hint || !ppJobList || !ppJobIdCounter)
    return FALSE;

  *ppJobList = NULL;
  *ppJobIdCounter = NULL;

  PVOID textAddress = NULL;
  SIZE_T textSize = 0;
  BOOL result = FindExecutableRegion(hint, &textAddress, &textSize);
  TRACEF("Beacon text section: hint: %p, start: %p, size: %x", hint,
         textAddress, (UINT)textSize);
  if (!result)
    return FALSE;

#ifdef _WIN64
  USHORT pattern[] = {
      0x8B,   0x05,   0xFF00, 0xFF00, 0xFF00, 0xFF00, 0x48,   0x8B,   0x15,
      0xFF00, 0xFF00, 0xFF00, 0xFF00, 0x89,   0x01,   0xFF,   0xC0,   0x89,
      0x05,   0xFF00, 0xFF00, 0xFF00, 0xFF00, 0x48,   0x85,   0xD2,   0x75,
      0x08,   0x48,   0x89,   0x0D,   0xFF00, 0xFF00, 0xFF00, 0xFF00, 0xC3,
      0x48,   0x8D,   0x02,   0x48,   0x8B,   0x52,   0xFF00, 0x48,   0x85,
      0xD2,   0x75,   0xF4,   0x48,   0x89,   0x48,   0xFF00, 0xC3};
  SIZE_T offset =
      FindPattern(textAddress, textSize, pattern, _countof(pattern));
  if (offset != SIZE_MAX) {
    PBYTE pAddJob = (PBYTE)textAddress + offset;
    *ppJobList = (JobEntry **)DECODE_REL32_OP(pAddJob + 9);
    *ppJobIdCounter = (int *)DECODE_REL32_OP(pAddJob + 2);
    TRACEF("pattern was found: JobList: %p, JobIdCounter: %p", *ppJobList,
           *ppJobIdCounter);
    return TRUE;
  }
  USHORT pattern_rl100k[] = {
      0x48,   0x89,   0x4C,   0x24,   0x08,   0x48,   0x83,   0xEC,   0x18,
      0x48,   0x8B,   0x05,   0xFF00, 0xFF00, 0xFF00, 0xFF00, 0x48,   0x89,
      0x04,   0x24,   0x48,   0xC7,   0x44,   0x24,   0x08,   0x00,   0x00,
      0x00,   0x00,   0x48,   0x8B,   0x44,   0x24,   0x20,   0x8B,   0x0D,
      0xFF00, 0xFF00, 0xFF00, 0xFF00, 0x89,   0x08,   0x8B,   0x05,   0xFF00,
      0xFF00, 0xFF00, 0xFF00, 0xFF,   0xC0,   0x89,   0x05,   0xFF00, 0xFF00,
      0xFF00, 0xFF00, 0x48,   0x83,   0x3D,   0xFF00, 0xFF00, 0xFF00, 0xFF00,
      0x00,   0x75,   0x0E,   0x48,   0x8B,   0x44,   0x24,   0x20,   0x48,
      0x89,   0x05,   0xFF00, 0xFF00, 0xFF00, 0xFF00, 0xEB,   0x2C,   0x48,
      0x83,   0x3C,   0x24,   0x00,   0x74,   0x17,   0x48,   0x8B,   0x04,
      0x24,   0x48,   0x89,   0x44,   0x24,   0x08,   0x48,   0x8B,   0x04,
      0x24,   0x48,   0x8B,   0x40,   0xFF00, 0x48,   0x89,   0x04,   0x24,
      0xEB,   0xE2,   0x48,   0x8B,   0x44,   0x24,   0x08,   0x48,   0x8B,
      0x4C,   0x24,   0x20,   0x48,   0x89,   0x48,   0xFF00, 0x48,   0x83,
      0xC4,   0x18,   0xC3};
  offset = FindPattern(textAddress, textSize, pattern_rl100k,
                       _countof(pattern_rl100k));
  if (offset != SIZE_MAX) {
    PBYTE pAddJob = (PBYTE)textAddress + offset;
    *ppJobList = (JobEntry **)DECODE_REL32_OP(pAddJob + 12);
    *ppJobIdCounter = (int *)DECODE_REL32_OP(pAddJob + 36);
    TRACEF("pattern_rl100k was found: JobList: %p, JobIdCounter: %p",
           *ppJobList, *ppJobIdCounter);
    return TRUE;
  }
#else
  USHORT pattern[] = {
      0x8B,   0x15,   0xFF00, 0xFF00, 0xFF00, 0xFF00, 0xA1,   0xFF00, 0xFF00,
      0xFF00, 0xFF00, 0xFF,   0x05,   0xFF00, 0xFF00, 0xFF00, 0xFF00, 0x89,
      0x11,   0x85,   0xC0,   0x75,   0xFF00, 0x89,   0x0D,   0xFF00, 0xFF00,
      0xFF00, 0xFF00, 0xC3,   0x8B,   0xD0,   0x8B,   0x40,   0xFF00, 0x85,
      0xC0,   0x75,   0xFF00, 0x89,   0x4A,   0xFF00, 0xC3};
  SIZE_T offset =
      FindPattern(textAddress, textSize, pattern, _countof(pattern));
  if (offset != SIZE_MAX) {
    PBYTE pAddJob = (PBYTE)textAddress + offset;
    *ppJobList = (JobEntry **)DECODE_IMM32_OP(pAddJob + 7);
    *ppJobIdCounter = (int *)DECODE_IMM32_OP(pAddJob + 2);
    TRACEF("pattern was found: JobList: %p, JobIdCounter: %p", *ppJobList,
           *ppJobIdCounter);
    return TRUE;
  }
#endif

  TRACEF("JobList and JobIdCounter were not found.");
  return FALSE;
}

_static _forceinline void AddJob_(JobEntry *entry, JobEntry **list) {
  JobEntry *current = *list;
  if (current) {
    JobEntry *previous = NULL;
    do {
      previous = current;
      current = current->Next;
    } while (current);
    previous->Next = entry;
  } else {
    *list = entry;
  }
}

_static _forceinline JobEntry *CreateJobV1(int id, HANDLE hReadPipe,
                                           HANDLE hWritePipe, int type,
                                           const char *description,
                                           JobEntry **list) {
  JobEntryV1 *entry = (JobEntryV1 *)malloc(sizeof(JobEntryV1));
  if (!entry)
    return NULL;
  entry->JobId = id;
  entry->OutputReadHandle = hReadPipe;
  entry->OutputWriteHandle = hWritePipe;
  entry->Next = NULL;
  entry->IsNamedPipe = TRUE;
  entry->IsCompleted = FALSE;
  entry->ProcessId = 0;
  entry->CallbackType = type;
  entry->IsPacket = FALSE;
  for (int i = 0; i < JobDescriptionMax; i++) {
    entry->Description[i] = description[i];
    if (description[i] == '\0')
      break;
  }
  entry->Description[JobDescriptionMax - 1] = '\0';
  TRACEF("JobEntryV1 was created: %p", entry);
  return (JobEntry *)entry;
}

_static _forceinline JobEntry *CreateJobV2(int id, HANDLE hReadPipe,
                                           HANDLE hWritePipe, int type,
                                           const char *description,
                                           JobEntry **list) {
  JobEntryV2 *entry = (JobEntryV2 *)malloc(sizeof(JobEntryV2));
  if (!entry)
    return NULL;
  entry->JobId = id;
  entry->OutputReadHandle = hReadPipe;
  entry->OutputWriteHandle = hWritePipe;
  entry->Next = NULL;
  entry->IsNamedPipe = TRUE;
  entry->IsCompleted = FALSE;
  entry->ProcessId = 0;
  entry->CallbackType = type;
  entry->RequestId = 0;
  entry->IsPacket = FALSE;
  for (int i = 0; i < JobDescriptionMax; i++) {
    entry->Description[i] = description[i];
    if (description[i] == '\0')
      break;
  }
  entry->Description[JobDescriptionMax - 1] = '\0';
  TRACEF("JobEntryV2 was created: %p", entry);
  return (JobEntry *)entry;
}

_static BOOL SimpleAddJob(int version, int type, const char *description,
                          JobEntry ***list, int *id, PHANDLE hWritePipe) {
  *list = NULL;
  *id = -1;
  *hWritePipe = NULL;

  // Find JobList and JobIdCounter by pattern matching
  JobEntry **jobList = NULL;
  int *jobIdCounter = NULL;
  if (!FindJobList((PVOID)&BeaconPrintf, &jobList, &jobIdCounter))
    return FALSE;

  // Create a pipe for the job
  HANDLE hJobReadPipe = NULL;
  HANDLE hJobWritePipe = NULL;
  if (!CreatePipe(&hJobReadPipe, &hJobWritePipe, NULL, 0)) {
    TRACEF("Failed to create a pipe.");
    return FALSE;
  }

  // Create a job entry
  int jobId = (*jobIdCounter)++;
  JobEntry *entry;
  if (version == 1)
    entry = CreateJobV1(jobId, hJobReadPipe, hJobWritePipe, type, description,
                        jobList);
  else if (version == 2)
    entry = CreateJobV2(jobId, hJobReadPipe, hJobWritePipe, type, description,
                        jobList);
  else
    entry = NULL;
  if (!entry) {
    TRACEF("Failed to create a job entry.");
    CloseHandle(hJobReadPipe);
    CloseHandle(hJobWritePipe);
    return FALSE;
  }
  TRACEF("JobEntry was created: %d", jobId);

  // Add the job to the job list
  AddJob_(entry, jobList);
  *list = jobList;
  *id = jobId;
  *hWritePipe = hJobWritePipe;
  return TRUE;
}
#endif

_static _forceinline JobEntry *RemoveJob(int id, JobEntry **list) {
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

_static _forceinline BOOL SimpleRemoveJob(JobEntry **list, int id) {
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
