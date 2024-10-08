#include "globalshareddata.h"

_static GlobalSharedData *GetGlobalSharedData() {
  // Create the key of shared data
  const char time[9] = /*__TIME__*/ "12:34:56";
  DWORD key = (DWORD)(SIZE_T)NtCurrentProcessId();
  for (size_t i = 0; i < sizeof(time); i++) {
    key = _rotr(key, 17);
    key += time[i];
  }
  char keyStr[9] = {};
  _ultoa(key, keyStr, 16);

  // Get or create shared data
  GlobalSharedData *data = NULL;
  char valueStr[17] = {};
  if (GetEnvironmentVariableA(keyStr, valueStr, sizeof(valueStr))) {
    SIZE_T value = (SIZE_T)_strtoui64(valueStr, NULL, 16);
    data = (GlobalSharedData *)(value ^ key);
  } else {
    while (!(data = (GlobalSharedData *)malloc(sizeof(GlobalSharedData))))
      ;
    memset(data, 0, sizeof(GlobalSharedData));
    SIZE_T value = (SIZE_T)data ^ key;
    _ui64toa((ULONG64)value, valueStr, 16);
    SetEnvironmentVariableA(keyStr, valueStr);
  }

  return data;
}
