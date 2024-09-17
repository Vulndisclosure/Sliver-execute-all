#pragma once
#include "inlinecommon.hpp"
#include <windows.h>

/*==============================================================================
 * Import
 *============================================================================*/

// clang-format off
WINBASEAPI NTSTATUS NTAPI NtFlushInstructionCache(HANDLE ProcessHandle, PVOID BaseAddress, ULONG NumberOfBytesToFlush);
#ifdef BOF
DFR(KERNEL32, CloseHandle)
#define CloseHandle KERNEL32$CloseHandle
DFR(KERNEL32, CreatePipe)
#define CreatePipe KERNEL32$CreatePipe
DFR(KERNEL32, GetModuleHandleA)
#define GetModuleHandleA KERNEL32$GetModuleHandleA
DFR(KERNEL32, GetProcAddress)
#define GetProcAddress KERNEL32$GetProcAddress
DFR(KERNEL32, LoadLibraryA)
#define LoadLibraryA KERNEL32$LoadLibraryA
DFR(KERNEL32, VirtualAlloc)
#define VirtualAlloc KERNEL32$VirtualAlloc
DFR(KERNEL32, VirtualFree)
#define VirtualFree KERNEL32$VirtualFree
DFR(KERNEL32, VirtualProtect)
#define VirtualProtect KERNEL32$VirtualProtect
DFR(KERNEL32, VirtualQuery)
#define VirtualQuery KERNEL32$VirtualQuery
DFR(NTDLL, NtFlushInstructionCache)
#define NtFlushInstructionCache NTDLL$NtFlushInstructionCache
#ifdef _WIN64
DFR(KERNEL32, RtlAddFunctionTable)
#define RtlAddFunctionTable KERNEL32$RtlAddFunctionTable
DFR(KERNEL32, RtlDeleteFunctionTable)
#define RtlDeleteFunctionTable KERNEL32$RtlDeleteFunctionTable
#endif
#endif
// clang-format on

#ifdef _WIN64

#ifdef _DEBUG
#include "hooks_x64d_exe.h"
#define HOOKS_EXE HOOKS_X64D_EXE
#else
#include "hooks_x64_exe.h"
#define HOOKS_EXE HOOKS_X64_EXE
#endif

#else

#ifdef _DEBUG
#include "hooks_x86d_exe.h"
#define HOOKS_EXE HOOKS_X86D_EXE
#else
#include "hooks_x86_exe.h"
#define HOOKS_EXE HOOKS_X86_EXE
#endif

#endif
