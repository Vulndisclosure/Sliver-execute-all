#pragma once
#include <windows.h>

LONG NTAPI ExGuard_ExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo);
void InitializeExceptionGuard();
