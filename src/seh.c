#ifdef _WIN64
PVOID GetFunctionTable(PVOID imageBase, PDWORD size) {
  PIMAGE_NT_HEADERS ntHeaders =
      (PIMAGE_NT_HEADERS)((PBYTE)imageBase +
                          ((PIMAGE_DOS_HEADER)imageBase)->e_lfanew);
  PIMAGE_DATA_DIRECTORY exceptionDirectory =
      &ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
  PIMAGE_RUNTIME_FUNCTION_ENTRY functionTable =
      (PIMAGE_RUNTIME_FUNCTION_ENTRY)((PBYTE)imageBase +
                                      exceptionDirectory->VirtualAddress);
  if (size)
    *size = exceptionDirectory->Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY);
  return (PVOID)functionTable;
}

PVOID EnableSEH(PVOID imageBase) {
  DWORD size = 0;
  PRUNTIME_FUNCTION functionTable =
      (PRUNTIME_FUNCTION)GetFunctionTable(imageBase, &size);
  size /= sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY);
  RtlAddFunctionTable(functionTable, size, (DWORD64)imageBase);
  return functionTable;
}
#else
#define ImageBaseListMax 16

// See seh.x86.asm
const unsigned char NtQueryVirtualMemoryHook[169] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x55, 0x8B, 0xEC, 0x53, 0xFF, 0x75, 0x1C, 0xFF,
    0x75, 0x18, 0xFF, 0x75, 0x14, 0xFF, 0x75, 0x10, 0xFF, 0x75, 0x0C, 0xFF,
    0x75, 0x08, 0xE8, 0x44, 0x00, 0x00, 0x00, 0x83, 0xF8, 0x00, 0x7C, 0x3A,
    0x83, 0x7D, 0x08, 0xFF, 0x75, 0x34, 0x83, 0x7D, 0x10, 0x00, 0x75, 0x2E,
    0xE8, 0x00, 0x00, 0x00, 0x00, 0x5B, 0x81, 0xC3, 0x8F, 0xFF, 0xFF, 0xFF,
    0x8B, 0x55, 0x14, 0x8B, 0x52, 0x04, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x3B,
    0x14, 0x8B, 0x75, 0x0C, 0x8B, 0x55, 0x14, 0xC7, 0x42, 0x18, 0x00, 0x00,
    0x00, 0x01, 0xEB, 0x06, 0x41, 0x83, 0xF9, 0x10, 0x72, 0xE9, 0x5B, 0xC9,
    0xC2, 0x18, 0x00, 0xB8, 0x00, 0x00, 0x00, 0x00, 0xE9, 0x05, 0x00, 0x00,
    0x00};

#define NtQueryVirtualMemoryHook_ImageBaseList 0
#define NtQueryVirtualMemoryHook_Detour 0x40
#define NtQueryVirtualMemoryHook_DetourCallRel32 0x56
#define NtQueryVirtualMemoryHook_TrampolineMovEax 0x9F
#define NtQueryVirtualMemoryHook_TrampolineJmpRel32 0xA4

#define ENCODE_JMP_REL32(from, to) (int)((char *)(to) - ((char *)(from) + 5))
#define DECODE_JMP_REL32(from)                                                 \
  (void *)(((char *)(from) + 5) + *(int *)((char *)(from) + 1))

// Hook NtQueryVirtualMemory to bypass RtlIsValidHandler
bool InstallNtQueryVirtualMemoryHook(PVOID **imageBaseList) {
  // Get ntdll!NtQueryVirtualMemory
  PVOID target =
      GetProcAddress2(GetOrLoadDll2("ntdll.dll"), "NtQueryVirtualMemory");
  if (!target) {
    PRINTF_ERR("Failed to get ntdll!NtQueryVirtualMemory");
    return false;
  }

  // Read original first instruction
  union {
    ULONG64 Value;
    BYTE Bytes[8];
  } instruction = {};
  instruction.Value = *(PULONG64)target;
  if (instruction.Bytes[0] != 0xB8 && instruction.Bytes[0] != 0xE9) {
    PRINTF_ERR("Not supported first instruction: %llx", instruction.Value);
    return false;
  }

  // Allocate memory to store hook code
  PVOID p = VirtualAlloc(NULL, sizeof(NtQueryVirtualMemoryHook),
                         MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  if (!p) {
    PRINTF_ERR("Failed to allocate NtQueryVirtualMemory hook code.");
    return false;
  }
  memcpy(p, NtQueryVirtualMemoryHook, sizeof(NtQueryVirtualMemoryHook));

  // Reloc for hook code
  if (instruction.Bytes[0] == 0xB8) {
    // First instruction is 'mov eax, imm32'
    TRACEF(
        "First instruction of ntdll!NtQueryVirtualMemory is 'mov eax, imm32'.");
    PBYTE myMovEax = (PBYTE)p + NtQueryVirtualMemoryHook_TrampolineMovEax;
    *(PUINT)(myMovEax + 1) = *(PUINT)(instruction.Bytes + 1);
    PBYTE myJmpRel32 = (PBYTE)p + NtQueryVirtualMemoryHook_TrampolineJmpRel32;
    *(PINT)(myJmpRel32 + 1) = ENCODE_JMP_REL32(myJmpRel32, (PBYTE)target + 5);
  } else if (instruction.Bytes[0] == 0xE9) {
    // First instruction is 'jmp rel32' (already hooked)
    TRACEF("First instruction of ntdll!NtQueryVirtualMemory is 'jmp rel32' "
           "(already hooked).");
    PVOID existingDetour = DECODE_JMP_REL32(target);
    PBYTE myCallRel32 = (PBYTE)p + NtQueryVirtualMemoryHook_DetourCallRel32;
    *(PINT)(myCallRel32 + 1) = ENCODE_JMP_REL32(myCallRel32, existingDetour);
  }

  // Set new first instruction bytes
  PVOID myDetour = (PBYTE)p + NtQueryVirtualMemoryHook_Detour;
  instruction.Bytes[0] = 0xE9;
  *(PINT)(instruction.Bytes + 1) = ENCODE_JMP_REL32(target, myDetour);

  // Apply the hook
  DWORD oldProtect = 0;
  VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
  *(PULONG64)target = instruction.Value;
  VirtualProtect(target, 5, oldProtect, &oldProtect);
  NtFlushInstructionCache((HANDLE)-1, NULL, 0);

  PRINTF("Hook of ntdll!NtQueryVirtualMemory was installed. (p: %p)", p);
  if (imageBaseList)
    *imageBaseList =
        (PVOID *)((PBYTE)p + NtQueryVirtualMemoryHook_ImageBaseList);
  return true;
}

PVOID EnableSEH(PVOID imageBase) {
  // Remove SafeSEH data directory if C++ x86
  PIMAGE_NT_HEADERS ntHeaders =
      (PIMAGE_NT_HEADERS)((PBYTE)imageBase +
                          ((PIMAGE_DOS_HEADER)imageBase)->e_lfanew);
  PIMAGE_DATA_DIRECTORY loadConfigDirectory =
      &ntHeaders->OptionalHeader
           .DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG];
  loadConfigDirectory->VirtualAddress = 0;
  loadConfigDirectory->Size = 0;
  TRACEF("Load config data directory was zeroed.");

  // Install NtQueryVirtualMemory hook
  GlobalSharedData *data = GetGlobalSharedData();
  if (!data->SEH_ImageBaseList) {
    TRACEF("Begin to install NtQueryVirtualMemory hook.");
    if (!InstallNtQueryVirtualMemoryHook((PVOID **)&data->SEH_ImageBaseList))
      return NULL;
  }

  // Add to ImageBaseList
  for (int i = 0; i < ImageBaseListMax; i++) {
    if (!data->SEH_ImageBaseList[i]) {
      data->SEH_ImageBaseList[i] = imageBase;
      TRACEF("SEH_ImageBaseList[%d] = %p", i, imageBase);
      return &data->SEH_ImageBaseList[i];
    }
  }

  PRINTF_ERR("Too many loaded PE images. Can't add to SEH_ImageBaseList.");
  return NULL;
}
#endif
