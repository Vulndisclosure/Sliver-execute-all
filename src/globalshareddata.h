#pragma once
#include <windows.h>

#include "hooks/hookspub.h"

#define LoadedPEImageMax 64

typedef struct {
  LoadedPEImage PEImages[LoadedPEImageMax];
  PVOID LastImageBase;
#ifndef _WIN64
  PVOID *SEH_ImageBaseList;
#endif
} GlobalSharedData;

GlobalSharedData *GetGlobalSharedData();
