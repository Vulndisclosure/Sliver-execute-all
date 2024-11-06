#pragma once
#include <windows.h>

#include "hooks/hookspub.h"

/*==============================================================================
 * Helpers
 *============================================================================*/

#ifndef NtCurrentProcessId
#define NtCurrentProcessId() (((PHANDLE)NtCurrentTeb())[8])
#endif

/*==============================================================================
 * Global Shared Data
 *============================================================================*/

#define LoadedPEImageMax 128

typedef struct {
  LoadedPEImage PEImages[LoadedPEImageMax];
  PVOID LastImageBase;
#ifndef _WIN64
  PVOID *SEH_ImageBaseList;
#endif
} GlobalSharedData;

GlobalSharedData *GetGlobalSharedData();
