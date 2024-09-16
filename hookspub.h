#pragma once
#include "pipeserver.h"
#include "simplelist.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef enum {
  HookFeatureRedirectStdio = 0x01,
  HookFeatureStreamingOutput = 0x02,
  HookFeatureCommandLine = 0x04,
  HookFeatureUnload = 0x08,

  HookFeatureCpp = 0x0100,
  HookFeatureCppSEH = 0x0200,

  HookFeatureGo = 0x010000,
  HookFeatureGoCobraFakeParentProcess = 0x020000
} HookFeatures;

typedef struct {
  int Type;
  int Size;
  char *Text;
} LogEntry;

#define EHTYPE_VEH 1
#define EHTYPE_VCH 2
//#define EHTYPE_UEF 3

typedef struct {
  int Type;
  PVOID Handle;
} ExceptionHandlerEntry;

typedef struct {
  /*
   * Fields
   */

  HookFeatures Features;

  PVOID HookControlEntry;

  SimpleList Logs;
  volatile LONG LogLock;

  HANDLE JobOutputReadHandle;
  HANDLE JobOutputWriteHandle;
  volatile LONG JobOutputLock;

  HANDLE ProcessExitedEvent;
  volatile HANDLE ProcessExitedThreadHandle;

  char *CommandLineA;
  WCHAR *CommandLineW;

  DWORD ParentProcessId;

  PipeServer StdinPipe;
  PipeServer StdoutPipe;
  PipeServer StderrPipe;

  SimpleList ThreadList;
  volatile LONG ThreadListLock;
  SimpleList ExceptionHandlerList;
  SimpleList MemoryList;
  volatile LONG MemoryListLock;
  SimpleList TlsList;
  volatile LONG TlsListLock;

  HMODULE Kernel32Base;

  /*
   * APIs
   */

  PVOID _AddVectoredContinueHandler;
  PVOID _AddVectoredExceptionHandler;
  PVOID _CloseHandle;
  PVOID _CreateEventA;
  PVOID _CreatePipe;
  PVOID _CreateThread;
  PVOID _ExitThread;
  PVOID _GetModuleHandleA;
  PVOID _GetProcAddress;
  PVOID _GetStdHandle;
  PVOID _GetThreadId;
  PVOID _MultiByteToWideChar;
  PVOID _OpenThread;
  PVOID _Process32NextW;
  PVOID _ReadFile;
  PVOID _RemoveVectoredContinueHandler;
  PVOID _RemoveVectoredExceptionHandler;
  PVOID _SetEvent;
  PVOID _SetUnhandledExceptionFilter;
  PVOID _Sleep;
  PVOID _TerminateThread;
  PVOID _TlsAlloc;
  PVOID _TlsFree;
  PVOID _VirtualAlloc;
  PVOID _VirtualFree;
  PVOID _WaitForSingleObject;
  PVOID _WideCharToMultiByte;
  PVOID _WriteFile;
  PVOID __vscprintf;
  PVOID _free;
  PVOID _malloc;
  PVOID _realloc;
  PVOID _vsprintf_s;
#ifdef _DEBUG
  PVOID _OutputDebugStringA;
#endif

  PVOID MyGetProcAddress;
} HookContext;

typedef enum _HookControl {
  HookControlCreate = 1,
  HookControlDelete,
  HookControlStart,
  HookControlStop
} HookControl;

typedef enum _HookResult {
  HookResultErrorInvalidControl = (int)0x80000000,
  HookResultErrorInvalidArgument,
  HookResultErrorNotYetCreated,
  HookResultErrorCreateLogs,
  HookResultErrorCreateProcessExitedEvent,
  HookResultErrorCreateCommandLine,
  HookResultErrorCreatePipeServers,
  HookResultErrorCreateNewThreadList,
  HookResultErrorCreateExceptionHandlerList,
  HookResultErrorCreateMemoryList,
  HookResultErrorCreateTlsList,
  HookResultErrorCreateKernel32Base,
  HookResultErrorStartNewThread,
  HookResultErrorStopWaitTimeout,
  HookResultErrorStopWaitLastThread,

  HookResultSuccess = 0,
} HookResult;

typedef HookResult (*HookControlEntryProc)(HookControl id, ...);
