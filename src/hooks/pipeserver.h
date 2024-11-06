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

BOOL CreateReadPipeServer(PipeServer *pipe);
void CloseReadPipeServer(PipeServer *pipe, BOOL freeBuffer);
BOOL CreateWritePipeServer(PipeServer *pipe, void *buffer, int size);
void CloseWritePipeServer(PipeServer *pipe);
