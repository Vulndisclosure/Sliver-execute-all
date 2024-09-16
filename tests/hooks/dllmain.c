#include "../../hookspub.h"
#include <windows.h>
#pragma comment(linker, "/SECTION:.text,RWE")

extern HookResult HookControlEntry(HookControl id, va_list args);
__declspec(dllexport) HookResult __HookControlEntry(HookControl id, ...) {
  va_list args;
  va_start(args, id);
  HookResult result = HookControlEntry(id, args);
  va_end(args);
  return result;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  return TRUE;
}
