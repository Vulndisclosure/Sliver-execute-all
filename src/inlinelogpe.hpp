#pragma once
#include "inlinecommon.hpp"
#include <windows.h>

/*==============================================================================
 * Import
 *============================================================================*/

#ifdef BOF
DFR(KERNEL32, CloseHandle)
#define CloseHandle KERNEL32$CloseHandle
#endif
