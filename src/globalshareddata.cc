#include "globalshareddata.h"

#include <memory.h>
#include <windows.h>

#ifdef _WIN64
// Maximum user space address in Windows 7 x64 is 0x000007FF`FFFFFFFF
#define GLOBAL_SHARED_DATA_ADDRESS 0x00000242F6070000
#define GLOBAL_SHARED_DATA_MARKER 0x414A2A2E6294742E
#define MAX_ATTEMPTS 5
#else
#define GLOBAL_SHARED_DATA_ADDRESS 0x3CF30000
#define GLOBAL_SHARED_DATA_MARKER 0x599077D2
#define MAX_ATTEMPTS 10
#endif

/*
 * IMPORTANT: VirtualAlloc allocates memory at 64KB boundaries even though page
 * granularity is 4KB!
 * See also: https://devblogs.microsoft.com/oldnewthing/20031008-00/?p=42223
 */

static_assert(((SIZE_T)GLOBAL_SHARED_DATA_ADDRESS & 0xFFFF) == 0, "");
static_assert(sizeof(SIZE_T) + sizeof(GlobalSharedData) < 0x1000, "");

GlobalSharedData *GetGlobalSharedData() {
  // Try to find the shared data
  PVOID address = (PVOID)GLOBAL_SHARED_DATA_ADDRESS;
  PVOID candidateAddress = NULL;
  for (int i = 0; i < MAX_ATTEMPTS; i++) {
    // If lpAddress specifies an address above the highest memory address
    // accessible to the process, the function fails with
    // ERROR_INVALID_PARAMETER.
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(address, &mbi, sizeof(mbi))) {
      return NULL;
    }
    if (mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READWRITE &&
        mbi.Type == MEM_PRIVATE && mbi.RegionSize == 0x1000 &&
        *(PSIZE_T)mbi.BaseAddress == GLOBAL_SHARED_DATA_MARKER) {
      return (GlobalSharedData *)((PBYTE)mbi.BaseAddress + sizeof(SIZE_T));
    }
    if (!candidateAddress && mbi.State == MEM_FREE) {
      candidateAddress = mbi.BaseAddress;
    }
    address = (PVOID)((SIZE_T)mbi.BaseAddress + 0x10000);
  }

  // Try to allocate the shared data
  if (!candidateAddress) {
#ifdef _DEBUG
    __debugbreak();
#endif
    return NULL;
  }
  address = VirtualAlloc(candidateAddress, 0x1000, MEM_COMMIT | MEM_RESERVE,
                         PAGE_READWRITE);
#ifdef _DEBUG
  if (address != candidateAddress) {
    __debugbreak();
  }
#endif
  if (!address) {
    return NULL;
  }

  // Initialize the shared data
  memset(address, 0, sizeof(SIZE_T) + sizeof(GlobalSharedData));
  *(PSIZE_T)address = GLOBAL_SHARED_DATA_MARKER;
  return (GlobalSharedData *)((PBYTE)address + sizeof(SIZE_T));
}
