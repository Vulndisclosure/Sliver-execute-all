#include <stdio.h>
#include <windows.h>

#include "globalshareddata.h"

static void DbgPrint(const char* fmt, ...) {
  char buffer[1000];
  va_list args;
  va_start(args, fmt);
  vsprintf_s(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  OutputDebugStringA(buffer);
}

#ifdef _DEBUG
#define _TRACE
#endif

#ifdef _TRACE
#define TRACEF(a, ...) DbgPrint("[DBG] " a, ##__VA_ARGS__)
#else
#define TRACEF(a, ...)
#endif

#ifndef NtCurrentProcessId
#define NtCurrentProcessId() (((PHANDLE)NtCurrentTeb())[8])
#endif

BOOL ReadBytes(HANDLE hPipe, PVOID buffer, DWORD size) {
  DWORD bytesRead;
  while (size) {
    if (!ReadFile(hPipe, buffer, size, &bytesRead, NULL)) {
      TRACEF("ReadFile failed: %d\n", GetLastError());
      return FALSE;
    }
    buffer = (PBYTE)buffer + bytesRead;
    size -= bytesRead;
  }
  return TRUE;
}

EXTERN_C VOID Main() {
  char pipeName[100] = {};
  HANDLE hPipe = NULL;
  DWORD size = 0;
  PVOID buffer = NULL;
  GlobalSharedData* data = NULL;
  FileServer* file = NULL;

  // Get the pipe name
  sprintf(pipeName, "\\\\.\\pipe\\%d415614",
          (UINT32)(SIZE_T)NtCurrentProcessId());
  TRACEF("Pipe name: %s\n", pipeName);

  // Create the pipe and wait for a connection
  hPipe = CreateNamedPipeA(pipeName, PIPE_ACCESS_INBOUND,
                           PIPE_READMODE_BYTE | PIPE_WAIT, 1, 0, 0, 0, NULL);
  if (hPipe == INVALID_HANDLE_VALUE) {
    TRACEF("CreateNamedPipe failed: %d\n", GetLastError());
    goto err;
  }
  if (!ConnectNamedPipe(hPipe, NULL)) {
    TRACEF("ConnectNamedPipe failed: %d\n", GetLastError());
    goto err;
  }

  // Read the data
  if (!ReadBytes(hPipe, &size, sizeof(size))) goto err;
  buffer = malloc(size);
  if (!buffer) {
    TRACEF("malloc failed\n");
    goto err;
  }
  if (!ReadBytes(hPipe, buffer, size)) goto err;

  // Get the global shared data
  data = GetGlobalSharedData();
  if (!data) {
    TRACEF("GetGlobalSharedData failed\n");
    goto err;
  }
  if (data->File) {
    TRACEF("File already set\n");
    goto err;
  }

  // Set the file server
  file = (FileServer*)malloc(sizeof(FileServer));
  if (!file) {
    TRACEF("malloc failed\n");
    goto err;
  }
  file->Data = buffer;
  file->Size = size;
  file->Self = (PVOID)&Main;
  data->File = file;

  // Clean up
  CloseHandle(hPipe);
  return;

err:
  if (file) free(file);
  if (buffer) free(buffer);
  if (hPipe && hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);
}