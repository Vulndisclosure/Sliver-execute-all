#pragma once
#include <windows.h>

#include "hooks/hookspub.h"

typedef struct {
  PVOID Data;
  DWORD Size;
  PVOID Self;
} FileServer;

#define LoadedPEImageMax 64

typedef struct {
  LoadedPEImage PEImages[LoadedPEImageMax];
  PVOID LastImageBase;
#ifndef _WIN64
  PVOID *SEH_ImageBaseList;
#endif
  FileServer *File;
} GlobalSharedData;

GlobalSharedData *GetGlobalSharedData();
