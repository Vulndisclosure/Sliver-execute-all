#pragma once
#include "inlinecommon.hpp"
#include <windows.h>

/*==============================================================================
 * Import
 *============================================================================*/

#ifdef BOF
DFR(KERNEL32, CloseHandle)
#define CloseHandle KERNEL32$CloseHandle
DFR(KERNEL32, VirtualFree)
#define VirtualFree KERNEL32$VirtualFree
#ifdef _WIN64
DFR(KERNEL32, RtlDeleteFunctionTable)
#define RtlDeleteFunctionTable KERNEL32$RtlDeleteFunctionTable
#endif
#endif
