#include <windows.h>
#pragma comment(linker, "/SECTION:.text,RWE")

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  return TRUE;
}
