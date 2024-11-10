#include <intrin.h>
#include <stdlib.h>
#include <windows.h>

#ifdef BOF
#include "inlinecommon.h"
#endif

/*==============================================================================
 * NT Def
 *============================================================================*/

typedef struct _UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWCH Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

typedef struct _LDR_DATA_TABLE_ENTRY {
  LIST_ENTRY InLoadOrderLinks;
  LIST_ENTRY InMemoryOrderLinks;
  LIST_ENTRY InInitializationOrderLinks;
  PVOID DllBase;
  PVOID EntryPoint;
  ULONG SizeOfImage;
  UNICODE_STRING FullDllName;
  UNICODE_STRING BaseDllName;
  ULONG Flags;
  USHORT ObsoleteLoadCount;
  USHORT TlsIndex;
  LIST_ENTRY HashLinks;
  ULONG TimeDateStamp;
  PVOID EntryPointActivationContext;
  PVOID Lock;  // RtlAcquireSRWLockExclusive
} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

typedef struct _PEB_LDR_DATA {
  ULONG Length;
  BOOLEAN Initialized;
  HANDLE SsHandle;
  LIST_ENTRY InLoadOrderModuleList;
  LIST_ENTRY InMemoryOrderModuleList;
  LIST_ENTRY InInitializationOrderModuleList;
  PVOID EntryInProgress;
  BOOLEAN ShutdownInProgress;
  HANDLE ShutdownThreadId;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _PEB {
  BOOLEAN InheritedAddressSpace;
  BOOLEAN ReadImageFileExecOptions;
  BOOLEAN BeingDebugged;
  BOOLEAN BitField;
  HANDLE Mutant;
  PVOID ImageBaseAddress;
  PPEB_LDR_DATA Ldr;
  PVOID ProcessParameters;
  PVOID SubSystemData;
  PVOID ProcessHeap;
  PRTL_CRITICAL_SECTION FastPebLock;
  PSLIST_HEADER AtlThunkSListPtr;
  PVOID IFEOKey;
} PEB, *PPEB;

#ifdef _M_IX86
#define NtCurrentPeb() ((PPEB)__readfsdword(0x30))
#elif _M_AMD64
#define NtCurrentPeb() ((PPEB)__readgsqword(0x60))
#elif _M_ARM
#define NtCurrentPeb() (*(PPEB *)(_MoveFromCoprocessor(15, 0, 13, 0, 2) + 0x30))
#elif _M_ARM64
#define NtCurrentPeb() (*(PPEB *)(__getReg(18) + 0x60))
#else
#error "This architecture is currently unsupported"
#endif

typedef HMODULE(WINAPI *GetModuleHandleAProc)(LPCSTR lpModuleName);
typedef HMODULE(WINAPI *LoadLibraryAProc)(LPCSTR lpLibFileName);
typedef FARPROC(WINAPI *GetProcAddressProc)(HMODULE hModule, LPCSTR lpProcName);
typedef BOOL(WINAPI *VirtualQueryProc)(LPCVOID lpAddress,
                                       PMEMORY_BASIC_INFORMATION lpBuffer,
                                       SIZE_T dwLength);
typedef BOOL(WINAPI *VirtualProtectProc)(LPVOID lpAddress, SIZE_T dwSize,
                                         DWORD flNewProtect,
                                         PDWORD lpflOldProtect);

/*==============================================================================
 * Hash
 *============================================================================*/

#ifndef HASH_KEY
#define HASH_KEY 17
#endif

#define HASH_ROUND(h, c) (_rotr((h), HASH_KEY) + (c))
#define HASH_1(s, h) HASH_ROUND((h), s[0])
#define HASH_2(s, h) HASH_ROUND(HASH_1(s, h), s[1])
#define HASH_3(s, h) HASH_ROUND(HASH_2(s, h), s[2])
#define HASH_4(s, h) HASH_ROUND(HASH_3(s, h), s[3])
#define HASH_5(s, h) HASH_ROUND(HASH_4(s, h), s[4])
#define HASH_6(s, h) HASH_ROUND(HASH_5(s, h), s[5])
#define HASH_7(s, h) HASH_ROUND(HASH_6(s, h), s[6])
#define HASH_8(s, h) HASH_ROUND(HASH_7(s, h), s[7])
#define HASH_9(s, h) HASH_ROUND(HASH_8(s, h), s[8])
#define HASH_10(s, h) HASH_ROUND(HASH_9(s, h), s[9])
#define HASH_11(s, h) HASH_ROUND(HASH_10(s, h), s[10])
#define HASH_12(s, h) HASH_ROUND(HASH_11(s, h), s[11])
#define HASH_13(s, h) HASH_ROUND(HASH_12(s, h), s[12])
#define HASH_14(s, h) HASH_ROUND(HASH_13(s, h), s[13])
#define HASH_15(s, h) HASH_ROUND(HASH_14(s, h), s[14])
#define HASH_16(s, h) HASH_ROUND(HASH_15(s, h), s[15])
#define HASH_17(s, h) HASH_ROUND(HASH_16(s, h), s[16])
#define HASH_18(s, h) HASH_ROUND(HASH_17(s, h), s[17])
#define HASH_19(s, h) HASH_ROUND(HASH_18(s, h), s[18])
#define HASH_20(s, h) HASH_ROUND(HASH_19(s, h), s[19])
#define HASH_21(s, h) HASH_ROUND(HASH_20(s, h), s[20])
#define HASH_22(s, h) HASH_ROUND(HASH_21(s, h), s[21])
#define HASH_23(s, h) HASH_ROUND(HASH_22(s, h), s[22])
#define HASH_24(s, h) HASH_ROUND(HASH_23(s, h), s[23])

#define KERNEL32DLL_HASH HASH_12("KERNEL32.DLL", 0)

#define GETMODULEHANDLEA_HASH HASH_16("GetModuleHandleA", 0)
#define LOADLIBRARYA_HASH HASH_12("LoadLibraryA", 0)
#define GETPROCADDRESS_HASH HASH_14("GetProcAddress", 0)
#define VIRTUALQUERY_HASH HASH_12("VirtualQuery", 0)
#define VIRTUALPROTECT_HASH HASH_14("VirtualProtect", 0)

/*==============================================================================
 * Startup
 *============================================================================*/

#ifdef _MSC_VER
#define bswap16(x) _byteswap_ushort(x)
#define bswap32(x) _byteswap_ulong(x)
#else
#define bswap16(x) __builtin_bswap16((x))
#define bswap32(x) __builtin_bswap32((x))
#endif

#ifdef BOF
// #define __DPRINTF TRACEF
#define __DPRINTF(...)
#else
#ifndef __Locator
#ifdef _MSC_VER
#define __Locator() _ReturnAddress()
#else
#define __Locator() __builtin_return_address(0)
#endif
#endif

#ifndef __DPRINTF
// #define __DPRINTF(...) printf(__VA_ARGS__)
#define __DPRINTF(...)
#endif
#endif

#ifdef BOF
UINT32 ReadCompressedUInt32(PBYTE *p) {
#else
static UINT32 ReadCompressedUInt32(PBYTE *p) {
#endif
  PBYTE q = *p;
  UINT32 a = *q;
  if (!(a & 0x80)) {
    *p = q + 1;
    return a;
  } else if (!(a & 0x40)) {
    a = bswap16(*(PUINT16)q & 0xFF3F);
    *p = q + 2;
    return a;
  } else {
    a = bswap32(*(PUINT32)q & 0xFFFFFF1F);
    *p = q + 4;
    return a;
  }
}

#ifdef BOF
void C2SCInitialize(PVOID __imageBase, PVOID __extra) {
  // Check if already initialized
  PBYTE initialized = (PBYTE)__extra;
  if (*initialized) {
    return;
  }
  PBYTE p = initialized + 1;
  PBYTE imageBase = (PBYTE)__imageBase;
#else
DECLSPEC_NOINLINE VOID __fastcall Initialize(SIZE_T __imageBase,
                                             SIZE_T __extra) {
  // Check if already initialized
  PBYTE __locator = (PBYTE)__Locator();
  PBYTE initialized = __locator + __extra;
  if (*initialized) {
    return;
  }
  PBYTE p = initialized + 1;
  PBYTE imageBase = __locator - __imageBase;

  // Get library loader APIs from PEB
  PBYTE hKernel32 = NULL;
  PPEB peb = NtCurrentPeb();
  for (PLIST_ENTRY listHead = &peb->Ldr->InLoadOrderModuleList,
                   listEntry = listHead->Flink;
       listEntry != listHead; listEntry = listEntry->Flink) {
    PLDR_DATA_TABLE_ENTRY entry =
        CONTAINING_RECORD(listEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
    __DPRINTF("BaseDllName: %wZ\n", &entry->BaseDllName);

    // Get kernel32.dll
    DWORD hash = 0;
    for (USHORT i = 0; i < entry->BaseDllName.Length / 2; i++) {
      hash = _rotr(hash, HASH_KEY);
      WCHAR c = entry->BaseDllName.Buffer[i];
      hash += c >= 'a' ? (c - 0x20) : c;
    }
    if (hash == KERNEL32DLL_HASH) {
      __DPRINTF("kernel32.dll was found\n");
      hKernel32 = (PBYTE)entry->DllBase;
      break;
    }
  }

  // Walk the export table and get the required APIs
  GetModuleHandleAProc GetModuleHandleA = NULL;
  LoadLibraryAProc LoadLibraryA = NULL;
  GetProcAddressProc GetProcAddress = NULL;
  VirtualQueryProc VirtualQuery = NULL;
  VirtualProtectProc VirtualProtect = NULL;
  PIMAGE_NT_HEADERS ntHeaders =
      (PIMAGE_NT_HEADERS)(hKernel32 + ((PIMAGE_DOS_HEADER)hKernel32)->e_lfanew);
  DWORD rva =
      ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
          .VirtualAddress;
  PIMAGE_EXPORT_DIRECTORY exportTable =
      (PIMAGE_EXPORT_DIRECTORY)(hKernel32 + rva);
  PDWORD names = (PDWORD)(hKernel32 + exportTable->AddressOfNames);
  for (DWORD i = 0; i < exportTable->NumberOfNames; i++) {
    // Hash the function name
    DWORD hash = 0;
    for (PCHAR c = (PCHAR)(hKernel32 + names[i]); *c; c++) {
      hash = _rotr(hash, HASH_KEY);
      hash += *c;
    }

    // Check if the hash matches the API
    PVOID *api = NULL;
    if (hash == GETMODULEHANDLEA_HASH) {
      __DPRINTF("GetModuleHandleA was found: %d\n", i);
      api = (PVOID *)&GetModuleHandleA;
    } else if (hash == LOADLIBRARYA_HASH) {
      __DPRINTF("LoadLibraryA was found: %d\n", i);
      api = (PVOID *)&LoadLibraryA;
    } else if (hash == GETPROCADDRESS_HASH) {
      __DPRINTF("GetProcAddress was found: %d\n", i);
      api = (PVOID *)&GetProcAddress;
    } else if (hash == VIRTUALQUERY_HASH) {
      __DPRINTF("VirtualQuery was found: %d\n", i);
      api = (PVOID *)&VirtualQuery;
    } else if (hash == VIRTUALPROTECT_HASH) {
      __DPRINTF("VirtualProtect was found: %d\n", i);
      api = (PVOID *)&VirtualProtect;
    } else {
      continue;
    }

    // Get the API address
    PWORD ordinals = (PWORD)(hKernel32 + exportTable->AddressOfNameOrdinals);
    PDWORD functions = (PDWORD)(hKernel32 + exportTable->AddressOfFunctions);
    *api = (PVOID)(hKernel32 + functions[ordinals[i]]);
    if (GetModuleHandleA && LoadLibraryA && GetProcAddress && VirtualQuery &&
        VirtualProtect) {
      __DPRINTF("All APIs were found: %d\n", i);
      __DPRINTF("GetModuleHandleA: %p\n", GetModuleHandleA);
      __DPRINTF("LoadLibraryA: %p\n", LoadLibraryA);
      __DPRINTF("GetProcAddress: %p\n", GetProcAddress);
      __DPRINTF("VirtualQuery: %p\n", VirtualQuery);
      __DPRINTF("VirtualProtect: %p\n", VirtualProtect);
      break;
    }
  }

  // TODO: Can I set using RWX memory in the cna script?
  MEMORY_BASIC_INFORMATION mbi;
  if (VirtualQuery(imageBase, &mbi, sizeof(mbi)) &&
      mbi.Protect != PAGE_EXECUTE_READWRITE) {
    DWORD oldProtect;
    VirtualProtect(imageBase, __extra - __imageBase, PAGE_EXECUTE_READWRITE,
                   &oldProtect);
  }
#endif

  // Resolve the imports
  PBYTE imports = p;
  __DPRINTF("Imports: %p\n", imports);
  while (*p) {
    // Read the module name
    LPSTR moduleName = (LPSTR)p;
    PVOID hModule = GetModuleHandleA(moduleName);
    if (!hModule) {
      hModule = LoadLibraryA(moduleName);
    }
    __DPRINTF("Module %s -> %p\n", moduleName, hModule);
    do {
      p++;
    } while (*p);
    p++;

    // Read the function names and IATs
    do {
      LPSTR functionName = (LPSTR)p;
      if (*p == 0xFF) {
        USHORT ordinal = *(PUSHORT)(p + 1);
        __DPRINTF("  Ordinal %d -> ", ordinal);
        functionName = (LPSTR)(SIZE_T)ordinal;
        p += 3;
      } else {
        LPSTR name = (LPSTR)p;
        __DPRINTF("  Name %s -> ", name);
        functionName = name;
        do {
          p++;
        } while (*p);
        p++;
      }
      PVOID function = (PVOID)GetProcAddress((HMODULE)hModule, functionName);
      __DPRINTF("%p\n", function);

      DWORD iatRVA = ReadCompressedUInt32(&p);
      __DPRINTF("  IAT RVA 0x%x\n", iatRVA);
      *(PVOID *)(imageBase + iatRVA) = function;
    } while (*p);
    p++;
  }
  p++;

  // Resolve the relocations
  PBYTE relocs = p;
  __DPRINTF("Relocs: %p\n", relocs);
  while (TRUE) {
    // Read the count and type
    DWORD count = *(PDWORD)p;
    p += 4;
    if (!count) {
      break;
    }
    BYTE type = (BYTE)(count >> 24);
    count &= 0xFFFFFF;
    __DPRINTF("Reloc type %d count %d\n", type, count);

    // Read the relocations
    DWORD rva = 0;
    while (count--) {
      DWORD deltaRVA = ReadCompressedUInt32(&p);
      rva += deltaRVA;
      __DPRINTF("  RVA 0x%x\n", rva);
      *(PBYTE *)(imageBase + rva) += (SIZE_T)imageBase;
    }
  }

  *initialized = TRUE;
}

#ifndef BOF
EXTERN_C VOID Main();

#ifdef _MSC_VER
#pragma code_seg(".text$A")
EXTERN_C
#else
EXTERN_C __attribute__((section(".text.entry")))
#endif
VOID Entry() {
  Initialize(0x11111111, 0x22222222);
  Main();
}
#endif
