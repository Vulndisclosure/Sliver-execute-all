#include "pipeserver.h"

BOOL CreatePipeServer(PipeServer *pipe, void *buffer, int size,
                      LPTHREAD_START_ROUTINE proc) {
  // Fill pipe server structure
  PipeServer t = {};
  HANDLE hThread = NULL;
  if (!CreatePipe((PHANDLE)&t.ReadHandle, (PHANDLE)&t.WriteHandle, NULL, 0))
    goto err;
  if (!(t.Buffer = (SimpleList *)malloc(sizeof(SimpleList))))
    goto err;
  if (!SimpleListAlloc(t.Buffer, size))
    goto err;
  if (buffer && !SimpleListAddBuffer(t.Buffer, buffer, size))
    goto err;
  t.StoppedEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
  if (!t.StoppedEvent)
    goto err;

  // Create read/write background thread
  *pipe = t;
  hThread = CreateThread(NULL, 0, proc, pipe, 0, NULL);
  if (!hThread) {
    *pipe = {};
    goto err;
  }
  CloseHandle(hThread);
  return TRUE;

err:
  if (t.StoppedEvent)
    CloseHandle(t.StoppedEvent);
  SimpleListFree(t.Buffer);
  if (t.Buffer)
    free(t.Buffer);
  if (t.WriteHandle)
    CloseHandle(t.WriteHandle);
  if (t.ReadHandle)
    CloseHandle(t.ReadHandle);
  return FALSE;
}

DWORD WINAPI ReadPipeServerProc(LPVOID lpThreadParameter) {
  PipeServer *pipe = (PipeServer *)lpThreadParameter;

  BOOL result = FALSE;
  do {
    // Read data from the pipe
    char buffer[2048];
    DWORD bytesRead = 0;
    result =
        ReadFile(pipe->ReadHandle, buffer, sizeof(buffer), &bytesRead, NULL);
    if (!result) {
      if (pipe->IsStopping)
        TRACEF("Failed to ReadFile in ReadPipeServerProc because pipe server "
               "is stopping.");
      else
        PRINTF_ERR(
            "Failed to ReadFile in ReadPipeServerProc. (size: %d, error: %x)",
            bytesRead, (UINT)NtLastError());
    }

    // Copy data to buffer
    if (bytesRead)
      SpinLock(pipe->Lock,
               SimpleListAddBuffer(pipe->Buffer, buffer, bytesRead));
  } while (result);

  SetEvent(pipe->StoppedEvent);

  return 0;
}

/*
_forceinline BOOL CreateReadPipeServer(PipeServer *pipe) {
  return CreatePipeServer(pipe, NULL, 0x1000, ReadPipeServerProc);
}
*/
_forceinline BOOL CreateReadPipeServer(PipeServer *pipe, void *proc) {
  return CreatePipeServer(pipe, NULL, 0x1000, (LPTHREAD_START_ROUTINE)proc);
}

void CloseReadPipeServer(PipeServer *pipe, BOOL freeBuffer) {
  // Close write handle first to let ReadFile returns then close read handle
  (pipe)->IsStopping = TRUE;
  if (pipe->WriteHandle) {
    CloseHandle(pipe->WriteHandle);
    pipe->WriteHandle = NULL;
  }
  if (pipe->ReadHandle) {
    CloseHandle(pipe->ReadHandle);
    pipe->ReadHandle = NULL;
  }

  // Wait for ReadPipeServerProc to complete
  if (pipe->StoppedEvent) {
    WaitForSingleObject(pipe->StoppedEvent, INFINITE);
    pipe->StoppedEvent = NULL;
  }

  // Free internal buffer
  if (freeBuffer) {
    SimpleListFree(pipe->Buffer);
    free(pipe->Buffer);
    pipe->Buffer = NULL;
  }
}

DWORD WINAPI WritePipeServerProc(LPVOID lpThreadParameter) {
  PipeServer *pipe = (PipeServer *)lpThreadParameter;

  // Write data to the pipe
  DWORD bytesWritten = 0;
  BOOL result = WriteFile(pipe->WriteHandle, pipe->Buffer->Buffer,
                          pipe->Buffer->Size, &bytesWritten, NULL);
  if (!result) {
    if (pipe->IsStopping)
      TRACEF("Failed to WriteFile in WritePipeServerProc because pipe server "
             "is stopping.");
    else
      PRINTF_ERR("Failed to WriteFile in WritePipeServerProc. (size: %d vs %d, "
                 "error: %x)",
                 pipe->Buffer->Size, bytesWritten, (UINT)NtLastError());
  }

  SetEvent(pipe->StoppedEvent);

  return 0;
}

/*
_forceinline BOOL CreateWritePipeServer(PipeServer *pipe, void *buffer,
                                         int size) {
  return CreatePipeServer(pipe, buffer, size, WritePipeServerProc);
}
*/
_forceinline BOOL CreateWritePipeServer(PipeServer *pipe, void *buffer,
                                        int size, void *proc) {
  return CreatePipeServer(pipe, buffer, size, (LPTHREAD_START_ROUTINE)proc);
}

void CloseWritePipeServer(PipeServer *pipe) {
  // Close read handle first to let WriteFile returns then close write handle
  pipe->IsStopping = TRUE;
  if (pipe->ReadHandle) {
    CloseHandle(pipe->ReadHandle);
    pipe->ReadHandle = NULL;
  }
  if (pipe->WriteHandle) {
    CloseHandle(pipe->WriteHandle);
    pipe->WriteHandle = NULL;
  }

  // Wait for WritePipeServerProc to complete
  if (pipe->StoppedEvent) {
    WaitForSingleObject(pipe->StoppedEvent, INFINITE);
    pipe->StoppedEvent = NULL;
  }

  // Free internal buffer
  SimpleListFree(pipe->Buffer);
  free(pipe->Buffer);
  pipe->Buffer = NULL;
}
