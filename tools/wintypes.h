#pragma once
#if _MSC_VER
#define USE_WINNT
#include <windows.h>
#endif
#ifndef USE_WINNT

#ifndef __WIN_TYPES
#define __WIN_TYPES__

#ifdef _MSC_VER
    #include <stdint.h>
#else
    #include <inttypes.h>
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef BYTE
typedef unsigned char BYTE;
#endif

#ifndef BOOLEAN
   typedef BYTE BOOLEAN;
#endif

#ifndef BOOL
   typedef BYTE BOOL; 
#endif

#ifndef WORD
   typedef uint16_t WORD; 
#endif

#ifndef USHORT
   typedef uint16_t USHORT; 
#endif

#ifndef DWORD
   typedef uint32_t DWORD; 
#endif

#ifndef ULONG
  typedef uint32_t ULONG;
#endif

#ifndef ULONGLONG
   typedef int64_t ULONGLONG;
#endif

#ifndef ULONGULONG
   typedef uint64_t ULONGULONG;
#endif

#ifndef CHAR
    typedef char CHAR;
#endif

// wchar_t is 32-bits on Linux
#ifndef WCHAR
    typedef uint16_t WCHAR;
#endif

#ifndef VOID
    #define VOID void
    typedef char CHAR;
    typedef uint16_t SHORT;
    typedef uint32_t LONG;

    #if !defined(MIDL_PASS)
	typedef int INT;
    #endif

    #if defined(_WIN64)
    typedef __int64 INT_PTR, *PINT_PTR;
    typedef unsigned __int64 UINT_PTR, *PUINT_PTR;

    typedef __int64 LONG_PTR, *PLONG_PTR;
    typedef unsigned __int64 ULONG_PTR, *PULONG_PTR;
    #else
    typedef int INT_PTR, *PINT_PTR;
    typedef unsigned int UINT_PTR, *PUINT_PTR;

    typedef long LONG_PTR, *PLONG_PTR;
    typedef unsigned long ULONG_PTR, *PULONG_PTR;
    #endif

#endif //VOID

#ifndef FIELD_OFFSET
#define FIELD_OFFSET(type, field)    ((LONG)(LONG_PTR)&(((type *)0)->field))
#define UFIELD_OFFSET(type, field)    ((DWORD)(LONG_PTR)&(((type *)0)->field))
#endif

#endif // __WIN_TYPES__
#endif // #ifndef USE_WINNT
