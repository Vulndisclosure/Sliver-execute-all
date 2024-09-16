#pragma once
#include "hookspub.h"
#include <stdlib.h>
#include <windows.h>

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
  PVOID ImageBase;
  HookContext *HookContext;
  BOOL IsRunning;
  PVOID JobList;
  int JobId;
} LoadedPEImage;

typedef struct {
  LoadedPEImage PEImages[LoadedPEImageMax];
  PVOID LastImageBase;
#ifndef _WIN64
  PVOID *SEH_ImageBaseList;
#endif
} GlobalSharedData;
