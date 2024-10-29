#pragma once
#include <windows.h>

#include "simplelist.h"

typedef struct {
  volatile HANDLE ReadHandle;
  volatile HANDLE WriteHandle;
  SimpleList *Buffer;
  volatile LONG Lock;
  volatile BOOL IsStopping;
  HANDLE StoppedEvent;
} PipeServer;
