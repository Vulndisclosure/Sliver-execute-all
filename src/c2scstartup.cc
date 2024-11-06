#include <intrin.h>
#include <stdlib.h>
#include <windows.h>

#include "inlinecommon.h"

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

UINT32 ReadCompressedUInt32(PBYTE *p) {
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

void C2SCInitialize(PVOID __imageBase, PVOID __extra) {
  // Check if already initialized
  PBYTE initialized = (PBYTE)__extra;
  if (*initialized) {
    return;
  }
  PBYTE p = initialized + 1;
  PBYTE imageBase = (PBYTE)__imageBase;

  // Resolve the imports
  PBYTE imports = p;
  TRACEF("Imports: %p\n", imports);
  while (*p) {
    // Read the module name
    LPSTR moduleName = (LPSTR)p;
    PVOID hModule = GetModuleHandleA(moduleName);
    if (!hModule) {
      hModule = LoadLibraryA(moduleName);
    }
    TRACEF("Module %s -> %p\n", moduleName, hModule);
    do {
      p++;
    } while (*p);
    p++;

    // Read the function names and IATs
    do {
      LPSTR functionName = (LPSTR)p;
      if (*p == 0xFF) {
        USHORT ordinal = *(PUSHORT)(p + 1);
        TRACEF("  Ordinal %d -> ", ordinal);
        functionName = (LPSTR)(SIZE_T)ordinal;
        p += 3;
      } else {
        LPSTR name = (LPSTR)p;
        TRACEF("  Name %s -> ", name);
        functionName = name;
        do {
          p++;
        } while (*p);
        p++;
      }
      PVOID function = (PVOID)GetProcAddress((HMODULE)hModule, functionName);
      TRACEF("%p\n", function);

      DWORD iatRVA = ReadCompressedUInt32(&p);
      TRACEF("  IAT RVA 0x%x\n", iatRVA);
      *(PVOID *)(imageBase + iatRVA) = function;
    } while (*p);
    p++;
  }
  p++;

  // Resolve the relocations
  PBYTE relocs = p;
  TRACEF("Relocs: %p\n", relocs);
  while (TRUE) {
    // Read the count and type
    DWORD count = *(PDWORD)p;
    p += 4;
    if (!count) {
      break;
    }
    BYTE type = (BYTE)(count >> 24);
    count &= 0xFFFFFF;
    TRACEF("Reloc type %d count %d\n", type, count);

    // Read the relocations
    DWORD rva = 0;
    while (count--) {
      DWORD deltaRVA = ReadCompressedUInt32(&p);
      rva += deltaRVA;
      TRACEF("  RVA 0x%x\n", rva);
      *(PBYTE *)(imageBase + rva) += (SIZE_T)imageBase;
    }
  }

  *initialized = TRUE;
}
