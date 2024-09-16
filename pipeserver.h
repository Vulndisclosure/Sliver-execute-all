#pragma once
#include "simplelist.h"
#include <windows.h>

typedef struct {
  volatile HANDLE ReadHandle;
  volatile HANDLE WriteHandle;
  SimpleList Buffer;
  volatile LONG Lock;
  volatile BOOL IsStopping;
  HANDLE StoppedEvent;
} PipeServer;
